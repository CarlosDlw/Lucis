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



void Checker::checkUseDecls(LucisParser::ProgramContext* tree) {
    auto processUse = [&](LucisParser::UseDeclContext* useDecl) {
        auto* attrs = [&]() -> LucisParser::AttributeListContext* {
            if (auto* r = dynamic_cast<LucisParser::UseRootContext*>(useDecl)) return r->attributeList();
            if (auto* i = dynamic_cast<LucisParser::UseItemContext*>(useDecl)) return i->attributeList();
            if (auto* g = dynamic_cast<LucisParser::UseGroupContext*>(useDecl)) return g->attributeList();
            if (auto* e = dynamic_cast<LucisParser::UseEnumWildcardContext*>(useDecl)) return e->attributeList();
            return nullptr;
        }();
        if (!isDeclActive(attrs)) return;
        if (auto* root = dynamic_cast<LucisParser::UseRootContext*>(useDecl)) {
            auto rootName = root->IDENTIFIER()->getText();
            if (rootName == "std") {
                userImports_[rootName] = rootName;
            } else if (moduleRegistry_ && moduleRegistry_->hasModule(rootName)) {
                userImports_[rootName] = rootName;
            } else if (!moduleRegistry_ || !findModuleFile(ModuleRegistry::usePathToModulePath(rootName)).empty()) {
                userImports_[rootName] = rootName;
            } else {
                error(root, "unknown module root '" + rootName + "'");
            }
        } else if (auto* item = dynamic_cast<LucisParser::UseItemContext*>(useDecl)) {
            std::string path;
            for (auto* id : item->modulePath()->IDENTIFIER()) {
                if (!path.empty()) path += "::";
                path += id->getText();
            }
            auto symbolName = item->IDENTIFIER()->getText();
            auto modPath = ModuleRegistry::usePathToModulePath(path);

            if (ImportResolver::isStdModule(path)) {
                if (ImportResolver::moduleExportsSymbol(path, symbolName)) {
                    imports_.addImport(path, symbolName);
                } else if (moduleRegistry_ && moduleRegistry_->hasModule(modPath)) {
                    auto* sym = moduleRegistry_->findSymbol(modPath, symbolName);
                    if (!sym) {
                        error(item, "module '" + path +
                                    "' does not export '" + symbolName + "'");
                    } else {
                        userImports_[symbolName] = modPath;
                    }
                } else if (!moduleRegistry_ || !findModuleFile(modPath).empty()) {
                    userImports_[symbolName] = modPath;
                } else {
                    error(item, "unknown module '" + path + "'");
                }
            } else if (ImportResolver::isStdModule(path + "::" + symbolName)) {
                userImports_[symbolName] = ModuleRegistry::usePathToModulePath(path + "::" + symbolName);
            } else if (moduleRegistry_ && moduleRegistry_->hasModule(modPath)) {
                auto* sym = moduleRegistry_->findSymbol(modPath, symbolName);
                if (!sym) {
                    // Search all modules (handles path-mismatch from directory scan)
                    for (auto& ns : moduleRegistry_->allModules()) {
                        sym = moduleRegistry_->findSymbol(ns, symbolName);
                        if (sym) break;
                    }
                }
                if (!sym) {
                    error(item, "module '" + path +
                                "' does not export '" + symbolName + "'");
                } else {
                    userImports_[symbolName] = modPath;
                }
            } else if (!moduleRegistry_ || !findModuleFile(modPath).empty()) {
                userImports_[symbolName] = modPath;
            } else {
                error(item, "unknown module '" + path + "'");
            }
        } else if (auto* grp = dynamic_cast<LucisParser::UseGroupContext*>(useDecl)) {
            std::string path;
            for (auto* id : grp->modulePath()->IDENTIFIER()) {
                if (!path.empty()) path += "::";
                path += id->getText();
            }
            auto modPath = ModuleRegistry::usePathToModulePath(path);

            if (ImportResolver::isStdModule(path)) {
                for (auto* id : grp->IDENTIFIER()) {
                    auto symbolName = id->getText();
                    if (ImportResolver::moduleExportsSymbol(path, symbolName)) {
                        imports_.addImport(path, symbolName);
                    } else if (moduleRegistry_ && moduleRegistry_->hasModule(modPath)) {
                        auto* sym = moduleRegistry_->findSymbol(modPath, symbolName);
                        if (!sym) {
                            for (auto& ns : moduleRegistry_->allModules()) {
                                sym = moduleRegistry_->findSymbol(ns, symbolName);
                                if (sym) break;
                            }
                        }
                        if (!sym) {
                            error(grp, "module '" + path +
                                        "' does not export '" + symbolName + "'");
                        } else {
                            userImports_[symbolName] = modPath;
                        }
                    } else if (!moduleRegistry_ || !findModuleFile(modPath).empty()) {
                        userImports_[symbolName] = modPath;
                    } else {
                        error(grp, "unknown module '" + path + "'");
                    }
                }
            } else if (moduleRegistry_ && moduleRegistry_->hasModule(modPath)) {
                for (auto* id : grp->IDENTIFIER()) {
                    auto symbolName = id->getText();
                    auto* sym = moduleRegistry_->findSymbol(modPath, symbolName);
                    if (!sym) {
                        error(grp, "module '" + path +
                                    "' does not export '" + symbolName + "'");
                    } else {
                        userImports_[symbolName] = modPath;
                    }
                }
            } else if (!moduleRegistry_ || !findModuleFile(modPath).empty()) {
                for (auto* id : grp->IDENTIFIER()) {
                    userImports_[id->getText()] = modPath;
                }
            } else {
                error(grp, "unknown module '" + path + "'");
            }
        } else if (auto* ew = dynamic_cast<LucisParser::UseEnumWildcardContext*>(useDecl)) {
            unsigned arrayDims = 0;
            auto* enumType = resolveTypeSpec(ew->typeSpec(), arrayDims);
            if (!enumType) {
                error(ew, "unknown type in 'use' wildcard");
            } else if (enumType->kind != TypeKind::Enum) {
                error(ew, "type '" + enumType->name + "' is not an enum");
            } else if (arrayDims > 0) {
                error(ew, "cannot use array type in 'use' wildcard");
            } else {
                for (const auto& vi : enumType->enumVariantInfos) {
                    auto [it, inserted] = enumVariantImports_.try_emplace(
                        vi.name,
                        InjectedVariant{ enumType, &vi });
                    if (!inserted) {
                        error(ew, "ambiguous variant name '" + vi.name +
                                   "': already imported from '" +
                                   it->second.enumType->name + "'");
                    }
                }
            }
        }
    };

    for (auto* pre : tree->preambleDecl()) {
        auto* useDecl = pre->useDecl();
        if (!useDecl) continue;
        processUse(useDecl);
    }
    for (auto* tld : tree->topLevelDecl()) {
        auto* useDecl = tld->useDecl();
        if (!useDecl) continue;
        processUse(useDecl);
    }
}



void Checker::checkTypeAliasDecl(LucisParser::TypeAliasDeclContext* decl) {
    if (!isDeclActive(decl->attributeList())) return;
    auto name = decl->IDENTIFIER()->getText();

    auto* existing = typeRegistry_.lookup(name);
    bool canUpgradeSkeleton = false;
    if (existing) {
        canUpgradeSkeleton =
            existing->kind == TypeKind::Struct &&
            existing->fields.empty() &&
            existing->bitWidth == 0;
        if (!canUpgradeSkeleton) {
            error(decl, "type '" + name + "' already defined");
            return;
        }
    }

    // For now, we register a Function TypeInfo for fn(...) -> T aliases
    auto* typeSpecCtx = decl->typeSpec();
    if (auto* fnSpec = typeSpecCtx->fnTypeSpec()) {
        TypeInfo ti;
        ti.name = name;
        ti.kind = TypeKind::Function;
        ti.bitWidth = 0;
        ti.isSigned = false;
        ti.builtinSuffix = "ptr";

        // Resolve return type
        auto retSpecs = fnSpec->typeSpec();
        auto retName = resolveBaseTypeName(retSpecs.back());
        auto* retTI = typeRegistry_.lookup(retName);
        if (!retTI) {
            error(decl, "unknown return type '" + retName +
                             "' in type alias '" + name + "'");
            return;
        }
        ti.returnType = retTI;

        // Resolve parameter types (all typeSpecs except the last one which is return)
        for (size_t i = 0; i + 1 < retSpecs.size(); i++) {
            auto paramName = resolveBaseTypeName(retSpecs[i]);
            auto* paramTI = typeRegistry_.lookup(paramName);
            if (!paramTI) {
                error(decl, "unknown param type '" + paramName +
                                 "' in type alias '" + name + "'");
                return;
            }
            ti.paramTypes.push_back(paramTI);
        }

        syncToSemanticDB_TypeAlias(ti, currentModulePath_, decl); // Phase 1
        typeRegistry_.registerType(std::move(ti));
    } else {
        // General alias: type MyInt = int32; / type Result = tuple<string, int32>;
        unsigned dims = 0;
        auto* baseTI = resolveTypeSpec(typeSpecCtx, dims);
        if (!baseTI) {
            error(decl, "unknown type in type alias '" + name + "'");
            return;
        }
        // Register as a copy with the alias name
        TypeInfo ti = *baseTI;
        ti.name = name;
        syncToSemanticDB_TypeAlias(ti, currentModulePath_, decl);
        typeRegistry_.registerType(std::move(ti));
    }
}



void Checker::checkStructDecl(LucisParser::StructDeclContext* decl) {
    if (!isDeclActive(decl->attributeList())) return;
    auto name = decl->IDENTIFIER()->getText();

    // Generic struct template — register as template, not as concrete type
    if (auto* tpl = decl->typeParamList()) {
        if (genericStructTemplates_.count(name)) {
            error(decl, "generic struct '" + name + "' already defined");
            return;
        }
        GenericStructTemplate tmpl;
        for (auto* tp : tpl->typeParam())
            tmpl.typeParams.push_back(tp->IDENTIFIER(0)->getText());
        auto savedTypeParams = tmpl.typeParams;
        tmpl.decl = decl;
        genericStructTemplates_[name] = std::move(tmpl);
        syncToSemanticDB_GenericStruct(name, savedTypeParams, decl);
        return;
    }

    auto* existing = typeRegistry_.lookup(name);
    if (existing) {
        bool canUpgradeSkeleton =
            (existing->kind == TypeKind::Struct &&
             existing->fields.empty() &&
             existing->bitWidth == 0) ||
            existing->fromDependencyWalk;
        if (!canUpgradeSkeleton) {
            error(decl, "type '" + name + "' already defined");
            return;
        }
    }

    // Register skeleton first so self-referencing pointer fields
    // (e.g. *Node inside Node) can resolve the type.
    TypeInfo skeleton;
    skeleton.name = name;
    skeleton.kind = TypeKind::Struct;
    skeleton.bitWidth = 0;
    skeleton.isSigned = false;
    skeleton.fromDependencyWalk = registeringImported_;
    if (!existing)
        typeRegistry_.registerType(skeleton);

    TypeInfo ti = skeleton;
    std::unordered_set<std::string> seen;

    // ── Parent struct inheritance ─────────────────────────────────────
    if (decl->COLON() && decl->typeSpec()) {
        unsigned parentDims = 0;
        auto* parentTI = resolveTypeSpec(decl->typeSpec(), parentDims);
        if (!parentTI || parentTI->kind != TypeKind::Struct) {
            error(decl, "parent type not found or is not a struct");
            return;
        }

        ti.parentType = parentTI;
        for (auto& pf : parentTI->fields) {
            seen.insert(pf.name);
            ti.fields.push_back(pf);
        }
    }

    for (auto* field : decl->structField()) {
        if (!isDeclActive(field->attributeList())) continue;
        unsigned fieldDims = 0;
        auto* fieldTI = resolveTypeSpec(field->typeSpec(), fieldDims);
        if (!fieldTI) {
            error(field, "unknown field type in struct '" + name + "'");
            return;
        }
        auto fieldName = field->IDENTIFIER()->getText();
        if (!seen.insert(fieldName).second) {
            error(field, "duplicate field '" + fieldName +
                             "' in struct '" + name + "'");
            continue;
        }
        std::vector<unsigned> fieldSizes;
        {
            auto* spec = field->typeSpec();
            while (spec && spec->LBRACKET()) {
                if (spec->INT_LIT())
                    fieldSizes.push_back(static_cast<unsigned>(
                        std::stoul(spec->INT_LIT()->getText())));
                spec = spec->typeSpec(0);
            }
        }
        ti.fields.push_back({ fieldName, fieldTI, fieldDims, fieldSizes });
    }

    syncToSemanticDB_Struct(ti, currentModulePath_, decl);
    typeRegistry_.registerType(std::move(ti));
}



void Checker::checkUnionDecl(LucisParser::UnionDeclContext* decl) {
    if (!isDeclActive(decl->attributeList())) return;
    auto name = decl->IDENTIFIER()->getText();

    if (auto* tpl = decl->typeParamList()) {
        if (genericUnionTemplates_.count(name)) {
            error(decl, "generic union '" + name + "' already defined");
            return;
        }
        GenericUnionTemplate tmpl;
        for (auto* tp : tpl->typeParam())
            tmpl.typeParams.push_back(tp->IDENTIFIER(0)->getText());
        auto savedTypeParams = tmpl.typeParams;
        tmpl.decl = decl;
        genericUnionTemplates_[name] = std::move(tmpl);
        syncToSemanticDB_GenericUnion(name, savedTypeParams, decl);
        return;
    }

    auto* existing = typeRegistry_.lookup(name);
    bool canUpgradeSkeleton = false;
    if (existing) {
        canUpgradeSkeleton =
            (existing->kind == TypeKind::Union &&
             existing->fields.empty() &&
             existing->bitWidth == 0) ||
            existing->fromDependencyWalk;
        if (!canUpgradeSkeleton) {
            error(decl, "type '" + name + "' already defined");
            return;
        }
    }

    TypeInfo ti;
    ti.name = name;
    ti.kind = TypeKind::Union;
    ti.bitWidth = 0;
    ti.isSigned = false;
    ti.fromDependencyWalk = registeringImported_;

    std::unordered_set<std::string> seen;
    for (auto* field : decl->unionField()) {
        if (!isDeclActive(field->attributeList())) continue;
        unsigned fieldDims = 0;
        auto* fieldTI = resolveTypeSpec(field->typeSpec(), fieldDims);
        if (!fieldTI) {
            error(field, "unknown field type in union '" + name + "'");
            return;
        }
        auto fieldName = field->IDENTIFIER()->getText();
        if (!seen.insert(fieldName).second) {
            error(field, "duplicate field '" + fieldName +
                             "' in union '" + name + "'");
            continue;
        }
        ti.fields.push_back({ fieldName, fieldTI, fieldDims });
    }

    syncToSemanticDB_Union(ti, currentModulePath_, decl);
    typeRegistry_.registerType(std::move(ti));
}



void Checker::checkBitStructDecl(LucisParser::BitstructDeclContext* decl) {
    if (!isDeclActive(decl->attributeList())) return;
    auto name = decl->IDENTIFIER()->getText();

    // Resolve backing type
    unsigned dummyDims = 0;
    bool isAuto = decl->typeSpec() && decl->typeSpec()->AUTO() != nullptr;
    auto* backType = isAuto ? nullptr : resolveTypeSpec(decl->typeSpec(), dummyDims);
    if (!backType && !isAuto) return;

    // Validate backing type is unsigned integer or auto
    if (backType && backType->kind != TypeKind::Integer) {
        error(decl, "bitstruct backing type must be an unsigned integer type, got '" +
                     backType->name + "'");
        return;
    }
    if (backType && backType->isSigned) {
        error(decl, "bitstruct backing type must be unsigned, got signed '" +
                     backType->name + "'");
        return;
    }

    // Collect fields and compute offsets
    TypeInfo ti;
    ti.name = name;
    ti.kind = TypeKind::BitStruct;
    ti.bitWidth = 0;
    ti.isSigned = false;
    ti.backingType = backType;

    unsigned runningOffset = 0;
    for (auto* field : decl->bitstructField()) {
        auto ids = field->IDENTIFIER();
        auto ints = field->INT_LIT();

        // Padding field: _ : N
        if (ints.size() == 2) {
            unsigned width = std::stoul(ints[1]->getText());
            BitFieldInfo bfi;
            bfi.offset = runningOffset;
            bfi.width = width;
            bfi.isPadding = true;
            ti.bitFields.push_back(bfi);
            runningOffset += width;
            continue;
        }

        // Named field: name : N
        if (ints.size() == 1 && ids) {
            auto fieldName = ids->getText();
            unsigned width = std::stoul(ints[0]->getText());
            if (width == 0) {
                error(field, "bitstruct field '" + fieldName + "' must have width > 0");
                continue;
            }
            BitFieldInfo bfi;
            bfi.name = fieldName;
            bfi.offset = runningOffset;
            bfi.width = width;
            bfi.isPadding = false;
            ti.bitFields.push_back(bfi);
            runningOffset += width;
        }
    }

    // Validate total bits fit in backing type
    unsigned backBits = 0;
    if (isAuto) {
        // Pick smallest unsigned type
        if (runningOffset <= 8) backBits = 8;
        else if (runningOffset <= 16) backBits = 16;
        else if (runningOffset <= 32) backBits = 32;
        else if (runningOffset <= 64) backBits = 64;
        else if (runningOffset <= 128) backBits = 128;
        else {
            error(decl, "bitstruct '" + name + "' requires more than 128 bits");
            return;
        }
        // Create or resolve the auto-chosen backing type
        auto typeName = "uint" + std::to_string(backBits);
        ti.backingType = typeRegistry_.lookup(typeName);
    } else {
        backBits = backType->bitWidth == 0 ? 64 : backType->bitWidth;
    }

    if (runningOffset > backBits) {
        error(decl, "bitstruct '" + name + "' requires " + std::to_string(runningOffset) +
                     " bits but backing type '" + ti.backingType->name + "' only has " +
                     std::to_string(backBits) + " bits");
        return;
    }

    // Register in TypeRegistry
    ti.bitWidth = backBits;
    typeRegistry_.registerType(std::move(ti));
}



void Checker::checkEnumDecl(LucisParser::EnumDeclContext* decl) {
    if (!isDeclActive(decl->attributeList())) return;
    auto name = decl->IDENTIFIER()->getText();

    if (typeRegistry_.lookup(name) || genericEnumTemplates_.count(name)) {
        error(decl, "type '" + name + "' already defined");
        return;
    }

    if (auto* tpl = decl->typeParamList()) {
        GenericEnumTemplate tmpl;
        for (auto* tp : tpl->typeParam())
            tmpl.typeParams.push_back(tp->IDENTIFIER(0)->getText());
        auto savedTypeParams = tmpl.typeParams;
        tmpl.decl = decl;
        genericEnumTemplates_[name] = std::move(tmpl);
        syncToSemanticDB_GenericEnum(name, savedTypeParams, decl);
        return;
    }

    TypeInfo ti;
    ti.name = name;
    ti.kind = TypeKind::Enum;
    ti.bitWidth = 32;
    ti.isSigned = false;
    ti.builtinSuffix = "i32";

    std::unordered_set<std::string> seen;
    unsigned nextDiscriminant = 0;
    for (auto* variantDecl : decl->enumVariant()) {
        if (!isDeclActive(variantDecl->attributeList())) continue;
        auto variant = variantDecl->IDENTIFIER()->getText();
        if (!seen.insert(variant).second) {
            error(variantDecl, "duplicate enum variant '" + variant +
                                "' in enum '" + name + "'");
            continue;
        }

        ti.enumVariants.push_back(variant);

        EnumVariantInfo info;
        info.name = variant;
        info.isError = hasAttribute(variantDecl->attributeList(), "error");

        if (variantDecl->ASSIGN()) {
            auto discVal = tryEvalUSizeExpr(variantDecl->expression());
            if (!discVal) {
                error(variantDecl->expression(),
                      "enum variant discriminant must be a compile-time constant unsigned integer");
                continue;
            }
            info.discriminant = static_cast<unsigned>(*discVal);
            if (static_cast<uint64_t>(info.discriminant) != *discVal) {
                error(variantDecl->expression(),
                      "enum variant discriminant value too large");
                continue;
            }
            bool dup = false;
            for (auto& existing : ti.enumVariantInfos) {
                if (existing.discriminant == info.discriminant) {
                    error(variantDecl,
                          "duplicate enum variant discriminant " + std::to_string(info.discriminant) +
                          " in enum '" + name + "'");
                    dup = true;
                    break;
                }
            }
            if (dup) continue;
            nextDiscriminant = info.discriminant + 1;
        } else {
            info.discriminant = nextDiscriminant++;
        }

        if (variantDecl->LPAREN()) {
            info.payloadKind = EnumPayloadKind::Tuple;
            auto payloadTypes = variantDecl->typeSpec();
            for (size_t i = 0; i < payloadTypes.size(); i++) {
                unsigned fieldDims = 0;
                auto* fieldTI = resolveTypeSpec(payloadTypes[i], fieldDims);
                if (!fieldTI) {
                    error(payloadTypes[i], "unknown payload type in enum variant '" +
                                          name + "::" + variant + "'");
                    return;
                }
                std::vector<unsigned> fieldSizes;
                auto* spec = payloadTypes[i];
                while (spec && spec->LBRACKET()) {
                    if (spec->INT_LIT())
                        fieldSizes.push_back(static_cast<unsigned>(std::stoul(spec->INT_LIT()->getText())));
                    spec = spec->typeSpec(0);
                }
                if (fieldDims > 0 && fieldSizes.empty()) {
                    error(payloadTypes[i],
                          "enum payload arrays must have fixed size; use '[N]T' in variant '" +
                          name + "::" + variant + "' or switch to 'vec<T>'");
                    return;
                }
                info.payloadFields.push_back({"_" + std::to_string(i), fieldTI, fieldDims, fieldSizes});
            }
        } else if (variantDecl->LBRACE()) {
            info.payloadKind = EnumPayloadKind::Named;
            std::unordered_set<std::string> fieldSeen;
            for (auto* payloadField : variantDecl->enumPayloadField()) {
                auto fieldName = payloadField->IDENTIFIER()->getText();
                if (!fieldSeen.insert(fieldName).second) {
                    error(payloadField, "duplicate payload field '" + fieldName +
                                       "' in enum variant '" + name + "::" + variant + "'");
                    return;
                }

                unsigned fieldDims = 0;
                auto* fieldTI = resolveTypeSpec(payloadField->typeSpec(), fieldDims);
                if (!fieldTI) {
                    error(payloadField, "unknown payload type in enum variant '" +
                                        name + "::" + variant + "'");
                    return;
                }
                std::vector<unsigned> fieldSizes;
                auto* spec = payloadField->typeSpec();
                while (spec && spec->LBRACKET()) {
                    if (spec->INT_LIT())
                        fieldSizes.push_back(static_cast<unsigned>(std::stoul(spec->INT_LIT()->getText())));
                    spec = spec->typeSpec(0);
                }
                if (fieldDims > 0 && fieldSizes.empty()) {
                    error(payloadField,
                          "enum payload arrays must have fixed size; use '[N]T' in variant '" +
                          name + "::" + variant + "' or switch to 'vec<T>'");
                    return;
                }
                info.payloadFields.push_back({fieldName, fieldTI, fieldDims, fieldSizes});
            }
        }

        ti.enumVariantInfos.push_back(std::move(info));
    }

    syncToSemanticDB_Enum(ti, currentModulePath_, decl);
    typeRegistry_.registerType(std::move(ti));
}

// ── Operator internal name mapping ─────────────────────────────


void Checker::checkExtendDecl(LucisParser::ExtendDeclContext* decl) {
    if (!isDeclActive(decl->attributeList())) return;
    auto structName = decl->IDENTIFIER()->getText();

    bool isGenericStruct = genericStructTemplates_.count(structName) != 0;
    bool isGenericEnum = genericEnumTemplates_.count(structName) != 0;
    auto* targetTI = typeRegistry_.lookup(structName);
    if (isGenericEnum) {
        error(decl, "enum '" + structName + "' cannot be extended; only structs and unions support 'extend'");
        return;
    }
    if (!targetTI && !isGenericStruct) {
        error(decl, "cannot extend unknown type '" + structName + "'");
        return;
    }
    if (targetTI && targetTI->kind == TypeKind::Enum) {
        error(decl, "enum '" + structName + "' cannot be extended; only structs and unions support 'extend'");
        return;
    }
    if (targetTI && targetTI->kind != TypeKind::Struct) {
        error(decl, "only structs and unions can be extended; '" + structName + "' is not a supported extend target");
        return;
    }

    // Generic extend block — register as template and skip body registration
    if (auto* tpl = decl->typeParamList()) {
        if (genericExtendTemplates_.count(structName)) {
            error(decl, "generic extend for '" + structName + "' already defined");
            return;
        }
        GenericExtendTemplate tmpl;
        for (auto* tp : tpl->typeParam())
            tmpl.typeParams.push_back(tp->IDENTIFIER(0)->getText());
        auto savedTypeParams = tmpl.typeParams;
        tmpl.decl = decl;
        genericExtendTemplates_[structName] = std::move(tmpl);
        syncToSemanticDB_GenericExtend(structName, savedTypeParams, decl);
        return;
    }

    for (auto* method : decl->extendMethod()) {
        // ── Operator overload ──
        if (auto* opDecl = method->operatorDecl()) {
            auto* opName = opDecl->operatorName();
            auto internalName = opInternalName(opName);
            auto sym = opSymbol(opName);

            unsigned retDims = 0;
            auto* retType = resolveTypeSpec(opDecl->typeSpec(), retDims);
            if (!retType) continue;

            OperatorInfo info;
            info.internalName = internalName;
            info.symbol = sym;
            info.returnType = retType;
            info.isInstance = (opDecl->AMPERSAND() != nullptr);

            // Instance ops ([] / ()) have &self + params
            std::vector<LucisParser::ParamContext*> params;
            if (info.isInstance) {
                params = opDecl->param();
            } else if (auto* pl = opDecl->paramList()) {
                params = pl->param();
            }

            for (auto* param : params) {
                unsigned pDims = 0;
                auto* pType = resolveTypeSpec(param->typeSpec(), pDims);
                if (!pType) continue;
                info.paramTypes.push_back(pType);
            }

            // ── Operator arity validation ──────────────────────────
            if (internalName != "opIndex" && internalName != "opCall") {
                int numOperands = -1; // expected number of operands
                if (internalName == "opNot" || internalName == "opBitNot")
                    numOperands = 1;
                else if (internalName == "opIncr" || internalName == "opDecr")
                    numOperands = 1;
                else if (internalName == "opSub")
                    numOperands = -1; // both unary (-x) and binary (x - y) allowed
                else if (internalName != "opUnknown")
                    numOperands = 2;

                if (numOperands > 0) {
                    size_t paramCount = info.paramTypes.size();
                    size_t expected = info.isInstance ? (numOperands - 1) : numOperands;
                    if (paramCount != expected) {
                        std::string msg = "operator '" + sym + "' expects " +
                                          std::to_string(expected) + " parameter(s)";
                        if (info.isInstance) {
                            msg += " after &self";
                        } else if (paramCount < expected && paramCount > 0) {
                            msg += "; missing left-hand side operand or add '&self'";
                        }
                        msg += ", got " + std::to_string(paramCount);
                        error(opDecl, msg);
                    }
                }
            }
            // ── End arity validation ───────────────────────────────

            operatorMethods_[structName].push_back(std::move(info));
            continue;
        }

        auto methodName = method->name->getText();

        unsigned retDims = 0;
        auto* retType = resolveTypeSpec(method->typeSpec(), retDims);
        if (!retType) continue;

        StructMethodInfo info;
        info.name = methodName;
        info.returnType = retType;
        info.isStatic = (method->AMPERSAND() == nullptr);

        // Static methods use paramList, instance methods use direct param()
        std::vector<LucisParser::ParamContext*> params;
        if (info.isStatic) {
            if (auto* pl = method->paramList())
                params = pl->param();
        } else {
            params = method->param();
        }

        for (auto* param : params) {
            unsigned pDims = 0;
            auto* pType = resolveTypeSpec(param->typeSpec(), pDims);
            if (!pType) continue;
            info.paramTypes.push_back(pType);
        }

        if (methodName == "drop" && !info.isStatic && info.paramTypes.empty()) {
            if (auto* mutTI = typeRegistry_.lookupMutable(structName))
                mutTI->dropTracked = true;
        }

        structMethods_[structName].push_back(std::move(info));
    }
    // Phase 1: sync extend methods to SemanticDB
    syncToSemanticDB_Extend(structName, structMethods_[structName]);
}



const Checker::StructMethodInfo* Checker::findMethodInChain(
    const TypeInfo* ti, const std::string& name) const {
    for (auto* t = ti; t; t = t->parentType) {
        auto it = structMethods_.find(t->name);
        if (it == structMethods_.end()) continue;
        for (auto& m : it->second) {
            if (m.name == name) return &m;
        }
    }
    return nullptr;
}



const TraitMethodInfo* Checker::findMethodInTraits(
    const TypeInfo* ti, const std::string& name) const {
    for (const auto& traitName : ti->implementedTraits) {
        auto* impl = traitRegistry_.findImpl(traitName, ti->name);
        if (!impl) continue;
        for (auto& m : impl->methodBodies) {
            if (m.name == name) return &m;
        }
    }
    return nullptr;
}



const Checker::OperatorInfo* Checker::findOperatorInChain(
    const TypeInfo* ti, const std::string& internalName) const {
    for (auto* t = ti; t; t = t->parentType) {
        auto it = operatorMethods_.find(t->name);
        if (it == operatorMethods_.end()) continue;
        for (auto& op : it->second) {
            if (op.internalName == internalName) return &op;
        }
    }
    return nullptr;
}

std::vector<const Checker::StructMethodInfo*> Checker::collectMethodsInChain(
    const TypeInfo* ti) const {
    std::vector<const StructMethodInfo*> result;
    for (auto* t = ti; t; t = t->parentType) {
        auto it = structMethods_.find(t->name);
        if (it == structMethods_.end()) continue;
        for (auto& m : it->second)
            result.push_back(&m);
    }
    return result;
}



void Checker::checkExtendMethodBodies(LucisParser::ExtendDeclContext* decl) {
    // Generic extend blocks are processed lazily during struct instantiation
    if (decl->typeParamList()) return;

    auto structName = decl->IDENTIFIER()->getText();
    auto* structTI = typeRegistry_.lookup(structName);
    if (!structTI || structTI->kind != TypeKind::Struct) return;

    for (auto* method : decl->extendMethod()) {
        // ── Operator overload body ──
        if (auto* opDecl = method->operatorDecl()) {
            locals_.clear();
            for (auto& [name, vi] : globalVars_)
                locals_[name] = vi;

            unsigned retDims = 0;
            auto* retType = resolveTypeSpec(opDecl->typeSpec(), retDims);
            if (!retType) retType = typeRegistry_.lookup("void");

            bool isInstance = (opDecl->AMPERSAND() != nullptr);

            // Register 'self' for instance operators ([] / ())
            if (isInstance) {
                auto* ptrType = getPointerType(structTI);
                locals_["self"] = {ptrType, 0, {}, true, true, nullptr};
            }

            // Register parameters
            std::vector<LucisParser::ParamContext*> params;
            if (isInstance) {
                params = opDecl->param();
            } else if (auto* pl = opDecl->paramList()) {
                params = pl->param();
            }

            for (auto* param : params) {
                auto paramName = param->IDENTIFIER()->getText();
                unsigned pDims = 0;
                auto* pType = resolveTypeSpec(param->typeSpec(), pDims);
                if (!pType) continue;
                if (locals_.count(paramName)) {
                    error(param, "duplicate parameter name '" + paramName + "'");
                    continue;
                }
                if (param->SPREAD())
                    locals_[paramName] = {pType, 1, {}, true, true, nullptr};
                else
                    locals_[paramName] = {pType, pDims, {}, true, true, nullptr};
            }

            auto* prevRet = currentReturnType_;
            currentReturnType_ = retType;
            checkBlock(opDecl->block(), retType);
            currentReturnType_ = prevRet;

            auto opName = opSymbol(opDecl->operatorName());
            if (retType->kind != TypeKind::Void) {
                if (!blockAlwaysReturns(opDecl->block())) {
                    error(opDecl, "operator '" + opName +
                                  "' must return a value of type '" + retType->name +
                                  "' on all code paths");
                }
            }
            warnUnusedLocals(opDecl);
            continue;
        }

        locals_.clear();
        for (auto& [name, vi] : globalVars_)
            locals_[name] = vi;

        unsigned retDims = 0;
        auto* retType = resolveTypeSpec(method->typeSpec(), retDims);
        if (!retType) retType = typeRegistry_.lookup("void");

        bool isInstance = (method->AMPERSAND() != nullptr);

        // Register 'self' for instance methods as *StructName
        if (isInstance) {
            auto* ptrType = getPointerType(structTI);
            locals_["self"] = {ptrType, 0, {}, true, true, nullptr};
        }

        // Register parameters
        std::vector<LucisParser::ParamContext*> params;
        if (isInstance) {
            params = method->param();
        } else {
            if (auto* pl = method->paramList())
                params = pl->param();
        }

        for (auto* param : params) {
            auto paramName = param->IDENTIFIER()->getText();
            unsigned pDims = 0;
            auto* pType = resolveTypeSpec(param->typeSpec(), pDims);
            if (!pType) continue;

            if (locals_.count(paramName)) {
                error(param, "duplicate parameter name '" + paramName + "'");
                continue;
            }

            if (param->SPREAD())
                locals_[paramName] = {pType, 1, {}, true, true, nullptr};
            else
locals_[paramName] = {pType, pDims, {}, true, true, nullptr};
        }

        auto* prevRet = currentReturnType_;
        currentReturnType_ = retType;
        checkBlock(method->block(), retType);
        currentReturnType_ = prevRet;

        if (retType->kind != TypeKind::Void) {
            if (!blockAlwaysReturns(method->block())) {
                auto methodName = method->name->getText();
                error(method, "method '" + structName + "." + methodName +
                              "' must return a value of type '" + retType->name +
                              "' on all code paths");
            }
        }

        warnUnusedLocals(method);
    }
}

// ── Trait declaration checking ──────────────────────────────────────────────



void Checker::checkTraitDecl(LucisParser::TraitDeclContext* decl)
{
    auto name = decl->IDENTIFIER()->getText();

    if (traitRegistry_.lookup(name) || genericTraitTemplates_.count(name)) {
        error(decl, "trait '" + name + "' already defined");
        return;
    }

    // Handle generic trait (e.g., `trait From<T>`)
    if (auto* tpl = decl->typeParamList()) {
        GenericTraitTemplate tmpl;
        for (auto* tp : tpl->typeParam())
            tmpl.typeParams.push_back(tp->IDENTIFIER(0)->getText());
        tmpl.decl = decl;
        genericTraitTemplates_[name] = std::move(tmpl);
        syncToSemanticDB_GenericTrait(name, tmpl.typeParams, decl);
        return;
    }

    TraitInfo ti;
    ti.name = name;
    ti.modulePath = currentModulePath_;
    ti.decl = decl;

    for (auto* method : decl->traitMethod()) {
        TraitMethodInfo mi;
        mi.name = method->name->getText();
        mi.hasSelfReceiver = (method->AMPERSAND() != nullptr);
        mi.isStatic = !mi.hasSelfReceiver;

        unsigned retDims = 0;
        mi.returnType = resolveTypeSpec(method->typeSpec(), retDims);

        if (mi.isStatic) {
            if (auto* pl = method->paramList()) {
                for (auto* p : pl->param()) {
                    unsigned dims = 0;
                    mi.paramTypes.push_back(resolveTypeSpec(p->typeSpec(), dims));
                }
            }
        } else {
            for (auto* p : method->param()) {
                unsigned dims = 0;
                mi.paramTypes.push_back(resolveTypeSpec(p->typeSpec(), dims));
            }
        }

        ti.methods.push_back(std::move(mi));
    }

    traitRegistry_.registerTrait(std::move(ti));
    syncToSemanticDB_Trait(name, decl);
}

// ── Impl declaration checking ────────────────────────────────────────────────



void Checker::checkImplDecl(LucisParser::ImplDeclContext* decl)
{
    // If generic impl, store as template
    if (auto* tpl = decl->typeParamList()) {
        GenericImplTemplate tmpl;
        for (auto* tp : tpl->typeParam()) {
            tmpl.typeParams.push_back(tp->IDENTIFIER(0)->getText());
            if (tp->COLON()) {
                // Extract constraint names (e.g. T: Hashable + Comparable)
                for (size_t i = 1; i < tp->IDENTIFIER().size(); ++i)
                    tmpl.typeParamConstraints.push_back(tp->IDENTIFIER()[i]->getText());
            }
        }
        tmpl.decl = decl;
        // Key by the implementing type base name
        auto implTypeName = decl->typeSpec(1)->getText();
        genericImplTemplates_[implTypeName] = std::move(tmpl);

        std::vector<std::string> savedParams;
        for (const auto& tp : tmpl.typeParams) savedParams.push_back(tp);
        syncToSemanticDB_GenericImpl(implTypeName, savedParams, decl);
        return;
    }

    // Step 1: resolve trait type spec
    unsigned traitDims = 0;
    const TypeInfo* traitTI = resolveTypeSpec(decl->typeSpec(0), traitDims);
    if (!traitTI) {
        error(decl, "unknown trait type in impl declaration");
        return;
    }

    // Look up the trait definition
    const TraitInfo* trait = traitRegistry_.lookup(traitTI->name);
    TraitInfo genericTraitStorage; // lives for the scope of this function
    if (!trait && genericTraitTemplates_.count(traitTI->name)) {
        // Generic trait — extract methods from the template with substitution
        auto& gtmpl = genericTraitTemplates_[traitTI->name];

        // Build substitution map: type param → concrete type arg
        std::unordered_map<std::string, const TypeInfo*> subst;
        if (gtmpl.typeParams.size() == decl->typeSpec(0)->typeSpec().size()) {
            for (size_t i = 0; i < gtmpl.typeParams.size(); ++i) {
                unsigned argDims = 0;
                auto* argTI = resolveTypeSpec(decl->typeSpec(0)->typeSpec(i), argDims);
                if (argTI) subst[gtmpl.typeParams[i]] = argTI;
            }
        }

        genericTraitStorage.name = traitTI->name;
        genericTraitStorage.modulePath = currentModulePath_;
        genericTraitStorage.decl = gtmpl.decl;

        for (auto* method : gtmpl.decl->traitMethod()) {
            TraitMethodInfo mi;
            mi.name = method->name->getText();
            mi.hasSelfReceiver = (method->AMPERSAND() != nullptr);
            mi.isStatic = !mi.hasSelfReceiver;

            unsigned retDims = 0;
            if (subst.empty())
                mi.returnType = resolveTypeSpec(method->typeSpec(), retDims);
            else
                mi.returnType = resolveTypeSpecWithSubst(method->typeSpec(), subst, retDims);

            if (mi.isStatic) {
                if (auto* pl = method->paramList()) {
                    for (auto* p : pl->param()) {
                        unsigned dims = 0;
                        if (subst.empty())
                            mi.paramTypes.push_back(resolveTypeSpec(p->typeSpec(), dims));
                        else
                            mi.paramTypes.push_back(resolveTypeSpecWithSubst(p->typeSpec(), subst, dims));
                    }
                }
            } else {
                for (auto* p : method->param()) {
                    unsigned dims = 0;
                    if (subst.empty())
                        mi.paramTypes.push_back(resolveTypeSpec(p->typeSpec(), dims));
                    else
                        mi.paramTypes.push_back(resolveTypeSpecWithSubst(p->typeSpec(), subst, dims));
                }
            }

            genericTraitStorage.methods.push_back(std::move(mi));
        }

        trait = &genericTraitStorage;
    }
    if (!trait) {
        error(decl, "'" + traitTI->name + "' is not a trait");
        return;
    }

    // Step 2: resolve the implementing type
    unsigned implDims = 0;
    const TypeInfo* implTI = resolveTypeSpec(decl->typeSpec(1), implDims);
    if (!implTI) {
        error(decl, "unknown type in impl declaration");
        return;
    }

    // Validate impl type is a struct/union/enum
    if (implTI->kind != TypeKind::Struct &&
        implTI->kind != TypeKind::Union &&
        implTI->kind != TypeKind::Enum) {
        error(decl, "cannot implement trait for primitive type '" +
                   implTI->name + "'");
        return;
    }

    // Step 3: verify each impl method matches a trait method
    ImplInfo implInfo;
    implInfo.traitName = trait->name;
    implInfo.typeName = implTI->name;
    implInfo.decl = decl;
    implInfo.modulePath = currentModulePath_;

    // Build Self -> impl type substitution
    std::unordered_map<std::string, const TypeInfo*> subst;
    subst["Self"] = implTI;

    for (auto* method : decl->implMethod()) {
        auto methodName = method->name->getText();

        // Find matching trait method
        const TraitMethodInfo* traitMethod = nullptr;
        for (auto& tm : trait->methods) {
            if (tm.name == methodName) {
                traitMethod = &tm;
                break;
            }
        }

        if (!traitMethod) {
            error(method, "method '" + methodName + "' not found in trait '" +
                          trait->name + "'");
            continue;
        }

        // Verify static/instance consistency
        bool isStatic = (method->AMPERSAND() == nullptr);
        if (isStatic != traitMethod->isStatic) {
            error(method, "method '" + methodName + "' should be " +
                          (traitMethod->isStatic ? "static" : "instance") +
                          " to match trait '" + trait->name + "'");
            continue;
        }

        // Register the impl method info (body checked in later pass)
        TraitMethodInfo implMethod;
        implMethod.name = methodName;
        implMethod.isStatic = isStatic;
        implMethod.hasSelfReceiver = !isStatic;

        unsigned retDims = 0;
        implMethod.returnType = resolveTypeSpec(method->typeSpec(), retDims);

        if (isStatic) {
            if (auto* pl = method->paramList()) {
                for (auto* p : pl->param()) {
                    unsigned dims = 0;
                    implMethod.paramTypes.push_back(resolveTypeSpec(p->typeSpec(), dims));
                }
            }
        } else {
            for (auto* p : method->param()) {
                unsigned dims = 0;
                implMethod.paramTypes.push_back(resolveTypeSpec(p->typeSpec(), dims));
            }
        }

        implInfo.methodBodies.push_back(std::move(implMethod));
    }

    // Mark the type as implementing this trait
    auto* mutableTI = typeRegistry_.lookupMutable(implTI->name);
    if (mutableTI)
        mutableTI->implementedTraits.insert(trait->name);

    traitRegistry_.registerImpl(std::move(implInfo));
    syncToSemanticDB_Impl(trait->name, implTI->name, decl);
}

// ── Impl method body checking ─────────────────────────────────────────────────



void Checker::checkImplMethodBodies(LucisParser::ImplDeclContext* decl)
{
    // Skip generic impls — bodies checked during monomorphization
    if (decl->typeParamList()) return;

    // Resolve the impl type
    unsigned implDims = 0;
    const TypeInfo* implTI = resolveTypeSpec(decl->typeSpec(1), implDims);
    if (!implTI) return;

    const TypeInfo* prevImplType = currentImplType_;
    currentImplType_ = implTI;

    for (auto* method : decl->implMethod()) {
        locals_.clear();
        for (auto& [name, vi] : globalVars_)
            locals_[name] = vi;

        auto methodName = method->name->getText();
        bool isStatic = (method->AMPERSAND() == nullptr);

        // Resolve return type
        unsigned retDims = 0;
        auto* retType = resolveTypeSpec(method->typeSpec(), retDims);

        // Set up &self for instance methods
        if (!isStatic) {
            auto* ptrType = getPointerType(implTI);
            locals_["self"] = {ptrType, 0, {}, true, true, nullptr};
        }

        // Register params as locals
        if (isStatic) {
            if (auto* pl = method->paramList()) {
                for (auto* p : pl->param()) {
                    if (p->SPREAD()) continue;
                    auto paramName = p->IDENTIFIER()->getText();
                    unsigned dims = 0;
                    auto* pType = resolveTypeSpec(p->typeSpec(), dims);
                    if (!pType) continue;
                    if (locals_.count(paramName)) {
                        error(p, "duplicate parameter name '" + paramName + "'");
                        continue;
                    }
                    locals_[paramName] = {pType, dims, {}, true, false, nullptr};
                }
            }
        } else {
            for (auto* p : method->param()) {
                if (p->SPREAD()) continue;
                auto paramName = p->IDENTIFIER()->getText();
                unsigned dims = 0;
                auto* pType = resolveTypeSpec(p->typeSpec(), dims);
                if (!pType) continue;
                if (locals_.count(paramName)) {
                    error(p, "duplicate parameter name '" + paramName + "'");
                    continue;
                }
                locals_[paramName] = {pType, dims, {}, true, false, nullptr};
            }
        }

        // Check the body
        auto* prevReturnType = currentReturnType_;
        currentReturnType_ = retType;
        checkBlock(method->block(), retType, nullptr);
        currentReturnType_ = prevReturnType;

        warnUnusedLocals(method);
    }

    currentImplType_ = prevImplType;
}

// ── Trait constraint resolution ──────────────────────────────────────────────



const TraitInfo* Checker::resolveTraitConstraint(const std::string& name,
                                                   antlr4::ParserRuleContext* ctx)
{
    if (auto* trait = traitRegistry_.lookup(name))
        return trait;

    error(ctx, "unknown constraint '" + name +
               "': not a trait or built-in constraint");
    return nullptr;
}



bool Checker::typeSatisfiesTraitConstraint(const TypeInfo* type,
                                             const TraitInfo* trait,
                                             const std::string& paramName,
                                             antlr4::ParserRuleContext* ctx)
{
    if (!trait) return true;

    if (trait->isBuiltin && trait->builtinValidator) {
        if (!trait->builtinValidator(type)) {
            error(ctx, "type '" + type->name + "' does not satisfy '" +
                       trait->name + "' constraint for parameter '" +
                       paramName + "'");
            return false;
        }
        return true;
    }

    if (!traitRegistry_.typeSatisfiesTrait(type, trait)) {
        error(ctx, "type '" + type->name + "' does not implement trait '" +
                   trait->name + "' (required by parameter '" + paramName + "')");
        return false;
    }
    return true;
}



const ImplInfo* Checker::findTraitImpl(const std::string& traitName,
                                         const TypeInfo* type) const
{
    return traitRegistry_.findImpl(traitName, type->name);
}



const TraitMethodInfo* Checker::findTraitMethodImpl(
    const std::string& traitName,
    const TypeInfo* type,
    const std::string& methodName) const
{
    auto* impl = findTraitImpl(traitName, type);
    if (!impl) return nullptr;

    for (auto& m : impl->methodBodies) {
        if (m.name == methodName)
            return &m;
    }
    return nullptr;
}



void Checker::registerFunctionSignature(LucisParser::FunctionDeclContext* func,
                                        bool isLocal) {
    if (!func || func->IDENTIFIER().empty()) {
        error(func, "function must have a valid name");
        return;
    }
    if (!isDeclActive(func->attributeList())) return;
    auto funcName = func->name->getText();

    // Comptime functions: register in comptime registry, skip normal registration
    if (func->COMPTIME()) {
        comptimeRegistry_.registerFunction(funcName,
            static_cast<void*>(func),
            func->typeParamList() != nullptr);
        return;
    }

    // Generic function template — register as template, not as a concrete function
    if (auto* tpl = func->typeParamList()) {
        if (genericFuncTemplates_.count(funcName)) {
            error(func, "generic function '" + funcName + "' already defined");
            return;
        }
        GenericFuncTemplate tmpl;
        for (auto* tp : tpl->typeParam())
            tmpl.typeParams.push_back(tp->IDENTIFIER(0)->getText());
        auto savedTypeParams = tmpl.typeParams;
        tmpl.decl = func;
        genericFuncTemplates_[funcName] = std::move(tmpl);
        syncToSemanticDB_GenericFunc(funcName, savedTypeParams, func);
        return;
    }

    // Detect duplicate function definitions (skip builtins/externs)
    if (functions_.count(funcName) && !globalBuiltins_.count(funcName)) {
        std::string errorMsg = "function '" + funcName + "' already defined";
        if (auto prevDecl = functionDecls_[funcName]) {
            auto prevLine = prevDecl->getStart()->getLine();
            errorMsg += " (first defined at line " + std::to_string(prevLine) + ")";
        }
        error(func, errorMsg);
        return;
    }

    unsigned retDims = 0;
    auto* retType = resolveTypeSpec(func->typeSpec(), retDims);
    if (!retType) return;

    bool isVariadic = false;
    bool isTypedVariadic = false;
    const TypeInfo* variadicElemType = nullptr;
    size_t variadicIndex = 0;
    std::vector<const TypeInfo*> paramTypes;
    std::vector<unsigned> paramDims;
    if (auto* paramList = func->paramList()) {
        auto params = paramList->param();
        for (size_t i = 0; i < params.size(); i++) {
            auto* param = params[i];

            // Untyped variadic: ...
            if (param->SPREAD() && !param->typeSpec()) {
                isVariadic = true;
                variadicIndex = i;
                break;
            }

            unsigned pDims = 0;
            auto* pType = resolveTypeSpec(param->typeSpec(), pDims);
            if (!pType) return;
            paramTypes.push_back(pType);
            paramDims.push_back(pDims);

            if (param->SPREAD()) {
                // Typed variadic: T ...name
                isVariadic = true;
                isTypedVariadic = true;
                variadicElemType = pType;
                variadicIndex = i;
            }
        }
    }

    if (isVariadic) {
        if (!isTypedVariadic && paramTypes.empty())
            error(func, "untyped variadic function '" + funcName +
                        "' must have at least one fixed parameter before '...'");

        if (auto* paramList = func->paramList()) {
            auto params = paramList->param();
            if (variadicIndex < params.size() - 1)
                error(func, "'...' must be the last parameter in function '" +
                            funcName + "'");
        }
    }

    auto* funcType = makeFunctionType(retType, paramTypes, isVariadic, variadicElemType, &paramDims);
    functions_[funcName] = funcType;
    functionDecls_[funcName] = func;
    syncToSemanticDB_Function(funcName, *funcType, currentModulePath_, func);

    // ── #[test] attribute — collect as a test case (local functions only) ──
    if (isLocal) {
        if (auto* testAttr = findAttribute(func->attributeList(), "test")) {
            Attribute attr;
            populateAttribute(testAttr, attr);
            TestSpec spec;
            std::vector<std::string> ignoredErrors;
            // Argument errors are already reported by validateAttributeList;
            // on failure keep the default spec (compilation aborts anyway).
            parseTestSpec(attr, spec, ignoredErrors);

            if (funcName == "main")
                error(func, "main cannot be marked #[test]");
            if (auto* paramList = func->paramList()) {
                if (!paramList->param().empty())
                    error(func, "test function '" + funcName + "' must not take parameters");
            }
            if (!retType || (retType->name != "void" && retType->name != "int32"))
                error(func, "test function '" + funcName + "' must return 'void' or 'int32', got '"
                      + (retType ? retType->name : "unknown") + "'");

            TestCase tc;
            tc.modulePath = currentModulePath_;
            tc.funcName = funcName;
            tc.spec = spec;
            tc.filePath = currentFile_;
            tc.returnsVoid = retType && retType->name == "void";
            if (auto* start = func->getStart()) {
                tc.line = start->getLine();
                tc.col = start->getCharPositionInLine();
            }
            testCases_.push_back(std::move(tc));
        }

        // ── #[fuzz] attribute — collect the program's fuzz target ────────
        if (auto* fuzzAttr = findAttribute(func->attributeList(), "fuzz")) {
            (void)fuzzAttr;  // argument errors are reported by validateAttributeList

            if (funcName == "main")
                error(func, "main cannot be marked #[fuzz]");
            if (retType && retType->name != "void")
                error(func, "fuzz target '" + funcName + "' must return 'void', got '"
                      + retType->name + "'");

            bool validSignature = false;
            if (auto* paramList = func->paramList()) {
                auto params = paramList->param();
                if (params.size() == 1 && params[0]->typeSpec()) {
                    unsigned pDims = 0;
                    auto* pType = resolveTypeSpec(params[0]->typeSpec(), pDims);
                    if (pDims == 1 && pType && pType->name == "uint8")
                        validSignature = true;
                }
            }
            if (!validSignature)
                error(func, "fuzz target '" + funcName + "' must take exactly one parameter of type '[]uint8'");

            if (!fuzzTargets_.empty())
                error(func, "only one #[fuzz] target is allowed per program");

            FuzzTarget ft;
            ft.modulePath = currentModulePath_;
            ft.funcName = funcName;
            ft.filePath = currentFile_;
            if (auto* start = func->getStart()) {
                ft.line = start->getLine();
                ft.col = start->getCharPositionInLine();
            }
            fuzzTargets_.push_back(std::move(ft));
        }
    }

    // ── Validate main() signature ───────────────────────────────────────
    if (funcName == "main" && !func->COMPTIME()) {
        if (!retType || retType->name != "int32") {
            error(func, "main function must return 'int32', got '" +
                  (retType ? retType->name : "unknown") + "'");
        }
        if (auto* paramList = func->paramList()) {
            auto params = paramList->param();
            if (params.size() > 1) {
                error(func, "main function accepts at most 1 parameter");
            } else if (params.size() == 1) {
                auto* param = params[0];
                unsigned pDims = 0;
                auto* pType = resolveTypeSpec(param->typeSpec(), pDims);
                if (pType) {
                    bool valid = false;
                    if (pType->name == "Vec<string>")
                        valid = true;
                    if (pDims == 1 && pType->kind == TypeKind::Pointer &&
                        pType->pointeeType && pType->pointeeType->name == "char")
                        valid = true;
                    if (!valid) {
                        std::string typeName = pType->name;
                        if (pDims > 0) typeName = "[]" + typeName;
                        error(param, "main function parameter must be 'Vec<string>' or '[]cstring', "
                              "got '" + typeName + "'");
                    }
                }
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  Function body check
// ═══════════════════════════════════════════════════════════════════════



void Checker::checkFunction(LucisParser::FunctionDeclContext* func) {
    if (!isDeclActive(func->attributeList())) return;
    // Generic function templates are not checked directly — only their instantiations are.
    if (func->typeParamList()) return;

    // Comptime functions: register (if not already) and type-check the body
    if (func->COMPTIME()) {
        auto funcName = func->name->getText();
        if (!comptimeRegistry_.isComptime(funcName))
            comptimeRegistry_.registerFunction(funcName, func);
        // Fall through to type-check the body for error reporting
    }

    unsigned retDims = 0;
    auto* retType = resolveTypeSpec(func->typeSpec(), retDims);
    if (!retType) {
        // Use void as sentinel so the body still gets checked
        retType = typeRegistry_.lookup("void");
    }

    // Clear labels from previous function
    currentFunctionLabels_.clear();
    asmGotoLabelRefs_.clear();

    // Register parameters as locals (params are always initialized and used)
    if (auto* paramList = func->paramList()) {
        for (auto* param : paramList->param()) {
            // Skip untyped variadic ... (no identifier/type)
            if (param->SPREAD() && !param->IDENTIFIER())
                continue;

            auto paramName = param->IDENTIFIER()->getText();
            unsigned pDims = 0;
            auto* pType = resolveTypeSpec(param->typeSpec(), pDims);
            if (!pType) continue;

            if (param->SPREAD()) {
                // Typed variadic: T ...name → register as []T slice
                pDims = 1;
            }

            if (locals_.count(paramName)) {
                error(param, "duplicate parameter name '" + paramName + "'");
                continue;
            }

            locals_[paramName] = {pType, pDims, {}, true, true, nullptr};
        }
    }

    auto* prevRet = currentReturnType_;
    currentReturnType_ = retType;
    checkBlock(func->block(), retType);
    currentReturnType_ = prevRet;

    // Validate asm goto label references (forward references are now resolved)
    for (auto& refName : asmGotoLabelRefs_) {
        if (!currentFunctionLabels_.count(refName))
            error(func, "undefined label '" + refName + "' in asm goto");
    }

    if (retType->kind != TypeKind::Void) {
        if (!blockAlwaysReturns(func->block())) {
            if (!func || func->IDENTIFIER().empty()) {
                error(func, "function must return a value of type '" + retType->name +
                            "' on all code paths");
            } else {
                error(func, "function '" + func->name->getText() +
                            "' must return a value of type '" + retType->name +
                            "' on all code paths");
            }
        }
    }

    // Warn about unused local variables
    warnUnusedLocals(func);
}



static int64_t parseIntLiteral(const std::string& text, int base = 0) {
    std::string clean;
    clean.reserve(text.size());
    for (char c : text) {
        if (c == '\'' && base != 16) continue; // digit separators (exclude hex 'a'..'f')
        clean += c;
    }
    auto pos = clean.find_first_not_of("+-xXoObB");
    if (pos == std::string::npos) return 0;
    std::string num = clean.substr(pos);
    try {
        return std::stoll(num, nullptr, base);
    } catch (...) { return 0; }
}



static std::string stripSuffix(const std::string& text) {
    static const char* suffixes[] = {
        "i8","i16","i32","i64","i128","iinf","isize",
        "u8","u16","u32","u64","u128","usize",
        "f32","f64","f80","f128", nullptr
    };
    for (const char** s = suffixes; *s; ++s) {
        auto slen = strlen(*s);
        if (text.size() > slen && text.compare(text.size() - slen, slen, *s) == 0)
            return text.substr(0, text.size() - slen);
    }
    return text;
}



std::optional<ComptimeValue> Checker::evaluateConstExpr(LucisParser::ExpressionContext* expr) {
    if (!expr) return std::nullopt;

    // ── @cfg(…) — unwrap and evaluate inner expression ──────────
    if (auto* cfg = dynamic_cast<LucisParser::CfgExprContext*>(expr))
        return evaluateConstExpr(cfg->expression());

    // ── Unsuffixed integer literals ────────────────────────────────
    auto evalIntLit = [](const std::string& text, int base) -> std::optional<ComptimeValue> {
        return ComptimeValue::intVal(parseIntLiteral(text, base));
    };

    if (auto* n = dynamic_cast<LucisParser::IntLitExprContext*>(expr))
        return evalIntLit(n->INT_LIT()->getText(), 0);
    if (auto* n = dynamic_cast<LucisParser::HexLitExprContext*>(expr))
        return evalIntLit(n->HEX_LIT()->getText(), 16);
    if (auto* n = dynamic_cast<LucisParser::OctLitExprContext*>(expr))
        return evalIntLit(n->OCT_LIT()->getText(), 8);
    if (auto* n = dynamic_cast<LucisParser::BinLitExprContext*>(expr))
        return evalIntLit(n->BIN_LIT()->getText(), 2);

    // ── Suffixed integer literals ───────────────────────────────────
    if (auto* n = dynamic_cast<LucisParser::SuffixedIntLitExprContext*>(expr))
        return ComptimeValue::intVal(parseIntLiteral(stripSuffix(n->SUFFIXED_INT()->getText()), 0));
    if (auto* n = dynamic_cast<LucisParser::SuffixedHexLitExprContext*>(expr))
        return ComptimeValue::intVal(parseIntLiteral(stripSuffix(n->SUFFIXED_HEX()->getText()), 16));
    if (auto* n = dynamic_cast<LucisParser::SuffixedOctLitExprContext*>(expr))
        return ComptimeValue::intVal(parseIntLiteral(stripSuffix(n->SUFFIXED_OCT()->getText()), 8));
    if (auto* n = dynamic_cast<LucisParser::SuffixedBinLitExprContext*>(expr))
        return ComptimeValue::intVal(parseIntLiteral(stripSuffix(n->SUFFIXED_BIN()->getText()), 2));

    // ── Float literals ──────────────────────────────────────────────
    auto evalFloatLit = [](const std::string& text) -> std::optional<ComptimeValue> {
        try { return ComptimeValue::floatVal(std::stod(text)); } catch (...) { return std::nullopt; }
    };
    if (dynamic_cast<LucisParser::FloatLitExprContext*>(expr)) {
        auto raw = static_cast<LucisParser::FloatLitExprContext*>(expr)->FLOAT_LIT()->getText();
        return evalFloatLit(raw);
    }
    if (auto* ld = dynamic_cast<LucisParser::LeadingDotFloatLitExprContext*>(expr)) {
        if (ld->INT_LIT()) {
            return evalFloatLit("0." + ld->INT_LIT()->getText());
        }
        return evalFloatLit("0.0");
    }
    if (auto* n = dynamic_cast<LucisParser::SuffixedFloatLitExprContext*>(expr))
        return evalFloatLit(stripSuffix(n->SUFFIXED_FLOAT()->getText()));
    if (auto* sd = dynamic_cast<LucisParser::SuffixedLeadingDotFloatExprContext*>(expr)) {
        auto raw = sd->SUFFIXED_DOT_FLOAT()->getText();
        return evalFloatLit("0." + stripSuffix(raw));
    }
    if (auto* n = dynamic_cast<LucisParser::SuffixedFloatIntExprContext*>(expr))
        return evalFloatLit(stripSuffix(n->SUFFIXED_FLOAT_INT()->getText()));
    if (auto* n = dynamic_cast<LucisParser::SuffixedIntFloatExprContext*>(expr))
        return evalFloatLit(stripSuffix(n->SUFFIXED_INT_FLOAT()->getText()));

    // ── Bool, char, null literals ───────────────────────────────────
    if (dynamic_cast<LucisParser::BoolLitExprContext*>(expr)) {
        auto raw = static_cast<LucisParser::BoolLitExprContext*>(expr)->BOOL_LIT()->getText();
        return ComptimeValue::boolVal(raw == "true");
    }
    if (auto* ch = dynamic_cast<LucisParser::CharLitExprContext*>(expr)) {
        auto raw = ch->CHAR_LIT()->getText();
        if (raw.size() >= 2 && raw[0] == '\'') {
            if (raw.size() == 3 && raw[1] != '\\')
                return ComptimeValue::intVal(static_cast<uint8_t>(raw[1]));
            if (raw.size() > 2 && raw[1] == '\\') {
                if (raw[2] == 'n') return ComptimeValue::intVal(10);
                if (raw[2] == 't') return ComptimeValue::intVal(9);
                if (raw[2] == '0') return ComptimeValue::intVal(0);
                if (raw[2] == '\\') return ComptimeValue::intVal(92);
                if (raw[2] == '\'') return ComptimeValue::intVal(39);
                return ComptimeValue::intVal(static_cast<uint8_t>(raw[2]));
            }
        }
        return ComptimeValue::intVal(0);
    }
    if (dynamic_cast<LucisParser::NullLitExprContext*>(expr))
        return ComptimeValue::intVal(0);

    // ── String literals ─────────────────────────────────────────────
    if (auto* s = dynamic_cast<LucisParser::StrLitExprContext*>(expr)) {
        auto raw = s->STR_LIT()->getText();
        if (raw.size() >= 2) {
            std::string unescaped;
            for (size_t i = 1; i < raw.size() - 1; i++) {
                if (raw[i] == '\\' && i + 1 < raw.size() - 1) {
                    switch (raw[++i]) {
                        case 'n': unescaped += '\n'; break;
                        case 't': unescaped += '\t'; break;
                        case '0': unescaped += '\0'; break;
                        case '\\': unescaped += '\\'; break;
                        case '"': unescaped += '"'; break;
                        default: unescaped += raw[i]; break;
                    }
                } else {
                    unescaped += raw[i];
                }
            }
            return ComptimeValue::stringVal(unescaped);
        }
        return ComptimeValue::stringVal("");
    }

    // ── Identifier reference (other const) ──────────────────────────
    if (auto* id = dynamic_cast<LucisParser::IdentExprContext*>(expr)) {
        auto name = id->IDENTIFIER()->getText();
        auto it = compileTimeValues_.find(name);
        if (it != compileTimeValues_.end())
            return it->second;
        return std::nullopt;
    }

    // ── Parenthesized ───────────────────────────────────────────────
    if (auto* paren = dynamic_cast<LucisParser::ParenExprContext*>(expr))
        return evaluateConstExpr(paren->expression());

    // ── Unary operators ─────────────────────────────────────────────
    if (auto* neg = dynamic_cast<LucisParser::NegExprContext*>(expr)) {
        auto v = evaluateConstExpr(neg->expression());
        if (!v) return std::nullopt;
        if (v->kind() == ComptimeValue::Kind::Int) return ComptimeValue::intVal(-v->asInt());
        if (v->kind() == ComptimeValue::Kind::Float) return ComptimeValue::floatVal(-v->asFloat());
        return std::nullopt;
    }
    if (auto* lnot = dynamic_cast<LucisParser::LogicalNotExprContext*>(expr)) {
        auto v = evaluateConstExpr(lnot->expression());
        if (!v) return std::nullopt;
        if (v->kind() == ComptimeValue::Kind::Bool) return ComptimeValue::boolVal(!v->asBool());
        if (v->kind() == ComptimeValue::Kind::Int) return ComptimeValue::boolVal(!v->asInt());
        return std::nullopt;
    }
    if (auto* bnot = dynamic_cast<LucisParser::BitNotExprContext*>(expr)) {
        auto v = evaluateConstExpr(bnot->expression());
        if (!v || v->kind() != ComptimeValue::Kind::Int) return std::nullopt;
        return ComptimeValue::intVal(~v->asInt());
    }

    // ── Binary operators ────────────────────────────────────────────
    auto evalBinary = [this](auto* ctx, auto opFn) -> std::optional<ComptimeValue> {
        auto sub = ctx->expression();
        if (sub.size() != 2) return std::nullopt;
        auto lhs = evaluateConstExpr(sub[0]);
        auto rhs = evaluateConstExpr(sub[1]);
        if (!lhs || !rhs) return std::nullopt;
        return opFn(*lhs, *rhs);
    };

    // Arithmetic
    if (auto* mul = dynamic_cast<LucisParser::MulExprContext*>(expr)) {
        return evalBinary(mul, [&](const ComptimeValue& a, const ComptimeValue& b)
                         -> std::optional<ComptimeValue> {
            if (a.kind() == ComptimeValue::Kind::Int && b.kind() == ComptimeValue::Kind::Int) {
                // Determine op from token text
                auto raw = mul->getText();
                if (raw.find('*') != std::string::npos) return ComptimeValue::intVal(a.asInt() * b.asInt());
                if (raw.find('/') != std::string::npos) {
                    if (b.asInt() == 0) return std::nullopt;
                    return ComptimeValue::intVal(a.asInt() / b.asInt());
                }
                if (raw.find('%') != std::string::npos) {
                    if (b.asInt() == 0) return std::nullopt;
                    return ComptimeValue::intVal(a.asInt() % b.asInt());
                }
                return std::nullopt;
            }
            if (a.kind() == ComptimeValue::Kind::Float && b.kind() == ComptimeValue::Kind::Float) {
                auto raw = mul->getText();
                if (raw.find('*') != std::string::npos) return ComptimeValue::floatVal(a.asFloat() * b.asFloat());
                if (raw.find('/') != std::string::npos) {
                    if (b.asFloat() == 0.0) return std::nullopt;
                    return ComptimeValue::floatVal(a.asFloat() / b.asFloat());
                }
                return std::nullopt;
            }
            return std::nullopt;
        });
    }

    if (auto* add = dynamic_cast<LucisParser::AddSubExprContext*>(expr)) {
        return evalBinary(add, [&](const ComptimeValue& a, const ComptimeValue& b)
                         -> std::optional<ComptimeValue> {
            if (a.kind() == ComptimeValue::Kind::Int && b.kind() == ComptimeValue::Kind::Int) {
                auto raw = add->getText();
                if (raw.find('+') != std::string::npos) return ComptimeValue::intVal(a.asInt() + b.asInt());
                if (raw.find('-') != std::string::npos) return ComptimeValue::intVal(a.asInt() - b.asInt());
                return std::nullopt;
            }
            if (a.kind() == ComptimeValue::Kind::Float && b.kind() == ComptimeValue::Kind::Float) {
                auto raw = add->getText();
                if (raw.find('+') != std::string::npos) return ComptimeValue::floatVal(a.asFloat() + b.asFloat());
                if (raw.find('-') != std::string::npos) return ComptimeValue::floatVal(a.asFloat() - b.asFloat());
                return std::nullopt;
            }
            if (a.kind() == ComptimeValue::Kind::String && b.kind() == ComptimeValue::Kind::String) {
                return ComptimeValue::stringVal(a.asString() + b.asString());
            }
            return std::nullopt;
        });
    }

    // Shift
    if (auto* lsh = dynamic_cast<LucisParser::LshiftExprContext*>(expr)) {
        return evalBinary(lsh, [](const ComptimeValue& a, const ComptimeValue& b)
                         -> std::optional<ComptimeValue> {
            if (a.kind() != ComptimeValue::Kind::Int || b.kind() != ComptimeValue::Kind::Int)
                return std::nullopt;
            return ComptimeValue::intVal(a.asInt() << b.asInt());
        });
    }
    if (auto* rsh = dynamic_cast<LucisParser::RshiftExprContext*>(expr)) {
        return evalBinary(rsh, [](const ComptimeValue& a, const ComptimeValue& b)
                         -> std::optional<ComptimeValue> {
            if (a.kind() != ComptimeValue::Kind::Int || b.kind() != ComptimeValue::Kind::Int)
                return std::nullopt;
            // Use logical shift for unsigned, arithmetic for signed
            // We default to arithmetic (signed) for simplicity
            return ComptimeValue::intVal(a.asInt() >> b.asInt());
        });
    }

    // Relational
    if (auto* rel = dynamic_cast<LucisParser::RelExprContext*>(expr)) {
        return evalBinary(rel, [&](const ComptimeValue& a, const ComptimeValue& b)
                         -> std::optional<ComptimeValue> {
            bool result = false;
            auto raw = rel->getText();
            if (a.kind() == ComptimeValue::Kind::Int && b.kind() == ComptimeValue::Kind::Int) {
                if (raw.find("<=") != std::string::npos) result = a.asInt() <= b.asInt();
                else if (raw.find(">=") != std::string::npos) result = a.asInt() >= b.asInt();
                else if (raw.find('<') != std::string::npos) result = a.asInt() < b.asInt();
                else if (raw.find('>') != std::string::npos) result = a.asInt() > b.asInt();
                return ComptimeValue::boolVal(result);
            }
            if (a.kind() == ComptimeValue::Kind::Float && b.kind() == ComptimeValue::Kind::Float) {
                if (raw.find("<=") != std::string::npos) result = a.asFloat() <= b.asFloat();
                else if (raw.find(">=") != std::string::npos) result = a.asFloat() >= b.asFloat();
                else if (raw.find('<') != std::string::npos) result = a.asFloat() < b.asFloat();
                else if (raw.find('>') != std::string::npos) result = a.asFloat() > b.asFloat();
                return ComptimeValue::boolVal(result);
            }
            return std::nullopt;
        });
    }

    // Equality
    if (auto* eq = dynamic_cast<LucisParser::EqExprContext*>(expr)) {
        return evalBinary(eq, [&](const ComptimeValue& a, const ComptimeValue& b)
                         -> std::optional<ComptimeValue> {
            bool result = false;
            auto raw = eq->getText();
            bool isEq = raw.find("==") != std::string::npos;
            if (a.kind() == ComptimeValue::Kind::Int && b.kind() == ComptimeValue::Kind::Int)
                result = (a.asInt() == b.asInt());
            else if (a.kind() == ComptimeValue::Kind::Float && b.kind() == ComptimeValue::Kind::Float)
                result = (a.asFloat() == b.asFloat());
            else if (a.kind() == ComptimeValue::Kind::Bool && b.kind() == ComptimeValue::Kind::Bool)
                result = (a.asBool() == b.asBool());
            else if (a.kind() == ComptimeValue::Kind::String && b.kind() == ComptimeValue::Kind::String)
                result = (a.asString() == b.asString());
            else return std::nullopt;
            return ComptimeValue::boolVal(isEq ? result : !result);
        });
    }

    // Bitwise
    if (auto* ba = dynamic_cast<LucisParser::BitAndExprContext*>(expr)) {
        return evalBinary(ba, [](const ComptimeValue& a, const ComptimeValue& b)
                         -> std::optional<ComptimeValue> {
            if (a.kind() != ComptimeValue::Kind::Int || b.kind() != ComptimeValue::Kind::Int)
                return std::nullopt;
            return ComptimeValue::intVal(a.asInt() & b.asInt());
        });
    }
    if (auto* bx = dynamic_cast<LucisParser::BitXorExprContext*>(expr)) {
        return evalBinary(bx, [](const ComptimeValue& a, const ComptimeValue& b)
                         -> std::optional<ComptimeValue> {
            if (a.kind() != ComptimeValue::Kind::Int || b.kind() != ComptimeValue::Kind::Int)
                return std::nullopt;
            return ComptimeValue::intVal(a.asInt() ^ b.asInt());
        });
    }
    if (auto* bo = dynamic_cast<LucisParser::BitOrExprContext*>(expr)) {
        return evalBinary(bo, [](const ComptimeValue& a, const ComptimeValue& b)
                         -> std::optional<ComptimeValue> {
            if (a.kind() != ComptimeValue::Kind::Int || b.kind() != ComptimeValue::Kind::Int)
                return std::nullopt;
            return ComptimeValue::intVal(a.asInt() | b.asInt());
        });
    }

    // Logical
    if (auto* la = dynamic_cast<LucisParser::LogicalAndExprContext*>(expr)) {
        return evalBinary(la, [](const ComptimeValue& a, const ComptimeValue& b)
                         -> std::optional<ComptimeValue> {
            if (a.kind() == ComptimeValue::Kind::Bool && b.kind() == ComptimeValue::Kind::Bool)
                return ComptimeValue::boolVal(a.asBool() && b.asBool());
            if (a.kind() == ComptimeValue::Kind::Int && b.kind() == ComptimeValue::Kind::Int)
                return ComptimeValue::boolVal(a.asInt() && b.asInt());
            return std::nullopt;
        });
    }
    if (auto* lo = dynamic_cast<LucisParser::LogicalOrExprContext*>(expr)) {
        return evalBinary(lo, [](const ComptimeValue& a, const ComptimeValue& b)
                         -> std::optional<ComptimeValue> {
            if (a.kind() == ComptimeValue::Kind::Bool && b.kind() == ComptimeValue::Kind::Bool)
                return ComptimeValue::boolVal(a.asBool() || b.asBool());
            if (a.kind() == ComptimeValue::Kind::Int && b.kind() == ComptimeValue::Kind::Int)
                return ComptimeValue::boolVal(a.asInt() || b.asInt());
            return std::nullopt;
        });
    }
    if (auto* nc = dynamic_cast<LucisParser::NullCoalExprContext*>(expr)) {
        return evalBinary(nc, [](const ComptimeValue& a, const ComptimeValue& b)
                         -> std::optional<ComptimeValue> {
            if (a.kind() == ComptimeValue::Kind::Int)
                return a.asInt() != 0 ? a : b;
            if (a.kind() == ComptimeValue::Kind::Bool)
                return a.asBool() ? a : b;
            return b;
        });
    }

    // ── Ternary ─────────────────────────────────────────────────────
    if (auto* tern = dynamic_cast<LucisParser::TernaryExprContext*>(expr)) {
        auto sub = tern->expression();
        if (sub.size() != 3) return std::nullopt;
        auto cond = evaluateConstExpr(sub[0]);
        if (!cond) return std::nullopt;
        bool isTrue = (cond->kind() == ComptimeValue::Kind::Bool) ? cond->asBool()
                    : (cond->kind() == ComptimeValue::Kind::Int) ? (cond->asInt() != 0)
                    : false;
        return evaluateConstExpr(isTrue ? sub[1] : sub[2]);
    }

    // ── sizeof / typeof ─────────────────────────────────────────────
    if (auto* sz = dynamic_cast<LucisParser::SizeofExprContext*>(expr)) {
        if (auto* ts = sz->typeSpec()) {
            unsigned dims = 0;
            auto* ti = resolveTypeSpec(ts, dims);
            if (!ti) return std::nullopt;
            // Estimate size from bitWidth
            if (ti->bitWidth > 0)
                return ComptimeValue::intVal(ti->bitWidth / 8);
            if (ti->kind == TypeKind::Pointer)
                return ComptimeValue::intVal(8); // assume 64-bit pointer
            return ComptimeValue::intVal(1);
        }
    }

    // ── Enum access: EnumType::Variant ──────────────────────────────
    auto findEnumVariant = [&](const TypeInfo* enumTI,
                               const std::string& varName) -> std::optional<ComptimeValue> {
        if (!enumTI || enumTI->kind != TypeKind::Enum) return std::nullopt;
        for (auto& v : enumTI->enumVariantInfos) {
            if (v.name == varName)
                return ComptimeValue::intVal(static_cast<int64_t>(v.discriminant));
        }
        return std::nullopt;
    };
    if (auto* ea = dynamic_cast<LucisParser::EnumAccessExprContext*>(expr)) {
        auto ids = ea->IDENTIFIER();
        if (ids.size() >= 2) {
            return findEnumVariant(typeRegistry_.lookup(ids[0]->getText()),
                                   ids[1]->getText());
        }
        return std::nullopt;
    }
    if (auto* gea = dynamic_cast<LucisParser::GenericEnumAccessExprContext*>(expr)) {
        auto ids = gea->IDENTIFIER();
        if (ids.size() >= 2) {
            return findEnumVariant(typeRegistry_.lookup(ids[0]->getText()),
                                   ids.back()->getText());
        }
        return std::nullopt;
    }

    // ── Built-in function calls (all, any within @cfg) ────────────────
    if (auto* fc = dynamic_cast<LucisParser::FnCallExprContext*>(expr)) {
        // Check if the callee is a bare identifier (not a method chain)
        if (auto* ident = dynamic_cast<LucisParser::IdentExprContext*>(fc->expression())) {
            auto name = ident->IDENTIFIER()->getText();
            auto* args = fc->argList();
            if (!args) return ComptimeValue::boolVal(true);
            auto argExprs = args->expression();

            if (name == "all") {
                for (auto* a : argExprs) {
                    auto v = evaluateConstExpr(a);
                    if (!v || (v->kind() == ComptimeValue::Kind::Bool && !v->asBool()))
                        return ComptimeValue::boolVal(false);
                    if (v->kind() != ComptimeValue::Kind::Bool) return std::nullopt;
                }
                return ComptimeValue::boolVal(true);
            }

            if (name == "any") {
                for (auto* a : argExprs) {
                    auto v = evaluateConstExpr(a);
                    if (v && v->kind() == ComptimeValue::Kind::Bool && v->asBool())
                        return ComptimeValue::boolVal(true);
                    if (v && v->kind() != ComptimeValue::Kind::Bool) return std::nullopt;
                }
                return ComptimeValue::boolVal(false);
            }

            if (name == "not") {
                if (argExprs.size() == 1) {
                    auto v = evaluateConstExpr(argExprs[0]);
                    if (v && v->kind() == ComptimeValue::Kind::Bool)
                        return ComptimeValue::boolVal(!v->asBool());
                }
                return std::nullopt;
            }
        }
    }

    return std::nullopt;
}



bool Checker::isValidConstExpr(LucisParser::ExpressionContext* expr) {
    // Literals — always valid
    if (dynamic_cast<LucisParser::IntLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::HexLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::OctLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::BinLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::SuffixedIntLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::SuffixedHexLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::SuffixedOctLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::SuffixedBinLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::SuffixedFloatIntExprContext*>(expr) ||
        dynamic_cast<LucisParser::SuffixedIntFloatExprContext*>(expr) ||
        dynamic_cast<LucisParser::FloatLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::LeadingDotFloatLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::SuffixedFloatLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::SuffixedLeadingDotFloatExprContext*>(expr) ||
        dynamic_cast<LucisParser::SuffixedFloatIntExprContext*>(expr) ||
        dynamic_cast<LucisParser::BoolLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::CharLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::StrLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::CStrLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::NullLitExprContext*>(expr))
        return true;

    // Identifier reference (other const)
    if (dynamic_cast<LucisParser::IdentExprContext*>(expr))
        return true;

    // Enum variant without payload
    if (dynamic_cast<LucisParser::EnumAccessExprContext*>(expr) ||
        dynamic_cast<LucisParser::GenericEnumAccessExprContext*>(expr))
        return true;

    // Parenthesized
    if (auto* paren = dynamic_cast<LucisParser::ParenExprContext*>(expr))
        return isValidConstExpr(paren->expression());

    // Unary operators
    if (auto* neg = dynamic_cast<LucisParser::NegExprContext*>(expr))
        return isValidConstExpr(neg->expression());
    if (auto* lnot = dynamic_cast<LucisParser::LogicalNotExprContext*>(expr))
        return isValidConstExpr(lnot->expression());
    if (auto* bnot = dynamic_cast<LucisParser::BitNotExprContext*>(expr))
        return isValidConstExpr(bnot->expression());

    // Function call to a comptime function — allowed
    if (auto* call = dynamic_cast<LucisParser::FnCallExprContext*>(expr)) {
        if (auto* ident = dynamic_cast<LucisParser::IdentExprContext*>(call->expression())) {
            auto calleeName = ident->IDENTIFIER()->getText();
            if (comptimeRegistry_.isComptime(calleeName)) {
                // Also validate all arguments
                if (auto* argList = call->argList()) {
                    for (auto* arg : argList->expression()) {
                        if (!isValidConstExpr(arg)) return false;
                    }
                }
                return true;
            }
        }
        return false;
    }

    // Binary expressions
    auto checkBinary = [this](auto* ctx) -> bool {
        if (!ctx || ctx->expression().size() != 2) return false;
        return isValidConstExpr(ctx->expression()[0]) &&
               isValidConstExpr(ctx->expression()[1]);
    };

    if (auto* mul = dynamic_cast<LucisParser::MulExprContext*>(expr))
        return checkBinary(mul);
    if (auto* add = dynamic_cast<LucisParser::AddSubExprContext*>(expr))
        return checkBinary(add);
    if (auto* lsh = dynamic_cast<LucisParser::LshiftExprContext*>(expr))
        return checkBinary(lsh);
    if (auto* rsh = dynamic_cast<LucisParser::RshiftExprContext*>(expr))
        return checkBinary(rsh);
    if (auto* rel = dynamic_cast<LucisParser::RelExprContext*>(expr))
        return checkBinary(rel);
    if (auto* eq = dynamic_cast<LucisParser::EqExprContext*>(expr))
        return checkBinary(eq);
    if (auto* ba = dynamic_cast<LucisParser::BitAndExprContext*>(expr))
        return checkBinary(ba);
    if (auto* bx = dynamic_cast<LucisParser::BitXorExprContext*>(expr))
        return checkBinary(bx);
    if (auto* bo = dynamic_cast<LucisParser::BitOrExprContext*>(expr))
        return checkBinary(bo);
    if (auto* la = dynamic_cast<LucisParser::LogicalAndExprContext*>(expr))
        return checkBinary(la);
    if (auto* lo = dynamic_cast<LucisParser::LogicalOrExprContext*>(expr))
        return checkBinary(lo);
    if (auto* nc = dynamic_cast<LucisParser::NullCoalExprContext*>(expr))
        return checkBinary(nc);

    // Ternary expression — all three sub-expressions must be valid
    if (auto* tern = dynamic_cast<LucisParser::TernaryExprContext*>(expr)) {
        auto sub = tern->expression();
        return sub.size() == 3 &&
               isValidConstExpr(sub[0]) &&
               isValidConstExpr(sub[1]) &&
               isValidConstExpr(sub[2]);
    }

    // sizeof / typeof — always valid
    if (dynamic_cast<LucisParser::SizeofExprContext*>(expr) ||
        dynamic_cast<LucisParser::TypeofExprContext*>(expr))
        return true;

    // Struct/union positional init: Point { 10, 20 }
    if (auto* spi = dynamic_cast<LucisParser::StructPosInitExprContext*>(expr)) {
        for (auto* e : spi->expression())
            if (!isValidConstExpr(e)) return false;
        return true;
    }

    // Struct/union named init: Point { x: 10, y: 20 }
    if (auto* sl = dynamic_cast<LucisParser::StructLitExprContext*>(expr)) {
        for (auto* e : sl->expression())
            if (!isValidConstExpr(e)) return false;
        return true;
    }

    // Qualified positional init: Namespace::Point { 10, 20 }
    if (auto* qspi = dynamic_cast<LucisParser::QualifiedStructPosInitExprContext*>(expr)) {
        for (auto* e : qspi->expression())
            if (!isValidConstExpr(e)) return false;
        return true;
    }

    // Qualified named init: Namespace::Point { x: 10, y: 20 }
    if (auto* qsn = dynamic_cast<LucisParser::QualifiedStructNamedInitExprContext*>(expr)) {
        for (auto* e : qsn->expression())
            if (!isValidConstExpr(e)) return false;
        return true;
    }

    return false;
}

// ── Attribute validation ────────────────────────────────────────



void Checker::checkExternDecl(LucisParser::ExternDeclContext* decl) {
    if (!isDeclActive(decl->attributeList())) return;
    auto funcName = decl->IDENTIFIER()->getText();

    unsigned retDims = 0;
    auto* retType = resolveTypeSpec(decl->typeSpec(), retDims);
    if (!retType) return;

    bool isVariadic = (decl->SPREAD() != nullptr);

    std::vector<const TypeInfo*> paramTypes;
    if (auto* paramList = decl->externParamList()) {
        for (auto* param : paramList->externParam()) {
            unsigned pDims = 0;
            auto* pType = resolveTypeSpec(param->typeSpec(), pDims);
            if (!pType) return;
            paramTypes.push_back(pType);
        }
    }

    auto* funcType = makeFunctionType(retType, paramTypes, isVariadic);
    functions_[funcName] = funcType;
    globalBuiltins_.insert(funcName);
}


