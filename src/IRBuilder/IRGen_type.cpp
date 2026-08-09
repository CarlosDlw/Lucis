#include "IRBuilder/IRGen.h"
#include "IRBuilder/IRGen_helpers.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/DebugInfo.h>
#include <llvm/Transforms/Utils/ModuleUtils.h>

#include <vector>
#include <unordered_map>
#include <optional>
#include <filesystem>
#include <fstream>

#ifdef LLVM_VERSION_15_OR_NEWER
#  include <llvm/TargetParser/Host.h>
#else
#  include <llvm/Support/Host.h>
#endif

#include "generated/LucisLexer.h"
#include "namespace/ModuleRegistry.h"
#include "ffi/CBindings.h"
#include "ffi/CMacroEval.h"

#include <cctype>
#include <iostream>



const TypeInfo* IRGen::resolveTypeInfo(LucisParser::TypeSpecContext* ctx) {
    // Guard: null context always returns int32 fallback
    if (!ctx) {
        std::cerr << "lucis: internal error: null type context\n";
        return typeRegistry_.lookup("int32");
    }

    // Check for volatile qualifier: volatile T
    if (ctx->VOLATILE()) {
        if (ctx->typeSpec().empty()) return typeRegistry_.lookup("int32");
        auto* innerTI = resolveTypeInfo(ctx->typeSpec(0));
        if (!innerTI) return typeRegistry_.lookup("int32");
        auto volName = "volatile " + innerTI->name;
        if (auto* existing = typeRegistry_.lookup(volName))
            return existing;
        TypeInfo ti = *innerTI;
        ti.name = volName;
        ti.isVolatile = true;
        typeRegistry_.registerType(std::move(ti));
        return typeRegistry_.lookup(volName);
    }

    // Check for pointer type: *T
    if (isPointerType(ctx)) {
        if (ctx->typeSpec().empty()) return typeRegistry_.lookup("int32");
        auto* inner = ctx->typeSpec(0);
        auto* pointeeTI = resolveTypeInfo(inner);
        if (!pointeeTI) return typeRegistry_.lookup("int32");
        auto ptrName = "*" + pointeeTI->name;

        // Check if pointer type already registered
        if (auto* existing = typeRegistry_.lookup(ptrName))
            return existing;

        // Create and register pointer type
        TypeInfo ti;
        ti.name = ptrName;
        ti.kind = TypeKind::Pointer;
        ti.bitWidth = 0;
        ti.isSigned = false;
        // For *char, use "cstr" suffix to support C string functions in std::log
        if (pointeeTI && pointeeTI->name == "char") {
            ti.builtinSuffix = "cstr";
        } else {
            ti.builtinSuffix = "ptr";
        }
        ti.pointeeType = pointeeTI;
        typeRegistry_.registerType(std::move(ti));
        return typeRegistry_.lookup(ptrName);
    }

    // Check for inline function type: fn(T1, T2) -> T3
    if (auto* fnSpec = ctx->fnTypeSpec()) {
        auto specs = fnSpec->typeSpec();
        auto* retTI = resolveTypeInfo(specs.back());
        if (!retTI) return typeRegistry_.lookup("int32");

        // Build a canonical name for the function type
        std::string fnName = "fn(";
        for (size_t i = 0; i + 1 < specs.size(); i++) {
            if (i > 0) fnName += ", ";
            auto* pTI = resolveTypeInfo(specs[i]);
            fnName += pTI ? pTI->name : "<error>";
        }
        fnName += ") -> " + retTI->name;

        if (auto* existing = typeRegistry_.lookup(fnName))
            return existing;

        TypeInfo ti;
        ti.name = fnName;
        ti.kind = TypeKind::Function;
        ti.bitWidth = 0;
        ti.isSigned = false;
        ti.builtinSuffix = "ptr";
        ti.returnType = retTI;
        for (size_t i = 0; i + 1 < specs.size(); i++) {
            ti.paramTypes.push_back(resolveTypeInfo(specs[i]));
        }
        typeRegistry_.registerType(std::move(ti));
        return typeRegistry_.lookup(fnName);
    }

    // Fallible type: !T or !T:E
    if (ctx->NOT()) {
        auto* inner = resolveTypeInfo(ctx->typeSpec(0));
        if (!inner) return typeRegistry_.lookup("int32");

        const TypeInfo* errType = nullptr;
        if (ctx->typeSpec().size() > 1)
            errType = resolveTypeInfo(ctx->typeSpec(1));
        else
            errType = typeRegistry_.lookup("Error");

        if (!errType) return typeRegistry_.lookup("int32");

        if (errType->name != "Error") {
            bool isErrorDerived = false;
            for (auto* p = errType; p; p = p->parentType) {
                if (p->name == "Error") { isErrorDerived = true; break; }
            }
            if (!isErrorDerived) {
                std::cerr << "lucis: error type in !T must be 'Error' or inherit from 'Error', got '"
                          << errType->name << "'\n";
                return typeRegistry_.lookup("int32");
            }
        }

        auto name = "!" + inner->name;
        if (errType->name != "Error")
            name += ":" + errType->name;

        if (auto* existing = typeRegistry_.lookup(name))
            return existing;

        TypeInfo ti;
        ti.name = name;
        ti.kind = TypeKind::Fallible;
        ti.successType = inner;
        ti.errorType = errType;
        ti.bitWidth = 0;
        ti.isSigned = false;
        typeRegistry_.registerType(std::move(ti));
        return typeRegistry_.lookup(name);
    }

    // Built-in collection types: vec<T>, map<K,V>, set<T>
    std::string nativeBaseName;
    if (ctx->VEC())      nativeBaseName = "Vec";
    else if (ctx->MAP()) nativeBaseName = "Map";
    else if (ctx->SET()) nativeBaseName = "Set";

    if (!nativeBaseName.empty()) {
        auto typeParams = ctx->typeSpec();
        if (typeParams.size() == 1) {
            auto* elemTI = resolveTypeInfo(typeParams[0]);
            if (!elemTI) return typeRegistry_.lookup("int32");
            auto fullName = nativeBaseName + "<" + elemTI->name + ">";
            if (auto* existing = typeRegistry_.lookup(fullName))
                return existing;

            TypeInfo ti;
            ti.name = fullName;
            ti.kind = TypeKind::Extended;
            ti.bitWidth = 0;
            ti.isSigned = false;
            ti.builtinSuffix = elemTI->builtinSuffix.empty() ? "raw" : elemTI->builtinSuffix;
            ti.elementType = elemTI;
            ti.extendedKind = nativeBaseName;
            typeRegistry_.registerType(std::move(ti));
            return typeRegistry_.lookup(fullName);
        } else if (typeParams.size() == 2) {
            auto* keyTI = resolveTypeInfo(typeParams[0]);
            auto* valTI = resolveTypeInfo(typeParams[1]);
            if (!keyTI || !valTI) return typeRegistry_.lookup("int32");
            auto fullName = nativeBaseName + "<" + keyTI->name + ", " + valTI->name + ">";
            if (auto* existing = typeRegistry_.lookup(fullName))
                return existing;

            TypeInfo ti;
            ti.name = fullName;
            ti.kind = TypeKind::Extended;
            ti.bitWidth = 0;
            ti.isSigned = false;
            auto valSuffix = valTI->builtinSuffix.empty() ? "raw" : valTI->builtinSuffix;
            ti.builtinSuffix = keyTI->builtinSuffix + "_" + valSuffix;
            ti.keyType = keyTI;
            ti.valueType = valTI;
            ti.extendedKind = nativeBaseName;
            typeRegistry_.registerType(std::move(ti));
            return typeRegistry_.lookup(fullName);
        }
        std::cerr << "lucis: invalid type parameters for '" << nativeBaseName << "'\n";
        return typeRegistry_.lookup("int32");
    }

    // Built-in tuple type: tuple<T1, T2, ...>
    if (ctx->TUPLE()) {
        auto typeParams = ctx->typeSpec();
        std::string fullName = "tuple<";
        std::vector<const TypeInfo*> elems;
        for (size_t i = 0; i < typeParams.size(); i++) {
            if (i > 0) fullName += ", ";
            auto* elemTI = resolveTypeInfo(typeParams[i]);
            fullName += elemTI ? elemTI->name : "<error>";
            elems.push_back(elemTI);
        }
        fullName += ">";
        if (auto* existing = typeRegistry_.lookup(fullName))
            return existing;

        TypeInfo ti;
        ti.name = fullName;
        ti.kind = TypeKind::Tuple;
        ti.bitWidth = 0;
        ti.isSigned = false;
        ti.tupleElements = std::move(elems);
        typeRegistry_.registerType(std::move(ti));
        return typeRegistry_.lookup(fullName);
    }

    // Generic extended type: Task<int32>, or user-defined generic struct: Node<int32>
    if (ctx->LT()) {
        std::string baseName;
        std::string nsPrefix;
        if (ctx->VEC()) baseName = "Vec";
        else if (ctx->MAP()) baseName = "Map";
        else if (ctx->SET()) baseName = "Set";
        else if (ctx->IDENTIFIER()) baseName = ctx->IDENTIFIER()->getText();
        if (baseName.empty()) {
            std::cerr << "lucis: invalid generic type syntax '" << ctx->getText() << "'\n";
            return typeRegistry_.lookup("int32");
        }
        auto typeParams = ctx->typeSpec();

        // Resolve all type arguments
        std::vector<const TypeInfo*> resolvedArgs;
        for (auto* tp : typeParams) {
            auto* argTI = resolveTypeInfo(tp);
            if (!argTI) argTI = typeRegistry_.lookup("int32");
            resolvedArgs.push_back(argTI);
        }

        // Check if it's a user-defined generic struct
        auto structIt = genericStructTemplates_.find(baseName);
        if (structIt != genericStructTemplates_.end()) {
            return instantiateGenericStruct(baseName, structIt->second, resolvedArgs);
        }
        auto unionIt = genericUnionTemplates_.find(baseName);
        if (unionIt != genericUnionTemplates_.end()) {
            return instantiateGenericUnion(baseName, unionIt->second, resolvedArgs);
        }
        auto enumIt = genericEnumTemplates_.find(baseName);
        if (enumIt != genericEnumTemplates_.end()) {
            return instantiateGenericEnum(baseName, enumIt->second, resolvedArgs);
        }
        auto traitIt = genericTraitTemplates_.find(baseName);
        if (traitIt != genericTraitTemplates_.end()) {
            static TypeInfo traitTI;
            traitTI.name = baseName;
            traitTI.kind = TypeKind::Trait;
            return &traitTI;
        }

        // Lazy load: search module registry for a generic type not yet visited
        if (moduleRegistry_) {
            auto loadGenericType = [&](const ExportedSymbol* sym) -> const TypeInfo* {
                if (!sym || !sym->decl) return nullptr;
                if (sym->kind == ExportedSymbol::Enum) {
                    visitEnumDecl(static_cast<LucisParser::EnumDeclContext*>(sym->decl));
                } else if (sym->kind == ExportedSymbol::Struct) {
                    visitStructDecl(static_cast<LucisParser::StructDeclContext*>(sym->decl));
                } else if (sym->kind == ExportedSymbol::Union) {
                    visitUnionDecl(static_cast<LucisParser::UnionDeclContext*>(sym->decl));
                } else if (sym->kind == ExportedSymbol::BitStruct) {
                    visitBitstructDecl(static_cast<LucisParser::BitstructDeclContext*>(sym->decl));
                } else if (sym->kind == ExportedSymbol::TraitDecl) {
                    visitTraitDecl(static_cast<LucisParser::TraitDeclContext*>(sym->decl));
                } else {
                    return nullptr;
                }
                // Retry: the visit should have populated the template map
                if (auto eIt = genericEnumTemplates_.find(baseName); eIt != genericEnumTemplates_.end())
                    return instantiateGenericEnum(baseName, eIt->second, resolvedArgs);
                if (auto sIt = genericStructTemplates_.find(baseName); sIt != genericStructTemplates_.end())
                    return instantiateGenericStruct(baseName, sIt->second, resolvedArgs);
                if (auto uIt = genericUnionTemplates_.find(baseName); uIt != genericUnionTemplates_.end())
                    return instantiateGenericUnion(baseName, uIt->second, resolvedArgs);
                if (auto tIt = genericTraitTemplates_.find(baseName); tIt != genericTraitTemplates_.end()) {
                    static TypeInfo traitTI;
                    traitTI.name = baseName;
                    traitTI.kind = TypeKind::Trait;
                    return &traitTI;
                }
                return nullptr;
            };

            // Check current module namespace
            if (auto* sym = moduleRegistry_->findSymbol(currentModulePath_, baseName)) {
                if (auto* loaded = loadGenericType(sym))
                    return loaded;
            }

            // Check all imported namespaces
            for (const auto& [_, ns] : userImports_) {
                if (auto* sym = moduleRegistry_->findSymbol(ns, baseName)) {
                    if (auto* loaded = loadGenericType(sym))
                        return loaded;
                }
            }

            // Final fallback: scan all modules
            for (auto& candidateNs : moduleRegistry_->allModules()) {
                if (auto* sym = moduleRegistry_->findSymbol(candidateNs, baseName)) {
                    if (auto* loaded = loadGenericType(sym))
                        return loaded;
                }
            }
        }

        // Check for user-defined generic type that was just loaded
        // (the lazy load above may have populated the template map)
        if (auto sIt = genericStructTemplates_.find(baseName); sIt != genericStructTemplates_.end())
            return instantiateGenericStruct(baseName, sIt->second, resolvedArgs);
        if (auto uIt = genericUnionTemplates_.find(baseName); uIt != genericUnionTemplates_.end())
            return instantiateGenericUnion(baseName, uIt->second, resolvedArgs);
        if (auto eIt = genericEnumTemplates_.find(baseName); eIt != genericEnumTemplates_.end())
            return instantiateGenericEnum(baseName, eIt->second, resolvedArgs);

        // Built-in extended generics (Task<T>, Result<T>, Option<T>)
        if (resolvedArgs.size() == 1) {
            auto* elemTI = resolvedArgs[0];
            if (!elemTI) return typeRegistry_.lookup("int32");
            auto fullName = baseName + "<" + elemTI->name + ">";
            // Only create Extended types for known built-in base names, not for
            // unknown user types (which would have been caught by the checks above)
            auto* extDesc = extTypeRegistry_.lookup(baseName);
            if (extDesc && extDesc->genericArity == 1) {
                if (auto* existing = typeRegistry_.lookup(fullName))
                    return existing;
                TypeInfo ti;
                ti.name = fullName;
                ti.kind = TypeKind::Extended;
                ti.bitWidth = 0;
                ti.isSigned = false;
                ti.builtinSuffix = elemTI->builtinSuffix.empty() ? "raw" : elemTI->builtinSuffix;
                ti.elementType = elemTI;
                ti.extendedKind = baseName;
                typeRegistry_.registerType(std::move(ti));
                return typeRegistry_.lookup(fullName);
            }
        } else if (resolvedArgs.size() == 2) {
            auto* keyTI = resolvedArgs[0];
            auto* valTI = resolvedArgs[1];
            if (!keyTI || !valTI) return typeRegistry_.lookup("int32");
            auto* extDesc = extTypeRegistry_.lookup(baseName);
            if (extDesc && extDesc->genericArity == 2) {
                auto fullName = baseName + "<" + keyTI->name + ", " + valTI->name + ">";
                if (auto* existing = typeRegistry_.lookup(fullName))
                    return existing;
                TypeInfo ti;
                ti.name = fullName;
                ti.kind = TypeKind::Extended;
                ti.bitWidth = 0;
                ti.isSigned = false;
                ti.builtinSuffix = keyTI->builtinSuffix + "_" + valTI->builtinSuffix;
                ti.keyType = keyTI;
                ti.valueType = valTI;
                ti.extendedKind = baseName;
                typeRegistry_.registerType(std::move(ti));
                return typeRegistry_.lookup(fullName);
            }
        }
        if (!genericTraitTemplates_.count(baseName) && !traitRegistry_.lookup(baseName))
            std::cerr << "lucis: unknown generic type '" << baseName << "'\n";
        return typeRegistry_.lookup("int32");
    }

    // Unwrap only array dimensions so []T resolves to T, but []*U resolves to *U.
    auto* inner = ctx;
    while (inner->LBRACKET() && !inner->typeSpec().empty())
        inner = inner->typeSpec(0);

    if (inner != ctx)
        return resolveTypeInfo(inner);

    auto name = inner->getText();

    // Self resolves to the enclosing extend block's struct type
    if (inner->SELF()) {
        for (auto* p = inner->parent; p; p = p->parent) {
            if (auto* ext = dynamic_cast<LucisParser::ExtendDeclContext*>(p)) {
                auto* ti = typeRegistry_.lookup(ext->IDENTIFIER()->getText());
                if (ti) return ti;
                break;
            }
        }
        std::cerr << "lucis: 'Self' used outside of an 'extend' block\n";
        return typeRegistry_.lookup("int32");
    }

    // cstring is a built-in alias for *char
    if (name == "cstring") {
        auto ptrName = "*char";
        if (auto* existing = typeRegistry_.lookup(ptrName))
            return existing;
        auto* charTI = typeRegistry_.lookup("char");
        TypeInfo ti;
        ti.name = ptrName;
        ti.kind = TypeKind::Pointer;
        ti.bitWidth = 0;
        ti.isSigned = false;
        ti.builtinSuffix = "ptr";
        ti.pointeeType = charTI;
        typeRegistry_.registerType(std::move(ti));
        return typeRegistry_.lookup(ptrName);
    }

    auto* ti  = typeRegistry_.lookup(name);
    if (!ti && traitRegistry_.lookup(name)) {
        // Trait name — return a sentinel TypeInfo
        static TypeInfo traitTypeInfo;
        traitTypeInfo.name = name;
        traitTypeInfo.kind = TypeKind::Trait;
        ti = &traitTypeInfo;
    }
    if (!ti) {
        // Check active generic type-param substitution (e.g. T → int32 inside generic body)
        auto substIt = currentGenericSubst_.find(name);
        if (substIt != currentGenericSubst_.end()) return substIt->second;

        // Namespace-aware lazy type loading (cross-file support).
        if (moduleRegistry_) {
            auto loadSymbolType = [&](const ExportedSymbol* sym) {
                if (!sym || !sym->decl) return;
                if (sym->kind == ExportedSymbol::Enum) {
                    visitEnumDecl(static_cast<LucisParser::EnumDeclContext*>(sym->decl));
                } else if (sym->kind == ExportedSymbol::Struct) {
                    visitStructDecl(static_cast<LucisParser::StructDeclContext*>(sym->decl));
                } else if (sym->kind == ExportedSymbol::Union) {
                    visitUnionDecl(static_cast<LucisParser::UnionDeclContext*>(sym->decl));
                } else if (sym->kind == ExportedSymbol::TypeAlias) {
                    visitTypeAliasDecl(static_cast<LucisParser::TypeAliasDeclContext*>(sym->decl));
                } else if (sym->kind == ExportedSymbol::BitStruct) {
                    visitBitstructDecl(static_cast<LucisParser::BitstructDeclContext*>(sym->decl));
                } else if (sym->kind == ExportedSymbol::TraitDecl) {
                    visitTraitDecl(static_cast<LucisParser::TraitDeclContext*>(sym->decl));
                }
            };

            if (auto* sameNs = moduleRegistry_->findSymbol(currentModulePath_, name)) {
                loadSymbolType(sameNs);
                if (auto* loaded = typeRegistry_.lookup(name)) return loaded;
            }

            auto importIt = userImports_.find(name);
            if (importIt != userImports_.end()) {
                if (auto* imported = moduleRegistry_->findSymbol(importIt->second, name)) {
                    loadSymbolType(imported);
                    if (auto* loaded = typeRegistry_.lookup(name)) return loaded;
                }
            }

            // Fallback: scan all unique imported namespaces for the type.
            if (!userImports_.empty()) {
                std::unordered_set<std::string> scannedNs;
                for (const auto& [_, ns] : userImports_) {
                    if (scannedNs.insert(ns).second) {
                        if (auto* found = moduleRegistry_->findSymbol(ns, name)) {
                            loadSymbolType(found);
                            if (auto* loaded = typeRegistry_.lookup(name)) return loaded;
                        }
                    }
                }
            }

            // Final fallback: scan all modules for the type (e.g. transitive type
            // aliases not explicitly imported by the current file).
            for (auto& candidateNs : moduleRegistry_->allModules()) {
                if (auto* found = moduleRegistry_->findSymbol(candidateNs, name)) {
                    loadSymbolType(found);
                    if (auto* loaded = typeRegistry_.lookup(name)) return loaded;
                }
            }
        }

        // Generic template placeholders may appear transiently in IR-only paths.
        // Checker already validates user-facing unknown-type errors.
        if (!(name.size() == 1 && std::isupper(static_cast<unsigned char>(name[0])))
            && !traitRegistry_.lookup(name))
            std::cerr << "lucis: unknown type '" << name << "'\n";
        return typeRegistry_.lookup("int32");
    }
    return ti;
}



bool IRGen::isPointerType(LucisParser::TypeSpecContext* ctx) {
    return ctx->STAR() != nullptr;
}



const TypeInfo* IRGen::resolveExprTypeInfo(LucisParser::ExpressionContext* ctx) {
    // Check cache first (populated by visitFnCallExpr and others)
    auto cacheIt = exprTypeCache_.find(ctx);
    if (cacheIt != exprTypeCache_.end())
        return cacheIt->second;



    // ── Suffixed literals ────────────────────────────────────────────
    static const std::unordered_map<std::string, std::string> kSufMap = {
        {"i8", "int8"}, {"i16", "int16"}, {"i32", "int32"}, {"i64", "int64"},
        {"i128", "int128"}, {"iinf", "intinf"}, {"isize", "isize"},
        {"u8", "uint8"}, {"u16", "uint16"}, {"u32", "uint32"}, {"u64", "uint64"},
        {"u128", "uint128"}, {"usize", "usize"},
        {"f32", "float32"}, {"f64", "float64"}, {"f80", "float80"}, {"f128", "float128"}
    };
    auto resolveSuf = [&](const std::string& text) -> const TypeInfo* {
        std::string suffix;
        splitSuffix(text, suffix);
        if (suffix.empty()) return nullptr;
        auto it = kSufMap.find(suffix);
        return it != kSufMap.end() ? typeRegistry_.lookup(it->second) : nullptr;
    };
    if (auto* si = dynamic_cast<LucisParser::SuffixedIntLitExprContext*>(ctx)) {
        if (auto* t = resolveSuf(si->SUFFIXED_INT()->getText())) return t;
    }
    if (auto* sh = dynamic_cast<LucisParser::SuffixedHexLitExprContext*>(ctx)) {
        if (auto* t = resolveSuf(sh->SUFFIXED_HEX()->getText())) return t;
    }
    if (auto* so = dynamic_cast<LucisParser::SuffixedOctLitExprContext*>(ctx)) {
        if (auto* t = resolveSuf(so->SUFFIXED_OCT()->getText())) return t;
    }
    if (auto* sb = dynamic_cast<LucisParser::SuffixedBinLitExprContext*>(ctx)) {
        if (auto* t = resolveSuf(sb->SUFFIXED_BIN()->getText())) return t;
    }
    if (auto* sf = dynamic_cast<LucisParser::SuffixedFloatLitExprContext*>(ctx)) {
        if (auto* t = resolveSuf(sf->SUFFIXED_FLOAT()->getText())) return t;
    }
    if (auto* sd = dynamic_cast<LucisParser::SuffixedLeadingDotFloatExprContext*>(ctx)) {
        if (auto* t = resolveSuf(sd->SUFFIXED_DOT_FLOAT()->getText())) return t;
    }
    if (auto* si = dynamic_cast<LucisParser::SuffixedIntFloatExprContext*>(ctx)) {
        if (auto* t = resolveSuf(si->SUFFIXED_INT_FLOAT()->getText())) return t;
    }
    if (auto* sf = dynamic_cast<LucisParser::SuffixedFloatIntExprContext*>(ctx)) {
        if (auto* t = resolveSuf(sf->SUFFIXED_FLOAT_INT()->getText())) return t;
    }

    // ── Unsuffixed literals ──────────────────────────────────────────
    if (dynamic_cast<LucisParser::IntLitExprContext*>(ctx) ||
        dynamic_cast<LucisParser::HexLitExprContext*>(ctx) ||
        dynamic_cast<LucisParser::OctLitExprContext*>(ctx) ||
        dynamic_cast<LucisParser::BinLitExprContext*>(ctx))
        return typeRegistry_.lookup("int32");

    if (dynamic_cast<LucisParser::FloatLitExprContext*>(ctx) ||
        dynamic_cast<LucisParser::LeadingDotFloatLitExprContext*>(ctx))
        return typeRegistry_.lookup("float64");

    if (dynamic_cast<LucisParser::BoolLitExprContext*>(ctx))
        return typeRegistry_.lookup("bool");

    if (dynamic_cast<LucisParser::CharLitExprContext*>(ctx))
        return typeRegistry_.lookup("char");

    if (dynamic_cast<LucisParser::StrLitExprContext*>(ctx))
        return typeRegistry_.lookup("string");

    if (dynamic_cast<LucisParser::CStrLitExprContext*>(ctx)) {
        auto ptrName = "*char";
        if (auto* existing = typeRegistry_.lookup(ptrName))
            return existing;
        auto* charTI = typeRegistry_.lookup("char");
        TypeInfo ti;
        ti.name = ptrName;
        ti.kind = TypeKind::Pointer;
        ti.bitWidth = 0;
        ti.isSigned = false;
        ti.builtinSuffix = "ptr";
        ti.pointeeType = charTI;
        typeRegistry_.registerType(std::move(ti));
        return typeRegistry_.lookup(ptrName);
    }

    // ── Backtick string expressions ───────────────────────────────
    if (dynamic_cast<LucisParser::BtickExprContext*>(ctx) ||
        dynamic_cast<LucisParser::RawBtickExprContext*>(ctx) ||
        dynamic_cast<LucisParser::IntBtickExprContext*>(ctx) ||
        dynamic_cast<LucisParser::ShellBtickExprContext*>(ctx) ||
        dynamic_cast<LucisParser::CmptBtickExprContext*>(ctx))
        return typeRegistry_.lookup("string");

    // ── Function call to comptime function ─────────────────────────
    if (auto* call = dynamic_cast<LucisParser::FnCallExprContext*>(ctx)) {
        if (auto* ident = dynamic_cast<LucisParser::IdentExprContext*>(call->expression())) {
            auto calleeName = ident->IDENTIFIER()->getText();
            if (comptimeEngine_ && comptimeEngine_->isReady() &&
                comptimeEngine_->registry().isComptime(calleeName)) {
                auto* decl = static_cast<LucisParser::FunctionDeclContext*>(
                    comptimeEngine_->registry().lookup(calleeName));
                if (decl && decl->typeSpec())
                    return resolveTypeInfo(decl->typeSpec());
            }
        }
        // Fall through to the general FnCallExpr handler below
    }

    // ── Lambda expression: |params| body ────────────────────────────
    if (auto* lexpr = dynamic_cast<LucisParser::LambdaExprContext*>(ctx)) {
        // Register params temporarily to resolve body type
        std::vector<const TypeInfo*> paramTIs;
        std::vector<std::string> paramNames;
        auto* params = lexpr->paramList();
        if (params) {
            for (auto* p : params->param()) {
                if (p->SPREAD()) continue;
                auto* pti = resolveTypeInfo(p->typeSpec());
                paramTIs.push_back(pti);
                auto pname = p->IDENTIFIER()->getText();
                paramNames.push_back(pname);
                auto* pTy = pti ? pti->toLLVMType(*context_, module_->getDataLayout())
                                : llvm::Type::getInt32Ty(*context_);
                auto* alloca = builder_->CreateAlloca(pTy, nullptr, pname + ".tmp_param");
                locals_[pname] = { alloca, pti, 0 };
            }
        }
        auto* bodyTI = resolveExprTypeInfo(lexpr->expression());
        // Clean up temporary param bindings
        for (auto& pn : paramNames)
            locals_.erase(pn);
        if (!bodyTI) bodyTI = typeRegistry_.lookup("int32");
        // Build function type name
        std::string ftName = "fn(";
        for (size_t i = 0; i < paramTIs.size(); i++) {
            if (i > 0) ftName += ", ";
            ftName += (paramTIs[i] ? paramTIs[i]->name : "?");
        }
        ftName += ") -> " + bodyTI->name;
        // Check if type already exists
        if (auto* existing = typeRegistry_.lookup(ftName))
            return existing;
        // Create new function type
        TypeInfo fti;
        fti.name = ftName;
        fti.kind = TypeKind::Function;
        fti.bitWidth = 0;
        fti.isSigned = false;
        fti.builtinSuffix = "ptr";
        fti.returnType = bodyTI;
        fti.paramTypes = paramTIs;
        typeRegistry_.registerType(std::move(fti));
        return typeRegistry_.lookup(ftName);
    }
    if (auto* lblk = dynamic_cast<LucisParser::LambdaBlockExprContext*>(ctx)) {
        // Register params temporarily to resolve block's return type
        std::vector<const TypeInfo*> paramTIs;
        std::vector<std::string> paramNames;
        std::unordered_set<std::string> paramNameSet;
        auto* params = lblk->paramList();
        if (params) {
            for (auto* p : params->param()) {
                if (p->SPREAD()) continue;
                auto* pti = resolveTypeInfo(p->typeSpec());
                paramTIs.push_back(pti);
                auto pname = p->IDENTIFIER()->getText();
                paramNames.push_back(pname);
                paramNameSet.insert(pname);
                // Temporarily register params so return expr can resolve them
                auto* pTy = pti ? pti->toLLVMType(*context_, module_->getDataLayout())
                                : llvm::Type::getInt32Ty(*context_);
                auto* alloca = builder_->CreateAlloca(pTy, nullptr, pname + ".tmp_param");
                locals_[pname] = { alloca, pti, 0 };
            }
        }
        // Walk block to find return stmts
        const TypeInfo* retTI = typeRegistry_.lookup("void");
        for (auto* stmt : lblk->block()->statement()) {
            if (auto* retStmt = stmt->returnStmt()) {
                if (auto* retExpr = retStmt->expression()) {
                    if (auto* rti = resolveExprTypeInfo(retExpr)) {
                        retTI = rti;
                        break;
                    }
                }
            }
        }
        // Clean up temporary param bindings
        for (auto& pn : paramNames)
            locals_.erase(pn);
        std::string ftName = "fn(";
        for (size_t i = 0; i < paramTIs.size(); i++) {
            if (i > 0) ftName += ", ";
            ftName += (paramTIs[i] ? paramTIs[i]->name : "?");
        }
        ftName += ") -> " + retTI->name;
        if (auto* existing = typeRegistry_.lookup(ftName))
            return existing;
        TypeInfo fti;
        fti.name = ftName;
        fti.kind = TypeKind::Function;
        fti.bitWidth = 0;
        fti.isSigned = false;
        fti.builtinSuffix = "ptr";
        fti.returnType = retTI;
        fti.paramTypes = paramTIs;
        typeRegistry_.registerType(std::move(fti));
        return typeRegistry_.lookup(ftName);
    }

    // ── Identifier ───────────────────────────────────────────────────
    if (auto* ident = dynamic_cast<LucisParser::IdentExprContext*>(ctx)) {
        auto name = ident->IDENTIFIER()->getText();
        auto it = locals_.find(name);
        if (it != locals_.end()) return it->second.typeInfo;
        
        // Variadic parameter: return element type info (the slice's element type)
        auto vit = variadicParams_.find(name);
        if (vit != variadicParams_.end()) {
            // Construct a synthetic TypeInfo for the slice []T
            auto elemName = vit->second.elementType ? vit->second.elementType->name : "?";
            std::string sliceName = "[]" + elemName;
            if (auto* existing = typeRegistry_.lookup(sliceName))
                return existing;
            TypeInfo sti;
            sti.name = sliceName;
            sti.kind = vit->second.elementType ? vit->second.elementType->kind : TypeKind::Void;
            sti.elementType = vit->second.elementType;
            sti.bitWidth = 0;
            sti.isSigned = false;
            sti.builtinSuffix = "ptr";
            typeRegistry_.registerType(std::move(sti));
            return typeRegistry_.lookup(sliceName);
        }
        
        // Check top-level consts
        auto cit = topLevelConsts_.find(name);
        if (cit != topLevelConsts_.end()) return cit->second.typeInfo;
        
        auto git = cGlobals_.find(name);
        if (git != cGlobals_.end()) return git->second.type;
        return nullptr;
    }

    // ── Dot access: resolve base struct/union, find field type ───────
    if (auto* dot = dynamic_cast<LucisParser::FieldAccessExprContext*>(ctx)) {
        auto* baseTI = resolveExprTypeInfo(dot->expression());
        if (baseTI && baseTI->kind == TypeKind::Pointer && baseTI->pointeeType) {
            baseTI = baseTI->pointeeType;
        }
        if (!baseTI || (baseTI->kind != TypeKind::Struct && baseTI->kind != TypeKind::Union)) return nullptr;
        auto fname = dot->IDENTIFIER()->getText();
        for (auto& f : baseTI->fields) {
            if (f.name == fname) return f.typeInfo;
        }
        return nullptr;
    }

    // ── Arrow access: resolve base pointer, dereference, find field ──
    if (auto* arrow = dynamic_cast<LucisParser::ArrowAccessExprContext*>(ctx)) {
        auto* baseTI = resolveExprTypeInfo(arrow->expression());
        if (!baseTI || baseTI->kind != TypeKind::Pointer) return nullptr;
        auto* structTI = baseTI->pointeeType;
        if (!structTI || (structTI->kind != TypeKind::Struct && structTI->kind != TypeKind::Union)) return nullptr;
        auto fname = arrow->IDENTIFIER()->getText();
        for (auto& f : structTI->fields) {
            if (f.name == fname) return f.typeInfo;
        }
        return nullptr;
    }

    // ── Address-of: &expr → pointer type ──────────────────────────────
    if (auto* addr = dynamic_cast<LucisParser::AddrOfExprContext*>(ctx)) {
        auto* innerTI = resolveExprTypeInfo(addr->expression());
        if (!innerTI) return nullptr;
        auto ptrName = "*" + innerTI->name;
        if (auto* existing = typeRegistry_.lookup(ptrName))
            return existing;
        TypeInfo ti;
        ti.name = ptrName;
        ti.kind = TypeKind::Pointer;
        ti.bitWidth = 0;
        ti.isSigned = false;
        ti.builtinSuffix = "ptr";
        ti.pointeeType = innerTI;
        typeRegistry_.registerType(std::move(ti));
        return typeRegistry_.lookup(ptrName);
    }

    // ── @ptr(T, addr) → *T ─────────────────────────────────────────
    if (auto* ptr = dynamic_cast<LucisParser::AtPtrExprContext*>(ctx)) {
        auto* pointeeType = resolveTypeInfo(ptr->typeSpec());
        if (!pointeeType) return nullptr;
        auto ptrName = "*" + pointeeType->name;
        if (auto* existing = typeRegistry_.lookup(ptrName))
            return existing;
        TypeInfo ti;
        ti.name = ptrName;
        ti.kind = TypeKind::Pointer;
        ti.bitWidth = 0;
        ti.isSigned = false;
        ti.builtinSuffix = "ptr";
        ti.pointeeType = pointeeType;
        typeRegistry_.registerType(std::move(ti));
        return typeRegistry_.lookup(ptrName);
    }

    // ── @typeName(T), @fields(T), @variants(T) → string ────────────
    //     @typeId(T) → int64
    //     @isStruct(T), @isEnum(T), @isUnion(T) → bool
    if (auto* at = dynamic_cast<LucisParser::AtTypeExprContext*>(ctx)) {
        auto name = at->IDENTIFIER()->getText();
        if (name == "typeName" || name == "fields" || name == "variants")
            return typeRegistry_.lookup("string");
        if (name == "typeId")
            return typeRegistry_.lookup("int64");
        if (name == "isStruct" || name == "isEnum" || name == "isUnion")
            return typeRegistry_.lookup("bool");
        return typeRegistry_.lookup("string"); // fallback
    }

    // ── Dereference: *expr → pointee type ────────────────────────────
    if (auto* deref = dynamic_cast<LucisParser::DerefExprContext*>(ctx)) {
        auto* baseTI = resolveExprTypeInfo(deref->expression());
        if (baseTI && baseTI->kind == TypeKind::Pointer && baseTI->pointeeType)
            return baseTI->pointeeType;
        return nullptr;
    }

    // ── Cast: expr as TypeSpec → destination type ─────────────────────
    if (auto* cast = dynamic_cast<LucisParser::CastExprContext*>(ctx)) {
        if (!currentGenericSubst_.empty())
            return resolveTypeInfoWithSubst(cast->typeSpec(), currentGenericSubst_);
        return resolveTypeInfo(cast->typeSpec());
    }

    // ── Negation: -expr → same type ─────────────────────────────────
    if (auto* neg = dynamic_cast<LucisParser::NegExprContext*>(ctx))
        return resolveExprTypeInfo(neg->expression());

    // ── Logical NOT: !expr → bool ───────────────────────────────────
    if (dynamic_cast<LucisParser::LogicalNotExprContext*>(ctx))
        return typeRegistry_.lookup("bool");

    // ── Bitwise NOT: ~expr → same type ──────────────────────────────
    if (auto* bnot = dynamic_cast<LucisParser::BitNotExprContext*>(ctx))
        return resolveExprTypeInfo(bnot->expression());

    // ── Relational / Equality → bool ────────────────────────────────
    if (dynamic_cast<LucisParser::RelExprContext*>(ctx) ||
        dynamic_cast<LucisParser::EqExprContext*>(ctx) ||
        dynamic_cast<LucisParser::LogicalAndExprContext*>(ctx) ||
        dynamic_cast<LucisParser::LogicalOrExprContext*>(ctx))
        return typeRegistry_.lookup("bool");

    // ── Arithmetic: wider of two operands ───────────────────────────
    if (auto* mul = dynamic_cast<LucisParser::MulExprContext*>(ctx)) {
        auto* lhs = resolveExprTypeInfo(mul->expression(0));
        auto* rhs = resolveExprTypeInfo(mul->expression(1));
        if (lhs && rhs && lhs->kind == rhs->kind)
            return lhs->bitWidth >= rhs->bitWidth ? lhs : rhs;
        return lhs ? lhs : rhs;
    }

    // ── Additive: ptr + int → pointer, else wider ───────────────────
    if (auto* add = dynamic_cast<LucisParser::AddSubExprContext*>(ctx)) {
        auto* lhsTI = resolveExprTypeInfo(add->expression(0));
        if (lhsTI && lhsTI->kind == TypeKind::Pointer) return lhsTI;
        auto* rhsTI = resolveExprTypeInfo(add->expression(1));
        if (rhsTI && rhsTI->kind == TypeKind::Pointer) return rhsTI;
        if (lhsTI && rhsTI && lhsTI->kind == rhsTI->kind)
            return lhsTI->bitWidth >= rhsTI->bitWidth ? lhsTI : rhsTI;
        return lhsTI ? lhsTI : rhsTI;
    }

    // ── Shift: type of left operand ─────────────────────────────────
    if (auto* lsh = dynamic_cast<LucisParser::LshiftExprContext*>(ctx))
        return resolveExprTypeInfo(lsh->expression(0));
    if (auto* rsh = dynamic_cast<LucisParser::RshiftExprContext*>(ctx))
        return resolveExprTypeInfo(rsh->expression(0));

    // ── Bitwise AND/XOR/OR: wider of two ────────────────────────────
    if (auto* ba = dynamic_cast<LucisParser::BitAndExprContext*>(ctx)) {
        auto* lhs = resolveExprTypeInfo(ba->expression(0));
        auto* rhs = resolveExprTypeInfo(ba->expression(1));
        if (lhs && rhs) return lhs->bitWidth >= rhs->bitWidth ? lhs : rhs;
        return lhs ? lhs : rhs;
    }
    if (auto* bx = dynamic_cast<LucisParser::BitXorExprContext*>(ctx)) {
        auto* lhs = resolveExprTypeInfo(bx->expression(0));
        auto* rhs = resolveExprTypeInfo(bx->expression(1));
        if (lhs && rhs) return lhs->bitWidth >= rhs->bitWidth ? lhs : rhs;
        return lhs ? lhs : rhs;
    }
    if (auto* bo = dynamic_cast<LucisParser::BitOrExprContext*>(ctx)) {
        auto* lhs = resolveExprTypeInfo(bo->expression(0));
        auto* rhs = resolveExprTypeInfo(bo->expression(1));
        if (lhs && rhs) return lhs->bitWidth >= rhs->bitWidth ? lhs : rhs;
        return lhs ? lhs : rhs;
    }

    // ── Ternary: type of true branch ────────────────────────────────
    if (auto* tern = dynamic_cast<LucisParser::TernaryExprContext*>(ctx))
        return resolveExprTypeInfo(tern->expression(1));

    // ── Try expression: try expr — unwrap Ok payload for Result enums ─
    if (auto* te = dynamic_cast<LucisParser::TryExprContext*>(ctx)) {
        auto* sourceTI = resolveExprTypeInfo(te->expression(0));
        UnwrapCatchPatternInfo pattern;
        std::string reason;
        if (classifyUnwrapCatchEnum(sourceTI, pattern, reason))
            return singlePayloadType(*pattern.okVariant);
        return sourceTI;
    }

    // ── Match expression: type from first arm ────────────────────────
    if (auto* me = dynamic_cast<LucisParser::MatchExprContext*>(ctx)) {
        for (auto* arm : me->matchArm()) {
            if (arm->block()) continue; // block body, skip
            size_t idx = arm->IF() ? 1 : 0;
            if (idx < arm->expression().size())
                return resolveExprTypeInfo(arm->expression(idx));
        }
        return nullptr;
    }

    // ── Propagate operator: expr? — success payload type ────────────
    if (auto* pe = dynamic_cast<LucisParser::PropagateExprContext*>(ctx)) {
        auto* sourceTI = resolveExprTypeInfo(pe->expression());
        if (sourceTI && sourceTI->kind == TypeKind::Fallible)
            return sourceTI->successType;
        UnwrapCatchPatternInfo pattern;
        std::string reason;
        if (!classifyUnwrapCatchEnum(sourceTI, pattern, reason))
            return nullptr;
        return singlePayloadType(*pattern.okVariant);
    }

    // ── Catch unwrap expression: expr catch { ... } ─────────────────
    if (auto* cu = dynamic_cast<LucisParser::CatchUnwrapExprContext*>(ctx)) {
        auto* sourceTI = resolveExprTypeInfo(cu->expression());
        if (sourceTI && sourceTI->kind == TypeKind::Fallible)
            return sourceTI->successType;
        UnwrapCatchPatternInfo pattern;
        std::string reason;
        if (!classifyUnwrapCatchEnum(sourceTI, pattern, reason))
            return nullptr;
        return singlePayloadType(*pattern.okVariant);
    }

    // ── Cast: resolve target type ───────────────────────────────────
    if (auto* cast = dynamic_cast<LucisParser::CastExprContext*>(ctx))
        return resolveTypeInfo(cast->typeSpec());

    // ── Paren: unwrap ───────────────────────────────────────────────
    if (auto* paren = dynamic_cast<LucisParser::ParenExprContext*>(ctx))
        return resolveExprTypeInfo(paren->expression());

    // ── Struct literal: Type { fields } ─────────────────────────────
    if (auto* sl = dynamic_cast<LucisParser::StructLitExprContext*>(ctx)) {
        auto ids = sl->IDENTIFIER();
        if (!ids.empty()) return typeRegistry_.lookup(ids[0]->getText());
        return nullptr;
    }

    // ── Struct positional init: Type { expr, expr, ... } ────────────
    if (auto* spi = dynamic_cast<LucisParser::StructPosInitExprContext*>(ctx)) {
        return typeRegistry_.lookup(spi->IDENTIFIER()->getText());
    }

    // ── Generic struct/union literal: Type<T, ...> { fields } ──────
    if (auto* gsl = dynamic_cast<LucisParser::GenericStructLitExprContext*>(ctx)) {
        auto baseName = gsl->IDENTIFIER(0)->getText();
        std::vector<const TypeInfo*> typeArgs;
        for (auto* ts : gsl->typeSpec()) {
            auto* argTI = resolveTypeInfo(ts);
            if (!argTI) argTI = typeRegistry_.lookup("int32");
            typeArgs.push_back(argTI);
        }

        auto structIt = genericStructTemplates_.find(baseName);
        if (structIt != genericStructTemplates_.end()) {
            return instantiateGenericStruct(baseName, structIt->second, typeArgs);
        }

        auto unionIt = genericUnionTemplates_.find(baseName);
        if (unionIt != genericUnionTemplates_.end()) {
            return instantiateGenericUnion(baseName, unionIt->second, typeArgs);
        }

        auto enumIt = genericEnumTemplates_.find(baseName);
        if (enumIt != genericEnumTemplates_.end()) {
            return instantiateGenericEnum(baseName, enumIt->second, typeArgs);
        }

        return nullptr;
    }

    // ── Generic struct positional init: Type<T> { expr, ... } ──────
    if (auto* gspi = dynamic_cast<LucisParser::GenericStructPosInitExprContext*>(ctx)) {
        auto baseName = gspi->IDENTIFIER()->getText();
        std::vector<const TypeInfo*> typeArgs;
        for (auto* ts : gspi->typeSpec()) {
            auto* argTI = resolveTypeInfo(ts);
            if (!argTI) argTI = typeRegistry_.lookup("int32");
            typeArgs.push_back(argTI);
        }

        auto structIt = genericStructTemplates_.find(baseName);
        if (structIt != genericStructTemplates_.end()) {
            return instantiateGenericStruct(baseName, structIt->second, typeArgs);
        }

        auto unionIt = genericUnionTemplates_.find(baseName);
        if (unionIt != genericUnionTemplates_.end()) {
            return instantiateGenericUnion(baseName, unionIt->second, typeArgs);
        }

        return nullptr;
    }

    if (auto* qspi = dynamic_cast<LucisParser::QualifiedStructPosInitExprContext*>(ctx)) {
        if (qspi->IDENTIFIER().size() >= 2) return typeRegistry_.lookup(qspi->IDENTIFIER(1)->getText());
        return nullptr;
    }

    if (auto* qsni = dynamic_cast<LucisParser::QualifiedStructNamedInitExprContext*>(ctx)) {
        if (qsni->IDENTIFIER().size() >= 2) return typeRegistry_.lookup(qsni->IDENTIFIER(1)->getText());
        return nullptr;
    }

    if (auto* genv = dynamic_cast<LucisParser::GenericEnumNamedVariantExprContext*>(ctx)) {
        auto ids = genv->IDENTIFIER();
        auto baseName = ids.size() > 0 ? ids[0]->getText() : "";
        std::vector<const TypeInfo*> typeArgs;
        for (auto* ts : genv->typeSpec()) {
            auto* argTI = resolveTypeInfo(ts);
            if (!argTI) argTI = typeRegistry_.lookup("int32");
            typeArgs.push_back(argTI);
        }
        auto enumIt = genericEnumTemplates_.find(baseName);
        if (enumIt != genericEnumTemplates_.end()) {
            return instantiateGenericEnum(baseName, enumIt->second, typeArgs);
        }

        return nullptr;
    }

    if (auto* gepv = dynamic_cast<LucisParser::GenericEnumPosVariantExprContext*>(ctx)) {
        auto ids = gepv->IDENTIFIER();
        auto baseName = ids[0]->getText();
        std::vector<const TypeInfo*> typeArgs;
        for (auto* ts : gepv->typeSpec()) {
            auto* argTI = resolveTypeInfo(ts);
            if (!argTI) argTI = typeRegistry_.lookup("int32");
            typeArgs.push_back(argTI);
        }
        auto enumIt = genericEnumTemplates_.find(baseName);
        if (enumIt != genericEnumTemplates_.end()) {
            return instantiateGenericEnum(baseName, enumIt->second, typeArgs);
        }

        return nullptr;
    }

    // ── Bang-init: vec!{...}, map!{...}, set!{...} ───────────────────
    if (auto* bi = dynamic_cast<LucisParser::BangInitExprContext*>(ctx)) {
        std::string baseName;
        if (bi->VEC())       baseName = "Vec";
        else if (bi->MAP())  baseName = "Map";
        else if (bi->SET())  baseName = "Set";
        else if (bi->IDENTIFIER()) baseName = bi->IDENTIFIER()->getText();
        if (baseName.empty()) return nullptr;

        auto entries = bi->bangEntry();
        if (entries.empty()) return nullptr;

        if (baseName == "Vec" || baseName == "Set") {
            auto* elemTI = resolveExprTypeInfo(entries[0]->expression(0));
            if (!elemTI) return nullptr;
            std::string fullName = baseName + "<" + elemTI->name + ">";
            if (auto* existing = typeRegistry_.lookup(fullName)) return existing;
            return typeRegistry_.lookup(elemTI->name); // partial fallback
        } else if (baseName == "Map") {
            auto* keyTI = resolveExprTypeInfo(entries[0]->expression(0));
            auto* valTI = resolveExprTypeInfo(entries[0]->expression(1));
            if (!keyTI || !valTI) return nullptr;
            std::string fullName = baseName + "<" + keyTI->name + ", " + valTI->name + ">";
            if (auto* existing = typeRegistry_.lookup(fullName)) return existing;
            return typeRegistry_.lookup(keyTI->name); // partial fallback
        }
        return typeRegistry_.lookup(baseName);
    }

    // ── Array literal: [expr, ...] → element type ───────────────────
    if (auto* arr = dynamic_cast<LucisParser::ArrayLitExprContext*>(ctx)) {
        auto elems = arr->expression();
        if (!elems.empty()) return resolveExprTypeInfo(elems[0]);
        return nullptr;
    }

    // ── Tuple literal: (expr, expr, ...) ────────────────────────────
    if (auto* tl = dynamic_cast<LucisParser::TupleLitExprContext*>(ctx)) {
        auto exprs = tl->expression();
        std::string fullName = "tuple<";
        std::vector<const TypeInfo*> elems;
        for (size_t i = 0; i < exprs.size(); i++) {
            if (i > 0) fullName += ", ";
            auto* elemTI = resolveExprTypeInfo(exprs[i]);
            if (!elemTI) elemTI = typeRegistry_.lookup("int32");
            fullName += elemTI->name;
            elems.push_back(elemTI);
        }
        fullName += ">";
        if (auto* existing = typeRegistry_.lookup(fullName))
            return existing;
        TypeInfo ti;
        ti.name = fullName;
        ti.kind = TypeKind::Tuple;
        ti.bitWidth = 0;
        ti.isSigned = false;
        ti.tupleElements = std::move(elems);
        typeRegistry_.registerType(std::move(ti));
        return typeRegistry_.lookup(fullName);
    }

    // ── Tuple index: expr.N → element type ──────────────────────────
    if (auto* ti = dynamic_cast<LucisParser::TupleIndexExprContext*>(ctx)) {
        auto* baseTI = resolveExprTypeInfo(ti->expression());
        if (!baseTI || baseTI->kind != TypeKind::Tuple) return nullptr;
        unsigned idx = std::stoul(ti->INT_LIT()->getText());
        if (idx < baseTI->tupleElements.size())
            return baseTI->tupleElements[idx];
        return nullptr;
    }

    // ── Chained tuple index: expr.N.M → inner element type ─────────
    if (auto* cti = dynamic_cast<LucisParser::ChainedTupleIndexExprContext*>(ctx)) {
        auto* baseTI = resolveExprTypeInfo(cti->expression());
        if (!baseTI || baseTI->kind != TypeKind::Tuple) return nullptr;
        auto text = cti->FLOAT_LIT()->getText();
        auto dotPos = text.find('.');
        unsigned idx1 = std::stoul(text.substr(0, dotPos));
        unsigned idx2 = std::stoul(text.substr(dotPos + 1));
        if (idx1 >= baseTI->tupleElements.size()) return nullptr;
        auto* innerTI = baseTI->tupleElements[idx1];
        if (!innerTI || innerTI->kind != TypeKind::Tuple) return nullptr;
        if (idx2 >= innerTI->tupleElements.size()) return nullptr;
        return innerTI->tupleElements[idx2];
    }

    // ── Tuple arrow index: expr->N → element type ───────────────────
    if (auto* tai = dynamic_cast<LucisParser::TupleArrowIndexExprContext*>(ctx)) {
        auto* baseTI = resolveExprTypeInfo(tai->expression());
        if (!baseTI || baseTI->kind != TypeKind::Pointer) return nullptr;
        auto* tupleTI = baseTI->pointeeType;
        if (!tupleTI || tupleTI->kind != TypeKind::Tuple) return nullptr;
        unsigned idx = std::stoul(tai->INT_LIT()->getText());
        if (idx < tupleTI->tupleElements.size())
            return tupleTI->tupleElements[idx];
        return nullptr;
    }

    // ── Chained tuple arrow index: expr->N.M → inner element type ──
    if (auto* ctai = dynamic_cast<LucisParser::ChainedTupleArrowIndexExprContext*>(ctx)) {
        auto* baseTI = resolveExprTypeInfo(ctai->expression());
        if (!baseTI || baseTI->kind != TypeKind::Pointer) return nullptr;
        auto* tupleTI = baseTI->pointeeType;
        if (!tupleTI || tupleTI->kind != TypeKind::Tuple) return nullptr;
        auto text = ctai->FLOAT_LIT()->getText();
        auto dotPos = text.find('.');
        unsigned idx1 = std::stoul(text.substr(0, dotPos));
        unsigned idx2 = std::stoul(text.substr(dotPos + 1));
        if (idx1 >= tupleTI->tupleElements.size()) return nullptr;
        auto* innerTI = tupleTI->tupleElements[idx1];
        if (!innerTI || innerTI->kind != TypeKind::Tuple) return nullptr;
        if (idx2 >= innerTI->tupleElements.size()) return nullptr;
        return innerTI->tupleElements[idx2];
    }

    // ── Function call: return type from function signature ──────────
    if (auto* call = dynamic_cast<LucisParser::FnCallExprContext*>(ctx)) {
        auto* callee = call->expression();
        if (auto* ident = dynamic_cast<LucisParser::IdentExprContext*>(callee)) {
            auto fname = ident->IDENTIFIER()->getText();

            if (globalBuiltins_.count(fname)) {
                if (fname == "typeof" || fname == "toString" ||
                    fname == "fromCStr" || fname == "fromCStrCopy" ||
                    fname == "fromCStrLen" || fname == "sprintf")
                    return typeRegistry_.lookup("string");
                if (fname == "sizeof" || fname == "toInt")
                    return typeRegistry_.lookup("int64");
                if (fname == "toFloat")
                    return typeRegistry_.lookup("float64");
                if (fname == "toBool")
                    return typeRegistry_.lookup("bool");
            }

            // Builtins with special IRGen handling that need explicit return type
            // ── string → bool ───────────────────────────────────────────
            if (fname == "contains" || fname == "startsWith" || fname == "endsWith" ||
                fname == "isAlpha" || fname == "isDigit" || fname == "isAlphaNum" ||
                fname == "isUpper" || fname == "isLower" || fname == "isWhitespace" ||
                fname == "isPrintable" || fname == "isControl" || fname == "isHexDigit" ||
                fname == "isAscii" || fname == "isPow2" || fname == "testBit" ||
                fname == "isAbsolute" || fname == "isRelative" || fname == "isFile" ||
                fname == "isDir" || fname == "exists" || fname == "isNaN" ||
                fname == "isInf" || fname == "isFinite" || fname == "match" ||
                fname == "isValid" || fname == "hasEnv" || fname == "isTTY" ||
                fname == "isStdoutTTY" || fname == "isStderrTTY" ||
                fname == "isEOF" || fname == "readBool" || fname == "promptBool" ||
                fname == "randBool")
                return typeRegistry_.lookup("bool");
            // ── string → char ────────────────────────────────────────────
            if (fname == "charAt" || fname == "readChar" || fname == "readByte" ||
                fname == "randChar" || fname == "intToChar" || fname == "fromDigit" ||
                fname == "separator")
                return typeRegistry_.lookup("char");
            if (fname == "fromCharCode" || fname == "toDigit")
                return typeRegistry_.lookup("char");
            // ── string → int64 ───────────────────────────────────────────
            if (fname == "indexOf" || fname == "lastIndexOf" || fname == "parseInt" ||
                fname == "parseIntRadix" || fname == "fileSize" || fname == "randInt" ||
                fname == "randIntRange" || fname == "atoi" ||
                fname == "findIndex" || fname == "tcpSend")
                return typeRegistry_.lookup("int64");
            // ── string → int32 ───────────────────────────────────────────
            if (fname == "charToInt" || fname == "exec" || fname == "pid" ||
                fname == "getpid" || fname == "getppid" || fname == "errno" ||
                fname == "kill" || fname == "dup" || fname == "closeFd" ||
                fname == "dup2" || fname == "tcpConnect" || fname == "tcpListen" ||
                fname == "udpBind" || fname == "tcpAccept" ||
                fname == "year" || fname == "month" || fname == "day" ||
                fname == "hour" || fname == "minute" || fname == "second" ||
                fname == "weekday" || fname == "compare")
                return typeRegistry_.lookup("int32");
            // ── string → float64 ─────────────────────────────────────────
            if (fname == "parseFloat" || fname == "atof" || fname == "readFloat" ||
                fname == "promptFloat" || fname == "randFloat" || fname == "randFloatRange")
                return typeRegistry_.lookup("float64");
            // ── string → uint64 ──────────────────────────────────────────
            if (fname == "count")
                return typeRegistry_.lookup("usize");
            if (fname == "hashString" || fname == "hashInt" ||
                fname == "hashCombine" || fname == "randUint" ||
                fname == "now" || fname == "nowNanos" || fname == "nowMicros" ||
                fname == "clock" || fname == "elapsed" || fname == "parseTime" ||
                fname == "threadId" || fname == "fromHex" ||
                fname == "rotl" || fname == "rotr" || fname == "bswap" ||
                fname == "nextPow2" || fname == "bitReverse" ||
                fname == "setBit" || fname == "clearBit" || fname == "toggleBit" ||
                fname == "extractBits")
                return typeRegistry_.lookup("uint64");
            // ── → uint32 ─────────────────────────────────────────────────
            if (fname == "cpuCount" || fname == "crc32" || fname == "popcount" ||
                fname == "ctz" || fname == "clz" || fname == "getuid" || fname == "getgid")
                return typeRegistry_.lookup("uint32");
            // ── → string ─────────────────────────────────────────────────
            if (fname == "toUpper" || fname == "toLower" || fname == "trim" ||
                fname == "trimLeft" || fname == "trimRight" || fname == "reverse" ||
                fname == "replace" || fname == "replaceFirst" || fname == "repeat" ||
                fname == "padLeft" || fname == "padRight" || fname == "substring" ||
                fname == "slice" || fname == "fromBytes" || fname == "fromChars" ||
                fname == "joinVec" || fname == "readLine" || fname == "readAll" ||
                fname == "readPassword" || fname == "prompt" || fname == "promptPassword" ||
                fname == "promptInt" || fname == "readFile" || fname == "cwd" ||
                fname == "tempDir" || fname == "platform" || fname == "arch" ||
                fname == "homeDir" || fname == "executablePath" || fname == "env" ||
                fname == "execOutput" || fname == "hostname" || fname == "strerror" ||
                fname == "itoa" || fname == "itoaRadix" || fname == "utoa" ||
                fname == "ftoa" || fname == "ftoaPrecision" || fname == "toHex" ||
                fname == "toOctal" || fname == "toBinary" || fname == "join" ||
                fname == "parent" || fname == "fileName" || fname == "stem" ||
                fname == "extension" || fname == "normalize" || fname == "toAbsolute" ||
                fname == "withExtension" || fname == "withFileName" ||
                fname == "lpad" || fname == "rpad" || fname == "center" ||
                fname == "hex" || fname == "hexUpper" || fname == "oct" || fname == "bin" ||
                fname == "humanBytes" || fname == "commas" || fname == "fixed" ||
                fname == "scientific" || fname == "percent" ||
                fname == "uuid_v4" || fname == "timestamp" || fname == "formatTime" ||
                fname == "resolve" || fname == "tcpRecv" || fname == "udpRecvFrom" ||
                fname == "md5String" || fname == "sha1String" || fname == "sha256String" ||
                fname == "sha512String" || fname == "md5" || fname == "sha1" ||
                fname == "sha256" || fname == "sha512" ||
                fname == "gzipCompress" || fname == "gzipDecompress" ||
                fname == "deflate" || fname == "inflate" || fname == "compressLevel" ||
                fname == "base64EncodeStr" || fname == "base64DecodeStr" ||
                fname == "base64Encode" || fname == "joinAll" ||
                fname == "urlEncode" || fname == "urlDecode" ||
                fname == "regexReplace" || fname == "regexReplaceFirst" || fname == "find")
                return typeRegistry_.lookup("string");
            // ── → usize ──────────────────────────────────────────────────
            if (fname == "pageSize")
                return typeRegistry_.lookup("usize");
            // ── → void ───────────────────────────────────────────────────
            if (fname == "flush" || fname == "flushErr" || fname == "setCwd" ||
                fname == "freeStr" || fname == "remove" || fname == "removeDir" ||
                fname == "mkdir" || fname == "mkdirAll" || fname == "rename")
                return typeRegistry_.lookup("void");
            // ── → pointer ─────────────────────────────────────────────────
            if (fname == "alloc" || fname == "allocZeroed" || fname == "realloc" ||
                fname == "cstr")
                return typeRegistry_.lookup("*void");
            // ── Vec<string> ───────────────────────────────────────────────
            if (fname == "split" || fname == "splitN" || fname == "lines" ||
                fname == "words" || fname == "listDir" || fname == "readLines" ||
                fname == "findAll" || fname == "regexSplit") {
                auto* elemTI = typeRegistry_.lookup("string");
                auto fullName = std::string("Vec<") + elemTI->name + ">";
                if (auto* existing = typeRegistry_.lookup(fullName))
                    return existing;
                TypeInfo ti;
                ti.name = fullName;
                ti.kind = TypeKind::Extended;
                ti.bitWidth = 0;
                ti.isSigned = false;
                ti.builtinSuffix = elemTI->builtinSuffix.empty() ? "raw" : elemTI->builtinSuffix;
                ti.elementType = elemTI;
                ti.extendedKind = "Vec";
                typeRegistry_.registerType(std::move(ti));
                return typeRegistry_.lookup(fullName);
            }
            // ── Vec<char> ─────────────────────────────────────────────────
            if (fname == "chars") {
                auto* elemTI = typeRegistry_.lookup("char");
                auto fullName = std::string("Vec<") + elemTI->name + ">";
                if (auto* existing = typeRegistry_.lookup(fullName))
                    return existing;
                TypeInfo ti;
                ti.name = fullName;
                ti.kind = TypeKind::Extended;
                ti.bitWidth = 0;
                ti.isSigned = false;
                ti.builtinSuffix = elemTI->builtinSuffix.empty() ? "raw" : elemTI->builtinSuffix;
                ti.elementType = elemTI;
                ti.extendedKind = "Vec";
                typeRegistry_.registerType(std::move(ti));
                return typeRegistry_.lookup(fullName);
            }
            // ── Vec<uint8> ───────────────────────────────────────────────
            if (fname == "toBytes" || fname == "fromBytes" ||
                fname == "readBytes" || fname == "readNBytes" ||
                fname == "randomBytes" || fname == "base64Decode" ||
                fname == "hmacSha256") {
                auto* elemTI = typeRegistry_.lookup("uint8");
                auto fullName = std::string("Vec<") + elemTI->name + ">";
                if (auto* existing = typeRegistry_.lookup(fullName))
                    return existing;
                TypeInfo ti;
                ti.name = fullName;
                ti.kind = TypeKind::Extended;
                ti.bitWidth = 0;
                ti.isSigned = false;
                ti.builtinSuffix = elemTI->builtinSuffix.empty() ? "raw" : elemTI->builtinSuffix;
                ti.elementType = elemTI;
                ti.extendedKind = "Vec";
                typeRegistry_.registerType(std::move(ti));
                return typeRegistry_.lookup(fullName);
            }

            // Builtins from std::conv and similar namespaces
            if (fname == "toHex" || fname == "toBinary" || fname == "toOctal")
                return typeRegistry_.lookup("string");
            if (fname == "fromHex")
                return typeRegistry_.lookup("uint64");

            auto fit = genericFuncTemplates_.find(fname);
            if (fit != genericFuncTemplates_.end()) {
                std::vector<const TypeInfo*> argTypes;
                if (auto* argList = call->argList()) {
                    for (auto* exprCtx : argList->expression())
                        argTypes.push_back(resolveExprTypeInfo(exprCtx));
                }

                std::vector<LucisParser::ParamContext*> formalParams;
                if (auto* paramList = fit->second.decl->paramList())
                    formalParams = paramList->param();

                auto inferred = inferGenericTypeArgs(fit->second.typeParams, formalParams, argTypes);
                if (inferred) {
                    auto* fn = instantiateGenericFunc(fname, fit->second, *inferred);
                    auto mangledName = mangleGenericName(fname, *inferred);
                    if (fn) {
                        auto retIt = fnReturnTypes_.find(mangledName);
                        if (retIt != fnReturnTypes_.end())
                            return retIt->second;
                    }
                }
            }

            // Check C FFI bindings first (has accurate TypeInfo with pointers)
            if (cBindings_) {
                if (auto* cfunc = cBindings_->findFunction(fname))
                    if (cfunc->returnType)
                        return cfunc->returnType;
            }

            // Resolve mangled name for user functions
            auto emitName = resolveCallTarget(fname);

            // Check cached function return TypeInfo (handles tuples etc.)
            auto retIt = fnReturnTypes_.find(emitName);
            if (retIt != fnReturnTypes_.end())
                return retIt->second;

            // Look up user function in the LLVM module
            if (auto* fn = module_->getFunction(emitName)) {
                auto* retTy = fn->getReturnType();
                // Map LLVM return type back to TypeInfo
                if (retTy->isIntegerTy(1))   return typeRegistry_.lookup("bool");
                if (retTy->isIntegerTy(8))   return typeRegistry_.lookup("int8");
                if (retTy->isIntegerTy(16))  return typeRegistry_.lookup("int16");
                if (retTy->isIntegerTy(32))  return typeRegistry_.lookup("int32");
                if (retTy->isIntegerTy(64))  return typeRegistry_.lookup("int64");
                if (retTy->isIntegerTy(128)) return typeRegistry_.lookup("int128");
                if (retTy->isFloatTy())      return typeRegistry_.lookup("float32");
                if (retTy->isDoubleTy())     return typeRegistry_.lookup("float64");
                if (retTy->isVoidTy())       return typeRegistry_.lookup("void");
                if (retTy->isPointerTy())    return typeRegistry_.lookup("*void");
                if (auto* st = llvm::dyn_cast<llvm::StructType>(retTy)) {
                    if (st->hasName())
                        return typeRegistry_.lookup(st->getName().str());
                    // Lucis string is an unnamed {ptr, len} fat pointer —
                    // map it back so method calls on string-returning
                    // user functions resolve (e.g. f().padRight(...)).
                    if (st->getNumElements() == 2 &&
                        st->getElementType(0)->isPointerTy() &&
                        st->getElementType(1)->isIntegerTy())
                        return typeRegistry_.lookup("string");
                }
            }

            // Builtin string-returning functions (cwd, tempDir)
            if (fname == "cwd" || fname == "tempDir")
                return typeRegistry_.lookup("string");
        }
        return nullptr;
    }

    // ── Static method call: Struct::method(args) ────────────────────
    if (auto* smc = dynamic_cast<LucisParser::StaticMethodCallExprContext*>(ctx)) {
        auto ids = smc->IDENTIFIER();
        {
            std::vector<std::string> idTexts;
            for (auto* id : ids) idTexts.push_back(id->getText());
            if (IntrinsicRegistry::isIntrinsicPrefix(idTexts[0])) {
                std::string ns, funcName;
                IntrinsicRegistry::parseIntrinsicPath(idTexts, ns, funcName);
                auto* intrinsic = intrinsicRegistry_.lookup(ns, funcName);
                if (intrinsic && intrinsic->returnType != "_any"
                    && intrinsic->returnType != "_self")
                    return typeRegistry_.lookup(intrinsic->returnType);
            }
        }
        auto structName = ids[0]->getText();
        auto methodName = ids[1]->getText();

        if (!typeRegistry_.lookup(structName))
            syncTypeFromSemanticDB(structName);
        if (auto* enumType = typeRegistry_.lookup(structName);
            enumType && enumType->kind == TypeKind::Enum) {
            return enumType;
        }

        auto extIt = genericExtendTemplates_.find(structName);
        auto structIt = genericStructTemplates_.find(structName);
        if (extIt != genericExtendTemplates_.end() && structIt != genericStructTemplates_.end()) {
            std::vector<const TypeInfo*> argTypes;
            if (auto* argList = smc->argList()) {
                for (auto* argExpr : argList->expression())
                    argTypes.push_back(resolveExprTypeInfo(argExpr));
            }

            for (auto* method : extIt->second.decl->extendMethod()) {
                if (method->AMPERSAND()) continue;
                if (method->IDENTIFIER(0)->getText() != methodName) continue;

                std::vector<LucisParser::ParamContext*> formalParams;
                if (auto* paramList = method->paramList())
                    formalParams = paramList->param();

                auto inferred = inferGenericTypeArgs(extIt->second.typeParams, formalParams, argTypes);
                if (!inferred) break;

                auto* instanceTI = instantiateGenericStruct(structName, structIt->second, *inferred);
                if (!instanceTI) break;

                auto smIt = staticStructMethods_.find(instanceTI->name);
                if (smIt != staticStructMethods_.end()) {
                    auto mIt = smIt->second.find(methodName);
                    if (mIt != smIt->second.end())
                        return mIt->second.returnType;
                }
                break;
            }
        }

        auto smIt = staticStructMethods_.find(structName);
        if (smIt != staticStructMethods_.end()) {
            auto mIt = smIt->second.find(methodName);
            if (mIt != smIt->second.end())
                return mIt->second.returnType;
        }
        return nullptr;
    }

    // ── Generic qualified function call: ns::func<T>(args) ─────────
    if (auto* gqc = dynamic_cast<LucisParser::GenericQualifiedFnCallExprContext*>(ctx)) {
        std::vector<std::string> idTexts;
        for (auto* id : gqc->IDENTIFIER())
            idTexts.push_back(id->getText());
        if (idTexts.size() >= 2 && IntrinsicRegistry::isIntrinsicPrefix(idTexts[0])) {
            std::string ns, funcName;
            IntrinsicRegistry::parseIntrinsicPath(idTexts, ns, funcName);
            auto* intrinsic = intrinsicRegistry_.lookup(ns, funcName);
            if (intrinsic) {
                if (intrinsic->returnType != "_any" && intrinsic->returnType != "_self")
                    return typeRegistry_.lookup(intrinsic->returnType);
                // Generic intrinsic with _any return: resolve from type args.
                auto typeSpecs = gqc->typeSpec();
                if (!typeSpecs.empty()) {
                    auto* argTI = resolveTypeInfo(typeSpecs[0]);
                    if (argTI) return argTI;
                }
            }
        }
        return nullptr;
    }

    if (auto* gsmc = dynamic_cast<LucisParser::GenericStaticMethodCallExprContext*>(ctx)) {
        auto ids = gsmc->IDENTIFIER();
        auto baseName = ids[0]->getText();

        std::vector<const TypeInfo*> typeArgs;
        for (auto* ts : gsmc->typeSpec()) {
            auto* argTI = resolveTypeInfo(ts);
            if (!argTI) argTI = typeRegistry_.lookup("int32");
            typeArgs.push_back(argTI);
        }

        auto enumIt = genericEnumTemplates_.find(baseName);
        if (enumIt != genericEnumTemplates_.end())
            return instantiateGenericEnum(baseName, enumIt->second, typeArgs);

        auto structIt = genericStructTemplates_.find(baseName);
        if (structIt != genericStructTemplates_.end()) {
            auto* instanceTI = instantiateGenericStruct(baseName, structIt->second, typeArgs);
            if (!instanceTI) return nullptr;

            auto smIt = staticStructMethods_.find(instanceTI->name);
            if (smIt != staticStructMethods_.end()) {
                    auto mIt = smIt->second.find(ids[1]->getText());
                    if (mIt != smIt->second.end())
                        return mIt->second.returnType;
            }
        }
        return nullptr;
    }

    if (auto* ea = dynamic_cast<LucisParser::EnumAccessExprContext*>(ctx)) {
        auto ids = ea->IDENTIFIER();
        if (!ids.empty()) return typeRegistry_.lookup(ids[0]->getText());
        return nullptr;
    }

    if (auto* gea = dynamic_cast<LucisParser::GenericEnumAccessExprContext*>(ctx)) {
        auto ids = gea->IDENTIFIER();
        auto baseName = ids[0]->getText();
        std::vector<const TypeInfo*> typeArgs;
        for (auto* ts : gea->typeSpec()) {
            auto* argTI = resolveTypeInfo(ts);
            if (!argTI) argTI = typeRegistry_.lookup("int32");
            typeArgs.push_back(argTI);
        }
        auto enumIt = genericEnumTemplates_.find(baseName);
        if (enumIt != genericEnumTemplates_.end())
            return instantiateGenericEnum(baseName, enumIt->second, typeArgs);
        return nullptr;
    }

    // ── Method call: lookup in method registry ──────────────────────
    if (auto* mc = dynamic_cast<LucisParser::MethodCallExprContext*>(ctx)) {
        auto* recvTI = resolveExprTypeInfo(mc->expression());
        if (!recvTI) return nullptr;
        // Auto-dereference pointer receivers for chained calls (ptr.method())
        if (recvTI->kind == TypeKind::Pointer && recvTI->pointeeType)
            recvTI = recvTI->pointeeType;
        auto methodName = mc->IDENTIFIER()->getText();

        auto recvArrayDims = resolveExprArrayDims(mc->expression());
        if (recvArrayDims > 0) {
            auto* arrayDesc = methodRegistry_.lookupArrayMethod(methodName);
            if (arrayDesc) {
                if (arrayDesc->returnType == "_self") return recvTI;
                if (arrayDesc->returnType == "_elem") {
                    // For array receivers, resolveExprTypeInfo already denotes
                    // the element type T of []T.
                    return recvTI;
                }
                return typeRegistry_.lookup(arrayDesc->returnType);
            }
        }

        auto* desc = methodRegistry_.lookup(recvTI->kind, methodName);
        if (desc) {
            if (desc->returnType == "_self") return recvTI;
            if (desc->returnType == "_elem") return recvTI->elementType;
            if (desc->returnType == "_key")  return recvTI->keyType;
            if (desc->returnType == "_val")  return recvTI->valueType;
            if (desc->returnType == "_vec_key" && recvTI->keyType) {
                auto fullName = "Vec<" + recvTI->keyType->name + ">";
                if (auto* existing = typeRegistry_.lookup(fullName))
                    return existing;
            }
            if (desc->returnType == "_vec_val" && recvTI->valueType) {
                auto fullName = "Vec<" + recvTI->valueType->name + ">";
                if (auto* existing = typeRegistry_.lookup(fullName))
                    return existing;
            }
            return typeRegistry_.lookup(desc->returnType);
        }

        // Extended type method lookup via ExtendedTypeRegistry
        if (recvTI->kind == TypeKind::Extended) {
            auto* extDesc = extTypeRegistry_.lookup(recvTI->extendedKind);
            if (extDesc) {
                for (auto& m : extDesc->methods) {
                    if (m.name != methodName) continue;
                    if (m.returnType == "_self") return recvTI;
                    if (m.returnType == "_elem") return recvTI->elementType;
                    if (m.returnType == "_key")  return recvTI->keyType;
                    if (m.returnType == "_val")  return recvTI->valueType;
                    if (m.returnType == "_vec_key" && recvTI->keyType) {
                        auto fn = "Vec<" + recvTI->keyType->name + ">";
                        return typeRegistry_.lookup(fn);
                    }
                    if (m.returnType == "_vec_val" && recvTI->valueType) {
                        auto fn = "Vec<" + recvTI->valueType->name + ">";
                        return typeRegistry_.lookup(fn);
                    }
                    return typeRegistry_.lookup(m.returnType);
                }
            }
        }

        // User-defined struct/union instance method lookup (walks parent chain)
        if (recvTI->kind == TypeKind::Struct || recvTI->kind == TypeKind::Union) {
            auto* me = findMethodEntryInChain(recvTI, methodName);
            if (me)
                return me->returnType;
            // Function pointer field call
            for (auto& f : recvTI->fields) {
                if (f.name == methodName && f.typeInfo) {
                    auto* fnTI = f.typeInfo;
                    if (fnTI->kind == TypeKind::Pointer && fnTI->pointeeType &&
                        fnTI->pointeeType->kind == TypeKind::Function)
                        fnTI = fnTI->pointeeType;
                    if (fnTI->kind == TypeKind::Function)
                        return fnTI->returnType;
                }
            }
        }

        return nullptr;
    }

    // ── Arrow method call: ptr->method() ────────────────────────────
    if (auto* amc = dynamic_cast<LucisParser::ArrowMethodCallExprContext*>(ctx)) {
        auto* baseExpr = amc->expression();
        std::string methodName = amc->IDENTIFIER()->getText();

        auto* recvTI = resolveExprTypeInfo(baseExpr);
        if (!recvTI) return nullptr;

        // Dereference pointer to get pointee type
        if (recvTI->kind == TypeKind::Pointer && recvTI->pointeeType)
            recvTI = recvTI->pointeeType;

        // Lookup in struct methods
        if (recvTI->kind == TypeKind::Struct || recvTI->kind == TypeKind::Union) {
            auto* me = findMethodEntryInChain(recvTI, methodName);
            if (me)
                return me->returnType;
        }

        return nullptr;
    }

    // ── Index: base[i] → indexed element type ───────────────────────
    if (auto* idx = dynamic_cast<LucisParser::IndexExprContext*>(ctx)) {
        unsigned depth = 0;
        auto* current = static_cast<LucisParser::ExpressionContext*>(idx);
        while (auto* idxCtx = dynamic_cast<LucisParser::IndexExprContext*>(current)) {
            depth++;
            current = idxCtx->expression(0);
        }

        const TypeInfo* result = resolveExprTypeInfo(current);
        if (!result) return nullptr;

        // First consume array dimensions from the base expression itself.
        // Example: []string[i] -> string, []string[i][j] -> char.
        unsigned arrayDepth = resolveExprArrayDims(current);
        if (arrayDepth > 0) {
            if (depth <= arrayDepth)
                return result;
            depth -= arrayDepth;
        }

        for (unsigned i = 0; i < depth && result; i++) {
            // Map<K,V>[key] -> V
            if (result->kind == TypeKind::Extended && result->keyType && result->valueType) {
                result = result->valueType;
                continue;
            }
            // Vec<T>[i] / Set<T>[i]-like indexed containers -> T
            if (result->kind == TypeKind::Extended && result->elementType) {
                result = result->elementType;
                continue;
            }
            // *T[i] -> T
            if (result->kind == TypeKind::Pointer && result->pointeeType) {
                result = result->pointeeType;
                continue;
            }
            // string[i] -> char
            if (result->kind == TypeKind::String) {
                result = typeRegistry_.lookup("char");
                continue;
            }

            // Non-indexable or already elemental: stop preserving the best known type.
            break;
        }

        return result;
    }

    // ── Pre/Post increment/decrement: same type ─────────────────────
    if (auto* pi = dynamic_cast<LucisParser::PreIncrExprContext*>(ctx))
        return resolveExprTypeInfo(pi->expression());
    if (auto* pd = dynamic_cast<LucisParser::PreDecrExprContext*>(ctx))
        return resolveExprTypeInfo(pd->expression());
    if (auto* pi = dynamic_cast<LucisParser::PostIncrExprContext*>(ctx))
        return resolveExprTypeInfo(pi->expression());
    if (auto* pd = dynamic_cast<LucisParser::PostDecrExprContext*>(ctx))
        return resolveExprTypeInfo(pd->expression());

    // ── Sizeof → int64 ──────────────────────────────────────────────
    if (dynamic_cast<LucisParser::SizeofExprContext*>(ctx))
        return typeRegistry_.lookup("int64");

    // ── Typeof → string ─────────────────────────────────────────────
    if (dynamic_cast<LucisParser::TypeofExprContext*>(ctx))
        return typeRegistry_.lookup("string");


    // ── Alignof → int64 ─────────────────────────────────────────────
    if (dynamic_cast<LucisParser::AlignofExprContext*>(ctx))
        return typeRegistry_.lookup("int64");

    // ── Offsetof → int64 ────────────────────────────────────────────
    if (dynamic_cast<LucisParser::OffsetofExprContext*>(ctx))
        return typeRegistry_.lookup("int64");

    // ── Function call: lookup return type ───────────────────────────
    if (auto* call = dynamic_cast<LucisParser::FnCallExprContext*>(ctx)) {
        if (auto* ident = dynamic_cast<LucisParser::IdentExprContext*>(call->expression())) {
            auto emitName = resolveCallTarget(ident->IDENTIFIER()->getText());
            auto it = fnReturnTypes_.find(emitName);
            if (it != fnReturnTypes_.end()) {
                exprTypeCache_[ctx] = it->second;
                return it->second;
            }
        }
    }

    return nullptr;
}



unsigned IRGen::countArrayDims(LucisParser::TypeSpecContext* ctx) {
    if (!ctx) return 0;
    auto* originalCtx = ctx;

    // Preserve array depth under pointer wrappers, e.g. *[5]int32 -> 1.
    if (ctx->STAR() && !ctx->typeSpec().empty())
        return countArrayDims(ctx->typeSpec(0));

    unsigned dims = 0;
    while (!ctx->typeSpec().empty()) {
        // Skip volatile qualifier — it's not an array dimension
        if (ctx->VOLATILE()) {
            ctx = ctx->typeSpec(0);
            continue;
        }
        // Only count [] array dimensions, not other compound types
        if (ctx->TUPLE() || ctx->VEC() || ctx->MAP() || ctx->SET() ||
            ctx->LT() || ctx->fnTypeSpec())
            break;
        dims++;
        ctx = ctx->typeSpec(0);
    }

    // Fallback: some grammar shapes may not expose array nesting through
    // typeSpec() recursion for inferred/fixed arrays (e.g. []T, [N]T).
    if (dims == 0) {
        auto text = originalCtx ? originalCtx->getText() : std::string();
        size_t pos = 0;
        while (pos < text.size() && text[pos] == '[') {
            auto close = text.find(']', pos + 1);
            if (close == std::string::npos) break;
            dims++;
            pos = close + 1;
        }
    }

    return dims;
}



unsigned IRGen::resolveExprArrayDims(LucisParser::ExpressionContext* ctx) {
    if (!ctx) return 0;

    if (auto* id = dynamic_cast<LucisParser::IdentExprContext*>(ctx)) {
        auto it = locals_.find(id->IDENTIFIER()->getText());
        if (it != locals_.end()) return it->second.arrayDims;
        return 0;
    }

    if (auto* idx = dynamic_cast<LucisParser::IndexExprContext*>(ctx)) {
        auto baseDims = resolveExprArrayDims(idx->expression(0));
        return baseDims > 0 ? baseDims - 1 : 0;
    }

    if (auto* mc = dynamic_cast<LucisParser::MethodCallExprContext*>(ctx)) {
        auto baseDims = resolveExprArrayDims(mc->expression());
        if (baseDims == 0) return 0;

        auto* md = methodRegistry_.lookupArrayMethod(mc->IDENTIFIER()->getText());
        if (!md) return 0;

        if (md->returnType == "_self") return baseDims;
        if (md->returnType == "_elem") return baseDims > 0 ? baseDims - 1 : 0;
        return 0;
    }

    if (auto* arr = dynamic_cast<LucisParser::ArrayLitExprContext*>(ctx)) {
        auto elems = arr->expression();
        if (elems.empty()) return 1;
        return 1 + resolveExprArrayDims(elems[0]);
    }

    if (auto* paren = dynamic_cast<LucisParser::ParenExprContext*>(ctx))
        return resolveExprArrayDims(paren->expression());

    if (auto* deref = dynamic_cast<LucisParser::DerefExprContext*>(ctx))
        return resolveExprArrayDims(deref->expression());

    if (auto* cast = dynamic_cast<LucisParser::CastExprContext*>(ctx))
        return countArrayDims(cast->typeSpec());

    if (auto* call = dynamic_cast<LucisParser::FnCallExprContext*>(ctx)) {
        if (auto* ident = dynamic_cast<LucisParser::IdentExprContext*>(call->expression())) {
            auto emitName = resolveCallTarget(ident->IDENTIFIER()->getText());
            auto it = fnReturnArrayDims_.find(emitName);
            if (it != fnReturnArrayDims_.end()) return it->second;
        }
    }

    if (auto* arrow = dynamic_cast<LucisParser::ArrowAccessExprContext*>(ctx)) {
        auto* baseTI = resolveExprTypeInfo(arrow->expression());
        if (baseTI && baseTI->kind == TypeKind::Pointer && baseTI->pointeeType) {
            auto* structTI = baseTI->pointeeType;
            auto fieldName = arrow->IDENTIFIER()->getText();
            for (size_t f = 0; f < structTI->fields.size(); f++) {
                if (structTI->fields[f].name == fieldName)
                    return structTI->fields[f].arrayDims;
            }
        }
        return 0;
    }

    if (auto* field = dynamic_cast<LucisParser::FieldAccessExprContext*>(ctx)) {
        auto* baseTI = resolveExprTypeInfo(field->expression());
        if (baseTI && (baseTI->kind == TypeKind::Struct ||
                       baseTI->kind == TypeKind::Union)) {
            auto fieldName = field->IDENTIFIER()->getText();
            for (size_t f = 0; f < baseTI->fields.size(); f++) {
                if (baseTI->fields[f].name == fieldName)
                    return baseTI->fields[f].arrayDims;
            }
        }
        return 0;
    }

    if (auto* cu = dynamic_cast<LucisParser::CatchUnwrapExprContext*>(ctx)) {
        auto* sourceTI = resolveExprTypeInfo(cu->expression());
        if (sourceTI && sourceTI->kind == TypeKind::Fallible) return 0;
        UnwrapCatchPatternInfo pattern;
        std::string reason;
        if (!classifyUnwrapCatchEnum(sourceTI, pattern, reason)) return 0;
        if (!pattern.okVariant || pattern.okVariant->payloadFields.empty()) return 0;
        return pattern.okVariant->payloadFields[0].arrayDims;
    }

    return 0;
}


