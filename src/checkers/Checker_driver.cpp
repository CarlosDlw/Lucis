#include "checkers/Checker.h"
#include "checkers/Checker_helpers.h"

#include "generated/LucisLexer.h"
#include "ffi/CBindings.h"
#include "ffi/CMacroEval.h"
#include "attributes/CfgPredicate.h"
#include "attributes/TargetInfo.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <array>
#include <filesystem>
#include <functional>
#include <fstream>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <vector>



static CfgPredicate parseCfgAttrArg(LucisParser::AttrArgContext* arg) {
    CfgPredicate pred;
    if (!arg) return pred;

    // Key-value: target_os = "linux"
    if (arg->ASSIGN()) {
        pred.type = CfgPredicate::KeyValue;
        if (arg->IDENTIFIER())
            pred.name = arg->IDENTIFIER()->getText();
        if (auto* val = arg->attrArg()) {
            if (val->STR_LIT()) {
                auto raw = val->STR_LIT()->getText();
                if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"')
                    pred.stringValue = raw.substr(1, raw.size() - 2);
                else
                    pred.stringValue = raw;
            } else if (val->IDENTIFIER()) {
                pred.stringValue = val->IDENTIFIER()->getText();
            }
        }
        return pred;
    }

    // Function call: all(...), any(...), not(...)
    if (arg->LPAREN()) {
        pred.type = CfgPredicate::Call;
        if (arg->IDENTIFIER())
            pred.name = arg->IDENTIFIER()->getText();
        if (auto* argList = arg->attrArgList()) {
            for (auto* child : argList->attrArg())
                pred.args.push_back(parseCfgAttrArg(child));
        }
        return pred;
    }

    // Bare identifier: unix, windows, debug, etc.
    if (arg->IDENTIFIER()) {
        pred.type = CfgPredicate::Ident;
        pred.name = arg->IDENTIFIER()->getText();
        return pred;
    }

    return pred;
}

static CfgPredicate parseCfgFromAttribute(const Attribute& attr) {
    if (!attr.rawCtx) {
        CfgPredicate p;
        p.type = CfgPredicate::Ident;
        p.name = "true";
        return p;
    }
    // rawCtx is an AttributeContext*; dynamic_cast won't work cross-class
    // so use getRuleContext to find the child AttrArgListContext.
    auto* listCtx = attr.rawCtx->getRuleContext<LucisParser::AttrArgListContext>(0);
    if (!listCtx) {
        CfgPredicate p;
        p.type = CfgPredicate::Ident;
        p.name = "true";
        return p;
    }

    auto args = listCtx->attrArg();
    if (args.empty()) {
        CfgPredicate p;
        p.type = CfgPredicate::Ident;
        p.name = "true";
        return p;
    }

    // Single argument: use directly
    if (args.size() == 1)
        return parseCfgAttrArg(args[0]);

    // Multiple arguments: implicit all(...)
    CfgPredicate allPred;
    allPred.type = CfgPredicate::Call;
    allPred.name = "all";
    for (auto* a : args)
        allPred.args.push_back(parseCfgAttrArg(a));
    return allPred;
}

// ── Check if a declaration is active (not disabled by #[cfg(false)]) ───


bool Checker::isDeclActive(AttributeListContext* attrs) const {
    if (!attrs) return true;
    TargetInfo target(targetTriple_, debugMode_);
    for (auto* a : attrs->attribute()) {
        if (!a->IDENTIFIER()) continue;
        if (a->IDENTIFIER()->getText() != "cfg") continue;
        auto attrName = a->IDENTIFIER()->getText();
        auto* handler = attrRegistry_.lookup(attrName);
        if (!handler) continue;
        Attribute attr;
        attr.name = attrName;
        attr.rawCtx = a;
        auto pred = parseCfgFromAttribute(attr);
        if (!pred.evaluate(target))
            return false;
    }
    return true;
}

// ── Print target cfg info (--print-cfg) ──────────────────────────────


void Checker::printCfgInfo() const {
    TargetInfo target(targetTriple_, debugMode_);
    std::cerr << "Target triple: " << targetTriple_ << "\n";
    std::cerr << "  target_os:             " << target.get("target_os") << "\n";
    std::cerr << "  target_arch:           " << target.get("target_arch") << "\n";
    std::cerr << "  target_endian:         " << target.get("target_endian") << "\n";
    std::cerr << "  target_pointer_width:  " << target.get("target_pointer_width") << "\n";
    std::cerr << "  target_os_family:      " << target.get("target_os_family") << "\n";
    std::cerr << "  target_triple:         " << target.get("target_triple") << "\n";
    std::cerr << "  debug_assertions:      " << (debugMode_ ? "true" : "false") << "\n";
}

// ── Checker constructor ───────────────────────────────────────────────


Checker::Checker()
    : intrinsicRegistry_(typeRegistry_) {
    registerGlobalBuiltins();
    registerBuiltinAttributes(attrRegistry_);

    // Register #[cfg(...)] — special attribute that controls conditional compilation.
    attrRegistry_.registerAttribute("cfg", AttributeHandler{
        .validate = [](const Attribute& attr, const TypeInfo*, std::vector<std::string>& errors) -> bool {
            if (attr.rawCtx) {
                auto pred = parseCfgFromAttribute(attr);
                if (pred.type == CfgPredicate::Call && pred.name != "all" &&
                    pred.name != "any" && pred.name != "not") {
                    errors.push_back("unknown cfg predicate '" + pred.name + "'");
                    return false;
                }
                // Check 'not' has exactly 1 argument
                if (pred.type == CfgPredicate::Call && pred.name == "not" && pred.args.size() != 1) {
                    errors.push_back("'not' predicate requires exactly 1 argument");
                    return false;
                }
            }
            return true;
        }
    });
}



bool Checker::check(LucisParser::ProgramContext* tree) {
    if (!tree) {
        errors_.push_back("empty program");
        return false;
    }

    // Pass 0: register global builtins (always available, no import needed)
    registerGlobalBuiltins();

    // Phase 1: mirror builtins into SemanticDB
    initSemanticDB();

    // Pass 0.5: register C header bindings (from #include directives)
    // Determine which C functions are overridden by Lucis imports
    // based on preamble declaration order (last import wins).
    std::unordered_set<std::string> lucisOverridesC;
    if (cBindings_) {
        int lastIncludePos = -1;
        std::unordered_map<std::string, int> lucisImportPos;
        int pos = 0;
        for (auto* pre : tree->preambleDecl()) {
            if (pre->includeDecl()) {
                lastIncludePos = pos;
            } else if (auto* ud = pre->useDecl()) {
                if (auto* item = dynamic_cast<LucisParser::UseItemContext*>(ud)) {
                    if (item->IDENTIFIER()) {
                        auto n = item->IDENTIFIER()->getText();
                        if (cBindings_->findFunction(n))
                            lucisImportPos[n] = pos;
                    }
                } else if (auto* group = dynamic_cast<LucisParser::UseGroupContext*>(ud)) {
                    for (auto* id : group->IDENTIFIER()) {
                        auto n = id->getText();
                        if (cBindings_->findFunction(n))
                            lucisImportPos[n] = pos;
                    }
                }
            }
            pos++;
        }
        for (auto& [n, lucisPos] : lucisImportPos) {
            if (lucisPos > lastIncludePos)
                lucisOverridesC.insert(n);
        }
    }

    // Only register C bindings from #include directives when this file
    // actually includes C headers — prevents symbols from one file's
    // #include leaking into another file's scope.
    bool hasCInclude = false;
    for (auto* pre : tree->preambleDecl()) {
        if (pre->includeDecl()) { hasCInclude = true; break; }
    }

    if (cBindings_) {
        // Register C structs as types
        for (auto& [name, cstruct] : cBindings_->structs()) {
            if (!typeRegistry_.lookup(name)) {
                TypeInfo ti;
                ti.name = name;
                ti.kind = TypeKind::Struct;
                ti.bitWidth = 0;
                ti.isSigned = false;
                ti.fields = cstruct.fields;
                typeRegistry_.registerType(std::move(ti));
            }
        }

        // Register C enums as types + enum constant globals
        for (auto& [name, cenum] : cBindings_->enums()) {
            if (!typeRegistry_.lookup(name)) {
                TypeInfo ti;
                ti.name = name;
                ti.kind = TypeKind::Enum;
                ti.bitWidth = 32;
                ti.isSigned = false;
                ti.builtinSuffix = "i32";
                for (auto& [vname, val] : cenum.values)
                    ti.enumVariants.push_back(vname);
                typeRegistry_.registerType(std::move(ti));
            }
            // Register each enum constant as a known variable
            for (auto& [vname, val] : cenum.values) {
                auto* enumTI = typeRegistry_.lookup(name);
                if (enumTI)
                    cEnumConstants_[vname] = { enumTI, val };
            }
        }

        // Register C typedefs as type aliases
        for (auto& [name, ctdef] : cBindings_->typedefs()) {
            if (!typeRegistry_.lookup(name)) {
                TypeInfo ti = *ctdef.underlying;
                ti.name = name;
                typeRegistry_.registerType(std::move(ti));
            }
        }

        // Register C functions (skip those overridden by Lucis imports)
        for (auto& [name, cfunc] : cBindings_->functions()) {
            if (lucisOverridesC.count(name)) continue;
            auto* funcType = makeFunctionType(
                cfunc.returnType, cfunc.paramTypes, cfunc.isVariadic);
            functions_[name] = funcType;
            globalBuiltins_.insert(name);
        }

        // Register C #define constants (integer, float, string)
        for (auto& [name, cmacro] : cBindings_->macros()) {
            if (cmacro.isFloat) {
                auto* f64TI = typeRegistry_.lookup("float64");
                if (f64TI)
                    cEnumConstants_[name] = { f64TI, 0, cmacro.floatValue, true, false };
            } else if (cmacro.isString) {
                auto* charTI = typeRegistry_.lookup("char");
                if (charTI) {
                    // String macros are *char (pointer to char)
                    auto ptrName = "*" + charTI->name;
                    auto* ptrTI = typeRegistry_.lookup(ptrName);
                    if (!ptrTI) {
                        TypeInfo ti;
                        ti.name = ptrName;
                        ti.kind = TypeKind::Pointer;
                        ti.pointeeType = charTI;
                        ti.bitWidth = 0;
                        ti.isSigned = false;
                        ti.builtinSuffix = "cstr";  // *char uses cstr suffix for std::log
                        typeRegistry_.registerType(std::move(ti));
                        ptrTI = typeRegistry_.lookup(ptrName);
                    }
                    if (ptrTI)
                        cEnumConstants_[name] = { ptrTI, 0, 0.0, false, true };
                }
            } else {
                auto* int32TI = typeRegistry_.lookup("int32");
                if (int32TI)
                    cEnumConstants_[name] = { int32TI, cmacro.value };
            }
        }

        // Register C #define struct literal constants (e.g. RAYWHITE)
        for (auto& [name, sm] : cBindings_->structMacros()) {
            auto* structTI = typeRegistry_.lookup(sm.structType);
            if (structTI)
                cEnumConstants_[name] = { structTI, 0 };
        }

        // Register C global variables
        for (auto& [name, gvar] : cBindings_->globals()) {
            cGlobals_[name] = gvar.type;
        }

        // Register C function-like macros
        for (auto& [name, flm] : cBindings_->functionLikeMacros()) {
            cFunctionLikeMacros_[name] = &flm;
        }
    }

    // Pass 1: register all `use` declarations
    checkUseDecls(tree);

    // Pass 1.5: register cross-file structs, enums, and functions
    // from same module and user imports, so type resolution works.
    // Process in dependency order: struct/union skeletons first, then enums/typeAliases.
    if (moduleRegistry_) {
        // First pass: register struct/union skeletons so enum payloads can reference them.
        // Skip generic types (they have typeParamList) to prevent opaque skeletons
        // from masking missing type arguments.
        for (auto* decl : tree->topLevelDecl()) {
            if (auto* sd = decl->structDecl()) {
                auto name = sd->IDENTIFIER()->getText();
                if (!typeRegistry_.lookup(name) && !genericStructTemplates_.count(name) && !sd->typeParamList()) {
                    TypeInfo skeleton;
                    skeleton.name = name;
                    skeleton.kind = TypeKind::Struct;
                    skeleton.bitWidth = 0;
                    skeleton.isSigned = false;
                    typeRegistry_.registerType(std::move(skeleton));
                }
            } else if (auto* ud = decl->unionDecl()) {
                auto name = ud->IDENTIFIER()->getText();
                if (!typeRegistry_.lookup(name) && !genericUnionTemplates_.count(name) && !ud->typeParamList()) {
                    TypeInfo skeleton;
                    skeleton.name = name;
                    skeleton.kind = TypeKind::Union;
                    skeleton.bitWidth = 0;
                    skeleton.isSigned = false;
                    typeRegistry_.registerType(std::move(skeleton));
                }
            } else if (auto* bd = decl->bitstructDecl()) {
                auto name = bd->IDENTIFIER()->getText();
                if (!typeRegistry_.lookup(name) && !bd->typeParamList()) {
                    TypeInfo skeleton;
                    skeleton.name = name;
                    skeleton.kind = TypeKind::BitStruct;
                    skeleton.bitWidth = 0;
                    skeleton.isSigned = false;
                    typeRegistry_.registerType(std::move(skeleton));
                }
            }
        }

        // Pre-register imported enums so type aliases referencing them resolve
        for (auto& [symName, ns] : userImports_) {
            auto* sym = moduleRegistry_->findSymbol(ns, symName);
            if (!sym || sym->kind != ExportedSymbol::Enum) continue;
            if (!typeRegistry_.lookup(sym->name) && !genericEnumTemplates_.count(sym->name)) {
                auto* ed = static_cast<LucisParser::EnumDeclContext*>(sym->decl);
                checkEnumDecl(ed);
            }
        }

        // Second pass: enums and type aliases (struct/union skeletons now available)
        for (auto* decl : tree->topLevelDecl()) {
            if (auto* ed = decl->enumDecl()) {
                auto name = ed->IDENTIFIER()->getText();
                if (!typeRegistry_.lookup(name) && !genericEnumTemplates_.count(name))
                    checkEnumDecl(ed);
            } else if (auto* ta = decl->typeAliasDecl()) {
                auto name = ta->IDENTIFIER()->getText();
                if (!typeRegistry_.lookup(name))
                    checkTypeAliasDecl(ta);
            }
        }

        // Same-module external symbols
        auto extSyms = moduleRegistry_->getExternalSymbols(
            currentModulePath_, currentFile_);

        // Pre-register skeletons for external structs and unions so recursive
        // dependencies do not trigger duplicate full type registration.
        // Skip generic types to prevent opaque skeletons from masking missing
        // type arguments at the use site.
        for (auto* sym : extSyms) {
            if (sym->kind == ExportedSymbol::Struct &&
                !typeRegistry_.lookup(sym->name) &&
                !genericStructTemplates_.count(sym->name)) {
                // Check if the struct is generic (has type parameters)
                if (auto* sd = dynamic_cast<LucisParser::StructDeclContext*>(sym->decl)) {
                    if (sd->typeParamList())
                        continue;
                }
                TypeInfo skeleton;
                skeleton.name = sym->name;
                skeleton.kind = TypeKind::Struct;
                skeleton.bitWidth = 0;
                skeleton.isSigned = false;
                typeRegistry_.registerType(std::move(skeleton));
                // Phase 1: forward-declare in SemanticDB
                if (semanticDB_)
                    semanticDB_->forwardDeclare(sym->name, semantic::DeclKind::Struct,
                                                sym->modulePath,
                                                toSemanticLoc(sym->decl));
            } else if (sym->kind == ExportedSymbol::Union &&
                       !typeRegistry_.lookup(sym->name) &&
                       !genericUnionTemplates_.count(sym->name)) {
                // Check if the union is generic (has type parameters)
                if (auto* ud = dynamic_cast<LucisParser::UnionDeclContext*>(sym->decl)) {
                    if (ud->typeParamList())
                        continue;
                }
                TypeInfo skeleton;
                skeleton.name = sym->name;
                skeleton.kind = TypeKind::Union;
                skeleton.bitWidth = 0;
                skeleton.isSigned = false;
                typeRegistry_.registerType(std::move(skeleton));
                // Phase 1: forward-declare in SemanticDB
                if (semanticDB_)
                    semanticDB_->forwardDeclare(sym->name, semantic::DeclKind::Union,
                                                sym->modulePath,
                                                toSemanticLoc(sym->decl));
            } else if (sym->kind == ExportedSymbol::BitStruct &&
                       !typeRegistry_.lookup(sym->name)) {
                // Check if the bitstruct is generic (has type parameters)
                if (auto* bd = dynamic_cast<LucisParser::BitstructDeclContext*>(sym->decl)) {
                    if (bd->typeParamList())
                        continue;
                }
                TypeInfo skeleton;
                skeleton.name = sym->name;
                skeleton.kind = TypeKind::BitStruct;
                skeleton.bitWidth = 0;
                skeleton.isSigned = false;
                typeRegistry_.registerType(std::move(skeleton));
            }
        }
        for (auto& [symName, ns] : userImports_) {
            auto* sym = moduleRegistry_->findSymbol(ns, symName);
            if (!sym) continue;
            if (sym->kind == ExportedSymbol::Struct &&
                !typeRegistry_.lookup(sym->name) &&
                !genericStructTemplates_.count(sym->name)) {
                // Check if the struct is generic (has type parameters)
                if (auto* sd = dynamic_cast<LucisParser::StructDeclContext*>(sym->decl)) {
                    if (sd->typeParamList())
                        continue;
                }
                TypeInfo skeleton;
                skeleton.name = sym->name;
                skeleton.kind = TypeKind::Struct;
                skeleton.bitWidth = 0;
                skeleton.isSigned = false;
                typeRegistry_.registerType(std::move(skeleton));
            } else if (sym->kind == ExportedSymbol::Union &&
                       !typeRegistry_.lookup(sym->name) &&
                       !genericUnionTemplates_.count(sym->name)) {
                // Check if the union is generic (has type parameters)
                if (auto* ud = dynamic_cast<LucisParser::UnionDeclContext*>(sym->decl)) {
                    if (ud->typeParamList())
                        continue;
                }
                TypeInfo skeleton;
                skeleton.name = sym->name;
                skeleton.kind = TypeKind::Union;
                skeleton.bitWidth = 0;
                skeleton.isSigned = false;
                typeRegistry_.registerType(std::move(skeleton));
            } else if (sym->kind == ExportedSymbol::BitStruct &&
                       !typeRegistry_.lookup(sym->name)) {
                if (auto* bd = dynamic_cast<LucisParser::BitstructDeclContext*>(sym->decl)) {
                    if (bd->typeParamList())
                        continue;
                }
                TypeInfo skeleton;
                skeleton.name = sym->name;
                skeleton.kind = TypeKind::BitStruct;
                skeleton.bitWidth = 0;
                skeleton.isSigned = false;
                typeRegistry_.registerType(std::move(skeleton));
            }
        }

        // The topological sort below handles full registration.
        // Skeletons are registered by checkStructDecl internally.

        // Imported function signatures may reference generic types (e.g. Result<T>)
        // that were not explicitly imported via `use`. Ensure those type templates
        // are registered before function signature/type resolution.
        std::unordered_set<std::string> seenDeps;  // prevent cycles
        auto ensureTypeDependencyFromSpec =
            [&](auto&& self, LucisParser::TypeSpecContext* ts, const std::string& ns) -> void {
                if (!ts) return;
                if (ts->fnTypeSpec()) {
                    for (auto* inner : ts->fnTypeSpec()->typeSpec())
                        self(self, inner, ns);
                    return;
                }
                for (auto* inner : ts->typeSpec())
                    self(self, inner, ns);

                if (!ts->IDENTIFIER()) return;
                auto baseName = ts->IDENTIFIER()->getText();

                auto* depSym = moduleRegistry_->findSymbol(ns, baseName);
                if (!depSym) {
                    for (auto& candidateNs : moduleRegistry_->allModules()) {
                        depSym = moduleRegistry_->findSymbol(candidateNs, baseName);
                        if (depSym) break;
                    }
                }
                if (!depSym) return;
                if (depSym->sourceFile == currentFile_) return;

                // Check if the type needs processing (not registered, or only a skeleton)
                auto* existingTI = typeRegistry_.lookup(baseName);
                bool needsProc = !existingTI;
                if (existingTI && existingTI->kind == TypeKind::Struct &&
                    existingTI->fields.empty() && existingTI->bitWidth == 0)
                    needsProc = true;
                if (existingTI && existingTI->kind == TypeKind::Union &&
                    existingTI->fields.empty() && existingTI->bitWidth == 0)
                    needsProc = true;
                if (existingTI && existingTI->kind == TypeKind::Enum &&
                    existingTI->enumVariantInfos.empty())
                    needsProc = true;
                if (existingTI && existingTI->kind == TypeKind::BitStruct &&
                    existingTI->bitFields.empty())
                    needsProc = true;

                if (depSym->kind == ExportedSymbol::Enum &&
                    needsProc &&
                    !genericEnumTemplates_.count(baseName)) {
                    if (!seenDeps.insert(baseName).second) return;
                    auto* ed = static_cast<LucisParser::EnumDeclContext*>(depSym->decl);
                    // Pre-resolve payload types before checking the enum
                    for (auto* variant : ed->enumVariant())
                        for (auto* ts : variant->typeSpec())
                            self(self, ts, ns);
                    checkEnumDecl(ed);
                    return;
                }
                // Process struct dependency even if skeleton exists but
                // template isn't registered yet (e.g. LinkedListNode<T>
                // discovered via LinkedList<T>'s field processing).
                if (depSym->kind == ExportedSymbol::Struct &&
                    needsProc &&
                    !genericStructTemplates_.count(baseName)) {
                    if (!seenDeps.insert(baseName).second) return;
                    auto* sd = static_cast<LucisParser::StructDeclContext*>(depSym->decl);
                    // Pre-resolve field types before checking the struct
                    for (auto* field : sd->structField())
                        self(self, field->typeSpec(), ns);
            // Pre-resolve parent struct if inheriting
            if (sd->COLON() && sd->typeSpec()) {
                unsigned parentDims = 0;
                auto* parentTI = resolveTypeSpec(sd->typeSpec(), parentDims);
                if (parentTI && parentTI->kind == TypeKind::Struct &&
                    !genericStructTemplates_.count(parentTI->name) &&
                    !seenDeps.count(parentTI->name)) {
                    auto* parentSym = moduleRegistry_->findSymbol(ns, parentTI->name);
                    if (!parentSym) {
                        for (auto& candidateNs : moduleRegistry_->allModules()) {
                            parentSym = moduleRegistry_->findSymbol(candidateNs, parentTI->name);
                            if (parentSym) break;
                        }
                    }
                    if (parentSym && parentSym->kind == ExportedSymbol::Struct) {
                        auto* parentDecl = static_cast<LucisParser::StructDeclContext*>(parentSym->decl);
                        for (auto* pf : parentDecl->structField())
                            self(self, pf->typeSpec(), parentSym->modulePath);
                        registeringImported_ = true;
                        checkStructDecl(parentDecl);
                        registeringImported_ = false;
                    }
                }
            }
                    registeringImported_ = true;
                    checkStructDecl(sd);
                    registeringImported_ = false;
                    return;
                }
                if (depSym->kind == ExportedSymbol::Union &&
                    needsProc &&
                    !genericUnionTemplates_.count(baseName)) {
                    if (!seenDeps.insert(baseName).second) return;
                    auto* ud = static_cast<LucisParser::UnionDeclContext*>(depSym->decl);
                    // Pre-resolve field types before checking the union
                    for (auto* field : ud->unionField())
                        self(self, field->typeSpec(), ns);
                    registeringImported_ = true;
                    checkUnionDecl(ud);
                    registeringImported_ = false;
                    return;
                }
                if (depSym->kind == ExportedSymbol::BitStruct &&
                    needsProc &&
                    !genericStructTemplates_.count(baseName)) {
                    if (!seenDeps.insert(baseName).second) return;
                    auto* bd = static_cast<LucisParser::BitstructDeclContext*>(depSym->decl);
                    registeringImported_ = true;
                    checkBitStructDecl(bd);
                    registeringImported_ = false;
                    return;
                }
                if (depSym->kind == ExportedSymbol::TypeAlias && needsProc) {
                    if (!seenDeps.insert(baseName).second) return;
                    auto* ta = static_cast<LucisParser::TypeAliasDeclContext*>(depSym->decl);
                    // Pre-resolve the underlying type before checking the alias
                    self(self, ta->typeSpec(), ns);
                    checkTypeAliasDecl(ta);
                    return;
                }
            };

        auto ensureFunctionTypeDependencies =
            [&](LucisParser::FunctionDeclContext* decl, const std::string& ns) {
                if (!decl) return;
                ensureTypeDependencyFromSpec(ensureTypeDependencyFromSpec, decl->typeSpec(), ns);
                if (auto* params = decl->paramList()) {
                    for (auto* p : params->param())
                        ensureTypeDependencyFromSpec(ensureTypeDependencyFromSpec, p->typeSpec(), ns);
                }
            };

        // Phase A: enums and type aliases (these have no dependencies)
        for (auto* sym : extSyms) {
            if (sym->kind == ExportedSymbol::Enum) {
                if (typeRegistry_.lookup(sym->name) || genericEnumTemplates_.count(sym->name))
                    continue;
                auto* decl = static_cast<LucisParser::EnumDeclContext*>(sym->decl);
                checkEnumDecl(decl);
            } else if (sym->kind == ExportedSymbol::TypeAlias) {
                if (typeRegistry_.lookup(sym->name))
                    continue;
                auto* decl = static_cast<LucisParser::TypeAliasDeclContext*>(sym->decl);
                // Ensure transitive generic dependencies (e.g. Result<T>) are registered
                ensureTypeDependencyFromSpec(ensureTypeDependencyFromSpec,
                                             decl->typeSpec(), currentModulePath_);
                checkTypeAliasDecl(decl);
            }
        }
        for (auto& [symName, ns] : userImports_) {
            auto* sym = moduleRegistry_->findSymbol(ns, symName);
            if (!sym) continue;
            if (sym->kind == ExportedSymbol::Enum) {
                if (typeRegistry_.lookup(sym->name) || genericEnumTemplates_.count(sym->name))
                    continue;
                auto* decl = static_cast<LucisParser::EnumDeclContext*>(sym->decl);
                checkEnumDecl(decl);
            } else if (sym->kind == ExportedSymbol::TypeAlias) {
                if (typeRegistry_.lookup(sym->name))
                    continue;
                auto* decl = static_cast<LucisParser::TypeAliasDeclContext*>(sym->decl);
                // Ensure transitive generic dependencies
                ensureTypeDependencyFromSpec(ensureTypeDependencyFromSpec,
                                             decl->typeSpec(), ns);
                checkTypeAliasDecl(decl);
            }
        }

        // Phase B: unions (no inheritance, single pass)
        for (auto* sym : extSyms) {
            if (sym->kind == ExportedSymbol::Union) {
                if (genericUnionTemplates_.count(sym->name))
                    continue;
                auto* existing = typeRegistry_.lookup(sym->name);
                if (existing && !(existing->kind == TypeKind::Union && existing->fields.empty() && existing->bitWidth == 0))
                    continue;
                auto* decl = static_cast<LucisParser::UnionDeclContext*>(sym->decl);
                for (auto* field : decl->unionField())
                    ensureTypeDependencyFromSpec(
                        ensureTypeDependencyFromSpec, field->typeSpec(), currentModulePath_);
                checkUnionDecl(decl);
            }
        }
        
        // Phase B: structs (retry loop for parent ordering, covers both
        // same-module external symbols and cross-module imported symbols)
        {
            // Collect all structs: same-module + imported + transitive parents
            struct PendingStruct {
                LucisParser::StructDeclContext* decl;
                std::string ns;
            };
            std::unordered_map<std::string, PendingStruct> pendingMap;
            for (auto* sym : extSyms) {
                if (sym->kind == ExportedSymbol::Struct && !genericStructTemplates_.count(sym->name)) {
                    pendingMap[sym->name] = {
                        static_cast<LucisParser::StructDeclContext*>(sym->decl),
                        currentModulePath_
                    };
                }
            }
            for (auto& [symName, ns] : userImports_) {
                auto* sym = moduleRegistry_->findSymbol(ns, symName);
                if (sym && sym->kind == ExportedSymbol::Struct && !genericStructTemplates_.count(symName)) {
                    pendingMap[symName] = {
                        static_cast<LucisParser::StructDeclContext*>(sym->decl), ns
                    };
                }
            }
            // Add transitive parent dependencies
            bool added = true;
            while (added) {
                added = false;
                for (auto& [name, ps] : pendingMap) {
                    if (ps.decl->COLON() && ps.decl->typeSpec()) {
                        unsigned parentDims = 0;
                        auto* parentTI = resolveTypeSpec(ps.decl->typeSpec(), parentDims);
                        std::string parentName = parentTI ? parentTI->name : "";
                        if (!parentName.empty() && !pendingMap.count(parentName)) {
                            auto* parentSym = moduleRegistry_->findSymbol(ps.ns, parentName);
                            if (!parentSym) {
                                for (auto& mod : moduleRegistry_->allModules()) {
                                    parentSym = moduleRegistry_->findSymbol(mod, parentName);
                                    if (parentSym) break;
                                }
                            }
                            if (parentSym && parentSym->kind == ExportedSymbol::Struct
                                && !genericStructTemplates_.count(parentName)) {
                                pendingMap[parentName] = {
                                    static_cast<LucisParser::StructDeclContext*>(parentSym->decl),
                                    parentSym->modulePath
                                };
                                added = true;
                            }
                        }
                    }
                }
            }

            std::unordered_set<std::string> processed;
            bool madeProgress = true;
            while (madeProgress) {
                madeProgress = false;
                for (auto& [name, ps] : pendingMap) {
                    if (processed.count(name)) continue;
                    auto* existing = typeRegistry_.lookup(name);
                    if (existing && !(existing->kind == TypeKind::Struct && existing->fields.empty() && existing->bitWidth == 0)) {
                        processed.insert(name);
                        continue;
                    }

            if (ps.decl->COLON() && ps.decl->typeSpec()) {
                unsigned parentDims = 0;
                auto* parentTI = resolveTypeSpec(ps.decl->typeSpec(), parentDims);
                if (!parentTI || parentTI->kind != TypeKind::Struct || parentTI->fields.empty())
                    continue;
            }

                    for (auto* field : ps.decl->structField())
                        ensureTypeDependencyFromSpec(
                            ensureTypeDependencyFromSpec, field->typeSpec(), ps.ns);
                    if (ps.decl->typeParamList() && genericStructTemplates_.count(name))
                        continue;
                    checkStructDecl(ps.decl);
                    processed.insert(name);
                    madeProgress = true;
                }
            }
        }

        // Phase B unions: user imports (no inheritance, single pass)
        for (auto& [symName, ns] : userImports_) {
            auto* sym = moduleRegistry_->findSymbol(ns, symName);
            if (!sym || sym->kind != ExportedSymbol::Union) continue;
            if (genericUnionTemplates_.count(sym->name)) continue;
            auto* existing = typeRegistry_.lookup(sym->name);
            if (existing && !(existing->kind == TypeKind::Union && existing->fields.empty() && existing->bitWidth == 0))
                continue;
            auto* decl = static_cast<LucisParser::UnionDeclContext*>(sym->decl);
            for (auto* field : decl->unionField())
                ensureTypeDependencyFromSpec(
                    ensureTypeDependencyFromSpec, field->typeSpec(), ns);
            auto* recheck = typeRegistry_.lookup(sym->name);
            if (!recheck || (recheck->kind == TypeKind::Union && recheck->fields.empty() && recheck->bitWidth == 0))
                checkUnionDecl(decl);
        }

        // Phase B.5: extend blocks for imported structs (auto-resolved)
        // When a struct is imported, its extend blocks from the same module
        // must also be registered so methods like Type::new() work.
        for (auto* sym : extSyms) {
            if (sym->kind == ExportedSymbol::ExtendBlock) {
                auto* decl = static_cast<LucisParser::ExtendDeclContext*>(sym->decl);
                checkExtendDecl(decl);
            }
        }
        for (auto& [symName, ns] : userImports_) {
            // For each imported struct, look for its extend block in the same ns
            auto* sym = moduleRegistry_->findSymbol(ns, symName);
            if (!sym || (sym->kind != ExportedSymbol::Struct &&
                         sym->kind != ExportedSymbol::Union))
                continue;
            // Find extend block with the same name in the same module
            auto nsSyms = moduleRegistry_->getModuleSymbols(ns);
            for (auto* nsSym : nsSyms) {
                if (nsSym->kind == ExportedSymbol::ExtendBlock &&
                    nsSym->name == symName) {
                    auto* extDecl = static_cast<LucisParser::ExtendDeclContext*>(nsSym->decl);
                    // Pre-register types referenced in extend method signatures
                    // (e.g. return types like ReadFileResult) so resolveTypeSpec
                    // can find them when checkExtendDecl processes the block.
                    for (auto* method : extDecl->extendMethod()) {
                        ensureTypeDependencyFromSpec(
                            ensureTypeDependencyFromSpec, method->typeSpec(), ns);
                        for (auto* p : method->param()) {
                            ensureTypeDependencyFromSpec(
                                ensureTypeDependencyFromSpec, p->typeSpec(), ns);
                        }
                        if (auto* pl = method->paramList()) {
                            for (auto* p : pl->param()) {
                                ensureTypeDependencyFromSpec(
                                    ensureTypeDependencyFromSpec, p->typeSpec(), ns);
                            }
                        }
                    }
                    checkExtendDecl(extDecl);
                }
            }
        }

        // Phase B.6: trait declarations from imported modules
        for (auto* sym : extSyms) {
            if (sym->kind == ExportedSymbol::TraitDecl) {
                auto* decl = static_cast<LucisParser::TraitDeclContext*>(sym->decl);
                checkTraitDecl(decl);
            }
        }
        for (auto& [symName, ns] : userImports_) {
            auto* sym = moduleRegistry_->findSymbol(ns, symName);
            if (!sym || sym->kind != ExportedSymbol::TraitDecl) continue;
            auto* traitDecl = static_cast<LucisParser::TraitDeclContext*>(sym->decl);
            checkTraitDecl(traitDecl);
        }

        // Phase B.7: impl blocks from imported modules
        for (auto* sym : extSyms) {
            if (sym->kind == ExportedSymbol::ImplBlock) {
                auto* decl = static_cast<LucisParser::ImplDeclContext*>(sym->decl);
                // Pre-register type dependencies
                for (auto* method : decl->implMethod()) {
                    ensureTypeDependencyFromSpec(
                        ensureTypeDependencyFromSpec, method->typeSpec(),
                        currentModulePath_);
                    for (auto* p : method->param()) {
                        ensureTypeDependencyFromSpec(
                            ensureTypeDependencyFromSpec, p->typeSpec(),
                            currentModulePath_);
                    }
                    if (auto* pl = method->paramList()) {
                        for (auto* p : pl->param()) {
                            ensureTypeDependencyFromSpec(
                                ensureTypeDependencyFromSpec, p->typeSpec(),
                                currentModulePath_);
                        }
                    }
                }
                checkImplDecl(decl);
            }
        }
        for (auto& [symName, ns] : userImports_) {
            auto* sym = moduleRegistry_->findSymbol(ns, symName);
            if (!sym || (sym->kind != ExportedSymbol::Struct &&
                         sym->kind != ExportedSymbol::Union &&
                         sym->kind != ExportedSymbol::Enum))
                continue;
            auto nsSyms = moduleRegistry_->getModuleSymbols(ns);
            for (auto* nsSym : nsSyms) {
                if (nsSym->kind != ExportedSymbol::ImplBlock) continue;
                // Check if the impl block's type matches the imported type
                auto* implDecl = static_cast<LucisParser::ImplDeclContext*>(nsSym->decl);
                auto implTypeName = implDecl->typeSpec(1)->getText();
                if (implTypeName == symName) {
                    checkImplDecl(implDecl);
                }
            }
        }

        // Phase C: functions (may reference types from phases A and B)
        for (auto* sym : extSyms) {
            if (sym->kind == ExportedSymbol::Function) {
                auto* decl = static_cast<LucisParser::FunctionDeclContext*>(sym->decl);
                ensureFunctionTypeDependencies(decl, currentModulePath_);
                registerFunctionSignature(decl);
            }
        }
        for (auto& [symName, ns] : userImports_) {
            auto* sym = moduleRegistry_->findSymbol(ns, symName);
            if (!sym) continue;
            if (sym->kind == ExportedSymbol::Function) {
                auto* decl = static_cast<LucisParser::FunctionDeclContext*>(sym->decl);
                ensureFunctionTypeDependencies(decl, ns);
                registerFunctionSignature(decl);
            }
        }
    }

    // Pass 2/3: local type registration.
    if (!moduleRegistry_) {
        for (auto* decl : tree->topLevelDecl()) {
            if (auto* ta = decl->typeAliasDecl()) {
                checkTypeAliasDecl(ta);
            }
        }

        for (auto* decl : tree->topLevelDecl()) {
            if (auto* sd = decl->structDecl()) {
                auto name = sd->IDENTIFIER()->getText();
                if (sd->typeParamList() && genericStructTemplates_.count(name))
                    continue;
                auto* existing = typeRegistry_.lookup(name);
                if (existing && existing->kind == TypeKind::Struct &&
                    !existing->fields.empty())
                    continue;
                checkStructDecl(sd);
            } else if (auto* ud = decl->unionDecl()) {
                auto name = ud->IDENTIFIER()->getText();
                if (ud->typeParamList() && genericUnionTemplates_.count(name))
                    continue;
                auto* existing = typeRegistry_.lookup(name);
                if (existing && existing->kind == TypeKind::Union &&
                    !existing->fields.empty())
                    continue;
                checkUnionDecl(ud);
            } else if (auto* ed = decl->enumDecl()) {
                auto name = ed->IDENTIFIER()->getText();
                if (ed->typeParamList() && genericEnumTemplates_.count(name))
                    continue;
                auto* existing = typeRegistry_.lookup(name);
                if (existing && existing->kind == TypeKind::Enum &&
                    !existing->enumVariantInfos.empty())
                    continue;
                checkEnumDecl(ed);
            } else if (auto* bd = decl->bitstructDecl()) {
                auto name = bd->IDENTIFIER()->getText();
                auto* existing = typeRegistry_.lookup(name);
                if (!existing || (existing->kind == TypeKind::BitStruct && existing->bitFields.empty()))
                    checkBitStructDecl(bd);
            }
        }
    } else {
        // Under module context, aliases/enums were already registered in pass 1.5.
        // Validate/upgrade local struct and union declarations now.
        // Skip generic structs/templates already registered via the
        // cross-module phase above — avoids duplicate registration
        // when the file path is non-canonical (e.g. LSP mode).
        for (auto* decl : tree->topLevelDecl()) {
            if (auto* sd = decl->structDecl()) {
                auto name = sd->IDENTIFIER()->getText();
                if (sd->typeParamList() && genericStructTemplates_.count(name))
                    continue;
                auto* existing = typeRegistry_.lookup(name);
                if (existing && existing->kind == TypeKind::Struct &&
                    !existing->fields.empty())
                    continue;
                checkStructDecl(sd);
            } else if (auto* ud = decl->unionDecl()) {
                auto name = ud->IDENTIFIER()->getText();
                if (ud->typeParamList() && genericUnionTemplates_.count(name))
                    continue;
                auto* existing = typeRegistry_.lookup(name);
                if (existing && existing->kind == TypeKind::Union &&
                    !existing->fields.empty())
                    continue;
                checkUnionDecl(ud);
            } else if (auto* bd = decl->bitstructDecl()) {
                auto name = bd->IDENTIFIER()->getText();
                auto* existing = typeRegistry_.lookup(name);
                if (!existing || (existing->kind == TypeKind::BitStruct && existing->bitFields.empty()))
                    checkBitStructDecl(bd);
            }
        }
    }

    auto hasErrors = [&]() -> bool {
        for (const auto& d : diagnostics_) {
            if (d.severity == Diagnostic::Error)
                return true;
        }
        return false;
    };

    // Stop after type registration failures to avoid cascading diagnostics
    // (unknown types, undefined vars, and return-shape errors that are
    // direct consequences of earlier declaration errors).
    if (hasErrors())
        return false;

    // Pass 3.5: register struct methods via `extend` blocks
    // Skip generic extend blocks already registered via the
    // cross-module phase above (avoids duplicate registration
    // when the file path is non-canonical, e.g. LSP mode).
    for (auto* decl : tree->topLevelDecl()) {
        if (auto* ext = decl->extendDecl()) {
            auto extName = ext->IDENTIFIER()->getText();
            if (ext->typeParamList() && genericExtendTemplates_.count(extName))
                continue;
            checkExtendDecl(ext);
        }
    }

    if (hasErrors())
        return false;

    // Pass 3.6: register trait declarations
    for (auto* decl : tree->topLevelDecl()) {
        if (auto* td = decl->traitDecl()) {
            checkTraitDecl(td);
        }
    }

    if (hasErrors())
        return false;

    // Pass 3.7: register impl declarations
    for (auto* decl : tree->topLevelDecl()) {
        if (auto* id = decl->implDecl()) {
            checkImplDecl(id);
        }
    }

    if (hasErrors())
        return false;

    // Validate attributes on all top-level declarations
    for (auto* decl : tree->topLevelDecl()) {
        auto* attrs = decl->functionDecl()    ? static_cast<AttributeListContext*>(decl->functionDecl()->attributeList())
                    : decl->structDecl()       ? decl->structDecl()->attributeList()
                    : decl->unionDecl()        ? decl->unionDecl()->attributeList()
                    : decl->enumDecl()         ? decl->enumDecl()->attributeList()
                    : decl->typeAliasDecl()    ? decl->typeAliasDecl()->attributeList()
                    : decl->externDecl()       ? decl->externDecl()->attributeList()
                    : decl->extendDecl()       ? decl->extendDecl()->attributeList()
                    : decl->constDeclStmt()    ? decl->constDeclStmt()->attributeList()
                    : nullptr;
        validateAttributeList(attrs, "top-level declaration");
        if (auto* ed = decl->enumDecl())
            for (auto* v : ed->enumVariant())
                validateAttributeList(v->attributeList(), "enum variant");
        if (auto* sd = decl->structDecl())
            for (auto* f : sd->structField())
                validateAttributeList(f->attributeList(), "struct field");
        if (auto* ud = decl->unionDecl())
            for (auto* f : ud->unionField())
                validateAttributeList(f->attributeList(), "union field");
    }

    // Pass 4: register function signatures (before checking bodies)
    // When moduleRegistry_ is present, external symbols are registered
    // in pass 1.5, but we must still ensure local file functions are registered
    // (avoid double-registration by checking `functions_`).
    for (auto* decl : tree->topLevelDecl()) {
        if (auto* func = decl->functionDecl()) {
            if (func->IDENTIFIER().empty()) {
                error(func, "function must have a name");
                continue;
            }
            auto funcName = func->name->getText();
            if (!moduleRegistry_ || !functions_.count(funcName) || globalBuiltins_.count(funcName)) {
                registerFunctionSignature(func);
            }
        }
        if (auto* ext = decl->externDecl()) {
            checkExternDecl(ext);
        }
    }

    // Register extern functions even in module context
    for (auto* decl : tree->topLevelDecl()) {
        if (auto* ext = decl->externDecl()) {
            checkExternDecl(ext);
        }
    }

    // Pass 4.5: process c_macro { ... } blocks (must precede const/var
    //            resolution so function-like macros are registered)
    for (auto* decl : tree->topLevelDecl()) {
        if (auto* cm = decl->cMacroBlock()) {
            checkCMacroBlock(cm);
        }
        if (decl->asmBBlock()) {
            // asm_b blocks are opaque — no semantic checking needed
        }
    }
    // Also process c_macro blocks at statement level (inside functions)
    // This is handled in checkStmt via the statement visitor.

    // Pass 4.6: pre-register all top-level const names (for forward references)
    for (auto* decl : tree->topLevelDecl()) {
        if (auto* cd = decl->constDeclStmt()) {
            if (!isDeclActive(cd->attributeList())) continue;
            auto decls = cd->constDeclarator();
            for (auto* d : decls) {
                auto name = d->IDENTIFIER()->getText();
VarInfo vi{typeRegistry_.lookup("int32"), 0, {}, true, false, nullptr};
                vi.isConst = true;
                vi.scopeDepth = 0;
                locals_[name] = vi;
                globalVars_[name] = vi;
            }
        }
    }

    // Pass 4.7: resolve expressions and real types for top-level consts
    for (auto* decl : tree->topLevelDecl()) {
        if (auto* cd = decl->constDeclStmt()) {
            checkConstDeclStmt(cd);
        }
    }

    // Pass 5: check function bodies
    for (auto* decl : tree->topLevelDecl()) {
        if (auto* func = decl->functionDecl()) {
            locals_.clear();
            // Restore top-level consts into function scope
            for (auto& [name, vi] : globalVars_)
                locals_[name] = vi;
            checkFunction(func);
        }
    }

    // Pass 5.5: check extend method bodies
    for (auto* decl : tree->topLevelDecl()) {
        if (auto* ext = decl->extendDecl()) {
            checkExtendMethodBodies(ext);
        }
    }

    // Pass 5.6: check impl method bodies
    for (auto* decl : tree->topLevelDecl()) {
        if (auto* id = decl->implDecl()) {
            checkImplMethodBodies(id);
        }
    }

    // Only actual errors (not warnings) should cause a check failure
    verifySemanticDBConsistency();
    return !hasErrors();
}

// ── Phase 3: Consistency verification ────────────────────────────────────────



void Checker::verifySemanticDBConsistency() {
    if (!semanticDB_) return;

    // Count types in each registry for diagnostic purposes
    size_t trTypes = typeRegistry_.allTypes().size();
    size_t sdTypes = 0;
    for (const auto* d : semanticDB_->allTypes()) {
        if (!d->modulePath.empty() || d->kind == semantic::DeclKind::Struct) ++sdTypes;
    }

    // Verify key structural invariants
    for (const auto& name : typeRegistry_.allTypes()) {
        auto* trType = typeRegistry_.lookup(name);
        if (!trType) continue;

        // Skip primitives
        if (trType->kind == TypeKind::Integer || trType->kind == TypeKind::Float ||
            trType->kind == TypeKind::Bool    || trType->kind == TypeKind::Char ||
            trType->kind == TypeKind::Void    || trType->kind == TypeKind::String ||
            trType->kind == TypeKind::Pointer || trType->kind == TypeKind::Function ||
            trType->kind == TypeKind::VAList  || trType->kind == TypeKind::Extended)
            continue;

        auto* sdDecl = semanticDB_->lookupAny(name);
        if (!sdDecl && !semanticDB_->hasForwardDecl(name)) {
            // User-defined type registered in TypeRegistry but missing from SemanticDB
            // This indicates a gap in Phase 1 sync coverage
            continue; // non-fatal during transition
        }
    }

    (void)trTypes; (void)sdTypes;
}

// ═══════════════════════════════════════════════════════════════════════
//  Type resolution helpers
// ═══════════════════════════════════════════════════════════════════════



void Checker::setModuleContext(const ModuleRegistry* registry,
                                const std::string& modulePath,
                                const std::string& currentFile) {
    moduleRegistry_     = registry;
    currentModulePath_  = modulePath;
    currentFile_        = currentFile;
}



void Checker::setProjectPaths(const std::string& projectRoot,
                               const std::vector<std::string>& sourcePaths) {
    projectRoot_ = projectRoot;
    sourcePaths_ = sourcePaths;
}



std::string Checker::findModuleFile(const std::string& modPath) const {
    namespace fs = std::filesystem;
    // 1) Search stdlib directories.
    for (auto& dir : ImportResolver::stdlibSearchPaths()) {
        auto candidate = fs::path(dir) / (modPath + ".lc");
        std::error_code ec;
        if (fs::exists(candidate, ec)) return candidate.string();
    }
    // 2) Search project directories.
    if (!projectRoot_.empty()) {
        auto root = fs::path(projectRoot_);
        auto candidate = root / (modPath + ".lc");
        std::error_code ec;
        if (fs::exists(candidate, ec)) return candidate.string();
        for (auto& sp : sourcePaths_) {
            candidate = root / fs::path(sp) / (modPath + ".lc");
            if (fs::exists(candidate, ec)) return candidate.string();
        }
    }
    return {};
}



void Checker::setCBindings(const CBindings* bindings) {
    cBindings_ = bindings;
}

// ── c_macro { ... } block evaluation ──────────────────────────────────

// Tokenize a macro body string into tokens for evalMacroAdd.
// Splits on whitespace and separates operators: (, ), +, -, *, /, %, etc.
static std::vector<std::string> tokenizeMacroBody(const std::string& body) {
    std::vector<std::string> tokens;
    std::string tok;
    for (size_t i = 0; i < body.size(); i++) {
        char c = body[i];
        if (std::isspace(c)) {
            if (!tok.empty()) { tokens.push_back(tok); tok.clear(); }
            continue;
        }
        // Multi-char operators
        if (c == '<' && i+1 < body.size() && body[i+1] == '<') {
            if (!tok.empty()) { tokens.push_back(tok); tok.clear(); }
            tokens.push_back("<<");
            i++;
            continue;
        }
        if (c == '>' && i+1 < body.size() && body[i+1] == '>') {
            if (!tok.empty()) { tokens.push_back(tok); tok.clear(); }
            tokens.push_back(">>");
            i++;
            continue;
        }
        // Single-char operators/delimiters
        if (c == '(' || c == ')' || c == '+' || c == '-' || c == '*' ||
            c == '/' || c == '%' || c == '&' || c == '|' || c == '^' ||
            c == '~' || c == '?' || c == ':') {
            if (!tok.empty()) { tokens.push_back(tok); tok.clear(); }
            tokens.push_back(std::string(1, c));
            continue;
        }
        tok += c;
    }
    if (!tok.empty()) tokens.push_back(tok);
    return tokens;
}

// Extract macro name from "#define FOO ..." → "FOO"
static std::string extractMacroName(const std::string& line) {
    auto s = line.find("#define");
    if (s == std::string::npos) return {};
    s = line.find_first_not_of(" \t", s + 7);
    if (s == std::string::npos) return {};
    size_t e = s;
    while (e < line.size() && !std::isspace(line[e]) && line[e] != '(') e++;
    return line.substr(s, e - s);
}



void Checker::checkCMacroBlock(LucisParser::CMacroBlockContext* ctx) {
    namespace fs = std::filesystem;

    // 1. Extract raw C text from token: "c_macro { #define FOO 42\n... }"
    auto tok = ctx->C_MACRO_BLOCK();
    if (!tok) return;
    std::string fullText = tok->getText();

    // Strip "c_macro {" prefix (9 chars) and "}" suffix (1 char)
    if (fullText.size() < 11) {
        error(ctx, "empty c_macro block");
        return;
    }
    std::string rawC = fullText.substr(9, fullText.size() - 10);  // between braces

    // 2. Evaluate via gcc preprocessor + optional compilation
    std::string tempDir = projectRoot_.empty()
        ? "/tmp"
        : (fs::path(projectRoot_) / ".lucis" / "cmacro").string();

    CBindings localBindings;
    evalCMacroRaw(rawC, currentFile_, tok->getSymbol()->getLine(),
                  tempDir, localBindings, true);

    // 3. Register results into Checker's internal tables
    for (auto& [name, cm] : localBindings.macros()) {
        if (cMacroBindings_.findMacro(name)) continue;
        cMacroBindings_.addMacro(cm);
        auto* int32TI = typeRegistry_.lookup("int32");
        if (int32TI)
            cEnumConstants_[name] = CEnumConstant{int32TI, cm.value, 0.0, false, false};
    }

    for (auto& [name, flm] : localBindings.functionLikeMacros()) {
        if (cMacroBindings_.findFunctionLikeMacro(name)) continue;
        cMacroBindings_.addFunctionLikeMacro(flm);
        cFunctionLikeMacros_[name] = const_cast<CFunctionLikeMacro*>(
            cMacroBindings_.findFunctionLikeMacro(name));
    }
}



bool Checker::isKnownFunction(const std::string& name) const {
    // 1. Local file function
    if (functions_.count(name)) return true;
    // 1b. Local generic function template
    if (genericFuncTemplates_.count(name)) return true;
    // 1c. Comptime function
    if (comptimeRegistry_.isComptime(name)) return true;
    // 2. Builtin
    if (globalBuiltins_.count(name)) return true;
    // 3. Std import
    if (imports_.isImported(name)) return true;
    // 4. User import
    if (userImports_.count(name)) return true;
    // 5. Same-module symbol from another file
    if (moduleRegistry_ && !currentModulePath_.empty()) {
        auto* sym = moduleRegistry_->findSymbol(currentModulePath_, name);
        if (sym && sym->kind == ExportedSymbol::Function &&
            sym->sourceFile != currentFile_)
            return true;
    }
    return false;
}



bool Checker::isKnownType(const std::string& name) const {
    if (typeRegistry_.lookup(name)) return true;
    // Check user-defined generic struct templates
    if (genericStructTemplates_.count(name)) return true;
    if (genericUnionTemplates_.count(name)) return true;
    if (genericEnumTemplates_.count(name)) return true;
    // Check user imports for struct/enum/alias
    if (userImports_.count(name)) return true;
    // Check same-module types from other files
    if (moduleRegistry_ && !currentModulePath_.empty()) {
        auto* sym = moduleRegistry_->findSymbol(currentModulePath_, name);
        if (sym && (sym->kind == ExportedSymbol::Struct ||
                    sym->kind == ExportedSymbol::Union ||
                    sym->kind == ExportedSymbol::Enum ||
                    sym->kind == ExportedSymbol::TypeAlias) &&
            sym->sourceFile != currentFile_)
            return true;
    }
    return false;
}



void Checker::registerGlobalBuiltins() {
    auto* voidTy   = typeRegistry_.lookup("void");
    auto* i32Ty    = typeRegistry_.lookup("int32");
    auto* i64Ty    = typeRegistry_.lookup("int64");
    auto* f64Ty    = typeRegistry_.lookup("float64");
    auto* boolTy   = typeRegistry_.lookup("bool");
    auto* strTy    = typeRegistry_.lookup("string");

    // exit(int32) -> void
    functions_["exit"] = makeFunctionType(voidTy, { i32Ty });
    globalBuiltins_.insert("exit");

    // panic(string) -> void
    functions_["panic"] = makeFunctionType(voidTy, { strTy });
    globalBuiltins_.insert("panic");

    // assert(bool) -> void
    functions_["assert"] = makeFunctionType(voidTy, { boolTy });
    globalBuiltins_.insert("assert");

    // assertMsg(bool, string) -> void
    functions_["assertMsg"] = makeFunctionType(voidTy, { boolTy, strTy });
    globalBuiltins_.insert("assertMsg");

    // unreachable() -> void
    functions_["unreachable"] = makeFunctionType(voidTy, {});
    globalBuiltins_.insert("unreachable");

    // toInt(string) -> int64
    functions_["toInt"] = makeFunctionType(i64Ty, { strTy });
    globalBuiltins_.insert("toInt");

    // toFloat(string) -> float64
    functions_["toFloat"] = makeFunctionType(f64Ty, { strTy });
    globalBuiltins_.insert("toFloat");

    // toBool(string) -> bool
    functions_["toBool"] = makeFunctionType(boolTy, { strTy });
    globalBuiltins_.insert("toBool");

    // toString(T) -> string — accepts any primitive type
    // We register it with a dummy param; actual type checking is relaxed
    functions_["toString"] = makeFunctionType(strTy, { i32Ty });
    globalBuiltins_.insert("toString");

    // ── C FFI string conversion builtins ────────────────────────────
    auto* charPtrTy = getPointerType(typeRegistry_.lookup("char"));
    auto* usizeTy   = typeRegistry_.lookup("usize");

    // cstr(string) -> *char
    functions_["cstr"] = makeFunctionType(charPtrTy, { strTy });
    globalBuiltins_.insert("cstr");

    // fromCStr(*char) -> string
    functions_["fromCStr"] = makeFunctionType(strTy, { charPtrTy });
    globalBuiltins_.insert("fromCStr");

    // fromCStrCopy(*char) -> string
    functions_["fromCStrCopy"] = makeFunctionType(strTy, { charPtrTy });
    globalBuiltins_.insert("fromCStrCopy");

    // fromCStrLen(*char, usize) -> string
    functions_["fromCStrLen"] = makeFunctionType(strTy, { charPtrTy, usizeTy });
    globalBuiltins_.insert("fromCStrLen");

    // freeStr(string) -> void
    functions_["freeStr"] = makeFunctionType(voidTy, { strTy });
    globalBuiltins_.insert("freeStr");
}

// ═══════════════════════════════════════════════════════════════════════
//  FFI: extern function declarations
// ═══════════════════════════════════════════════════════════════════════



void Checker::initSemanticDB() {
    if (!semanticDB_) return;

    auto addPrim = [&](const char* name, semantic::DeclKind kind,
                       unsigned bw, bool signd, const char* suffix) {
        auto d = std::make_unique<semantic::PrimitiveDecl>(kind);
        d->name          = name;
        d->bitWidth      = bw;
        d->isSigned      = signd;
        d->builtinSuffix = suffix;
        semanticDB_->registerBuiltin(std::move(d));
    };

    addPrim("void",    semantic::DeclKind::Void,    0, true,  "");
    addPrim("bool",    semantic::DeclKind::Bool,    1, true,  "bool");
    addPrim("char",    semantic::DeclKind::Char,    8, true,  "char");
    addPrim("string",  semantic::DeclKind::String,  0, true,  "str");
    addPrim("int1",   semantic::DeclKind::Integer, 1,   true,  "i1");
    addPrim("int8",   semantic::DeclKind::Integer, 8,   true,  "i8");
    addPrim("int16",  semantic::DeclKind::Integer, 16,  true,  "i16");
    addPrim("int32",  semantic::DeclKind::Integer, 32,  true,  "i32");
    addPrim("int64",  semantic::DeclKind::Integer, 64,  true,  "i64");
    addPrim("int128", semantic::DeclKind::Integer, 128, true,  "i128");
    addPrim("intinf", semantic::DeclKind::Integer, 256, true,  "i128");
    addPrim("isize",  semantic::DeclKind::Integer, 0,   true,  "i64");
    addPrim("uint1",   semantic::DeclKind::Integer, 1,   false, "u1");
    addPrim("uint8",   semantic::DeclKind::Integer, 8,   false, "u8");
    addPrim("uint16",  semantic::DeclKind::Integer, 16,  false, "u16");
    addPrim("uint32",  semantic::DeclKind::Integer, 32,  false, "u32");
    addPrim("uint64",  semantic::DeclKind::Integer, 64,  false, "u64");
    addPrim("uint128", semantic::DeclKind::Integer, 128, false, "u128");
    addPrim("usize",   semantic::DeclKind::Integer, 0,   false, "u64");
    addPrim("float32",  semantic::DeclKind::Float, 32,  true, "f32");
    addPrim("float64",  semantic::DeclKind::Float, 64,  true, "f64");
    addPrim("float80",  semantic::DeclKind::Float, 80,  true, "f80");
    addPrim("float128", semantic::DeclKind::Float, 128, true, "f128");
    addPrim("double",   semantic::DeclKind::Float, 64,  true, "f64");

    {
        auto d = std::make_unique<semantic::PrimitiveDecl>(semantic::DeclKind::VAList);
        d->name = "va_list";
        d->builtinSuffix = "valist";
        semanticDB_->registerBuiltin(std::move(d));
    }

    auto addExtended = [&](const char* name, const char* kind,
                           const char* elem, const char* key,
                           const char* val, const char* cpfx) {
        auto d = std::make_unique<semantic::ExtendedDecl>();
        d->name         = name;
        d->extendedKind = kind;
        d->cPrefix      = cpfx;
        if (elem) d->elementType = const_cast<semantic::Decl*>(semanticDB_->lookupAny(elem));
        if (key)  d->keyType     = const_cast<semantic::Decl*>(semanticDB_->lookupAny(key));
        if (val)  d->valueType   = const_cast<semantic::Decl*>(semanticDB_->lookupAny(val));
        semanticDB_->registerBuiltin(std::move(d));
    };

    addExtended("Vec",   "Vec",   "void", nullptr, nullptr, "lucis_vec");
    addExtended("Map",   "Map",   nullptr, "void", "void", "lucis_map");
    addExtended("Set",   "Set",   "void", nullptr, nullptr, "lucis_set");
    addExtended("Task",  "Task",  "void", nullptr, nullptr, "lucis_task");

    {
        auto d = std::make_unique<semantic::ExtendedDecl>();
        d->name         = "Mutex";
        d->extendedKind = "Mutex";
        semanticDB_->registerBuiltin(std::move(d));
    }

    {
        auto ed = std::make_unique<semantic::StructDecl>();
        ed->name = "Error";
        auto* strTI = semanticDB_->lookupAny("string");
        auto* i32TI = semanticDB_->lookupAny("int32");
        semantic::FieldInfo f1; f1.name = "message"; f1.type = const_cast<semantic::Decl*>(strTI);
        semantic::FieldInfo f2; f2.name = "file";    f2.type = const_cast<semantic::Decl*>(strTI); f2.autoFill = true;
        semantic::FieldInfo f3; f3.name = "line";    f3.type = const_cast<semantic::Decl*>(i32TI); f3.autoFill = true;
        semantic::FieldInfo f4; f4.name = "column";  f4.type = const_cast<semantic::Decl*>(i32TI); f4.autoFill = true;
        ed->fields.push_back(std::move(f1));
        ed->fields.push_back(std::move(f2));
        ed->fields.push_back(std::move(f3));
        ed->fields.push_back(std::move(f4));
        semanticDB_->registerBuiltin(std::move(ed));
    }
}


