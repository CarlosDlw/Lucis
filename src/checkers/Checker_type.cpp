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



struct UnwrapCatchPatternInfo {
    const EnumVariantInfo* okVariant = nullptr;
    const EnumVariantInfo* errVariant = nullptr;
};



static const TypeInfo* singlePayloadType(const EnumVariantInfo& variant) {
    if (variant.payloadFields.size() != 1) return nullptr;
    return variant.payloadFields[0].typeInfo;
}

// Check if type is or inherits from a type with the given name


static bool isOrInheritsFrom(const TypeInfo* ti, const std::string& name) {
    for (auto* t = ti; t; t = t->parentType)
        if (t->name == name) return true;
    return false;
}



static bool classifyUnwrapCatchEnum(const TypeInfo* enumType,
                                    UnwrapCatchPatternInfo& out,
                                    std::string& reason) {
    if (!enumType || enumType->kind != TypeKind::Enum) {
        reason = "unwrap-catch requires an enum expression";
        return false;
    }

    if (enumType->enumVariantInfos.size() != 2) {
        reason = "unwrap-catch requires enum '" + enumType->name +
                 "' to have exactly 2 variants (success + error)";
        return false;
    }

    for (const auto& variant : enumType->enumVariantInfos) {
        // Error variant: 0 payload fields (e.g. Option<T>::Nothing),
        // or #[error] attribute, or payload type is Error.
        bool looksLikeErr = variant.payloadFields.empty()
            || variant.isError
            || variant.name == "Err"
            || (variant.payloadFields.size() == 1
                && variant.payloadFields[0].typeInfo
                && isOrInheritsFrom(variant.payloadFields[0].typeInfo, "Error"));



        if (looksLikeErr) {
            if (out.errVariant) {
                reason = "unwrap-catch requires exactly one error variant in enum '" +
                         enumType->name + "'";
                return false;
            }
            out.errVariant = &variant;
        } else {
            // Success variant must have exactly one payload field
            if (variant.payloadFields.size() != 1) {
                reason = "unwrap-catch requires success variant in enum '" +
                         enumType->name + "' to have exactly one payload field";
                return false;
            }
            if (out.okVariant) {
                reason = "unwrap-catch requires exactly one success variant in enum '" +
                         enumType->name + "'";
                return false;
            }
            out.okVariant = &variant;
        }
    }

    if (!out.errVariant || !out.okVariant) {
        reason = "unwrap-catch requires one error variant and one success variant in enum '" +
                 enumType->name + "'";
        return false;
    }

    return true;
}



const TypeInfo* Checker::resolveTypeSpec(LucisParser::TypeSpecContext* ctx,
                                         unsigned& arrayDims) {
    arrayDims = 0;
    auto* cur = ctx;

    // Unwrap array dimensions: [][]T → arrayDims=2, [N]T → arrayDims=1 (fixed)
    while (cur && cur->LBRACKET()) {
        arrayDims++;
        if (cur->INT_LIT()) {
            // Fixed-size array [N]T — validate size is positive
            int64_t size = std::stoll(cur->INT_LIT()->getText());
            if (size <= 0) {
                error(cur, "array size must be a positive integer, got " +
                      cur->INT_LIT()->getText());
                return nullptr;
            }
        } else if (cur->IDENTIFIER()) {
            // Fixed-size array [IDENTIFIER]T — must be a const with compile-time value
            auto identName = cur->IDENTIFIER()->getText();
            
            // Check if it's a const variable with known value
            auto lit = locals_.find(identName);
            if (lit != locals_.end() && lit->second.isConst) {
                auto cit = compileTimeValues_.find(identName);
                if (cit != compileTimeValues_.end()) {
                    if (cit->second.kind() != ComptimeValue::Kind::Int) {
                        error(cur, "const '" + identName + "' must be an integer to be used as array size");
                        return nullptr;
                    }
                    int64_t size = cit->second.asInt();
                    if (size <= 0) {
                        error(cur, "array size must be a positive integer, got '" +
                              identName + "' = " + std::to_string(size));
                        return nullptr;
                    }
                } else {
                    error(cur, "const '" + identName + "' must have a literal initializer to be used as array size");
                    return nullptr;
                }
            } else if (userImports_.count(identName) && moduleRegistry_) {
                // Imported const used as an array dimension: evaluate the
                // defining module's constant declaration.
                auto exported = importedSymbolName(identName);
                auto* sym = moduleRegistry_->findSymbol(
                    userImports_[identName], exported);
                if (!sym || sym->kind != ExportedSymbol::Constant || !sym->decl) {
                    error(cur, "array dimension must be a compile-time constant; '" +
                          identName + "' is not an imported const");
                    return nullptr;
                }
                auto* cd = static_cast<LucisParser::ConstDeclaratorContext*>(sym->decl);
                auto ev = evaluateConstExpr(cd->expression());
                if (!ev || ev->kind() != ComptimeValue::Kind::Int) {
                    error(cur, "imported const '" + identName + "' must have a literal integer initializer");
                    return nullptr;
                }
                int64_t size = ev->asInt();
                if (size <= 0) {
                    error(cur, "array size must be a positive integer, got '" +
                          identName + "' = " + std::to_string(size));
                    return nullptr;
                }
            } else {
                error(cur, "array dimension must be a compile-time constant (literal or const); '" +
                      identName + "' is not a const");
                return nullptr;
            }
        }
        cur = cur->typeSpec().empty() ? nullptr : cur->typeSpec(0);
    }

    // Volatile qualifier: volatile T
    if (cur && cur->VOLATILE()) {
        unsigned innerDims = 0;
        auto* inner = resolveTypeSpec(cur->typeSpec(0), innerDims);
        if (!inner) return nullptr;
        arrayDims = innerDims;
        // Return or create volatile-qualified version
        auto volName = "volatile " + inner->name;
        for (auto& dt : dynamicTypes_) {
            if (dt->name == volName && dt->isVolatile &&
                dt->kind == inner->kind &&
                dt->bitWidth == inner->bitWidth &&
                dt->isSigned == inner->isSigned)
                return dt.get();
        }
        auto ti = std::make_unique<TypeInfo>(*inner);
        ti->name = volName;
        ti->isVolatile = true;
        const TypeInfo* raw = ti.get();
        dynamicTypes_.push_back(std::move(ti));
        return raw;
    }

    // Pointer type: *T
    if (cur && cur->STAR()) {
        auto* childTS = cur->typeSpec(0);
        // auto should not be combined with * in any position:
        //   *auto  → use just 'auto' (auto already infers pointer types)
        //   auto*  → use just 'auto'
        if (childTS && childTS->AUTO()) {
            error(cur, "invalid use of '*' with 'auto': use 'auto' instead (auto infers pointer types automatically)");
            return nullptr;
        }
        if (childTS) {
            auto starIdx = cur->STAR()->getSymbol()->getTokenIndex();
            auto childIdx = childTS->getStart()->getTokenIndex();
            if (starIdx > childIdx) {
                // Postfix star: type* — wrong C-style syntax
                error(cur, "invalid pointer syntax: use '*" +
                      childTS->getText() + "' instead of '" +
                      childTS->getText() + "*'");
                return nullptr;
            }
        }
        unsigned innerDims = 0;
        auto* inner = resolveTypeSpec(cur->typeSpec(0), innerDims);
        if (!inner) return nullptr;
        // Keep inner array depth for pointer-to-array types like *[N]T.
        arrayDims = innerDims;
        return getPointerType(inner);
    }

    // Fallible type: !T or !T:E
    if (cur && cur->NOT()) {
        unsigned innerDims = 0;
        auto* inner = resolveTypeSpec(cur->typeSpec(0), innerDims);
        if (!inner) return nullptr;

        const TypeInfo* errType = nullptr;
        if (cur->typeSpec().size() > 1)
            errType = resolveTypeSpec(cur->typeSpec(1), innerDims);
        else
            errType = typeRegistry_.lookup("Error");

        if (!errType) {
            error(cur, "cannot resolve error type for !T");
            return nullptr;
        }

        // Error type must be 'Error' or a struct inheriting from 'Error'
        if (errType->name != "Error") {
            bool isErrorDerived = false;
            for (auto* p = errType; p; p = p->parentType) {
                if (p->name == "Error") { isErrorDerived = true; break; }
            }
            if (!isErrorDerived) {
                error(cur, "error type in !T must be 'Error' or inherit from 'Error', got '" +
                           errType->name + "'");
                return nullptr;
            }
        }

        auto ti = std::make_unique<TypeInfo>();
        ti->kind = TypeKind::Fallible;
        ti->name = "!" + inner->name;
        if (errType->name != "Error")
            ti->name += ":" + errType->name;
        ti->successType = inner;
        ti->errorType = errType;
        ti->bitWidth = 0;
        ti->isSigned = false;
        auto* raw = ti.get();
        dynamicTypes_.push_back(std::move(ti));
        return raw;
    }

    // Function type: fn(params) -> ret
    if (cur && cur->fnTypeSpec()) {
        auto* fnSpec = cur->fnTypeSpec();
        auto specs = fnSpec->typeSpec();
        if (specs.empty()) return nullptr;

        unsigned retDims = 0;
        auto* retType = resolveTypeSpec(specs.back(), retDims);
        if (!retType) return nullptr;

        std::vector<const TypeInfo*> paramTypes;
        for (size_t i = 0; i + 1 < specs.size(); i++) {
            unsigned pDims = 0;
            auto* pType = resolveTypeSpec(specs[i], pDims);
            if (!pType) return nullptr;
            paramTypes.push_back(pType);
        }

        return makeFunctionType(retType, paramTypes);
    }

    // Generic extended type: Vec<int32>, Map<string, int32>, etc.
    // Built-in collection types (vec, map, set) — no import required
    if (!cur) {
        error(ctx, "invalid type specification");
        return nullptr;
    }
    std::string baseName;
    if (cur->VEC())      baseName = "Vec";
    else if (cur->MAP()) baseName = "Map";
    else if (cur->SET()) baseName = "Set";

    // tuple<T1, T2, ...> type
    if (cur->TUPLE()) {
        auto typeParams = cur->typeSpec();
        if (typeParams.size() < 2) {
            error(cur, "tuple requires at least 2 type parameters");
            return nullptr;
        }
        std::vector<const TypeInfo*> elemTypes;
        std::string fullName = "tuple<";
        for (size_t i = 0; i < typeParams.size(); i++) {
            unsigned elemDims = 0;
            auto* elemType = resolveTypeSpec(typeParams[i], elemDims);
            if (!elemType) return nullptr;
            elemTypes.push_back(elemType);
            if (i > 0) fullName += ", ";
            fullName += elemType->name;
        }
        fullName += ">";

        // Check for cached tuple type
        for (auto& dt : dynamicTypes_) {
            if (dt->name == fullName)
                return dt.get();
        }
        auto ti = std::make_unique<TypeInfo>();
        ti->name = fullName;
        ti->kind = TypeKind::Tuple;
        ti->bitWidth = 0;
        ti->isSigned = false;
        ti->tupleElements = std::move(elemTypes);
        const TypeInfo* raw = ti.get();
        dynamicTypes_.push_back(std::move(ti));
        return raw;
    }

    if (!baseName.empty()) {
        auto* extDesc = extTypeRegistry_.lookup(baseName);
        if (!extDesc) {
            error(cur, "'" + baseName + "' is not a known generic type");
            return nullptr;
        }
        auto typeParams = cur->typeSpec();
        if (typeParams.empty()) {
            error(cur, "generic type '" + baseName + "' requires type parameters");
            return nullptr;
        }

        if (extDesc->genericArity == 1) {
            if (typeParams.size() != 1) {
                error(cur, "'" + baseName + "' expects 1 type parameter, got " +
                           std::to_string(typeParams.size()));
                return nullptr;
            }
            unsigned elemDims = 0;
            auto* elemType = resolveTypeSpec(typeParams[0], elemDims);
            if (!elemType) return nullptr;

            if (elemType->kind == TypeKind::Extended) {
                error(cur, "nested collection types are not supported: '" +
                           baseName + "<" + elemType->name + ">' — " +
                           "'" + baseName + "' cannot contain '" +
                           elemType->extendedKind + "' as element type");
                return nullptr;
            }

            auto fullName = baseName + "<" + elemType->name + ">";
            for (auto& dt : dynamicTypes_) {
                if (dt->name == fullName)
                    return dt.get();
            }
            auto ti = std::make_unique<TypeInfo>();
            ti->name = fullName;
            ti->kind = TypeKind::Extended;
            ti->bitWidth = 0;
            ti->isSigned = false;
            ti->builtinSuffix = elemType->builtinSuffix;
            ti->elementType = elemType;
            ti->extendedKind = baseName;
            const TypeInfo* raw = ti.get();
            dynamicTypes_.push_back(std::move(ti));
            return raw;
        } else if (extDesc->genericArity == 2) {
            if (typeParams.size() != 2) {
                error(cur, "'" + baseName + "' expects 2 type parameters, got " +
                           std::to_string(typeParams.size()));
                return nullptr;
            }
            unsigned keyDims = 0, valDims = 0;
            auto* keyType = resolveTypeSpec(typeParams[0], keyDims);
            auto* valType = resolveTypeSpec(typeParams[1], valDims);
            if (!keyType || !valType) return nullptr;

            if (keyType->kind == TypeKind::Extended) {
                error(cur, "nested collection types are not supported: '" +
                           baseName + "' cannot use '" + keyType->extendedKind +
                           "' as key type");
                return nullptr;
            }
            if (valType->kind == TypeKind::Extended) {
                error(cur, "nested collection types are not supported: '" +
                           baseName + "' cannot use '" + valType->extendedKind +
                           "' as value type");
                return nullptr;
            }

            auto fullName = baseName + "<" + keyType->name + ", " + valType->name + ">";
            for (auto& dt : dynamicTypes_) {
                if (dt->name == fullName)
                    return dt.get();
            }
            auto ti = std::make_unique<TypeInfo>();
            ti->name = fullName;
            ti->kind = TypeKind::Extended;
            ti->bitWidth = 0;
            ti->isSigned = false;
            ti->builtinSuffix = keyType->builtinSuffix + "_" + valType->builtinSuffix;
            ti->keyType = keyType;
            ti->valueType = valType;
            ti->extendedKind = baseName;
            const TypeInfo* raw = ti.get();
            dynamicTypes_.push_back(std::move(ti));
            return raw;
        } else {
            error(cur, "unsupported generic arity for '" + baseName + "'");
            return nullptr;
        }
    }

    // User-defined and extended generic types (e.g., Task<int32>, Node<int32>)
    if (cur->LT()) {
        auto baseName = cur->IDENTIFIER()->getText();
        auto typeParams = cur->typeSpec();

        if (typeParams.empty()) {
            error(cur, "generic type '" + baseName + "' requires type parameters");
            return nullptr;
        }

        // Resolve all type arguments first
        std::vector<const TypeInfo*> resolvedArgs;
        for (auto* tp : typeParams) {
            unsigned argDims = 0;
            auto* argTI = resolveTypeSpec(tp, argDims);
            if (!argTI) return nullptr;
            resolvedArgs.push_back(argTI);
        }

        // Check user-defined generic struct templates first
        // (an imported template under a local alias is keyed by its
        // exported name).
        auto genericBase = userImports_.count(baseName)
            ? importedSymbolName(baseName) : baseName;
        auto structIt = genericStructTemplates_.find(genericBase);
        if (structIt != genericStructTemplates_.end()) {
            return instantiateGenericStruct(genericBase, structIt->second, resolvedArgs, cur);
        }
        auto unionIt = genericUnionTemplates_.find(genericBase);
        if (unionIt != genericUnionTemplates_.end()) {
            return instantiateGenericUnion(genericBase, unionIt->second, resolvedArgs, cur);
        }
        auto enumIt = genericEnumTemplates_.find(genericBase);
        if (enumIt != genericEnumTemplates_.end()) {
            return instantiateGenericEnum(genericBase, enumIt->second, resolvedArgs, cur);
        }
        auto traitIt = genericTraitTemplates_.find(genericBase);
        if (traitIt != genericTraitTemplates_.end()) {
            // Generic trait reference — return a trait sentinel TypeInfo
            static TypeInfo traitTypeInfo;
            traitTypeInfo.name = genericBase;
            traitTypeInfo.kind = TypeKind::Trait;
            return &traitTypeInfo;
        }

        // Fall through to known extended types (Task, etc.)
        auto* extDesc = extTypeRegistry_.lookup(baseName);
        if (!extDesc) {
            error(cur, "'" + baseName + "' is not a known generic type");
            return nullptr;
        }
        if (!imports_.isImported(baseName)) {
            error(cur, "type '" + baseName + "' is not imported");
            return nullptr;
        }

        if (extDesc->genericArity == 1) {
            if (resolvedArgs.size() != 1) {
                error(cur, "'" + baseName + "' expects 1 type parameter, got " +
                           std::to_string(resolvedArgs.size()));
                return nullptr;
            }
            auto* elemType = resolvedArgs[0];

            if (elemType->kind == TypeKind::Extended) {
                error(cur, "nested collection types are not supported: '" +
                           baseName + "<" + elemType->name + ">' — " +
                           "'" + baseName + "' cannot contain '" +
                           elemType->extendedKind + "' as element type");
                return nullptr;
            }

            auto fullName = baseName + "<" + elemType->name + ">";
            for (auto& dt : dynamicTypes_) {
                if (dt->name == fullName)
                    return dt.get();
            }
            auto ti = std::make_unique<TypeInfo>();
            ti->name = fullName;
            ti->kind = TypeKind::Extended;
            ti->bitWidth = 0;
            ti->isSigned = false;
            ti->builtinSuffix = elemType->builtinSuffix;
            ti->elementType = elemType;
            ti->extendedKind = baseName;
            const TypeInfo* raw = ti.get();
            dynamicTypes_.push_back(std::move(ti));
            return raw;
        } else if (extDesc->genericArity == 2) {
            if (resolvedArgs.size() != 2) {
                error(cur, "'" + baseName + "' expects 2 type parameters, got " +
                           std::to_string(resolvedArgs.size()));
                return nullptr;
            }
            auto* keyType = resolvedArgs[0];
            auto* valType = resolvedArgs[1];

            if (keyType->kind == TypeKind::Extended) {
                error(cur, "nested collection types are not supported: '" +
                           baseName + "' cannot use '" + keyType->extendedKind +
                           "' as key type");
                return nullptr;
            }
            if (valType->kind == TypeKind::Extended) {
                error(cur, "nested collection types are not supported: '" +
                           baseName + "' cannot use '" + valType->extendedKind +
                           "' as value type");
                return nullptr;
            }

            auto fullName = baseName + "<" + keyType->name + ", " + valType->name + ">";
            for (auto& dt : dynamicTypes_) {
                if (dt->name == fullName)
                    return dt.get();
            }
            auto ti = std::make_unique<TypeInfo>();
            ti->name = fullName;
            ti->kind = TypeKind::Extended;
            ti->bitWidth = 0;
            ti->isSigned = false;
            ti->builtinSuffix = keyType->builtinSuffix + "_" + valType->builtinSuffix;
            ti->keyType = keyType;
            ti->valueType = valType;
            ti->extendedKind = baseName;
            const TypeInfo* raw = ti.get();
            dynamicTypes_.push_back(std::move(ti));
            return raw;
        } else {
            error(cur, "unsupported generic arity for '" + baseName + "'");
            return nullptr;
        }
    }

    // Auto type inference: auto x = 42;
    if (cur->AUTO()) return nullptr;  // nullptr signals "infer from initializer"

    // Primitive or named type
    auto name = cur->getText();

    // Self resolves to the enclosing extend block's struct type
    if (cur->SELF()) {
        for (auto* p = cur->parent; p; p = p->parent) {
            if (auto* ext = dynamic_cast<LucisParser::ExtendDeclContext*>(p)) {
                auto* ti = typeRegistry_.lookup(ext->IDENTIFIER()->getText());
                if (ti) return ti;
                break;
            }
        }
        error(cur, "'Self' used outside of an 'extend' block");
        return nullptr;
    }

    // cstring is a built-in alias for *char
    if (name == "cstring")
        return getPointerType(typeRegistry_.lookup("char"));

    auto* ti = typeRegistry_.lookup(name);
    if (!ti && traitRegistry_.lookup(name)) {
        // Not a concrete type, but a trait — return a trait sentinel
        static TypeInfo traitTypeInfo;
        traitTypeInfo.name = name;
        traitTypeInfo.kind = TypeKind::Trait;
        ti = &traitTypeInfo;
    }
    if (!ti && userImports_.count(name)) {
        // Imported type under a local alias: resolve the exported name and
        // check the registry again (the imported module's full check
        // registers the type under its exported name).
        auto exported = importedSymbolName(name);
        if (exported != name) {
            ti = typeRegistry_.lookup(exported);
            if (!ti && traitRegistry_.lookup(exported)) {
                static TypeInfo traitTypeInfo;
                traitTypeInfo.name = exported;
                traitTypeInfo.kind = TypeKind::Trait;
                ti = &traitTypeInfo;
            }
        }
    }
    if (!ti) {
        if (genericStructTemplates_.count(name) ||
            genericUnionTemplates_.count(name) ||
            genericEnumTemplates_.count(name)) {
            error(cur, "generic type '" + name + "' requires type parameters");
        } else {
            error(cur, "unknown type '" + name + "'");
        }
    }
    return ti;
}



const TypeInfo* Checker::getPointerType(const TypeInfo* pointee) {
    for (auto& dt : dynamicTypes_) {
        if (dt->kind == TypeKind::Pointer && dt->pointeeType == pointee)
            return dt.get();
    }
    auto ti = std::make_unique<TypeInfo>();
    ti->name = "*" + pointee->name;
    ti->kind = TypeKind::Pointer;
    ti->bitWidth = 0;
    ti->isSigned = false;
    // For *char, use "cstr" suffix to support C string functions in std::log
    if (pointee->name == "char") {
        ti->builtinSuffix = "cstr";
    } else {
        ti->builtinSuffix = "ptr";
    }
    ti->pointeeType = pointee;
    const TypeInfo* raw = ti.get();
    dynamicTypes_.push_back(std::move(ti));
    return raw;
}



const TypeInfo* Checker::makeFunctionType(const TypeInfo* returnType,
                                           const std::vector<const TypeInfo*>& paramTypes,
                                           bool isVariadic,
                                           const TypeInfo* variadicElementType,
                                           const std::vector<unsigned>* paramDims) {
    auto ti = std::make_unique<TypeInfo>();
    ti->kind = TypeKind::Function;
    ti->bitWidth = 0;
    ti->isSigned = false;
    ti->builtinSuffix = "ptr";
    ti->returnType = returnType;
    ti->paramTypes = paramTypes;
    if (paramDims)
        ti->paramDims = *paramDims;
    ti->isVariadic = isVariadic;
    ti->variadicElementType = variadicElementType;

    ti->name = "fn(";
    for (size_t i = 0; i < paramTypes.size(); i++) {
        if (i > 0) ti->name += ",";
        if (isVariadic && i == paramTypes.size() - 1)
            ti->name += "...";
        ti->name += paramTypes[i]->name;
    }
    ti->name += ")->" + returnType->name;

    const TypeInfo* raw = ti.get();
    dynamicTypes_.push_back(std::move(ti));
    return raw;
}



std::string Checker::resolveBaseTypeName(LucisParser::TypeSpecContext* ctx) {
    if (!ctx) return "";
    while (!ctx->typeSpec().empty())
        ctx = ctx->typeSpec(0);
    if (!ctx) return "";
    auto text = ctx->getText();
    while (!text.empty() && text[0] == '*')
        text = text.substr(1);
    return text;
}



const TypeInfo* Checker::resolveBuiltinReturnType(const std::string& retName) {
    // Handle "Vec<T>" return types from builtins
    if (retName.size() > 4 && retName.substr(0, 4) == "Vec<" && retName.back() == '>') {
        std::string elemName = retName.substr(4, retName.size() - 5);
        auto* elemType = typeRegistry_.lookup(elemName);
        if (!elemType) return nullptr;

        auto fullName = retName;
        for (auto& dt : dynamicTypes_) {
            if (dt->name == fullName)
                return dt.get();
        }
        auto ti = std::make_unique<TypeInfo>();
        ti->name = fullName;
        ti->kind = TypeKind::Extended;
        ti->bitWidth = 0;
        ti->isSigned = false;
        ti->builtinSuffix = elemType->builtinSuffix;
        ti->elementType = elemType;
        ti->extendedKind = "Vec";
        const TypeInfo* raw = ti.get();
        dynamicTypes_.push_back(std::move(ti));
        return raw;
    }
    return typeRegistry_.lookup(retName);
}

// ═══════════════════════════════════════════════════════════════════════
//  Type query helpers
// ═══════════════════════════════════════════════════════════════════════



bool Checker::isNumeric(const TypeInfo* ti) {
    return ti && (ti->kind == TypeKind::Integer || ti->kind == TypeKind::Float || ti->kind == TypeKind::Char);
}

bool Checker::isInteger(const TypeInfo* ti) {
    return ti && (ti->kind == TypeKind::Integer || ti->kind == TypeKind::Char);
}

bool Checker::isIntegerOrPointer(const TypeInfo* ti) {
    return ti && (ti->kind == TypeKind::Integer || ti->kind == TypeKind::Char || ti->kind == TypeKind::Pointer);
}

bool Checker::isConditionType(const TypeInfo* ti) {
    if (!ti) return true;  // null → unknown, don't flag
    return ti->kind == TypeKind::Bool ||
           ti->kind == TypeKind::Integer ||
           ti->kind == TypeKind::Float;
}



bool Checker::isAssignable(const TypeInfo* lhs, const TypeInfo* rhs) {
    if (!lhs || !rhs) return true;
    if (lhs == rhs) return true;
    if (lhs->name == rhs->name) return true;

    // Integer ↔ integer (same signedness required)
    if (lhs->kind == TypeKind::Integer && rhs->kind == TypeKind::Integer) {
        if (lhs->isSigned == rhs->isSigned) return true;
        unsigned lhsBW = lhs->bitWidth == 0 ? 64 : lhs->bitWidth;
        unsigned rhsBW = rhs->bitWidth == 0 ? 64 : rhs->bitWidth;
        // Unsigned → wider signed: safe (no sign loss)
        if (!lhs->isSigned && rhs->isSigned && lhsBW <= rhsBW) return true;
        if (!rhs->isSigned && lhs->isSigned && rhsBW <= lhsBW) return true;
    }

    // Enum ↔ integer (C enums are integers)
    if (lhs->kind == TypeKind::Enum && rhs->kind == TypeKind::Integer)
        return true;
    if (lhs->kind == TypeKind::Integer && rhs->kind == TypeKind::Enum)
        return true;
    if (lhs->kind == TypeKind::Enum && rhs->kind == TypeKind::Enum)
        return true;

    // Char ↔ integer (C char promotes to int)
    if ((lhs->kind == TypeKind::Char && rhs->kind == TypeKind::Integer) ||
        (lhs->kind == TypeKind::Integer && rhs->kind == TypeKind::Char))
        return true;

    // Float ↔ float
    if (lhs->kind == TypeKind::Float && rhs->kind == TypeKind::Float)
        return true;

    // Function ↔ function
    if (lhs->kind == TypeKind::Function && rhs->kind == TypeKind::Function)
        return true;

    // Function pointer ↔ function (C callback decay)
    if (lhs->kind == TypeKind::Pointer && lhs->pointeeType &&
        lhs->pointeeType->kind == TypeKind::Function &&
        rhs->kind == TypeKind::Function)
        return true;
    if (rhs->kind == TypeKind::Pointer && rhs->pointeeType &&
        rhs->pointeeType->kind == TypeKind::Function &&
        lhs->kind == TypeKind::Function)
        return true;

    // Same kind
    if (lhs->kind == rhs->kind) {
        // Extended types (Vec, Map, Set) must also match element/key/value types
        if (lhs->kind == TypeKind::Extended) {
            if (lhs->extendedKind != rhs->extendedKind) return false;
            if (lhs->elementType && rhs->elementType &&
                !isAssignable(lhs->elementType, rhs->elementType))
                return false;
            if (lhs->keyType && rhs->keyType &&
                !isAssignable(lhs->keyType, rhs->keyType))
                return false;
            if (lhs->valueType && rhs->valueType &&
                !isAssignable(lhs->valueType, rhs->valueType))
                return false;
        }
        return true;
    }

    // Struct inheritance: Child is-assignable-to Parent
    if (lhs->kind == TypeKind::Struct && rhs->kind == TypeKind::Struct) {
        for (auto* ti = rhs; ti; ti = ti->parentType) {
            if (ti == lhs) return true;
        }
    }

    // Fallible: !T = T (auto-wrap success), !T = Error (auto-wrap error)
    if (lhs->kind == TypeKind::Fallible) {
        if (lhs->successType && isAssignable(lhs->successType, rhs))
            return true;
        if (lhs->errorType && isAssignable(lhs->errorType, rhs))
            return true;
    }

    return false;
}

// Returns true if `from` can be safely widened (promoted) to `to`.
// Only widening conversions: int8→int16→int32→int64, float32→float64, etc.
// Same signedness only — no silent signed↔unsigned conversions.


static bool canWidenTo(const TypeInfo* from, const TypeInfo* to) {
    if (!from || !to) return false;
    if (from == to) return true;
    if (from->name == to->name) return true;

    // Integer → wider integer (same signedness)
    if (from->kind == TypeKind::Integer && to->kind == TypeKind::Integer) {
        if (from->isSigned != to->isSigned) return false;
        unsigned fromBW = from->bitWidth;
        unsigned toBW = to->bitWidth;
        if (fromBW == 0) fromBW = 64; // isize/usize → pointer width
        if (toBW == 0) toBW = 64;
        return fromBW <= toBW;
    }

    // Float → wider float
    if (from->kind == TypeKind::Float && to->kind == TypeKind::Float)
        return from->bitWidth <= to->bitWidth;

    return false;
}



void Checker::checkNegativeToUnsigned(const TypeInfo* target,
                                       LucisParser::ExpressionContext* expr,
                                       antlr4::ParserRuleContext* ctx) {
    if (!target || target->kind != TypeKind::Integer || target->isSigned)
        return;

    // Unwrap try expression
    if (auto* te = dynamic_cast<LucisParser::TryExprContext*>(expr))
        expr = te->expression(0);

    // Case 1: Positive integer literal → always safe for unsigned
    if (dynamic_cast<LucisParser::IntLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::HexLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::OctLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::BinLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::SuffixedIntLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::SuffixedHexLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::SuffixedOctLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::SuffixedBinLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::SuffixedFloatIntExprContext*>(expr))
        return;

    // Case 2: Negative literal (-N) → always invalid for unsigned
    if (dynamic_cast<LucisParser::NegExprContext*>(expr)) {
        error(ctx, "cannot assign negative value to unsigned type '" +
                    target->name + "'");
        return;
    }

    // Case 3: C macro constant — check its known value
    if (auto* ident = dynamic_cast<LucisParser::IdentExprContext*>(expr)) {
        auto id = ident->IDENTIFIER()->getText();
        auto cit = cEnumConstants_.find(id);
        if (cit != cEnumConstants_.end()) {
            if (cit->second.isFloat || cit->second.isString)
                return;  // float/string macros are not relevant here
            if (cit->second.value < 0) {
                error(ctx, "cannot assign negative value " +
                            std::to_string(cit->second.value) +
                            " ('" + id + "') to unsigned type '" +
                            target->name + "'");
            }
            return;  // known non-negative macro → OK
        }
    }

    // Case 4: Explicit cast (expr as uint32) → user took responsibility
    if (dynamic_cast<LucisParser::CastExprContext*>(expr))
        return;
}

// ═══════════════════════════════════════════════════════════════════════
//  Lambda / Closure helpers
// ═══════════════════════════════════════════════════════════════════════



static std::string buildLambdaTypeName(const TypeInfo* retType,
                                        const std::vector<const TypeInfo*>& paramTypes) {
    std::string name = "fn(";
    for (size_t i = 0; i < paramTypes.size(); i++) {
        if (i > 0) name += ", ";
        name += paramTypes[i]->name;
    }
    name += ") -> " + (retType ? retType->name : "void");
    return name;
}



static std::string buildClosureTypeName(const TypeInfo* retType,
                                          const std::vector<const TypeInfo*>& paramTypes,
                                          const std::vector<TypeInfo::CaptureInfo>& captures) {
    std::string name = "closure(";
    for (size_t i = 0; i < paramTypes.size(); i++) {
        if (i > 0) name += ", ";
        name += paramTypes[i]->name;
    }
    name += ") -> " + (retType ? retType->name : "void");
    name += " captures [";
    for (size_t i = 0; i < captures.size(); i++) {
        if (i > 0) name += ", ";
        name += captures[i].name + ": " + (captures[i].type ? captures[i].type->name : "?");
    }
    name += "]";
    return name;
}



static void collectFreeVars(LucisParser::ExpressionContext* expr,
                             const std::unordered_set<std::string>& paramNames,
                             std::unordered_set<std::string>& freeVars) {
    if (!expr) return;
    if (auto* id = dynamic_cast<LucisParser::IdentExprContext*>(expr)) {
        auto name = id->IDENTIFIER()->getText();
        if (paramNames.find(name) == paramNames.end())
            freeVars.insert(name);
        return;
    }
    // Recurse into all child expression nodes
    for (size_t i = 0; i < expr->children.size(); i++) {
        if (auto* child = dynamic_cast<LucisParser::ExpressionContext*>(expr->children[i])) {
            collectFreeVars(child, paramNames, freeVars);
        }
    }
}



static void collectFreeVarsFromBlock(LucisParser::BlockContext* block,
                                      const std::unordered_set<std::string>& paramNames,
                                      std::unordered_set<std::string>& freeVars) {
    if (!block) return;
    for (auto* stmt : block->statement()) {
        // Check all expression contexts within the statement
        for (size_t i = 0; i < stmt->children.size(); i++) {
            if (auto* child = dynamic_cast<LucisParser::ExpressionContext*>(stmt->children[i])) {
                collectFreeVars(child, paramNames, freeVars);
            }
        }
    }
}



const TypeInfo* Checker::resolveLambdaExpr(LucisParser::LambdaExprContext* lexpr) {
    auto id = lambdaCounter_++;
    auto funcName = "__lambda_" + std::to_string(id);

    // Parse params
    std::vector<const TypeInfo*> paramTypes;
    std::unordered_set<std::string> paramNames;
    auto* params = lexpr->paramList();
    if (params) {
        for (auto* p : params->param()) {
            if (p->SPREAD()) continue; // Skip variadic for now
            unsigned dims = 0;
            auto* ti = resolveTypeSpec(p->typeSpec(), dims);
            if (!ti) { error(p, "unknown type in lambda parameter"); continue; }
            auto pname = p->IDENTIFIER()->getText();
            paramTypes.push_back(ti);
            paramNames.insert(pname);
            // Temporarily register as local for body resolution
            locals_[pname] = {ti, dims, {}, true, true, nullptr};
        }
    }

    // Resolve body expression type → return type
    auto* bodyExpr = lexpr->expression();
    auto* retType = resolveExprType(bodyExpr);
    if (!retType) retType = typeRegistry_.lookup("int32");

    // Analyze captures: free vars that are local variables in enclosing scope
    std::unordered_set<std::string> freeVars;
    collectFreeVars(bodyExpr, paramNames, freeVars);

    std::vector<TypeInfo::CaptureInfo> captures;
    for (auto& fv : freeVars) {
        auto it = locals_.find(fv);
        if (it != locals_.end()) {
            // Only capture locals, not globals/functions
            captures.push_back({fv, it->second.type, it->second.arrayDims});
        }
    }

    // Remove temporary params from scope
    for (auto& pn : paramNames)
        locals_.erase(pn);

    // Build the function/closure TypeInfo
    auto ti = std::make_unique<TypeInfo>();
    ti->name = funcName;
    ti->returnType = retType;
    ti->paramTypes = paramTypes;
    ti->syntheticFuncName = funcName;

    // Store lambda info for IRGen
    LambdaInfo linfo;
    linfo.funcName = funcName;
    linfo.returnType = retType;
    linfo.paramTypes = paramTypes;
    linfo.captures = captures;
    linfo.isBlock = false;
    linfo.exprCtx = bodyExpr;
    lambdaInfo_[lexpr] = std::move(linfo);

    if (captures.empty()) {
        // Non-capturing → regular function type
        ti->kind = TypeKind::Function;
        ti->bitWidth = 0;
        ti->isSigned = false;
        ti->builtinSuffix = "ptr";
        ti->name = buildLambdaTypeName(retType, paramTypes);
    } else {
        // Capturing → closure type
        ti->kind = TypeKind::Closure;
        ti->bitWidth = 0;
        ti->isSigned = false;
        ti->builtinSuffix = "closure";
        ti->closureCaptures = std::move(captures);
        ti->name = buildClosureTypeName(retType, paramTypes, ti->closureCaptures);
    }

    const TypeInfo* raw = ti.get();
    dynamicTypes_.push_back(std::move(ti));
    return raw;
}



const TypeInfo* Checker::resolveLambdaBlockExpr(LucisParser::LambdaBlockExprContext* lblk) {
    auto id = lambdaCounter_++;
    auto funcName = "__lambda_" + std::to_string(id);

    // Parse params
    std::vector<const TypeInfo*> paramTypes;
    std::unordered_set<std::string> paramNames;
    auto* params = lblk->paramList();
    if (params) {
        for (auto* p : params->param()) {
            if (p->SPREAD()) continue;
            unsigned dims = 0;
            auto* ti = resolveTypeSpec(p->typeSpec(), dims);
            if (!ti) { error(p, "unknown type in lambda parameter"); continue; }
            auto pname = p->IDENTIFIER()->getText();
            paramTypes.push_back(ti);
            paramNames.insert(pname);
            locals_[pname] = {ti, dims, {}, true, true, nullptr};
        }
    }

    // Walk block to find return type and register locals
    auto* block = lblk->block();
    const TypeInfo* retType = typeRegistry_.lookup("void");
    for (auto* stmt : block->statement()) {
        if (auto* retStmt = stmt->returnStmt()) {
            if (auto* retExpr = retStmt->expression()) {
                auto* exprType = resolveExprType(retExpr);
                if (exprType) retType = exprType;
            }
            break;  // first return stmt determines the type
        }
        if (auto* varDecl = stmt->varDeclStmt()) {
            // Process var decls to keep locals_ populated
            auto* typeSpec = varDecl->typeSpec();
            for (auto* d : varDecl->varDeclarator()) {
                unsigned dims = 0;
                auto* ti = typeSpec ? resolveTypeSpec(typeSpec, dims) : nullptr;
                if (!ti) continue;
                auto vname = d->IDENTIFIER()->getText();
                locals_[vname] = {ti, dims, {}, true, true, nullptr};
                if (auto* initExpr = d->expression()) {
                    resolveExprType(initExpr);
                }
            }
        }
        if (auto* exprStmt = stmt->exprStmt()) {
            resolveExprType(exprStmt->expression());
        }
    }
    if (retType->kind == TypeKind::Void) {
        retType = typeRegistry_.lookup("int32");
    }

    // Analyze captures
    std::unordered_set<std::string> freeVars;
    collectFreeVarsFromBlock(block, paramNames, freeVars);

    std::vector<TypeInfo::CaptureInfo> captures;
    for (auto& fv : freeVars) {
        auto it = locals_.find(fv);
        if (it != locals_.end()) {
            captures.push_back({fv, it->second.type, it->second.arrayDims});
        }
    }

    // Remove temporary params
    for (auto& pn : paramNames)
        locals_.erase(pn);

    auto ti = std::make_unique<TypeInfo>();
    ti->returnType = retType;
    ti->paramTypes = paramTypes;
    ti->syntheticFuncName = funcName;

    LambdaInfo linfo;
    linfo.funcName = funcName;
    linfo.returnType = retType;
    linfo.paramTypes = paramTypes;
    linfo.captures = captures;
    linfo.isBlock = true;
    linfo.blockCtx = block;
    lambdaInfo_[lblk] = std::move(linfo);

    if (captures.empty()) {
        ti->kind = TypeKind::Function;
        ti->bitWidth = 0;
        ti->isSigned = false;
        ti->builtinSuffix = "ptr";
        ti->name = buildLambdaTypeName(retType, paramTypes);
    } else {
        ti->kind = TypeKind::Closure;
        ti->bitWidth = 0;
        ti->isSigned = false;
        ti->builtinSuffix = "closure";
        ti->closureCaptures = std::move(captures);
        ti->name = buildClosureTypeName(retType, paramTypes, ti->closureCaptures);
    }

    const TypeInfo* raw = ti.get();
    dynamicTypes_.push_back(std::move(ti));
    return raw;
}

// ═══════════════════════════════════════════════════════════════════════
//  Expression type resolution
// ═══════════════════════════════════════════════════════════════════════



const TypeInfo* Checker::resolveExprType(LucisParser::ExpressionContext* expr) {
    if (!expr) return nullptr;

    // Guard against infinite recursion in pathological type expressions.
    // RAII: decrements on EVERY exit path (early returns included), so the
    // counter reflects the real nesting depth instead of leaking +1 per call.
    struct DepthGuard {
        unsigned& d;
        explicit DepthGuard(unsigned& depth) : d(depth) { ++d; }
        ~DepthGuard() { --d; }
    } depthGuard_(recursionDepth_);
    if (recursionDepth_ > MAX_RECURSION_DEPTH) {
        error(expr, "type expression nesting exceeds maximum recursion depth of " +
                    std::to_string(MAX_RECURSION_DEPTH));
        return nullptr;  // never fall back to a fake type — it poisons the file
    }

    auto resolveTypeSpecInContext = [&](LucisParser::TypeSpecContext* ts,
                                        unsigned& dims) -> const TypeInfo* {
        if (!activeTypeSubst_.empty())
            return resolveTypeSpecWithSubst(ts, activeTypeSubst_, dims);
        return resolveTypeSpec(ts, dims);
    };

    // ── Helper: true if expr is a plain integer literal (not a variable, call, etc.)

    // ── Helper: check operator overload on struct types ──
    auto resolveOpOverload = [&](const TypeInfo* lhsTI, const TypeInfo* rhsTI,
                                  const std::string& opText) -> const TypeInfo* {
        if (!lhsTI || lhsTI->kind != TypeKind::Struct) return nullptr;
        // Map operator text to internal name
        std::string internal;
        if (opText == "+")       internal = "opAdd";
        else if (opText == "-")  internal = "opSub";
        else if (opText == "*")  internal = "opMul";
        else if (opText == "/")  internal = "opDiv";
        else if (opText == "%")  internal = "opMod";
        else if (opText == "==") internal = "opEq";
        else if (opText == "!=") internal = "opNe";
        else if (opText == "<")  internal = "opLt";
        else if (opText == ">")  internal = "opGt";
        else if (opText == "<=") internal = "opLe";
        else if (opText == ">=") internal = "opGe";
        else if (opText == "&")  internal = "opBitAnd";
        else if (opText == "|")  internal = "opBitOr";
        else if (opText == "^")  internal = "opBitXor";
        else if (opText == "<<") internal = "opShl";
        else if (opText == ">>") internal = "opShr";
        else if (opText == "&&") internal = "opAnd";
        else if (opText == "||") internal = "opOr";
        if (internal.empty()) return nullptr;
        if (auto* op = findOperatorInChain(lhsTI, internal))
            return op->returnType;
        return nullptr;
    };
    auto isIntLitExpr = [](LucisParser::ExpressionContext* e) -> bool {
        return dynamic_cast<LucisParser::IntLitExprContext*>(e) ||
               dynamic_cast<LucisParser::HexLitExprContext*>(e) ||
               dynamic_cast<LucisParser::OctLitExprContext*>(e) ||
               dynamic_cast<LucisParser::BinLitExprContext*>(e) ||
               dynamic_cast<LucisParser::SuffixedIntLitExprContext*>(e) ||
               dynamic_cast<LucisParser::SuffixedHexLitExprContext*>(e) ||
               dynamic_cast<LucisParser::SuffixedOctLitExprContext*>(e) ||
               dynamic_cast<LucisParser::SuffixedBinLitExprContext*>(e);
    };

    // ── Suffixed literals (Rust-style: 42u64, 0xFFi8, 3.14f32) ────────
    static const std::unordered_map<std::string, std::string> kSuffixTypeMap = {
        {"i8", "int8"}, {"i16", "int16"}, {"i32", "int32"}, {"i64", "int64"},
        {"i128", "int128"}, {"iinf", "intinf"}, {"isize", "isize"},
        {"u8", "uint8"}, {"u16", "uint16"}, {"u32", "uint32"}, {"u64", "uint64"},
        {"u128", "uint128"}, {"usize", "usize"},
        {"f32", "float32"}, {"f64", "float64"}, {"f80", "float80"}, {"f128", "float128"}
    };
    auto resolveSuffixed = [&](const std::string& text) -> const TypeInfo* {
        for (auto& [suf, typeName] : kSuffixTypeMap) {
            if (text.size() > suf.size() &&
                text.compare(text.size() - suf.size(), suf.size(), suf) == 0)
                return typeRegistry_.lookup(typeName);
        }
        return nullptr;
    };

    if (auto* si = dynamic_cast<LucisParser::SuffixedIntLitExprContext*>(expr)) {
        if (auto* t = resolveSuffixed(si->SUFFIXED_INT()->getText())) return t;
    }
    if (auto* sh = dynamic_cast<LucisParser::SuffixedHexLitExprContext*>(expr)) {
        if (auto* t = resolveSuffixed(sh->SUFFIXED_HEX()->getText())) return t;
    }
    if (auto* so = dynamic_cast<LucisParser::SuffixedOctLitExprContext*>(expr)) {
        if (auto* t = resolveSuffixed(so->SUFFIXED_OCT()->getText())) return t;
    }
    if (auto* sb = dynamic_cast<LucisParser::SuffixedBinLitExprContext*>(expr)) {
        if (auto* t = resolveSuffixed(sb->SUFFIXED_BIN()->getText())) return t;
    }
    if (auto* sf = dynamic_cast<LucisParser::SuffixedFloatLitExprContext*>(expr)) {
        if (auto* t = resolveSuffixed(sf->SUFFIXED_FLOAT()->getText())) return t;
    }
    if (auto* sd = dynamic_cast<LucisParser::SuffixedLeadingDotFloatExprContext*>(expr)) {
        if (auto* t = resolveSuffixed(sd->SUFFIXED_DOT_FLOAT()->getText())) return t;
    }
    if (auto* si = dynamic_cast<LucisParser::SuffixedIntFloatExprContext*>(expr)) {
        if (auto* t = resolveSuffixed(si->SUFFIXED_INT_FLOAT()->getText())) return t;
    }
    if (auto* sf = dynamic_cast<LucisParser::SuffixedFloatIntExprContext*>(expr)) {
        if (auto* t = resolveSuffixed(sf->SUFFIXED_FLOAT_INT()->getText())) return t;
    }

    // ── Unsuffixed literals ───────────────────────────────────────────
    if (dynamic_cast<LucisParser::IntLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::HexLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::OctLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::BinLitExprContext*>(expr))
        return typeRegistry_.lookup("int32");

    if (dynamic_cast<LucisParser::FloatLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::LeadingDotFloatLitExprContext*>(expr))
        return typeRegistry_.lookup("float64");

    if (dynamic_cast<LucisParser::BoolLitExprContext*>(expr))
        return typeRegistry_.lookup("bool");

    if (dynamic_cast<LucisParser::CharLitExprContext*>(expr))
        return typeRegistry_.lookup("char");

    if (dynamic_cast<LucisParser::StrLitExprContext*>(expr))
        return typeRegistry_.lookup("string");

    // C string literal: c"hello" → *char (null-terminated)
    if (dynamic_cast<LucisParser::CStrLitExprContext*>(expr))
        return getPointerType(typeRegistry_.lookup("char"));

    // Backtick strings — all return `string`
    if (dynamic_cast<LucisParser::BtickExprContext*>(expr) ||
        dynamic_cast<LucisParser::RawBtickExprContext*>(expr) ||
        dynamic_cast<LucisParser::IntBtickExprContext*>(expr) ||
        dynamic_cast<LucisParser::ShellBtickExprContext*>(expr) ||
        dynamic_cast<LucisParser::CmptBtickExprContext*>(expr))
        return typeRegistry_.lookup("string");

    // ── Inline assembly expression ────────────────────────────
    if (auto* asmE = dynamic_cast<LucisParser::AsmExprContext*>(expr)) {
        auto* outList = asmE->asmOutputList();
        size_t numOutputs = outList ? outList->asmOutput().size() : 0;

        // Validate outputs
        if (outList) {
            for (auto* out : outList->asmOutput()) {
                auto raw = out->constraint->getText();
                auto constraint = raw.substr(1, raw.size() - 2);
                if (constraint.empty()) {
                    error(out, "asm output constraint cannot be empty");
                } else if (constraint[0] != '=' && constraint[0] != '+'
                           && !std::isdigit(constraint[0])) {
                    error(out, "asm output constraint must start with '=', '+', or a digit");
                }

                if (auto* ident = out->IDENTIFIER()) {
                    auto varName = ident->getText();
                    auto it = locals_.find(varName);
                    if (it == locals_.end()) {
                        error(out, "undefined variable '" + varName + "' in asm output");
                    } else {
                        it->second.used = true;
                    }
                }
            }
        }

        // Validate inputs
        if (auto* inList = asmE->asmInputList()) {
            for (auto* operand : inList->asmOperand()) {
                auto raw = operand->constraint->getText();
                auto constraint = raw.substr(1, raw.size() - 2);
                if (constraint.empty()) {
                    error(operand, "asm input constraint cannot be empty");
                } else if (constraint[0] == '=') {
                    error(operand, "asm input constraint cannot use '=' (reserved for outputs)");
                } else if (constraint[0] == '+') {
                    error(operand, "asm input constraint cannot use '+' (use in output with '+r' instead)");
                } else if (std::isdigit(constraint[0])) {
                    int matchIdx = std::stoi(constraint);
                    if (matchIdx < 0 || static_cast<size_t>(matchIdx) >= numOutputs) {
                        error(operand, "asm matching constraint '" + constraint
                              + "' refers to output " + constraint + " but there "
                              + (numOutputs == 1 ? "is only 1 output"
                                                 : "are only " + std::to_string(numOutputs) + " outputs"));
                    }
                }
                resolveExprType(operand->expression());
            }
        }

        if (numOutputs == 0)
            return typeRegistry_.lookup("void");
        auto* output = outList->asmOutput()[0];
        if (auto* ident = output->IDENTIFIER()) {
            auto varName = ident->getText();
            auto it = locals_.find(varName);
            if (it != locals_.end())
                return it->second.type;
        }
        // Unnamed output: return int64 (register-wide default for auto)
        return typeRegistry_.lookup("int64");
    }

    // ── @cfg(…) compile-time evaluated expression ─────────────
    if (auto* cfg = dynamic_cast<LucisParser::CfgExprContext*>(expr)) {
        // Populate target constants on first use
        if (compileTimeValues_.find("target_os") == compileTimeValues_.end()) {
            TargetInfo target(targetTriple_, debugMode_);
            compileTimeValues_["target_os"] = ComptimeValue::stringVal(target.os);
            compileTimeValues_["target_arch"] = ComptimeValue::stringVal(target.arch);
            compileTimeValues_["target_endian"] = ComptimeValue::stringVal(target.endian);
            compileTimeValues_["target_pointer_width"] = ComptimeValue::intVal(
                static_cast<int64_t>(target.pointerWidth));
            compileTimeValues_["target_os_family"] = ComptimeValue::stringVal(target.osFamily);
            compileTimeValues_["target_triple"] = ComptimeValue::stringVal(target.triple);
            compileTimeValues_["debug_assertions"] = ComptimeValue::boolVal(target.debug);
        }
        auto inner = cfg->expression();
        auto result = evaluateConstExpr(inner);
        if (!result) {
            error(cfg, "'@cfg' predicate must be a constant expression");
            return typeRegistry_.lookup("bool");
        }
        if (result->kind() != ComptimeValue::Kind::Bool &&
            result->kind() != ComptimeValue::Kind::Int) {
            error(cfg, "'@cfg' must evaluate to a boolean");
            return typeRegistry_.lookup("bool");
        }
        return typeRegistry_.lookup("bool");
    }

    // ── @typeName(T), @fields(T), @variants(T) — comptime builtins ──
    if (auto* at = dynamic_cast<LucisParser::AtTypeExprContext*>(expr)) {
        auto name = at->IDENTIFIER()->getText();
        auto* typeCtx = at->typeSpec();
        unsigned dims = 0;
        resolveTypeSpec(typeCtx, dims);
        if (name == "typeName" || name == "fields" || name == "variants")
            return typeRegistry_.lookup("string");
        if (name == "typeId")
            return typeRegistry_.lookup("int64");
        if (name == "isStruct" || name == "isEnum" || name == "isUnion")
            return typeRegistry_.lookup("bool");
        error(at, "unknown comptime builtin '@" + name + "'");
        return typeRegistry_.lookup("int32");
    }

    // ── @ptr(T, addr) — convert integer address to pointer ────
    if (auto* ptr = dynamic_cast<LucisParser::AtPtrExprContext*>(expr)) {
        auto* typeCtx = ptr->typeSpec();
        unsigned dims = 0;
        auto* pointeeType = resolveTypeSpec(typeCtx, dims);
        if (!pointeeType) {
            error(ptr, "'@ptr' requires a valid pointee type as first argument");
            return nullptr;
        }
        if (dims > 0) {
            error(ptr, "'@ptr' does not support array types");
            return nullptr;
        }
        auto* addrExpr = ptr->expression();
        auto* addrType = resolveExprType(addrExpr);
        if (!addrType) return nullptr;
        if (!isInteger(addrType)) {
            error(addrExpr, "'@ptr' address argument must be an integer");
            return nullptr;
        }
        return getPointerType(pointeeType);
    }

    if (dynamic_cast<LucisParser::NullLitExprContext*>(expr))
        return nullptr; // null is compatible with any pointer

    // ── Identifier ───────────────────────────────────────────────────
    if (auto* id = dynamic_cast<LucisParser::IdentExprContext*>(expr)) {
        auto name = id->IDENTIFIER()->getText();
        auto it = locals_.find(name);
        if (it != locals_.end()) {
            validateExprNotMoved(expr, id);
            // Warn on use of uninitialized variable (skip bitstruct — zero-initialized by default)
            if (!it->second.initialized && !(it->second.type && it->second.type->kind == TypeKind::BitStruct)) {
                warning(id, "variable '" + name + "' is used before being initialized");
            }
            it->second.used = true;
            return it->second.type;
        }

        auto fit = functions_.find(name);
        if (fit != functions_.end())
            return fit->second;

        // Comptime functions referenced as values
        if (comptimeRegistry_.isComptime(name))
            return typeRegistry_.lookup("int32");

        // C enum constants from #include headers
        auto ceit = cEnumConstants_.find(name);
        if (ceit != cEnumConstants_.end())
            return ceit->second.type;

        // C global variables from #include headers
        auto cgit = cGlobals_.find(name);
        if (cgit != cGlobals_.end())
            return cgit->second;

        // Imported symbols — check constant registry first, then treat as
        // polymorphic function reference (nullptr).
        if (imports_.isImported(name)) {
            auto& constType = builtinRegistry_.lookupConstant(name);
            if (!constType.empty())
                return typeRegistry_.lookup(constType);
            return nullptr;
        }

        // User module import or same-module symbol
        if (userImports_.count(name)) {
            auto modPath = userImports_[name];
            if (moduleRegistry_) {
                auto* sym = moduleRegistry_->findSymbol(
                    modPath, importedSymbolName(name));
                if (sym && sym->kind == ExportedSymbol::Constant) {
                    if (auto* cd = dynamic_cast<LucisParser::ConstDeclaratorContext*>(sym->decl)) {
                        if (cd->typeSpec()) {
                            unsigned dims = 0;
                            auto* ti = resolveTypeSpec(cd->typeSpec(), dims);
                            if (ti) return ti;
                        }
                        if (cd->expression()) {
                            auto* ti = resolveExprType(cd->expression());
                            if (ti) return ti;
                        }
                    }
                    return typeRegistry_.lookup("int32");
                }
                // Imported symbol is a type (struct, union, enum) used as value — error.
                if (sym && (sym->kind == ExportedSymbol::Struct ||
                            sym->kind == ExportedSymbol::Union ||
                            sym->kind == ExportedSymbol::Enum)) {
                    const char* kindStr = sym->kind == ExportedSymbol::Struct ? "struct" :
                                          sym->kind == ExportedSymbol::Union  ? "union" : "enum";
                    std::string msg = std::string(kindStr) + " '" + name + "' cannot be used as a value";
                    if (sym->kind == ExportedSymbol::Struct || sym->kind == ExportedSymbol::Union)
                        msg += ", did you mean '" + name + " {}'?";
                    error(expr, msg);
                    return nullptr;
                }
            }
            return nullptr;
        }

        // Enum variant imported via `use EnumType::*;`
        auto evIt = enumVariantImports_.find(name);
        if (evIt != enumVariantImports_.end())
            return evIt->second.enumType;

        if (moduleRegistry_ && !currentModulePath_.empty()) {
            auto* sym = moduleRegistry_->findSymbol(currentModulePath_, name);
            if (sym && sym->sourceFile != currentFile_)
                return nullptr;
        }

        if (name == "it" && unwrapCatchItDepth_ == 0) {
            error(expr, "'it' is only available inside 'expr catch { ... }' unwrap blocks");
            return nullptr;
        }

        // Check if name matches a known type — struct, enum, union or alias.
        // Using a type name as an expression is an error; suggest {} for structs.
        auto* ti = typeRegistry_.lookup(name);
        if (ti) {
            if (ti->kind == TypeKind::Struct)
                error(expr, "struct '" + name + "' cannot be used as a value, did you mean '" + name + " {}'?");
            else if (ti->kind == TypeKind::Union)
                error(expr, "union '" + name + "' cannot be used as a value, did you mean '" + name + " {}'?");
            else if (ti->kind == TypeKind::Enum)
                error(expr, "enum '" + name + "' cannot be used as a value");
            else
                error(expr, "type '" + name + "' cannot be used as a value");
            return nullptr;
        }

        // Hint: is this an unimported function from another module?
        if (moduleRegistry_) {
            for (auto& ns : moduleRegistry_->allModules()) {
                auto* sym = moduleRegistry_->findSymbol(ns, name);
                if (sym && sym->kind == ExportedSymbol::Function &&
                    sym->sourceFile != currentFile_) {
                    error(expr, "undefined variable '" + name +
                                "'. Did you forget 'use " + ns + "::" + name + ";'?");
                    return nullptr;
                }
            }
        }

        error(expr, "undefined variable '" + name + "'");
        return nullptr;
    }

    // ── Lambda expression: |params| body ─────────────────────────────
    if (auto* lexpr = dynamic_cast<LucisParser::LambdaExprContext*>(expr)) {
        return resolveLambdaExpr(lexpr);
    }
    if (auto* lblk = dynamic_cast<LucisParser::LambdaBlockExprContext*>(expr)) {
        return resolveLambdaBlockExpr(lblk);
    }

    // ── Parenthesized ────────────────────────────────────────────────
    if (auto* p = dynamic_cast<LucisParser::ParenExprContext*>(expr))
        return resolveExprType(p->expression());

    // ── Tuple literal: (expr, expr, ...) ─────────────────────────────
    if (auto* tl = dynamic_cast<LucisParser::TupleLitExprContext*>(expr)) {
        auto elements = tl->expression();
        std::vector<const TypeInfo*> elemTypes;
        std::string fullName = "tuple<";
        for (size_t i = 0; i < elements.size(); i++) {
            auto* et = resolveExprType(elements[i]);
            if (!et) return nullptr;
            elemTypes.push_back(et);
            if (i > 0) fullName += ", ";
            fullName += et->name;
        }
        fullName += ">";

        for (auto& dt : dynamicTypes_) {
            if (dt->name == fullName)
                return dt.get();
        }
        auto ti = std::make_unique<TypeInfo>();
        ti->name = fullName;
        ti->kind = TypeKind::Tuple;
        ti->bitWidth = 0;
        ti->isSigned = false;
        ti->tupleElements = std::move(elemTypes);
        const TypeInfo* raw = ti.get();
        dynamicTypes_.push_back(std::move(ti));
        return raw;
    }

    // ── Tuple index: expr.0, expr.1, ... ─────────────────────────────
    if (auto* ti = dynamic_cast<LucisParser::TupleIndexExprContext*>(expr)) {
        auto* baseType = resolveExprType(ti->expression());
        if (!baseType || baseType->kind != TypeKind::Tuple) {
            if (baseType)
                error(expr, "'.N' index requires a tuple type, got '" + baseType->name + "'");
            return nullptr;
        }
        int index = std::stoi(ti->INT_LIT()->getText());
        if (index < 0 || index >= static_cast<int>(baseType->tupleElements.size())) {
            error(expr, "tuple index " + std::to_string(index) + " is out of range (tuple has " +
                        std::to_string(baseType->tupleElements.size()) + " elements)");
            return nullptr;
        }
        return baseType->tupleElements[index];
    }

    // ── Chained tuple index: expr.N.M (FLOAT_LIT) ──────────────────
    if (auto* cti = dynamic_cast<LucisParser::ChainedTupleIndexExprContext*>(expr)) {
        auto* baseType = resolveExprType(cti->expression());
        if (!baseType || baseType->kind != TypeKind::Tuple) {
            if (baseType)
                error(expr, "'.N.M' index requires a tuple type, got '" + baseType->name + "'");
            return nullptr;
        }
        auto text = cti->FLOAT_LIT()->getText();
        auto dotPos = text.find('.');
        int idx1 = std::stoi(text.substr(0, dotPos));
        int idx2 = std::stoi(text.substr(dotPos + 1));
        if (idx1 < 0 || idx1 >= static_cast<int>(baseType->tupleElements.size())) {
            error(expr, "tuple index " + std::to_string(idx1) + " is out of range");
            return nullptr;
        }
        auto* innerType = baseType->tupleElements[idx1];
        if (!innerType || innerType->kind != TypeKind::Tuple) {
            error(expr, "'.N.M' requires element " + std::to_string(idx1) + " to be a tuple");
            return nullptr;
        }
        if (idx2 < 0 || idx2 >= static_cast<int>(innerType->tupleElements.size())) {
            error(expr, "tuple index " + std::to_string(idx2) + " is out of range");
            return nullptr;
        }
        return innerType->tupleElements[idx2];
    }

    // ── Tuple arrow index: ptr->0, ptr->1, ... ──────────────────────
    if (auto* tai = dynamic_cast<LucisParser::TupleArrowIndexExprContext*>(expr)) {
        auto* baseType = resolveExprType(tai->expression());
        if (!baseType || baseType->kind != TypeKind::Pointer) {
            if (baseType)
                error(expr, "'->N' requires a pointer type, got '" + baseType->name + "'");
            return nullptr;
        }
        auto* pointee = baseType->pointeeType;
        if (!pointee || pointee->kind != TypeKind::Tuple) {
            if (pointee)
                error(expr, "'->N' requires pointer to tuple, got pointer to '" + pointee->name + "'");
            return nullptr;
        }
        int index = std::stoi(tai->INT_LIT()->getText());
        if (index < 0 || index >= static_cast<int>(pointee->tupleElements.size())) {
            error(expr, "tuple index " + std::to_string(index) + " is out of range (tuple has " +
                        std::to_string(pointee->tupleElements.size()) + " elements)");
            return nullptr;
        }
        return pointee->tupleElements[index];
    }

    // ── Chained tuple arrow index: ptr->N.M (FLOAT_LIT) ────────────
    if (auto* ctai = dynamic_cast<LucisParser::ChainedTupleArrowIndexExprContext*>(expr)) {
        auto* baseType = resolveExprType(ctai->expression());
        if (!baseType || baseType->kind != TypeKind::Pointer) {
            if (baseType)
                error(expr, "'->N.M' requires a pointer type, got '" + baseType->name + "'");
            return nullptr;
        }
        auto* pointee = baseType->pointeeType;
        if (!pointee || pointee->kind != TypeKind::Tuple) {
            if (pointee)
                error(expr, "'->N.M' requires pointer to tuple, got pointer to '" + pointee->name + "'");
            return nullptr;
        }
        auto text = ctai->FLOAT_LIT()->getText();
        auto dotPos = text.find('.');
        int idx1 = std::stoi(text.substr(0, dotPos));
        int idx2 = std::stoi(text.substr(dotPos + 1));
        if (idx1 < 0 || idx1 >= static_cast<int>(pointee->tupleElements.size())) {
            error(expr, "tuple index " + std::to_string(idx1) + " is out of range");
            return nullptr;
        }
        auto* innerType = pointee->tupleElements[idx1];
        if (!innerType || innerType->kind != TypeKind::Tuple) {
            error(expr, "'->N.M' requires element " + std::to_string(idx1) + " to be a tuple");
            return nullptr;
        }
        if (idx2 < 0 || idx2 >= static_cast<int>(innerType->tupleElements.size())) {
            error(expr, "tuple index " + std::to_string(idx2) + " is out of range");
            return nullptr;
        }
        return innerType->tupleElements[idx2];
    }

    // ── Try expression (unwrap — same type as inner) ─────────────────
    if (auto* te = dynamic_cast<LucisParser::TryExprContext*>(expr)) {
        auto* innerType = resolveExprType(te->expression(0));
        UnwrapCatchPatternInfo pattern;
        std::string reason;
        if (classifyUnwrapCatchEnum(innerType, pattern, reason))
            innerType = singlePayloadType(*pattern.okVariant);
        if (te->OR() && te->expression().size() >= 2) {
            auto* fallbackType = resolveExprType(te->expression(1));
            if (fallbackType && innerType && !isAssignable(innerType, fallbackType)) {
                error(te, "try-or fallback type '" +
                           (fallbackType ? fallbackType->name : "?") +
                           "' is not compatible with '" +
                           (innerType ? innerType->name : "?") + "'");
                return nullptr;
            }
        }
        return innerType;
    }

    // ── Match expression: match expr { pattern -> body, ... } ─────────
    if (auto* me = dynamic_cast<LucisParser::MatchExprContext*>(expr)) {
        auto* matchedType = resolveExprType(me->expression());
        if (!matchedType) return nullptr;

        // A match must have at least one arm; an arm must have a body
        // (syntactically tolerated while typing).
        if (me->matchArm().empty()) {
            error(expr, "match expression requires at least one arm");
            return nullptr;
        }
        for (auto* arm : me->matchArm()) {
            if (arm->expression().empty() && !arm->block()) {
                error(arm, "match arm is missing its body (expression or block after '->')");
                return nullptr;
            }
        }

        // Check if this is a literal match (patterns are literals, not enum variants)
        bool hasLiteralPattern = false;
        bool hasEnumPattern = false;
        for (auto* arm : me->matchArm()) {
            for (auto* pattern : arm->pattern()) {
                if (pattern->literalPattern()) hasLiteralPattern = true;
                if (!pattern->IDENTIFIER().empty()) hasEnumPattern = true;
            }
        }

        // Validate matched type based on pattern types
        if (hasLiteralPattern && !hasEnumPattern) {
            // Literal match: must be int, float, string, bool, or char
            bool validType = matchedType->kind == TypeKind::Integer ||
                            matchedType->kind == TypeKind::Float ||
                            matchedType->kind == TypeKind::String ||
                            matchedType->kind == TypeKind::Bool ||
                            matchedType->kind == TypeKind::Char;
            if (!validType) {
                error(me, "match with literal patterns requires int, float, string, bool, or char, got '" +
                           matchedType->name + "'");
                return nullptr;
            }
        } else if (matchedType->kind != TypeKind::Enum) {
            error(me, "match requires an enum expression, got '" +
                       matchedType->name + "'");
            return nullptr;
        }

        const TypeInfo* resultType = nullptr;
        for (size_t i = 0; i < me->matchArm().size(); i++) {
            auto* arm = me->matchArm(i);

            // Or-patterns: process each sub-pattern, share same body
            bool armHasWildcard = false;
            std::string armBindName;
            for (size_t pi = 0; pi < arm->pattern().size(); pi++) {
            auto* pattern = arm->pattern(pi);

            // Resolve pattern: find matching variant or literal
            const EnumVariantInfo* matchedVariant = nullptr;
            std::string bindName;
            std::string variantName;
            bool isWildcard = pattern->WILDCARD() != nullptr;
            bool isLiteral = pattern->literalPattern() != nullptr;
            if (isWildcard) armHasWildcard = true;

            if (isWildcard) {
                // _ wildcard — matches any remaining variant/value
                // still need to resolve body type below
            } else if (isLiteral) {
                // Literal pattern: validate type compatibility
                auto* lit = pattern->literalPattern();
                const TypeInfo* litType = nullptr;
                if (lit->INT_LIT() || lit->HEX_LIT() || lit->OCT_LIT() || lit->BIN_LIT()) {
                    litType = typeRegistry_.lookup("int32");
                } else if (lit->FLOAT_LIT()) {
                    litType = typeRegistry_.lookup("float64");
                } else if (lit->STR_LIT() || lit->C_STR_LIT()) {
                    litType = typeRegistry_.lookup("string");
                } else if (lit->BOOL_LIT()) {
                    litType = typeRegistry_.lookup("bool");
                } else if (lit->CHAR_LIT()) {
                    litType = typeRegistry_.lookup("char");
                } else if (lit->NULL_LIT()) {
                    litType = typeRegistry_.lookup("null");
                }
                if (litType && !isAssignable(matchedType, litType)) {
                    error(pattern, "literal type '" + litType->name +
                                   "' is not compatible with matched type '" +
                                   matchedType->name + "'");
                    return nullptr;
                }
            } else if (!pattern->IDENTIFIER().empty()) {
                if (pattern->SCOPE() && pattern->IDENTIFIER().size() >= 2) {
                    variantName = pattern->IDENTIFIER(1)->getText();
                } else {
                    variantName = pattern->IDENTIFIER(0)->getText();
                }
            }

            if (!variantName.empty()) {
                // Find variant in enum
                for (auto& v : matchedType->enumVariantInfos) {
                    if (v.name == variantName) {
                        matchedVariant = &v;
                        break;
                    }
                }
                if (!matchedVariant) {
                    error(arm, "enum '" + matchedType->name +
                               "' has no variant '" + variantName + "'");
                    return nullptr;
                }

                // If payload binding: Variant(name)
                if (pattern->LPAREN() && !pattern->IDENTIFIER().empty()) {
                    size_t bindIdx = pattern->SCOPE() ? 2 : 1;
                    if (bindIdx < pattern->IDENTIFIER().size()) {
                        bindName = pattern->IDENTIFIER(bindIdx)->getText();
                    } else if (pattern->WILDCARD()) {
                        bindName = "_";
                    }
                    if (!bindName.empty() && bindName != "_") {
                        if (matchedVariant->payloadFields.empty()) {
                            error(arm, "variant '" + variantName +
                                       "' has no payload to bind");
                            return nullptr;
                        }
                        auto* payloadType = matchedVariant->payloadFields[0].typeInfo;
                        armBindName = bindName;
                        locals_[bindName] = {payloadType, 0, {}, true, true, nullptr};
                    }
                }
            }
            } // end or-pattern for loop

            // Resolve guard condition if present
            if (arm->IF()) {
                auto* guardType = resolveExprType(arm->expression(0));
                if (guardType && guardType->kind != TypeKind::Bool) {
                    error(arm, "match guard must be a boolean expression");
                    return nullptr;
                }
            }

            // Resolve body type — must happen before scope cleanup
            const TypeInfo* bodyType = nullptr;
            if (arm->block()) {
                auto savedLocals = locals_;
                ++scopeDepth_;
                bool terminated = false;
                auto* block = arm->block();
                for (auto* stmt : block->statement()) {
                    if (terminated) break;
                    checkStmt(stmt, currentReturnType_, terminated);
                }
                --scopeDepth_;

                // Resolve type while variables are still in scope
                auto stmts = block->statement();
                if (!stmts.empty()) {
                    auto* lastStmt = stmts.back();
                    if (auto* exprS = lastStmt->exprStmt()) {
                        bodyType = resolveExprType(exprS->expression());
                    } else if (auto* ret = lastStmt->returnStmt()) {
                        if (ret->expression()) {
                            bodyType = resolveExprType(ret->expression());
                        } else {
                            bodyType = typeRegistry_.lookup("void");
                        }
                    } else {
                        bodyType = typeRegistry_.lookup("void");
                    }
                } else {
                    bodyType = typeRegistry_.lookup("void");
                }

                locals_ = savedLocals;
            } else {
                size_t bodyExprIdx = arm->IF() ? 1 : 0;
                bodyType = resolveExprType(arm->expression(bodyExprIdx));
            }

            if (bodyType && bodyType->kind != TypeKind::Void) {
                if (!resultType) {
                    resultType = bodyType;
                } else if (resultType != bodyType && !isAssignable(resultType, bodyType)) {
                    error(arm, "match arm type '" + bodyType->name +
                               "' is not compatible with previous arm type '" +
                               resultType->name + "'");
                    return nullptr;
                }
            }

            // Clean up binding from scope
            if (!armBindName.empty() && armBindName != "_") {
                locals_.erase(armBindName);
            }
        }

        // Exhaustiveness: check all variants are covered (unless wildcard present)
        // Skip exhaustiveness check for literal patterns (they are not exhaustive by nature)
        bool hasWildcard = false;
        for (size_t i = 0; i < me->matchArm().size() && !hasWildcard; i++) {
            for (size_t pi = 0; pi < me->matchArm(i)->pattern().size(); pi++) {
                if (me->matchArm(i)->pattern(pi)->WILDCARD()) {
                    hasWildcard = true; break;
                }
            }
        }
        if (!hasWildcard && !hasLiteralPattern && !matchedType->enumVariantInfos.empty()) {
            // Collect covered variants
            std::unordered_set<std::string> covered;
            for (size_t i = 0; i < me->matchArm().size(); i++) {
                for (size_t pi = 0; pi < me->matchArm(i)->pattern().size(); pi++) {
                auto* p = me->matchArm(i)->pattern(pi);
                if (!p->IDENTIFIER().empty()) {
                    std::string vname;
                    if (p->SCOPE() && p->IDENTIFIER().size() >= 2)
                        vname = p->IDENTIFIER(1)->getText();
                    else
                        vname = p->IDENTIFIER(0)->getText();
                    if (!vname.empty()) covered.insert(vname);
                }
            }
            } // inner or-pattern loop
            // Find missing variants
            std::string missing;
            for (auto& v : matchedType->enumVariantInfos) {
                if (!covered.count(v.name)) {
                    if (!missing.empty()) missing += ", ";
                    missing += v.name;
                }
            }
            if (!missing.empty()) {
                error(me, "non-exhaustive match: missing variants " + missing);
                return nullptr;
            }
        }

        return resultType ? resultType : typeRegistry_.lookup("void");
    }

    // ── Propagate operator: expr? — unwrap Result or return error ─────
    if (auto* pe = dynamic_cast<LucisParser::PropagateExprContext*>(expr)) {
        auto* sourceType = resolveExprType(pe->expression());
        if (!sourceType) return nullptr;

        // Phase 5: optional context block adds message on error re-propagation
        // Validate that the block (if present) is well-formed
        if (auto* ctxBlock = pe->block()) {
            // The block should contain at least a string expression as context
            // Full validation is done in IRGen; here we just ensure it's not empty
            if (ctxBlock->statement().empty()) {
                error(pe, "? context block must contain a context message");
                return nullptr;
            }
        }

        // Phase 3: void functions skip classifyUnwrapCatchEnum (allow unit variants)
        bool isVoidReturn = currentReturnType_ &&
                            currentReturnType_->kind == TypeKind::Void;

        UnwrapCatchPatternInfo pattern;
        std::string reason;
        if (isVoidReturn) {
            if (sourceType->kind != TypeKind::Enum || sourceType->enumVariantInfos.empty()) {
                error(pe, "cannot use '?': expression must be an enum type");
                return nullptr;
            }
            for (const auto& v : sourceType->enumVariantInfos) {
                bool isErr = v.payloadFields.empty()
                    || v.isError
                    || (v.payloadFields.size() == 1
                        && v.payloadFields[0].typeInfo
                        && isOrInheritsFrom(v.payloadFields[0].typeInfo, "Error"));
                if (isErr) { pattern.errVariant = &v; break; }
            }
            if (!pattern.errVariant) {
                error(pe, "cannot use '?' in void function: enum '" +
                           sourceType->name + "' has no error variant");
                return nullptr;
            }
            return typeRegistry_.lookup("void");
        }

        // Fallible source type: !T
        if (sourceType->kind == TypeKind::Fallible) {
            if (!currentReturnType_ || currentReturnType_->kind != TypeKind::Fallible) {
                error(pe, "'?' requer função envolvente com retorno !T");
                return nullptr;
            }
            return sourceType->successType;
        }

        if (!classifyUnwrapCatchEnum(sourceType, pattern, reason)) {
            error(pe, reason);
            return nullptr;
        }
        // '?' requires the function return type to match or be compatible
        if (currentReturnType_ && currentReturnType_ != sourceType) {
                // Check if enums are compatible (same error payload, assignable success)
                bool compatible = false;
                if (currentReturnType_->kind == TypeKind::Enum &&
                    sourceType->kind == TypeKind::Enum) {
                    UnwrapCatchPatternInfo retPattern;
                    std::string retReason;
                    if (classifyUnwrapCatchEnum(currentReturnType_, retPattern, retReason)) {
                        auto* srcErr = singlePayloadType(*pattern.errVariant);
                        auto* retErr = singlePayloadType(*retPattern.errVariant);
                        if (srcErr && retErr && srcErr == retErr) {
                            compatible = true;
                        }
                    }
                }
                if (!compatible) {
                    error(pe, "cannot use '?': function return type '" +
                               currentReturnType_->name + "' is not compatible with '" +
                               sourceType->name + "'");
                    return nullptr;
                }
        }
        return singlePayloadType(*pattern.okVariant);
    }

    // ── Catch unwrap expression: expr catch { ... } ───────────────────
    if (auto* cu = dynamic_cast<LucisParser::CatchUnwrapExprContext*>(expr)) {
        auto* sourceType = resolveExprType(cu->expression());
        if (!sourceType) return nullptr;

        // Fallible source type: !T
        if (sourceType->kind == TypeKind::Fallible) {
            auto* errPayloadTI = sourceType->errorType;

            if (errPayloadTI) {
                auto saved = locals_.find("it");
                bool hadSaved = saved != locals_.end();
                VarInfo savedInfo;
                if (hadSaved) savedInfo = saved->second;

                locals_["it"] = {errPayloadTI, 0, {}, true, true, nullptr};

                ++unwrapCatchItDepth_;
                auto* retCtx = currentReturnType_ ? currentReturnType_ : typeRegistry_.lookup("void");
                checkBlock(cu->block(), retCtx);
                --unwrapCatchItDepth_;

                if (hadSaved) locals_["it"] = savedInfo;
                else locals_.erase("it");
            } else {
                auto* retCtx = currentReturnType_ ? currentReturnType_ : typeRegistry_.lookup("void");
                checkBlock(cu->block(), retCtx);
            }

            return sourceType->successType;
        }

        UnwrapCatchPatternInfo pattern;
        std::string reason;
        if (!classifyUnwrapCatchEnum(sourceType, pattern, reason)) {
            error(cu, reason);
            return nullptr;
        }

        auto* errPayloadTI = pattern.errVariant
            ? singlePayloadType(*pattern.errVariant) : nullptr;

        if (errPayloadTI) {
            auto saved = locals_.find("it");
            bool hadSaved = saved != locals_.end();
            VarInfo savedInfo;
            if (hadSaved) savedInfo = saved->second;

            locals_["it"] = {errPayloadTI, 0, {}, true, true, nullptr};

            ++unwrapCatchItDepth_;
            auto* retCtx = currentReturnType_ ? currentReturnType_ : typeRegistry_.lookup("void");
            checkBlock(cu->block(), retCtx);
            --unwrapCatchItDepth_;

            if (hadSaved) locals_["it"] = savedInfo;
            else locals_.erase("it");
        } else {
            // No payload in error variant (e.g. Option<T>::Nothing)
            auto* retCtx = currentReturnType_ ? currentReturnType_ : typeRegistry_.lookup("void");
            checkBlock(cu->block(), retCtx);
        }

        return singlePayloadType(*pattern.okVariant);
    }

    // ── Spawn expression: spawn f() → Task<T> where f() returns T ───
    if (auto* se = dynamic_cast<LucisParser::SpawnExprContext*>(expr)) {
        auto* innerType = resolveExprType(se->expression());
        if (innerType) {
            auto fullName = "Task<" + innerType->name + ">";
            auto* cached = typeRegistry_.lookup(fullName);
            if (cached) return cached;
            auto ti = std::make_unique<TypeInfo>();
            ti->name = fullName;
            ti->kind = TypeKind::Extended;
            ti->bitWidth = 0;
            ti->isSigned = false;
            ti->elementType = innerType;
            ti->extendedKind = "Task";
            const TypeInfo* raw = ti.get();
            dynamicTypes_.push_back(std::move(ti));
            return raw;
        }
        error(expr, "cannot resolve type of spawned expression");
        return typeRegistry_.lookup("void");
    }

    // ── Await expression: await task → T where task is Task<T> ──────
    if (auto* ae = dynamic_cast<LucisParser::AwaitExprContext*>(expr)) {
        auto* taskType = resolveExprType(ae->expression());
        if (taskType && taskType->kind == TypeKind::Extended &&
            taskType->extendedKind == "Task" && taskType->elementType)
            return taskType->elementType;
        if (!taskType) {
            error(expr, "cannot resolve type of awaited expression");
            return typeRegistry_.lookup("void");
        }
        return taskType;
    }

    // ── Unary negation ───────────────────────────────────────────────
    if (auto* neg = dynamic_cast<LucisParser::NegExprContext*>(expr)) {
        auto* operand = resolveExprType(neg->expression());
        if (operand && !isNumeric(operand))
            error(expr, "unary '-' requires numeric operand, got '" +
                             operand->name + "'");
        if (operand && operand->kind == TypeKind::Integer && !operand->isSigned)
            error(expr, "unary '-' cannot be applied to unsigned type '" +
                             operand->name + "'");
        return operand;
    }

    // ── Logical NOT ──────────────────────────────────────────────────
    if (auto* lnot = dynamic_cast<LucisParser::LogicalNotExprContext*>(expr)) {
        resolveExprType(lnot->expression());
        return typeRegistry_.lookup("bool");
    }

    // ── Bitwise NOT ──────────────────────────────────────────────────
    if (auto* bnot = dynamic_cast<LucisParser::BitNotExprContext*>(expr)) {
        auto* operand = resolveExprType(bnot->expression());
        if (operand && !isIntegerOrPointer(operand))
            error(expr, "operator '~' requires integer operand, got '" +
                             operand->name + "'");
        return operand;
    }

    // ── Multiplicative: *, /, % ──────────────────────────────────────
    if (auto* mul = dynamic_cast<LucisParser::MulExprContext*>(expr)) {
        auto exprs = mul->expression();
        auto* lhs = resolveExprType(exprs[0]);
        auto* rhs = resolveExprType(exprs[1]);
        auto opText = mul->op->getText();

        // ── User-defined operator overload ──
        if (auto* opRet = resolveOpOverload(lhs, rhs, opText)) return opRet;

        if (opText == "%") {
            if (lhs && !isIntegerOrPointer(lhs))
                error(expr, "operator '%' requires integer operands, got '" +
                                 lhs->name + "'");
            if (rhs && !isIntegerOrPointer(rhs))
                error(expr, "operator '%' requires integer operands, got '" +
                                 rhs->name + "'");
        } else {
            bool lhsBad = lhs && !isNumeric(lhs);
            bool rhsBad = rhs && !isNumeric(rhs);
            if (lhsBad || rhsBad) {
                auto& t = lhsBad ? lhs : rhs;
                error(expr, "operator '" + opText +
                                 "' requires numeric operands, got '" + t->name + "'");
            }

            if (!lhsBad && !rhsBad && lhs && rhs) {
            bool kindMismatch = lhs->kind != rhs->kind;
            // Char promotes to integer like C (char + int → int)
            if (kindMismatch && ((lhs->kind == TypeKind::Char && rhs->kind == TypeKind::Integer) ||
                                (lhs->kind == TypeKind::Integer && rhs->kind == TypeKind::Char)))
                kindMismatch = false;
            bool signMismatch = lhs->kind == TypeKind::Integer &&
                                rhs->kind == TypeKind::Integer &&
                                lhs->isSigned != rhs->isSigned &&
                                !isIntLitExpr(exprs[0]) &&
                                !isIntLitExpr(exprs[1]);
                if (kindMismatch || signMismatch) {
                    error(expr, "operator '" + opText +
                                     "' does not allow mixed numeric kinds ('" +
                                     lhs->name + "' and '" + rhs->name +
                                     "'); cast explicitly");
                }
            }
        }

        // Compile-time division by zero check
        if (opText == "/" || opText == "%") {
            if (auto* intLit = dynamic_cast<LucisParser::IntLitExprContext*>(exprs[1])) {
                if (intLit->INT_LIT()->getText() == "0")
                    error(expr, "division by zero");
            }
        }

        if (lhs && rhs && lhs->kind == rhs->kind) {
            unsigned lhsBW = lhs->bitWidth == 0 ? 64 : lhs->bitWidth;
            unsigned rhsBW = rhs->bitWidth == 0 ? 64 : rhs->bitWidth;
            return lhsBW >= rhsBW ? lhs : rhs;
        }
        return lhs ? lhs : rhs;
    }

    // ── Additive: +, - ──────────────────────────────────────────────
    if (auto* add = dynamic_cast<LucisParser::AddSubExprContext*>(expr)) {
        auto exprs = add->expression();
        auto* lhs = resolveExprType(exprs[0]);
        auto* rhs = resolveExprType(exprs[1]);
        auto opText = add->op->getText();

        bool lhsPtr = lhs && lhs->kind == TypeKind::Pointer;
        bool rhsPtr = rhs && rhs->kind == TypeKind::Pointer;
        bool lhsNum = lhs && isNumeric(lhs);
        bool rhsNum = rhs && isNumeric(rhs);

        // pointer + int  or  int + pointer  → pointer
        if ((lhsPtr && rhsNum) || (lhsNum && rhsPtr)) {
            return lhsPtr ? lhs : rhs;
        }
        // pointer - pointer → i64 (ptrdiff)
        if (lhsPtr && rhsPtr && opText == "-") {
            return typeRegistry_.lookup("i64");
        }

        // string + string → string
        if (lhs && rhs && lhs->kind == TypeKind::String && rhs->kind == TypeKind::String) {
            if (opText != "+") {
                error(expr, "operator '-' does not support string operands");
                return lhs;
            }
            return typeRegistry_.lookup("string");
        }

        // ── User-defined operator overload ──
        if (lhs && lhs->kind == TypeKind::Struct) {
            auto internal = (opText == "+") ? "opAdd" : "opSub";
            if (auto* op = findOperatorInChain(lhs, internal))
                return op->returnType;
        }

        bool lhsBad = lhs && !isNumeric(lhs);
        bool rhsBad = rhs && !isNumeric(rhs);
        if (lhsBad || rhsBad) {
            auto& t = lhsBad ? lhs : rhs;
            error(expr, "operator '" + opText +
                             "' requires numeric operands, got '" + t->name + "'");
        }

        if (!lhsBad && !rhsBad && lhs && rhs) {
            bool kindMismatch = lhs->kind != rhs->kind;
            if (kindMismatch && ((lhs->kind == TypeKind::Char && rhs->kind == TypeKind::Integer) ||
                                (lhs->kind == TypeKind::Integer && rhs->kind == TypeKind::Char)))
                kindMismatch = false;
            bool signMismatch = lhs->kind == TypeKind::Integer &&
                                rhs->kind == TypeKind::Integer &&
                                lhs->isSigned != rhs->isSigned &&
                                !isIntLitExpr(exprs[0]) &&
                                !isIntLitExpr(exprs[1]);
            if (kindMismatch || signMismatch) {
                error(expr, "operator '" + opText +
                                 "' does not allow mixed numeric kinds ('" +
                                 lhs->name + "' and '" + rhs->name +
                                 "'); cast explicitly");
            }
        }

        if (lhs && rhs && (lhs->kind == rhs->kind ||
            (lhs->kind == TypeKind::Char && rhs->kind == TypeKind::Integer) ||
            (lhs->kind == TypeKind::Integer && rhs->kind == TypeKind::Char))) {
            unsigned lhsBW = lhs->bitWidth == 0 ? 64 : lhs->bitWidth;
            unsigned rhsBW = rhs->bitWidth == 0 ? 64 : rhs->bitWidth;
            return lhsBW >= rhsBW ? lhs : rhs;
        }
        return lhs ? lhs : rhs;
    }

    // ── Shift: <<, >> ───────────────────────────────────────────────
    auto checkShift = [&](auto* shift, const std::string& opText) -> const TypeInfo* {
        auto exprs = shift->expression();
        auto* lhs = resolveExprType(exprs[0]);
        auto* rhs = resolveExprType(exprs[1]);
        // ── User-defined operator overload ──
        if (auto* opRet = resolveOpOverload(lhs, rhs, opText)) return opRet;
        if (lhs && !isIntegerOrPointer(lhs))
            error(expr, "operator '" + opText +
                             "' requires integer operands, got '" + lhs->name + "'");
        if (rhs && !isIntegerOrPointer(rhs))
            error(expr, "operator '" + opText +
                             "' requires integer operands, got '" + rhs->name + "'");
        return lhs;
    };
    if (auto* lsh = dynamic_cast<LucisParser::LshiftExprContext*>(expr))
        return checkShift(lsh, "<<");
    if (auto* rsh = dynamic_cast<LucisParser::RshiftExprContext*>(expr))
        return checkShift(rsh, ">>");

    // ── Relational: <, >, <=, >= ────────────────────────────────────
    if (auto* rel = dynamic_cast<LucisParser::RelExprContext*>(expr)) {
        auto exprs = rel->expression();
        auto* lhs = resolveExprType(exprs[0]);
        auto* rhs = resolveExprType(exprs[1]);
        auto opText = rel->op->getText();

        // ── User-defined operator overload ──
        if (auto* opRet = resolveOpOverload(lhs, rhs, opText)) return opRet;

        // Pointer comparison: ptr < ptr2, ptr > ptr2, etc.
        if (lhs && rhs && lhs->kind == TypeKind::Pointer && rhs->kind == TypeKind::Pointer)
            return typeRegistry_.lookup("bool");

        if (lhs && !isNumeric(lhs))
            error(expr, "operator '" + opText +
                             "' requires numeric operands, got '" + lhs->name + "'");
        else if (rhs && !isNumeric(rhs))
            error(expr, "operator '" + opText +
                             "' requires numeric operands, got '" + rhs->name + "'");

        if (lhs && rhs && isNumeric(lhs) && isNumeric(rhs)) {
            bool kindMismatch = lhs->kind != rhs->kind;
            if (kindMismatch && ((lhs->kind == TypeKind::Char && rhs->kind == TypeKind::Integer) ||
                                (lhs->kind == TypeKind::Integer && rhs->kind == TypeKind::Char)))
                kindMismatch = false;
            bool signMismatch = lhs->kind == TypeKind::Integer &&
                                rhs->kind == TypeKind::Integer &&
                                lhs->isSigned != rhs->isSigned &&
                                !isIntLitExpr(exprs[0]) &&
                                !isIntLitExpr(exprs[1]);
            if (kindMismatch || signMismatch) {
                error(expr, "operator '" + opText +
                                 "' does not allow mixed numeric kinds ('" +
                                 lhs->name + "' and '" + rhs->name +
                                 "'); cast explicitly");
            }
        }
        return typeRegistry_.lookup("bool");
    }

    // ── Equality: ==, != ────────────────────────────────────────────
    if (auto* eq = dynamic_cast<LucisParser::EqExprContext*>(expr)) {
        auto exprs = eq->expression();
        auto* lhs = resolveExprType(exprs[0]);
        auto* rhs = resolveExprType(exprs[1]);
        auto opText = eq->op->getText();

        // ── User-defined operator overload ──
        if (auto* opRet = resolveOpOverload(lhs, rhs, opText)) return opRet;

        // Keep equality strict for numeric operands to prevent IR-time mismatches
        // such as float-vs-int comparisons reaching LLVM FCmp/ICmp with mixed types.
        bool eqKindMatch = (lhs && rhs) ? (lhs->kind == rhs->kind) : false;
        if (!eqKindMatch && lhs && rhs &&
            ((lhs->kind == TypeKind::Char && rhs->kind == TypeKind::Integer) ||
             (lhs->kind == TypeKind::Integer && rhs->kind == TypeKind::Char)))
            eqKindMatch = true;
        if (lhs && rhs && isNumeric(lhs) && isNumeric(rhs) && !eqKindMatch) {
            error(expr, "operator '" + opText +
                             "' does not allow mixed numeric kinds ('" +
                             lhs->name + "' and '" + rhs->name +
                             "'); cast explicitly");
        } else if (lhs && rhs && lhs->kind == TypeKind::Integer && rhs->kind == TypeKind::Integer) {
            // Integer ↔ integer comparison always allowed (sign/width mismatches handled in IR)
        } else if (lhs && rhs &&
                   !(isAssignable(lhs, rhs) || isAssignable(rhs, lhs))) {
            error(expr, "operator '" + opText +
                             "' has incompatible operand types '" +
                             lhs->name + "' and '" + rhs->name + "'");
        }

        return typeRegistry_.lookup("bool");
    }

    // ── Bitwise AND/XOR/OR ──────────────────────────────────────────
    auto checkBitwise = [&](auto* ctx, const std::string& opText) -> const TypeInfo* {
        auto exprs = ctx->expression();
        auto* lhs = resolveExprType(exprs[0]);
        auto* rhs = resolveExprType(exprs[1]);
        // ── User-defined operator overload ──
        if (auto* opRet = resolveOpOverload(lhs, rhs, opText)) return opRet;
        if (lhs && !isIntegerOrPointer(lhs))
            error(expr, "operator '" + opText + "' requires integer operands, got '" +
                             lhs->name + "'");
        if (rhs && !isIntegerOrPointer(rhs))
            error(expr, "operator '" + opText + "' requires integer operands, got '" +
                             rhs->name + "'");
        if (lhs && rhs && lhs->kind == TypeKind::Integer && rhs->kind == TypeKind::Integer &&
            lhs->isSigned != rhs->isSigned &&
            !isIntLitExpr(exprs[0]) && !isIntLitExpr(exprs[1])) {
            error(expr, "operator '" + opText +
                             "' does not allow mixed signed/unsigned integers ('" +
                             lhs->name + "' and '" + rhs->name + "'); cast explicitly");
        }
        if (lhs && rhs) {
            unsigned lhsBW = lhs->bitWidth == 0 ? 64 : lhs->bitWidth;
            unsigned rhsBW = rhs->bitWidth == 0 ? 64 : rhs->bitWidth;
            return lhsBW >= rhsBW ? lhs : rhs;
        }
        return lhs ? lhs : rhs;
    };
    if (auto* ba = dynamic_cast<LucisParser::BitAndExprContext*>(expr))
        return checkBitwise(ba, "&");
    if (auto* bx = dynamic_cast<LucisParser::BitXorExprContext*>(expr))
        return checkBitwise(bx, "^");
    if (auto* bo = dynamic_cast<LucisParser::BitOrExprContext*>(expr))
        return checkBitwise(bo, "|");

    // ── Logical AND/OR ──────────────────────────────────────────────
    if (auto* la = dynamic_cast<LucisParser::LogicalAndExprContext*>(expr)) {
        auto exprs = la->expression();
        auto* lhs = resolveExprType(exprs[0]);
        auto* rhs = resolveExprType(exprs[1]);
        // ── User-defined operator overload ──
        if (auto* opRet = resolveOpOverload(lhs, rhs, "&&")) return opRet;
        return typeRegistry_.lookup("bool");
    }

    if (auto* lo = dynamic_cast<LucisParser::LogicalOrExprContext*>(expr)) {
        auto exprs = lo->expression();
        auto* lhs = resolveExprType(exprs[0]);
        auto* rhs = resolveExprType(exprs[1]);
        // ── User-defined operator overload ──
        if (auto* opRet = resolveOpOverload(lhs, rhs, "||")) return opRet;
        return typeRegistry_.lookup("bool");
    }

    // ── Helper: check if an expression is an lvalue ─────────────────
    auto isLValue = [](LucisParser::ExpressionContext* e) -> bool {
        return dynamic_cast<LucisParser::IdentExprContext*>(e)
            || dynamic_cast<LucisParser::FieldAccessExprContext*>(e)
            || dynamic_cast<LucisParser::DerefExprContext*>(e)
            || dynamic_cast<LucisParser::IndexExprContext*>(e);
    };

    // ── Pre-increment/decrement ─────────────────────────────────────
    if (auto* pi = dynamic_cast<LucisParser::PreIncrExprContext*>(expr)) {
        auto* inner = pi->expression();
        auto* type = resolveExprType(inner);
        if (type && !isInteger(type) && type->kind != TypeKind::Pointer)
            error(expr, "operator '++' requires integer or pointer operand, got '" +
                             type->name + "'");
        if (!isLValue(inner))
            error(expr, "operator '++' requires a variable (lvalue)");
        return type;
    }

    if (auto* pd = dynamic_cast<LucisParser::PreDecrExprContext*>(expr)) {
        auto* inner = pd->expression();
        auto* type = resolveExprType(inner);
        if (type && !isInteger(type) && type->kind != TypeKind::Pointer)
            error(expr, "operator '--' requires integer or pointer operand, got '" +
                             type->name + "'");
        if (!isLValue(inner))
            error(expr, "operator '--' requires a variable (lvalue)");
        return type;
    }

    // ── Post-increment/decrement ────────────────────────────────────
    if (auto* pi = dynamic_cast<LucisParser::PostIncrExprContext*>(expr)) {
        auto* inner = pi->expression();
        auto* type = resolveExprType(inner);
        if (type && !isInteger(type) && type->kind != TypeKind::Pointer)
            error(expr, "operator '++' requires integer or pointer operand, got '" +
                             type->name + "'");
        if (!isLValue(inner))
            error(expr, "operator '++' requires a variable (lvalue)");
        return type;
    }

    if (auto* pd = dynamic_cast<LucisParser::PostDecrExprContext*>(expr)) {
        auto* inner = pd->expression();
        auto* type = resolveExprType(inner);
        if (type && !isInteger(type) && type->kind != TypeKind::Pointer)
            error(expr, "operator '--' requires integer or pointer operand, got '" +
                             type->name + "'");
        if (!isLValue(inner))
            error(expr, "operator '--' requires a variable (lvalue)");
        return type;
    }

    // ── Ternary: cond ? trueExpr : falseExpr ────────────────────────
    if (auto* tern = dynamic_cast<LucisParser::TernaryExprContext*>(expr)) {
        auto exprs = tern->expression();
        auto* condType = resolveExprType(exprs[0]); // condition
        if (condType && !isConditionType(condType))
            error(expr, "ternary condition has type '" + condType->name +
                         "', expected 'bool' or numeric type");
        auto* trueType = resolveExprType(exprs[1]);
        auto* falseType = resolveExprType(exprs[2]);
        if (trueType && falseType && !isAssignable(trueType, falseType))
            error(expr, "ternary branches have incompatible types: '" +
                             trueType->name + "' and '" + falseType->name + "'");
        return trueType ? trueType : falseType;
    }

    // ── Is: expr is type [::Variant] ────────────────────────────────
    if (auto* isE = dynamic_cast<LucisParser::IsExprContext*>(expr)) {
        auto* lhsType = resolveExprType(isE->expression());
        unsigned dims = 0;
        auto* rhsType = resolveTypeSpecInContext(isE->typeSpec(), dims);

        // Variant identity check: value is EnumType::Variant
        if (isE->SCOPE()) {
            if (isE->IDENTIFIER().empty()) {
                error(expr, "invalid enum variant check in 'is' expression");
                return typeRegistry_.lookup("bool");
            }
            auto* variantNode = isE->IDENTIFIER(0);
            if (!variantNode) {
                error(expr, "invalid enum variant check in 'is' expression");
                return typeRegistry_.lookup("bool");
            }

            auto variantName = variantNode->getText();
            if (!rhsType || rhsType->kind != TypeKind::Enum) {
                error(expr, "right side of variant 'is' check must be an enum type");
                return typeRegistry_.lookup("bool");
            }
            if (!lhsType || lhsType->kind != TypeKind::Enum) {
                error(expr, "left side of variant 'is' check must be an enum value");
                return typeRegistry_.lookup("bool");
            }
            if (lhsType != rhsType) {
                error(expr, "enum type mismatch in variant 'is' check: left is '" +
                             lhsType->name + "', right is '" + rhsType->name + "'");
                return typeRegistry_.lookup("bool");
            }

            auto* variantInfo = findEnumVariantInfo(rhsType, variantName);
            if (!variantInfo) {
                error(expr, "enum '" + rhsType->name + "' has no variant '" +
                             variantName + "'");
            }

            // Binding: value is EnumType::Variant(name) — validate the binding
            if (isE->LPAREN() && isE->IDENTIFIER().size() > 1) {
                auto* bindNode = isE->IDENTIFIER(1);
                if (!bindNode) {
                    error(expr, "invalid binding in variant 'is' check");
                } else if (!variantInfo || variantInfo->payloadFields.empty()) {
                    error(expr, "variant '" + variantName +
                                "' has no payload to bind");
                }
            } else if (isE->LPAREN()) {
                if (!variantInfo || variantInfo->payloadFields.empty()) {
                    error(expr, "variant '" + variantName +
                                "' has no payload to bind");
                }
                // Binding type resolution is handled in checkIfStmt.
            }

            return typeRegistry_.lookup("bool");
        }

        return typeRegistry_.lookup("bool");
    }

    // ── Null coalescing: expr ?? default ────────────────────────────
    if (auto* nc = dynamic_cast<LucisParser::NullCoalExprContext*>(expr)) {
        auto exprs = nc->expression();
        auto* lhsType = resolveExprType(exprs[0]);
        auto* rhsType = resolveExprType(exprs[1]);
        if (lhsType && lhsType->kind != TypeKind::Pointer)
            error(expr, "left side of '\?\?' must be a pointer type, got '" +
                             lhsType->name + "'");
        if (lhsType && rhsType && !isAssignable(lhsType, rhsType))
            error(expr, "'\?\?' type mismatch: left is '" +
                             lhsType->name + "', default is '" +
                             rhsType->name + "'");
        return lhsType ? lhsType : rhsType;
    }

    // ── Arrow method call: expr->method(args) ───────────────────────
    if (auto* amc = dynamic_cast<LucisParser::ArrowMethodCallExprContext*>(expr)) {
        auto* baseType = resolveExprType(amc->expression());
        auto methodName = amc->IDENTIFIER()->getText();

        std::vector<const TypeInfo*> argTypes;
        if (auto* argList = amc->argList()) {
            for (auto* argExpr : argList->expression())
                argTypes.push_back(resolveExprType(argExpr));
        }

        if (!baseType) return nullptr;

        if (baseType->kind != TypeKind::Pointer) {
            error(expr, "'->' requires a pointer type, got '" +
                             baseType->name + "'");
            return nullptr;
        }
        auto* pointee = baseType->pointeeType;
        if (!pointee || pointee->kind != TypeKind::Struct) {
            error(expr, "'->' method call requires pointer to struct");
            return nullptr;
        }

        auto* sm = findMethodInChain(pointee, methodName);
        if (sm && !sm->isStatic) {
            if (argTypes.size() != sm->paramTypes.size()) {
                error(expr, "method '" + methodName + "' expects " +
                    std::to_string(sm->paramTypes.size()) +
                    " arguments, got " + std::to_string(argTypes.size()));
            }
            for (size_t i = 0; i < argTypes.size() && i < sm->paramTypes.size(); i++) {
                if (!argTypes[i] || !sm->paramTypes[i]) continue;
                if (!isAssignable(sm->paramTypes[i], argTypes[i])) {
                    error(expr, "method '" + methodName + "' argument " +
                        std::to_string(i + 1) + " type mismatch: expected '" +
                        sm->paramTypes[i]->name + "', got '" +
                        argTypes[i]->name + "'");
                }
            }
            return sm->returnType;
        }
        auto* tm = findMethodInTraits(pointee, methodName);
        if (tm && !tm->isStatic) {
            if (argTypes.size() != tm->paramTypes.size()) {
                error(expr, "method '" + methodName + "' expects " +
                    std::to_string(tm->paramTypes.size()) +
                    " arguments, got " + std::to_string(argTypes.size()));
            }
            return tm->returnType;
        }
        error(expr, "struct '" + pointee->name +
                    "' has no method '" + methodName + "'");
        return nullptr;
    }

    // ── Arrow access: expr->field ───────────────────────────────────
    if (auto* aa = dynamic_cast<LucisParser::ArrowAccessExprContext*>(expr)) {
        auto* baseType = resolveExprType(aa->expression());
        auto fieldName = aa->IDENTIFIER()->getText();

        if (baseType && baseType->kind != TypeKind::Pointer) {
            error(expr, "'->' requires a pointer type, got '" +
                             baseType->name + "'");
            return nullptr;
        }
        auto* pointee = baseType ? baseType->pointeeType : nullptr;
        if (pointee && pointee->kind != TypeKind::Struct && pointee->kind != TypeKind::Union) {
            error(expr, "'->' requires pointer to struct or union, got pointer to '" +
                             pointee->name + "'");
            return nullptr;
        }
        if (pointee) {
            for (auto& field : pointee->fields) {
                if (field.name == fieldName)
                    return field.typeInfo;
            }
            error(expr, "'" + pointee->name +
                             "' has no field '" + fieldName + "'");
        }
        return nullptr;
    }

    // ── Range: expr..expr ───────────────────────────────────────────
    if (auto* rng = dynamic_cast<LucisParser::RangeExprContext*>(expr)) {
        auto exprs = rng->expression();
        auto* startType = resolveExprType(exprs[0]);
        auto* endType   = resolveExprType(exprs[1]);
        if (startType && !isInteger(startType))
            error(expr, "range start must be integer, got '" +
                             startType->name + "'");
        if (endType && !isInteger(endType))
            error(expr, "range end must be integer, got '" +
                             endType->name + "'");
        return startType;
    }

    // ── Range inclusive: expr..=expr ─────────────────────────────────
    if (auto* rng = dynamic_cast<LucisParser::RangeInclExprContext*>(expr)) {
        auto exprs = rng->expression();
        auto* startType = resolveExprType(exprs[0]);
        auto* endType   = resolveExprType(exprs[1]);
        if (startType && !isInteger(startType))
            error(expr, "range start must be integer, got '" +
                             startType->name + "'");
        if (endType && !isInteger(endType))
            error(expr, "range end must be integer, got '" +
                             endType->name + "'");
        return startType;
    }

    // ── Spread: ...expr ─────────────────────────────────────────────
    if (auto* sp = dynamic_cast<LucisParser::SpreadExprContext*>(expr)) {
        return resolveExprType(sp->expression());
    }

    // ── Cast: expr as type ──────────────────────────────────────────
    if (auto* cast = dynamic_cast<LucisParser::CastExprContext*>(expr)) {
        auto* srcType = resolveExprType(cast->expression());
        unsigned dims = 0;
        auto* dstType = resolveTypeSpecInContext(cast->typeSpec(), dims);

        if (srcType && dstType && srcType != dstType) {
            // []uint8 ↔ string — ABI-identical {ptr, len}, zero-cost view.
            unsigned srcDims = resolveExprArrayDims(cast->expression());
            bool srcIsU8Slice = srcDims == 1 && srcType->name == "uint8";
            bool dstIsU8Slice = dims == 1 && dstType->name == "uint8";
            if ((srcIsU8Slice && dstType->kind == TypeKind::String) ||
                (srcType->kind == TypeKind::String && dstIsU8Slice)) {
                return dstType;
            }

            auto sk = srcType->kind;
            auto dk = dstType->kind;

            // string → pointer (e.g. string as cstring / string as *char)
            if (sk == TypeKind::String && dk == TypeKind::Pointer) {
                if (dstType->pointeeType &&
                    dstType->pointeeType->kind == TypeKind::Char) {
                    error(expr,
                        "cannot cast 'string' to 'cstring' directly. "
                        "Use 'cstr(expr)' to convert a string to a "
                        "null-terminated C string");
                } else {
                    error(expr,
                        "cannot cast 'string' to '" + dstType->name +
                        "': incompatible types");
                }
            }
            // pointer → string
            else if (sk == TypeKind::Pointer && dk == TypeKind::String) {
                if (srcType->pointeeType &&
                    srcType->pointeeType->kind == TypeKind::Char) {
                    error(expr,
                        "cannot cast 'cstring' to 'string' directly. "
                        "Use 'fromCStr(expr)' to convert a C string to "
                        "a TM string");
                } else {
                    error(expr,
                        "cannot cast '" + srcType->name +
                        "' to 'string': incompatible types");
                }
            }
            // char → string (not supported, use .toString())
            else if (sk == TypeKind::Char && dk == TypeKind::String) {
                error(expr,
                    "cannot cast 'char' to 'string'. "
                    "Use char.toString() to convert a character to a string");
            }
            // struct/extended/string → numeric or vice-versa
            else if ((sk == TypeKind::Struct || sk == TypeKind::Extended ||
                      sk == TypeKind::String) &&
                     (dk == TypeKind::Integer || dk == TypeKind::Float ||
                      dk == TypeKind::Bool)) {
                error(expr,
                    "cannot cast '" + srcType->name + "' to '" +
                    dstType->name + "': incompatible types");
            }
            else if ((sk == TypeKind::Integer || sk == TypeKind::Float ||
                      sk == TypeKind::Bool) &&
                     (dk == TypeKind::Struct || dk == TypeKind::Extended ||
                      dk == TypeKind::String)) {
                error(expr,
                    "cannot cast '" + srcType->name + "' to '" +
                    dstType->name + "': incompatible types");
            }
        }

        return dstType;
    }

    // ── Sizeof: sizeof(type) ────────────────────────────────────────
    if (auto* sz = dynamic_cast<LucisParser::SizeofExprContext*>(expr)) {
        // sizeof requires concrete type sizes; unsized [] dimensions are not valid.
        auto* spec = sz->typeSpec();
        while (spec && spec->LBRACKET()) {
            if (!spec->INT_LIT()) {
                error(expr, "sizeof: unsized array type is not allowed; use fixed-size '[N]T'");
                return typeRegistry_.lookup("usize");
            }
            if (spec->typeSpec().empty()) break;
            spec = spec->typeSpec(0);
        }

        unsigned dims = 0;
        auto* ti = resolveTypeSpecInContext(sz->typeSpec(), dims);
        if (!ti) error(expr, "sizeof: unknown type");
        return typeRegistry_.lookup("usize");
    }

    // ── Typeof: typeof(expr) ────────────────────────────────────────
    if (auto* to = dynamic_cast<LucisParser::TypeofExprContext*>(expr)) {
        resolveExprType(to->expression());
        return typeRegistry_.lookup("string");
    }

    // ── Alignof: alignof(type) ────────────────────────────────────────
    if (auto* al = dynamic_cast<LucisParser::AlignofExprContext*>(expr)) {
        unsigned dims = 0;
        auto* ti = resolveTypeSpec(al->typeSpec(), dims);
        if (!ti) {
            error(al, "alignof: unknown type");
        }
        return typeRegistry_.lookup("int64");
    }

    // ── Offsetof: offsetof(type, field) ───────────────────────────────
    if (auto* of = dynamic_cast<LucisParser::OffsetofExprContext*>(expr)) {
        unsigned dims = 0;
        auto* ti = resolveTypeSpec(of->typeSpec(), dims);
        if (!ti) {
            error(of, "offsetof: unknown type");
            return typeRegistry_.lookup("int64");
        }
        if (ti->kind != TypeKind::Struct && ti->kind != TypeKind::Union) {
            error(of, "offsetof: expected a struct or union type, got  + ti->name + ");
            return typeRegistry_.lookup("int64");
        }
        std::string fieldName = of->IDENTIFIER()->getText();
        bool found = false;
        for (const auto& field : ti->fields) {
            if (field.name == fieldName) {
                found = true;
                break;
            }
        }
        if (!found) {
            error(of, "offsetof: type  + ti->name +  has no field named  + fieldName + ");
        }
        return typeRegistry_.lookup("int64");
    }

    // ── Method call: expr.method(args) ─────────────────────────────
    if (auto* mc = dynamic_cast<LucisParser::MethodCallExprContext*>(expr)) {
        auto* receiverType = resolveExprType(mc->expression());
        auto methodName = mc->IDENTIFIER()->getText();

        std::vector<const TypeInfo*> argTypes;
        if (auto* argList = mc->argList()) {
            for (auto* argExpr : argList->expression()) {
                argTypes.push_back(resolveExprType(argExpr));
            }
        }

        if (!receiverType) return nullptr;

        // Auto-dereference: ptr.method() → pointee.method()
        if (receiverType->kind == TypeKind::Pointer && receiverType->pointeeType)
            receiverType = receiverType->pointeeType;

        unsigned recvArrayDims = resolveExprArrayDims(mc->expression());

        const MethodDescriptor* desc = nullptr;

        if (recvArrayDims > 0) {
            // Array method
            desc = methodRegistry_.lookupArrayMethod(methodName);
            if (!desc) {
                error(expr, "array type '[]" + receiverType->name +
                            "' has no method '" + methodName + "'");
                return nullptr;
            }
            // Validate numeric-only constraint
            if (desc->requireNumeric && !isNumeric(receiverType)) {
                error(expr, "method '" + methodName +
                            "' requires numeric element type, got '" +
                            receiverType->name + "'");
                return nullptr;
            }
        } else if (receiverType->kind == TypeKind::Struct) {
            // Struct field function call: obj.callback(args)
            for (auto& field : receiverType->fields) {
                if (field.name == methodName && field.typeInfo) {
                    auto* fnTI = field.typeInfo;
                    if (fnTI->kind == TypeKind::Pointer && fnTI->pointeeType &&
                        fnTI->pointeeType->kind == TypeKind::Function)
                        fnTI = fnTI->pointeeType;
                    if (fnTI->kind == TypeKind::Function) {
                        if (argTypes.size() != fnTI->paramTypes.size()) {
                            error(expr, "function '" + methodName + "' expects " +
                                std::to_string(fnTI->paramTypes.size()) +
                                " arguments " + formatParamTypes(fnTI->paramTypes) +
                                ", got " + std::to_string(argTypes.size()));
                        }
                        return fnTI->returnType;
                    }
                }
            }
            // Struct method via `extend` block (walks parent chain)
            auto* sm = findMethodInChain(receiverType, methodName);
            if (sm && !sm->isStatic) {
                if (argTypes.size() != sm->paramTypes.size()) {
                    error(expr, "method '" + methodName + "' expects " +
                        std::to_string(sm->paramTypes.size()) +
                        " arguments " + formatParamTypes(sm->paramTypes) +
                        ", got " + std::to_string(argTypes.size()));
                }
                for (size_t i = 0; i < argTypes.size() && i < sm->paramTypes.size(); i++) {
                    if (!argTypes[i] || !sm->paramTypes[i]) continue;
                    if (!isAssignable(sm->paramTypes[i], argTypes[i])) {
                        error(expr, "method '" + methodName + "' argument " +
                            std::to_string(i + 1) + " type mismatch: expected '" +
                            sm->paramTypes[i]->name + "', got '" +
                            argTypes[i]->name + "'");
                    }
                }
                return sm->returnType;
            }
            // Trait method via `impl` block
            auto* tm = findMethodInTraits(receiverType, methodName);
            if (tm && !tm->isStatic) {
                if (argTypes.size() != tm->paramTypes.size()) {
                    error(expr, "method '" + methodName + "' expects " +
                        std::to_string(tm->paramTypes.size()) +
                        " arguments, got " + std::to_string(argTypes.size()));
                }
                return tm->returnType;
            }
            error(expr, "struct '" + receiverType->name +
                        "' has no method '" + methodName + "'");
            return nullptr;
        } else if (receiverType->kind == TypeKind::Extended) {
            // Extended type method (e.g. vec.push, vec.pop)
            auto* extDesc = extTypeRegistry_.lookup(receiverType->extendedKind);
            if (!extDesc) {
                error(expr, "unknown extended type '" + receiverType->extendedKind + "'");
                return nullptr;
            }
            for (auto& md : extDesc->methods) {
                if (md.name == methodName) {
                    desc = &md;
                    break;
                }
            }
            if (!desc) {
                error(expr, "type '" + receiverType->name +
                            "' has no method '" + methodName + "'");
                return nullptr;
            }
            if (desc->requireNumeric) {
                auto* elemTI = receiverType->elementType;
                if (elemTI && !isNumeric(elemTI)) {
                    error(expr, "method '" + methodName +
                                "' requires numeric element type, got '" +
                                elemTI->name + "'");
                    return nullptr;
                }
            }
        } else {
            // Built-in type method
            desc = methodRegistry_.lookup(receiverType->kind, methodName);
            if (!desc) {
                error(expr, "type '" + receiverType->name +
                            "' has no method '" + methodName + "'");
                return nullptr;
            }
            // Validate signed/unsigned constraints
            if (desc->requireSigned && receiverType->kind == TypeKind::Integer &&
                !receiverType->isSigned) {
                error(expr, "method '" + methodName +
                            "' requires a signed integer type");
                return nullptr;
            }
            if (desc->requireUnsigned && receiverType->kind == TypeKind::Integer &&
                receiverType->isSigned) {
                error(expr, "method '" + methodName +
                            "' requires an unsigned integer type");
                return nullptr;
            }
        }

        // Validate argument count
        if (argTypes.size() != desc->paramTypes.size()) {
            error(expr, "method '" + methodName + "' expects " +
                std::to_string(desc->paramTypes.size()) +
                " arguments " + formatParamTypes(desc->paramTypes) +
                ", got " + std::to_string(argTypes.size()));
            return nullptr;
        }

        // Validate argument types
        for (size_t i = 0; i < argTypes.size(); i++) {
            if (!argTypes[i]) continue;
            const TypeInfo* expectedParam = nullptr;
            if (desc->paramTypes[i] == "_self")
                expectedParam = receiverType;
            else if (desc->paramTypes[i] == "_elem") {
                if (receiverType->kind == TypeKind::Extended && receiverType->elementType)
                    expectedParam = receiverType->elementType;
                else
                    expectedParam = receiverType;
            } else if (desc->paramTypes[i] == "_key") {
                if (receiverType->kind == TypeKind::Extended && receiverType->keyType)
                    expectedParam = receiverType->keyType;
                else {
                    error(expr, "cannot resolve map key type for method '" + methodName + "'");
                    continue;
                }
            } else if (desc->paramTypes[i] == "_val") {
                if (receiverType->kind == TypeKind::Extended && receiverType->valueType)
                    expectedParam = receiverType->valueType;
                else {
                    error(expr, "cannot resolve map value type for method '" + methodName + "'");
                    continue;
                }
            } else
                expectedParam = typeRegistry_.lookup(desc->paramTypes[i]);

            if (expectedParam && !isAssignable(expectedParam, argTypes[i])) {
                error(expr, "method '" + methodName + "' argument " +
                    std::to_string(i + 1) + " type mismatch: expected '" +
                    expectedParam->name + "', got '" + argTypes[i]->name + "'");
            }
        }

        // Resolve return type
        if (desc->returnType == "_self")
            return receiverType;
        if (desc->returnType == "_backing") {
            if (receiverType->kind == TypeKind::BitStruct && receiverType->backingType)
                return receiverType->backingType;
            return receiverType;
        }
        if (desc->returnType == "_elem") {
            if (receiverType->kind == TypeKind::Extended && receiverType->elementType)
                return receiverType->elementType;
            return receiverType;
        }
        if (desc->returnType == "_key") {
            if (receiverType->kind == TypeKind::Extended && receiverType->keyType)
                return receiverType->keyType;
            return receiverType;
        }
        if (desc->returnType == "_val") {
            if (receiverType->kind == TypeKind::Extended && receiverType->valueType)
                return receiverType->valueType;
            return receiverType;
        }
        // _vec_key → Vec<keyType>, _vec_val → Vec<valType>,
        // _vec_elem → Vec<elementType>
        if (desc->returnType == "_vec_key" || desc->returnType == "_vec_val"
            || desc->returnType == "_vec_elem") {
            const TypeInfo* innerType = nullptr;
            if (desc->returnType == "_vec_key" && receiverType->keyType)
                innerType = receiverType->keyType;
            else if (desc->returnType == "_vec_val" && receiverType->valueType)
                innerType = receiverType->valueType;
            else if (desc->returnType == "_vec_elem" && receiverType->elementType)
                innerType = receiverType->elementType;
            if (innerType) {
                auto fullName = "Vec<" + innerType->name + ">";
                for (auto& dt : dynamicTypes_) {
                    if (dt->name == fullName) return dt.get();
                }
                auto ti = std::make_unique<TypeInfo>();
                ti->name = fullName;
                ti->kind = TypeKind::Extended;
                ti->bitWidth = 0;
                ti->isSigned = false;
                ti->builtinSuffix = innerType->builtinSuffix;
                ti->elementType = innerType;
                ti->extendedKind = "Vec";
                const TypeInfo* raw = ti.get();
                dynamicTypes_.push_back(std::move(ti));
                return raw;
            }
        }
        return typeRegistry_.lookup(desc->returnType);
    }

    // ── Function call: expr(args) ───────────────────────────────────
    if (auto* call = dynamic_cast<LucisParser::FnCallExprContext*>(expr)) {
        auto* callee = call->expression();

        // Detect callee name for polymorphic builtin detection
        std::string calleeName;
        if (auto* ident = dynamic_cast<LucisParser::IdentExprContext*>(callee))
            calleeName = ident->IDENTIFIER()->getText();

        std::vector<const TypeInfo*> argTypes;
        std::vector<LucisParser::ExpressionContext*> argExprs;
        if (auto* argList = call->argList()) {
            for (auto* argExpr : argList->expression()) {
                argExprs.push_back(argExpr);
                argTypes.push_back(resolveExprType(argExpr));
            }
        }

        // A value argument cannot have type void.
        for (size_t i = 0; i < argTypes.size(); i++) {
            auto* ti = argTypes[i];
            if (ti && ti->kind == TypeKind::Void) {
                error(argExprs[i],
                      "argument " + std::to_string(i + 1) +
                      " has type 'void'; functions returning void cannot be used as values");
                return nullptr;
            }
        }

        // Lucis std::log::sprintf expects Lucis `string` values, not `*char`.
        // Guard expression calls during semantic checking.
        if (calleeName == "sprintf" && imports_.isImported("sprintf")) {
            for (size_t i = 0; i < argTypes.size(); i++) {
                auto* ti = argTypes[i];
                if (!ti) continue;
                if (ti->kind == TypeKind::Pointer &&
                    ti->pointeeType &&
                    ti->pointeeType->kind == TypeKind::Char) {
                    error(argExprs[i],
                          "'sprintf' from Lucis does not accept '*char'; "
                          "pass 'string' values (use 'fromCStr(...)' if needed)");
                    return nullptr;
                }
            }
        }

        // Generic function call with inferred type arguments: foo(10)
        if (!calleeName.empty()) {
            auto funcIt = genericFuncTemplates_.find(calleeName);
            if (funcIt != genericFuncTemplates_.end()) {
                std::vector<LucisParser::ParamContext*> formalParams;
                if (auto* paramList = funcIt->second.decl->paramList())
                    formalParams = paramList->param();

                if (argTypes.size() != formalParams.size()) {
                    error(expr, "generic function '" + calleeName +
                                 "' expects " + std::to_string(formalParams.size()) +
                                 " argument(s), got " + std::to_string(argTypes.size()));
                    return nullptr;
                }

                auto inferred = inferGenericTypeArgs(
                    calleeName,
                    funcIt->second.typeParams,
                    funcIt->second.decl->typeParamList(),
                    formalParams,
                    argTypes,
                    expr);
                if (!inferred) return nullptr;

                applyCallOwnershipEffects(calleeName, argExprs, expr);
                return instantiateGenericFunc(calleeName, funcIt->second, *inferred, expr);
            }
        }

        // Bare enum variant call via `use EnumType::*;`
        if (!calleeName.empty()) {
            auto evIt = enumVariantImports_.find(calleeName);
            if (evIt != enumVariantImports_.end()) {
                auto* enumType = evIt->second.enumType;
                auto* variantInfo = evIt->second.variantInfo;

                if (variantInfo->payloadKind == EnumPayloadKind::Named) {
                    error(expr, "variant '" + calleeName +
                                 "' uses named payload; use braces instead of parentheses");
                    return enumType;
                }
                if (argTypes.size() != variantInfo->payloadFields.size()) {
                    error(expr, "variant '" + calleeName +
                                 "' expects " + std::to_string(variantInfo->payloadFields.size()) +
                                 " argument(s), got " + std::to_string(argTypes.size()));
                    return enumType;
                }
                for (size_t i = 0; i < argTypes.size(); i++) {
                    auto* expected = variantInfo->payloadFields[i].typeInfo;
                    if (argTypes[i] && expected && !isAssignable(expected, argTypes[i])) {
                        error(expr, "variant '" + calleeName +
                                     "' argument " + std::to_string(i + 1) +
                                     " type mismatch: expected '" + expected->name +
                                     "', got '" + argTypes[i]->name + "'");
                    }
                }
                return enumType;
            }
        }

        // Function-like macro call: KEY_F(1) etc.
        if (!calleeName.empty()) {
            auto flmIt = cFunctionLikeMacros_.find(calleeName);
            if (flmIt != cFunctionLikeMacros_.end()) {
                auto* flm = flmIt->second;

                if (argTypes.size() != flm->paramNames.size()) {
                    error(expr, "function-like macro '" + calleeName + "' expects " +
                          std::to_string(flm->paramNames.size()) + " argument(s), got " +
                          std::to_string(argTypes.size()));
                }
                // Function-like macros expand to integer constant expressions
                auto* int32TI = typeRegistry_.lookup("int32");
                if (int32TI) return int32TI;
                return nullptr;
            }
        }

        auto* calleeType = resolveExprType(callee);

        // User-defined function with full type info
        if (calleeType && calleeType->kind == TypeKind::Function) {
            size_t paramCount = calleeType->paramTypes.size();

            if (calleeType->isVariadic) {
                // Variadic param itself is optional; only fixed params before it are required
                size_t requiredCount = paramCount > 0 ? paramCount - 1 : 0;
                if (argTypes.size() < requiredCount) {
                    error(expr, "function call expects at least " +
                                     std::to_string(requiredCount) +
                                     " arguments " + formatParamTypes(calleeType->paramTypes) +
                                     ", got " +
                                     std::to_string(argTypes.size()));
                }
                // Validate fixed parameter types
                for (size_t i = 0; i < std::min(argTypes.size(), paramCount); i++) {
                    if (argTypes[i] && calleeType->paramTypes[i] &&
                        !isAssignable(calleeType->paramTypes[i], argTypes[i])) {
                        error(expr,
                            "argument " + std::to_string(i + 1) +
                            " type mismatch: expected '" +
                            calleeType->paramTypes[i]->name + "', got '" +
                            argTypes[i]->name + "'");
                    }
                }
            } else {
                if (argTypes.size() != paramCount) {
                    error(expr, "function call expects " +
                                     std::to_string(paramCount) +
                                     " arguments " + formatParamTypes(calleeType->paramTypes) +
                                     ", got " +
                                     std::to_string(argTypes.size()));
                } else if (calleeName != "toString") {
                    for (size_t i = 0; i < argTypes.size(); i++) {
                        if (argTypes[i] && calleeType->paramTypes[i] &&
                            !isAssignable(calleeType->paramTypes[i], argTypes[i])) {
                            error(expr,
                                "argument " + std::to_string(i + 1) +
                                " type mismatch: expected '" +
                                calleeType->paramTypes[i]->name + "', got '" +
                                argTypes[i]->name + "'");
                        }
                    }
                }
            }
            if (!calleeName.empty()) {
                analyzeUnsafeCBufferCall(calleeName, expr, argExprs);
                auto declIt = functionDecls_.find(calleeName);
                if (declIt != functionDecls_.end() &&
                    declIt->second->attributeList() &&
                    hasAttribute(declIt->second->attributeList(), "deprecated"))
                    warning(expr, "call to deprecated function '" + calleeName + "'");
            }
            applyCallOwnershipEffects(calleeName, argExprs, expr);
            return calleeType->returnType;
        }

        // Comptime function call — resolve return type from declaration
        if (!calleeName.empty() && comptimeRegistry_.isComptime(calleeName)) {
            applyCallOwnershipEffects(calleeName, argExprs, expr);
            auto* decl = static_cast<LucisParser::FunctionDeclContext*>(
                comptimeRegistry_.lookup(calleeName));
            if (decl && decl->typeSpec()) {
                unsigned dims = 0;
                return resolveTypeSpec(decl->typeSpec(), dims);
            }
            return typeRegistry_.lookup("int32");
        }

        // Builtin function via registry
        if (!calleeName.empty()) {
            auto* sig = builtinRegistry_.lookup(calleeName);
            if (sig) {
                // Validate argument count
                if (sig->isVariadic) {
                    if (argTypes.size() < sig->paramTypes.size()) {
                        error(expr, "'" + calleeName + "' expects at least " +
                            std::to_string(sig->paramTypes.size()) +
                            " argument(s) " + formatParamTypes(sig->paramTypes) +
                            ", got " +
                            std::to_string(argTypes.size()));
                    }
                } else if (argTypes.size() != sig->paramTypes.size()) {
                    error(expr, "'" + calleeName + "' expects " +
                        std::to_string(sig->paramTypes.size()) +
                        " argument(s) " + formatParamTypes(sig->paramTypes) +
                        ", got " +
                        std::to_string(argTypes.size()));
                } else if (!sig->isPolymorphic) {
                    // Validate argument types
                    for (size_t i = 0; i < argTypes.size(); i++) {
                        if (!argTypes[i]) continue;
                        auto& expected = sig->paramTypes[i];
                        if (expected == "_any" || expected == "_numeric" ||
                            expected == "_integer" || expected == "_float")
                            continue;
                        auto* expectedTI = resolveBuiltinReturnType(expected);
                        if (expectedTI && !isAssignable(expectedTI, argTypes[i])) {
                            error(expr, "'" + calleeName + "' argument " +
                                std::to_string(i + 1) + ": expected '" +
                                expected + "', got '" + argTypes[i]->name + "'");
                        }
                    }
                }

                // Cross-parameter validation for polymorphic builtins
                if (sig->isPolymorphic && argTypes.size() >= 2) {
                    auto isNumeric = [](const TypeInfo* t) {
                        return t && (t->kind == TypeKind::Integer ||
                                     t->kind == TypeKind::Float);
                    };
                    auto isScalar = [](const TypeInfo* t) {
                        return t && (t->kind == TypeKind::Integer ||
                                     t->kind == TypeKind::Float ||
                                     t->kind == TypeKind::Bool ||
                                     t->kind == TypeKind::Char);
                    };
                    bool allAny = true, allNumeric = true;
                    for (auto& p : sig->paramTypes) {
                        if (p != "_any") allAny = false;
                        if (p != "_numeric") allNumeric = false;
                    }
                    if (allAny) {
                        auto* firstType = argTypes[0];
                        for (size_t i = 1; i < argTypes.size(); i++) {
                            if (!argTypes[i] || !firstType) continue;
                            bool stringVsScalar =
                                (firstType->kind == TypeKind::String && isScalar(argTypes[i])) ||
                                (argTypes[i]->kind == TypeKind::String && isScalar(firstType));
                            if (stringVsScalar) {
                                error(expr, "'" + calleeName + "' argument " +
                                    std::to_string(i + 1) + ": type mismatch, expected '" +
                                    firstType->name + "', got '" + argTypes[i]->name + "'");
                            }
                        }
                    }
                    if (allNumeric) {
                        for (size_t i = 0; i < argTypes.size(); i++) {
                            if (!argTypes[i]) continue;
                            if (!isNumeric(argTypes[i])) {
                                error(expr, "'" + calleeName + "' argument " +
                                    std::to_string(i + 1) + ": expected numeric type, got '" +
                                    argTypes[i]->name + "'");
                            }
                        }
                    }
                }

                // Resolve return type
                auto& retName = sig->returnType;
                if (retName == "_any" || retName == "_numeric") {
                    // Polymorphic: return type matches first argument
                    if (!argTypes.empty() && argTypes[0])
                    {
                        analyzeUnsafeCBufferCall(calleeName, expr, argExprs);
                        applyCallOwnershipEffects(calleeName, argExprs, expr);
                        return argTypes[0];
                    }
                    analyzeUnsafeCBufferCall(calleeName, expr, argExprs);
                    applyCallOwnershipEffects(calleeName, argExprs, expr);
                    return typeRegistry_.lookup("int32");
                }
                analyzeUnsafeCBufferCall(calleeName, expr, argExprs);
                applyCallOwnershipEffects(calleeName, argExprs, expr);
                return resolveBuiltinReturnType(retName);
            }
        }

        return nullptr;
    }

    // ── Field access: expr.field ─────────────────────────────────────
    if (auto* fa = dynamic_cast<LucisParser::FieldAccessExprContext*>(expr)) {
        auto* baseType = resolveExprType(fa->expression());
        auto fieldName = fa->IDENTIFIER()->getText();

        // .len/.length on array/variadic params → int64
        if (fieldName == "len" || fieldName == "length") {
            if (auto* ident = dynamic_cast<LucisParser::IdentExprContext*>(fa->expression())) {
                auto it = locals_.find(ident->IDENTIFIER()->getText());
                if (it != locals_.end() && it->second.arrayDims > 0)
                    return typeRegistry_.lookup("usize");
            }
            // .len/.length on string → usize
            if (baseType && baseType->kind == TypeKind::String)
                return typeRegistry_.lookup("usize");
            // .len/.length on Vec<T> → usize
            if (baseType && baseType->kind == TypeKind::Extended &&
                (baseType->extendedKind == "Vec" || baseType->extendedKind == "Set" ||
                 baseType->extendedKind == "Map"))
                return typeRegistry_.lookup("usize");
        }

        // Auto-dereference: ptr.field → pointee.field
        if (baseType && baseType->kind == TypeKind::Pointer && baseType->pointeeType)
            baseType = baseType->pointeeType;

        if (baseType && baseType->kind == TypeKind::BitStruct) {
            for (auto& bf : baseType->bitFields) {
                if (bf.name == fieldName && !bf.isPadding) {
                    // Return an unsigned integer type matching the field width
                    std::string typeName = "uint" + std::to_string(bf.width <= 8 ? 8
                                          : bf.width <= 16 ? 16
                                          : bf.width <= 32 ? 32 : 64);
                    return typeRegistry_.lookup(typeName);
                }
            }
            error(expr, "[" + currentFile_ + "] '" + baseType->name +
                             "' has no field '" + fieldName + "'");
        } else if (baseType && (baseType->kind == TypeKind::Struct || baseType->kind == TypeKind::Union)) {
            for (auto& field : baseType->fields) {
                if (field.name == fieldName)
                    return field.typeInfo;
            }
            error(expr, "[" + currentFile_ + "] '" + baseType->name +
                             "' has no field '" + fieldName + "'");
        } else if (baseType) {
            error(expr, "[" + currentFile_ + "] '" + baseType->name +
                             "' has no field '" + fieldName + "'");
        }
        return nullptr;
    }

    // ── Index: expr[index] ──────────────────────────────────────────
    if (auto* idx = dynamic_cast<LucisParser::IndexExprContext*>(expr)) {
        auto exprs = idx->expression();
        auto* baseType = resolveExprType(exprs[0]);
        auto* indexType = resolveExprType(exprs[1]);

        // User-defined opIndex on struct
        if (baseType && baseType->kind == TypeKind::Struct) {
            if (auto* op = findOperatorInChain(baseType, "opIndex")) {
                if (indexType && op->paramTypes.size() >= 2 &&
                    !isAssignable(op->paramTypes[1], indexType)) {
                    error(expr, "index type mismatch: expected '" +
                                 op->paramTypes[1]->name + "', got '" +
                                 indexType->name + "'");
                }
                return op->returnType;
            }
        }

        // Map<K,V>[key] returns V — key type must match K
        if (baseType && baseType->kind == TypeKind::Extended && baseType->keyType) {
            if (indexType && !isAssignable(baseType->keyType, indexType))
                error(expr, "map key type mismatch: expected '" +
                             baseType->keyType->name + "', got '" +
                             indexType->name + "'");
            if (!baseType->valueType) {
                error(expr, "map value type could not be resolved");
                return typeRegistry_.lookup("void");
            }
            return baseType->valueType;
        }

        // Ranges in [] are for loops/comprehensions, not for string (see docs/language/ranges.md, docs/stdlib/string.md)
        {
            auto* indexExpr = exprs[1];
            while (auto* p = dynamic_cast<LucisParser::ParenExprContext*>(indexExpr))
                indexExpr = p->expression();
            if (baseType && baseType->kind == TypeKind::String &&
                (dynamic_cast<LucisParser::RangeExprContext*>(indexExpr) ||
                 dynamic_cast<LucisParser::RangeInclExprContext*>(indexExpr))) {
                error(expr,
                    "cannot subscript 'string' with a range: use .slice(start, end) on the "
                    "string or std::str::slice (see docs/stdlib/string.md)");
                return nullptr;
            }
        }

        // Vec<T>[i] and array[i] — index must be integer
        if (indexType && !isInteger(indexType))
            error(expr, "index must be integer, got '" +
                             indexType->name + "'");

        // Array/Slice index: []T[i] returns T - check BEFORE pointer dereference.
        // Without this, []cstring (which resolves to *char) is confused with
        // a pointer dereference, incorrectly stripping the pointer type.
        unsigned baseDims = resolveExprArrayDims(exprs[0]);
        if (baseDims > 0)
            return baseType;
        // Dereference pointer if necessary
        auto* derefType = baseType;
        if (derefType && derefType->kind == TypeKind::Pointer && derefType->pointeeType)
            derefType = derefType->pointeeType;

        // Set<T> supports no indexing — reject it instead of letting codegen
        // emit vec builtins against the set header (memory garbage).
        if (derefType && derefType->kind == TypeKind::Extended &&
            derefType->extendedKind == "Set") {
            error(expr, "set<" + (derefType->elementType ?
                       derefType->elementType->name : "?") +
                       "> does not support indexing");
            return nullptr;
        }

        // Vec<T>[i] returns T
        if (derefType && derefType->kind == TypeKind::Extended && derefType->elementType)
            return derefType->elementType;

        // string[i] returns char (uint8)
        if (derefType && derefType->kind == TypeKind::String)
            return typeRegistry_.lookup("char");

        // If original type is pointer and dereferenced type is not handled above, return deref type
        if (baseType && baseType->kind == TypeKind::Pointer && derefType)
            return derefType;

        return baseType;
    }

    // ── Struct / Union literal: Name { field: expr, ... } ────────────
    if (auto* sl = dynamic_cast<LucisParser::StructLitExprContext*>(expr)) {
        auto ids = sl->IDENTIFIER();
        if (ids.empty()) return nullptr;

        auto typeName = ids[0]->getText();
        auto* typeInfo = typeRegistry_.lookup(typeName);
        if (!typeInfo && userImports_.count(typeName)) {
            // Imported type under a local alias.
            auto exported = importedSymbolName(typeName);
            typeInfo = typeRegistry_.lookup(exported);
        }

        if (!typeInfo) {
            error(expr, "unknown type '" + typeName + "'");
            return nullptr;
        }
        if (typeInfo->kind != TypeKind::Struct && typeInfo->kind != TypeKind::Union) {
            error(expr, "'" + typeName + "' is not a struct or union type");
            return nullptr;
        }

        auto fieldExprs = sl->expression();
        size_t fieldCount = ids.size() - 1;

        if (typeInfo->kind == TypeKind::Union) {
            if (fieldCount != 1) {
                error(expr, "union '" + typeName +
                                 "' literal must initialize exactly 1 field, got " +
                                 std::to_string(fieldCount));
            }
        } else {
            if (fieldCount > typeInfo->fields.size()) {
                error(expr, "struct '" + typeName + "' has " +
                                 std::to_string(typeInfo->fields.size()) +
                                 " fields, got " + std::to_string(fieldCount));
            }
        }

        for (size_t i = 0; i < fieldCount; i++) {
            auto fieldName = ids[i + 1]->getText();
            bool found = false;
            for (auto& sf : typeInfo->fields) {
                if (sf.name == fieldName) {
                    found = true;
                    if (i < fieldExprs.size()) {
                        auto* valType = resolveExprType(fieldExprs[i]);
                        if (valType && sf.typeInfo &&
                            !isAssignable(sf.typeInfo, valType)) {
                            error(expr, "field '" + fieldName +
                                             "' expects type '" +
                                             sf.typeInfo->name + "', got '" +
                                             valType->name + "'");
                        }
                        // Track ownership: if field expects an owned type,
                        // mark the source expression as moved
                        if (sf.typeInfo && isDropTrackedType(sf.typeInfo, 0))
                            markExprAsMoved(fieldExprs[i], expr);
                    }
                    break;
                }
            }
            if (!found)
                error(expr, "'" + typeName +
                                 "' has no field '" + fieldName + "'");
        }
        return typeInfo;
    }

    // ── Struct / Union positional init: Name { expr, expr, ... } ────
    if (auto* spi = dynamic_cast<LucisParser::StructPosInitExprContext*>(expr)) {
        auto typeName = spi->IDENTIFIER()->getText();
        auto* typeInfo = typeRegistry_.lookup(typeName);

        if (!typeInfo) {
            error(expr, "unknown type '" + typeName + "'");
            return nullptr;
        }
        if (typeInfo->kind != TypeKind::Struct && typeInfo->kind != TypeKind::Union) {
            error(expr, "'" + typeName + "' is not a struct or union type");
            return nullptr;
        }

        auto fieldExprs = spi->expression();
        if (typeInfo->kind == TypeKind::Union) {
            if (fieldExprs.size() != 1) {
                error(expr, "union '" + typeName +
                             "' literal must initialize exactly 1 field, got " +
                             std::to_string(fieldExprs.size()));
            }
        } else {
            if (fieldExprs.size() > typeInfo->fields.size()) {
                error(expr, "struct '" + typeName + "' has " +
                             std::to_string(typeInfo->fields.size()) +
                             " fields, got " + std::to_string(fieldExprs.size()));
            }
        }

        for (size_t i = 0; i < fieldExprs.size(); i++) {
            if (i >= typeInfo->fields.size()) break;
            auto& sf = typeInfo->fields[i];
            auto* valType = resolveExprType(fieldExprs[i]);
            if (valType && sf.typeInfo &&
                !isAssignable(sf.typeInfo, valType)) {
                error(expr, "field '" + sf.name + "' expects type '" +
                             sf.typeInfo->name + "', got '" +
                             valType->name + "'");
            }
            // Track ownership: if field expects an owned type,
            // mark the source expression as moved
            if (sf.typeInfo && isDropTrackedType(sf.typeInfo, 0))
                markExprAsMoved(fieldExprs[i], expr);
        }
        return typeInfo;
    }

    // ── Qualified positional struct/union init or enum variant init: LIB::Point { x, y } or Shape::Circle { 1, 2 } ───
    if (auto* qspi = dynamic_cast<LucisParser::QualifiedStructPosInitExprContext*>(expr)) {
        if (qspi->IDENTIFIER().empty()) {
            error(expr, "invalid qualified type initialization");
            return nullptr;
        }
        auto first = qspi->IDENTIFIER(0)->getText();
        auto second = qspi->IDENTIFIER().size() > 1 ? qspi->IDENTIFIER(1)->getText() : "";
        auto* typeInfo = tryResolveQualifiedType(expr, first, second);
        if (!typeInfo) return nullptr;

        if (typeInfo->kind == TypeKind::Enum) {
            return typeInfo;
        }

        if (typeInfo->kind != TypeKind::Struct && typeInfo->kind != TypeKind::Union) {
            error(expr, "'" + first + "::" + second + "' is not a struct or union type");
            return nullptr;
        }

        auto fieldExprs = qspi->expression();
        if (typeInfo->kind == TypeKind::Union) {
            if (fieldExprs.size() != 1) {
                error(expr, "union '" + first + "::" + second +
                             "' literal must initialize exactly 1 field, got " +
                             std::to_string(fieldExprs.size()));
            }
        } else {
            if (fieldExprs.size() > typeInfo->fields.size()) {
                error(expr, "struct '" + first + "::" + second + "' has " +
                             std::to_string(typeInfo->fields.size()) +
                             " fields, got " + std::to_string(fieldExprs.size()));
            }
        }

        for (size_t i = 0; i < fieldExprs.size(); i++) {
            if (i >= typeInfo->fields.size()) break;
            auto& sf = typeInfo->fields[i];
            auto* valType = resolveExprType(fieldExprs[i]);
            if (valType && sf.typeInfo &&
                !isAssignable(sf.typeInfo, valType)) {
                error(expr, "field '" + sf.name + "' expects type '" +
                             sf.typeInfo->name + "', got '" +
                             valType->name + "'");
            }
        }
        return typeInfo;
    }

    // ── Qualified named struct/union init or enum variant init: LIB::Point { x: 10, y: 20 } or Shape::Circle { r: 4.0 } ───
    if (auto* qsni = dynamic_cast<LucisParser::QualifiedStructNamedInitExprContext*>(expr)) {
        auto first = qsni->IDENTIFIER().size() > 0 ? qsni->IDENTIFIER(0)->getText() : "";
        auto second = qsni->IDENTIFIER().size() > 1 ? qsni->IDENTIFIER(1)->getText() : "";
        auto* typeInfo = tryResolveQualifiedType(expr, first, second);
        if (!typeInfo) return nullptr;

        if (typeInfo->kind == TypeKind::Enum) {
            return typeInfo;
        }

        if (typeInfo->kind != TypeKind::Struct && typeInfo->kind != TypeKind::Union) {
            error(expr, "'" + first + "::" + second + "' is not a struct or union type");
            return nullptr;
        }

        auto fieldExprs = qsni->expression();
        auto ids = qsni->IDENTIFIER();
        size_t fieldCount = fieldExprs.size();

        if (typeInfo->kind == TypeKind::Union) {
            if (fieldCount != 1) {
                error(expr, "union '" + first + "::" + second +
                             "' literal must initialize exactly 1 field");
            }
        } else {
            if (fieldCount > typeInfo->fields.size()) {
                error(expr, "struct '" + first + "::" + second + "' has " +
                             std::to_string(typeInfo->fields.size()) +
                             " fields, got " + std::to_string(fieldCount));
            }
        }

        for (size_t i = 0; i < fieldCount; i++) {
            auto fieldName = ids[i + 2]->getText();
            bool found = false;
            for (auto& sf : typeInfo->fields) {
                if (sf.name == fieldName) {
                    found = true;
                    if (i < fieldExprs.size()) {
                        auto* valType = resolveExprType(fieldExprs[i]);
                        if (valType && sf.typeInfo &&
                            !isAssignable(sf.typeInfo, valType)) {
                            error(expr, "field '" + fieldName +
                                             "' expects type '" +
                                             sf.typeInfo->name + "', got '" +
                                             valType->name + "'");
                        }
                    }
                    break;
                }
            }
            if (!found)
                error(expr, "'" + first + "::" + second +
                                 "' has no field '" + fieldName + "'");
        }
        return typeInfo;
    }

    // ── Static method call: Struct::method(args) ─────────────────────
    if (auto* smc = dynamic_cast<LucisParser::StaticMethodCallExprContext*>(expr)) {
        auto ids = smc->IDENTIFIER();
        if (ids.size() < 2) {
            error(expr, "invalid static call expression");
            return nullptr;
        }

        // ── Intrinsic call: lucis::core::trap() ────────────────────────
        {
            std::vector<std::string> idTexts;
            for (auto* id : ids)
                idTexts.push_back(id->getText());

            if (IntrinsicRegistry::isIntrinsicPrefix(idTexts[0])) {
                std::string ns, funcName;
                if (!IntrinsicRegistry::parseIntrinsicPath(idTexts, ns, funcName)) {
                    error(expr, "invalid intrinsic path: expected 'lucis::module::function'");
                    return nullptr;
                }

                auto* intrinsic = intrinsicRegistry_.lookup(ns, funcName);
                if (!intrinsic) {
                    error(expr, "unknown intrinsic '" + ns + "::" + funcName + "'");
                    return nullptr;
                }

                // Validate argument count
                auto argExprs = smc->argList()
                    ? smc->argList()->expression()
                    : std::vector<LucisParser::ExpressionContext*>{};

                size_t fixedCount = intrinsic->params.size();
                if (intrinsic->isVariadic) {
                    if (argExprs.size() < fixedCount) {
                        error(expr, "intrinsic '" + ns + "::" + funcName +
                              "' expects at least " + std::to_string(fixedCount) +
                              " argument(s), got " + std::to_string(argExprs.size()));
                        return nullptr;
                    }
                } else if (argExprs.size() != fixedCount) {
                    error(expr, "intrinsic '" + ns + "::" + funcName + "' expects " +
                          std::to_string(fixedCount) + " argument(s), got " +
                          std::to_string(argExprs.size()));
                    return nullptr;
                }

                // Validate argument types (fixed params only for variadic)
                for (size_t i = 0; i < std::min(argExprs.size(), fixedCount); i++) {
                    auto* argType = resolveExprType(argExprs[i]);
                    auto& expected = intrinsic->params[i].type;
                    if (expected != "_any" && argType) {
                        auto* expectedType = typeRegistry_.lookup(expected);
                        if (!canWidenTo(argType, expectedType)) {
                            error(argExprs[i], "intrinsic '" + ns + "::" + funcName +
                                  "' argument " + std::to_string(i + 1) + ": expected '" +
                                  expected + "', got '" + argType->name + "'");
                        }
                    }
                }

                return resolveBuiltinReturnType(intrinsic->returnType);
            }
        }
        // ── End intrinsic call check ─────────────────────────────────

        auto structName = ids.front()->getText();
        auto methodName = ids.back()->getText();

        auto checkStdModuleCall = [&](const std::string& modulePath) -> const TypeInfo* {
            ImportResolver probe;
            probe.addImport(modulePath, methodName);

            std::vector<const TypeInfo*> moduleArgTypes;
            if (auto* argList = smc->argList()) {
                for (auto* argExpr : argList->expression())
                    moduleArgTypes.push_back(resolveExprType(argExpr));
            }

            auto* sig = builtinRegistry_.lookup(methodName);
            if (!probe.isImported(methodName) || !sig) {
                error(expr, "module '" + modulePath +
                             "' does not export callable symbol '" + methodName + "'");
                return nullptr;
            }

            if (sig->isVariadic) {
                if (moduleArgTypes.size() < sig->paramTypes.size()) {
                    error(expr, "'" + methodName + "' expects at least " +
                                 std::to_string(sig->paramTypes.size()) +
                                 " argument(s) " + formatParamTypes(sig->paramTypes) +
                                 ", got " + std::to_string(moduleArgTypes.size()));
                }
            } else if (moduleArgTypes.size() != sig->paramTypes.size()) {
                error(expr, "'" + methodName + "' expects " +
                             std::to_string(sig->paramTypes.size()) +
                             " argument(s) " + formatParamTypes(sig->paramTypes) +
                             ", got " + std::to_string(moduleArgTypes.size()));
            }

            // Validate argument types for non-polymorphic builtins
            if (!sig->isPolymorphic) {
                for (size_t i = 0; i < moduleArgTypes.size(); i++) {
                    if (!moduleArgTypes[i]) continue;
                    auto& expected = sig->paramTypes[i];
                    if (expected == "_any" || expected == "_numeric" ||
                        expected == "_integer" || expected == "_float")
                        continue;
                    auto* expectedTI = resolveBuiltinReturnType(expected);
                    if (expectedTI && !isAssignable(expectedTI, moduleArgTypes[i])) {
                        error(expr, "'" + methodName + "' argument " +
                            std::to_string(i + 1) + ": expected '" +
                            expected + "', got '" + moduleArgTypes[i]->name + "'");
                    }
                }
            }

            // Cross-parameter validation for polymorphic builtins
            if (sig->isPolymorphic && moduleArgTypes.size() >= 2) {
                auto isNumeric = [](const TypeInfo* t) {
                    return t && (t->kind == TypeKind::Integer ||
                                 t->kind == TypeKind::Float);
                };
                auto isScalar = [](const TypeInfo* t) {
                    return t && (t->kind == TypeKind::Integer ||
                                 t->kind == TypeKind::Float ||
                                 t->kind == TypeKind::Bool ||
                                 t->kind == TypeKind::Char);
                };
                bool allAny = true, allNumeric = true;
                for (auto& p : sig->paramTypes) {
                    if (p != "_any") allAny = false;
                    if (p != "_numeric") allNumeric = false;
                }
                if (allAny) {
                    auto* firstType = moduleArgTypes[0];
                    for (size_t i = 1; i < moduleArgTypes.size(); i++) {
                        if (!moduleArgTypes[i] || !firstType) continue;
                        bool stringVsScalar =
                            (firstType->kind == TypeKind::String && isScalar(moduleArgTypes[i])) ||
                            (moduleArgTypes[i]->kind == TypeKind::String && isScalar(firstType));
                        if (stringVsScalar) {
                            error(expr, "'" + methodName + "' argument " +
                                std::to_string(i + 1) + ": type mismatch, expected '" +
                                firstType->name + "', got '" + moduleArgTypes[i]->name + "'");
                        }
                    }
                }
                if (allNumeric) {
                    for (size_t i = 0; i < moduleArgTypes.size(); i++) {
                        if (!moduleArgTypes[i]) continue;
                        if (!isNumeric(moduleArgTypes[i])) {
                            error(expr, "'" + methodName + "' argument " +
                                std::to_string(i + 1) + ": expected numeric type, got '" +
                                moduleArgTypes[i]->name + "'");
                        }
                    }
                }
            }

            auto& retName = sig->returnType;
            if (retName == "_any" || retName == "_numeric") {
                if (!moduleArgTypes.empty() && moduleArgTypes[0])
                    return moduleArgTypes[0];
                return typeRegistry_.lookup("int32");
            }
            return resolveBuiltinReturnType(retName);
        };

        if (ids.size() > 2) {
            auto rootName = ids.front()->getText();
            auto rootImport = userImports_.find(rootName);
            if (rootImport == userImports_.end() || rootImport->second != rootName) {
                error(expr, "module root '" + rootName +
                             "' is not imported; add 'use " + rootName + ";'");
                return nullptr;
            }

            std::string modulePath;
            for (size_t i = 0; i + 1 < ids.size(); ++i) {
                if (!modulePath.empty()) modulePath += "::";
                modulePath += ids[i]->getText();
            }

            if (ImportResolver::isStdModule(modulePath)) {
                return checkStdModuleCall(modulePath);
            }

            // Qualified static method call on user type: LIB::User::new(args)
            if (ids.size() == 3) {
                auto nsName = ids[0]->getText();
                auto typeName = ids[1]->getText();
                auto methodName2 = ids[2]->getText();

                // Validate type exists in module
                if (moduleRegistry_) {
                    if (!moduleRegistry_->hasModule(nsName)) {
                        error(expr, "unknown module '" + nsName + "'");
                        return nullptr;
                    }
                    auto* sym = moduleRegistry_->findSymbol(nsName, typeName);
                    if (!sym) {
                        error(expr, "'" + nsName + "::" + typeName + "' is not a known type");
                        return nullptr;
                    }
                }
                auto* ti = typeRegistry_.lookup(typeName);
                if (!ti || (ti->kind != TypeKind::Struct && ti->kind != TypeKind::Union)) {
                    error(expr, "'" + nsName + "::" + typeName + "' does not support static methods");
                    return nullptr;
                }

                // Look up static method from struct methods (walks parent chain)
                auto* sm = findMethodInChain(ti, methodName2);
                if (sm && sm->isStatic) {
                    std::vector<LucisParser::ExpressionContext*> argExprs;
                    if (auto* argList = smc->argList()) {
                        for (auto* a : argList->expression()) {
                            argExprs.push_back(a);
                            resolveExprType(a);
                        }
                    }
                    applyCallOwnershipEffects(typeName + "::" + methodName2, argExprs, expr);
                    return sm->returnType;
                }
                auto* tm = findMethodInTraits(ti, methodName2);
                if (tm && tm->isStatic) {
                    std::vector<LucisParser::ExpressionContext*> argExprs;
                    if (auto* argList = smc->argList()) {
                        for (auto* a : argList->expression()) {
                            argExprs.push_back(a);
                            resolveExprType(a);
                        }
                    }
                    applyCallOwnershipEffects(typeName + "::" + methodName2, argExprs, expr);
                    return tm->returnType;
                }
                error(expr, "type '" + nsName + "::" + typeName +
                             "' has no static method '" + methodName2 + "'");
                return nullptr;
            }

            error(expr, "unsupported qualified static call '" + modulePath +
                         "::" + methodName + "'");
            return nullptr;
        }

        auto importedModule = userImports_.find(structName);
        if (importedModule != userImports_.end()) {
            if (ImportResolver::isStdModule(importedModule->second)) {
                return checkStdModuleCall(importedModule->second);
            }
            if (moduleRegistry_ && moduleRegistry_->hasModule(importedModule->second)) {
                auto* sym = moduleRegistry_->findSymbol(importedModule->second, methodName);
                if (sym) {
                    if (sym->kind == ExportedSymbol::Function) {
                        auto* funcDecl = static_cast<LucisParser::FunctionDeclContext*>(sym->decl);
                        unsigned retDims = 0;
                        auto* retType = resolveTypeSpec(funcDecl->typeSpec(), retDims);
                        if (!retType) return nullptr;

                        std::vector<const TypeInfo*> paramTypes;
                        if (auto* paramList = funcDecl->paramList()) {
                            for (auto* param : paramList->param()) {
                                unsigned pDims = 0;
                                auto* pType = resolveTypeSpec(param->typeSpec(), pDims);
                                if (!pType) return nullptr;
                                paramTypes.push_back(pType);
                            }
                        }
                        return makeFunctionType(retType, paramTypes, false);
                    }
                    error(expr, "module '" + importedModule->second + "' does not export callable symbol '" + methodName + "'");
                    return nullptr;
                }
            }
        }

        std::vector<const TypeInfo*> argTypes;
        std::vector<LucisParser::ExpressionContext*> argExprs;
        if (auto* argList = smc->argList()) {
            for (auto* argExpr : argList->expression()) {
                argExprs.push_back(argExpr);
                argTypes.push_back(resolveExprType(argExpr));
            }
        }

        auto* enumType = typeRegistry_.lookup(structName);
        if (enumType && enumType->kind == TypeKind::Enum) {
            auto* variantInfo = findEnumVariantInfo(enumType, methodName);
            if (!variantInfo) {
                error(expr, "enum '" + structName + "' has no variant '" + methodName +
                             "'; enums do not support static methods via 'extend'");
                return enumType;
            }
            if (variantInfo->payloadKind == EnumPayloadKind::Named) {
                error(expr, "variant '" + structName + "::" + methodName +
                             "' uses named payload; use braces instead of parentheses");
                return enumType;
            }
            if (argTypes.size() != variantInfo->payloadFields.size()) {
                error(expr, "variant '" + structName + "::" + methodName +
                             "' expects " + std::to_string(variantInfo->payloadFields.size()) +
                             " argument(s), got " + std::to_string(argTypes.size()));
                return enumType;
            }
            auto args = smc->argList() ? smc->argList()->expression()
                                       : std::vector<LucisParser::ExpressionContext*>{};
            for (size_t i = 0; i < argTypes.size(); i++) {
                auto* expected = variantInfo->payloadFields[i].typeInfo;
                bool literalToVecOk = false;
                if (expected && expected->kind == TypeKind::Extended &&
                    expected->extendedKind == "Vec" &&
                    i < args.size() &&
                    dynamic_cast<LucisParser::ArrayLitExprContext*>(args[i])) {
                    literalToVecOk = true;
                    auto* arr = dynamic_cast<LucisParser::ArrayLitExprContext*>(args[i]);
                    for (auto* e : arr->expression()) {
                        auto* et = resolveExprType(e);
                        if (et && expected->elementType &&
                            !isAssignable(expected->elementType, et)) {
                            error(e, "element type mismatch: expected '" +
                                         expected->elementType->name + "', got '" +
                                         et->name + "'");
                            literalToVecOk = false;
                        }
                    }
                }
                if (argTypes[i] && expected &&
                    !literalToVecOk &&
                    !isAssignable(expected, argTypes[i])) {
                    error(expr, "variant '" + structName + "::" + methodName +
                                 "' argument " + std::to_string(i + 1) +
                                 " type mismatch: expected '" + expected->name +
                                 "', got '" + argTypes[i]->name + "'");
                }
            }
            return enumType;
        }

        // Generic enum constructor inside an instantiated generic function,
        // e.g. in sum<T>: Result::Ok(value) where activeTypeSubst_ has T.
        auto genEnumIt = genericEnumTemplates_.find(structName);
        if (genEnumIt != genericEnumTemplates_.end() && !activeTypeSubst_.empty()) {
            std::vector<const TypeInfo*> inferredArgs;
            for (const auto& tp : genEnumIt->second.typeParams) {
                auto it = activeTypeSubst_.find(tp);
                if (it == activeTypeSubst_.end() || !it->second) {
                    error(expr, "cannot infer type arguments for generic enum '" +
                                    structName + "'; use explicit type arguments");
                    return nullptr;
                }
                inferredArgs.push_back(it->second);
            }

            auto* inferredEnum = instantiateGenericEnum(structName, genEnumIt->second,
                                                        inferredArgs, expr);
            if (!inferredEnum) return nullptr;

            auto* variantInfo = findEnumVariantInfo(inferredEnum, methodName);
            if (!variantInfo) {
                error(expr, "enum '" + inferredEnum->name + "' has no variant '" + methodName +
                             "'; enums do not support static methods via 'extend'");
                return inferredEnum;
            }
            if (variantInfo->payloadKind == EnumPayloadKind::Named) {
                error(expr, "variant '" + inferredEnum->name + "::" + methodName +
                             "' uses named payload; use braces instead of parentheses");
                return inferredEnum;
            }
            if (argTypes.size() != variantInfo->payloadFields.size()) {
                error(expr, "variant '" + inferredEnum->name + "::" + methodName +
                             "' expects " + std::to_string(variantInfo->payloadFields.size()) +
                             " argument(s), got " + std::to_string(argTypes.size()));
                return inferredEnum;
            }
            auto args = smc->argList() ? smc->argList()->expression()
                                       : std::vector<LucisParser::ExpressionContext*>{};
            for (size_t i = 0; i < argTypes.size(); i++) {
                auto* expected = variantInfo->payloadFields[i].typeInfo;
                bool literalToVecOk = false;
                if (expected && expected->kind == TypeKind::Extended &&
                    expected->extendedKind == "Vec" &&
                    i < args.size() &&
                    dynamic_cast<LucisParser::ArrayLitExprContext*>(args[i])) {
                    literalToVecOk = true;
                    auto* arr = dynamic_cast<LucisParser::ArrayLitExprContext*>(args[i]);
                    for (auto* e : arr->expression()) {
                        auto* et = resolveExprType(e);
                        if (et && expected->elementType &&
                            !isAssignable(expected->elementType, et)) {
                            error(e, "element type mismatch: expected '" +
                                         expected->elementType->name + "', got '" +
                                         et->name + "'");
                            literalToVecOk = false;
                        }
                    }
                }
                if (argTypes[i] && expected &&
                    !literalToVecOk &&
                    !isAssignable(expected, argTypes[i])) {
                    error(expr, "variant '" + inferredEnum->name + "::" + methodName +
                                 "' argument " + std::to_string(i + 1) +
                                 " type mismatch: expected '" + expected->name +
                                 "', got '" + argTypes[i]->name + "'");
                }
            }
            return inferredEnum;
        }

        // Generic static method inference: Node::create(42)
        auto extIt = genericExtendTemplates_.find(structName);
        auto structIt = genericStructTemplates_.find(structName);
        if (extIt != genericExtendTemplates_.end() && structIt != genericStructTemplates_.end()) {
            for (auto* method : extIt->second.decl->extendMethod()) {
                if (method->AMPERSAND()) continue;
                if (method->name->getText() != methodName) continue;

                std::vector<LucisParser::ParamContext*> formalParams;
                if (auto* paramList = method->paramList())
                    formalParams = paramList->param();

                if (argTypes.size() != formalParams.size()) {
                    error(expr, "static method '" + structName + "::" + methodName +
                                 "' expects " + std::to_string(formalParams.size()) +
                                 " argument(s), got " + std::to_string(argTypes.size()));
                    return nullptr;
                }

                auto inferred = inferGenericTypeArgs(
                    structName + "::" + methodName,
                    extIt->second.typeParams,
                    extIt->second.decl->typeParamList(),
                    formalParams,
                    argTypes,
                    expr);
                if (!inferred) return nullptr;

                auto* instanceTI = instantiateGenericStruct(structName, structIt->second,
                                                            *inferred, expr);
                if (!instanceTI) return nullptr;

                auto* sm = findMethodInChain(instanceTI, methodName);
                if (!sm || !sm->isStatic) {
                    auto* tm = findMethodInTraits(instanceTI, methodName);
                    if (tm && tm->isStatic) {
                        if (argTypes.size() != tm->paramTypes.size()) {
                            error(expr, "static method '" + structName + "::" + methodName +
                                "' expects " + std::to_string(tm->paramTypes.size()) +
                                " arguments, got " + std::to_string(argTypes.size()));
                            return nullptr;
                        }
                        return tm->returnType;
                    }
                    error(expr, "struct '" + instanceTI->name + "' has no static method '" + methodName + "'");
                    return nullptr;
                }

                if (argTypes.size() != sm->paramTypes.size()) {
                    error(expr, "static method '" + structName + "::" + methodName + "' argument count mismatch: expected " +
                        std::to_string(sm->paramTypes.size()) + ", got " + std::to_string(argTypes.size()));
                    return nullptr;
                }
                for (size_t i = 0; i < argTypes.size(); i++) {
                    if (!argTypes[i] || !sm->paramTypes[i]) continue;
                    if (!isAssignable(sm->paramTypes[i], argTypes[i])) {
                        error(expr, "static method '" + structName + "::" +
                            methodName + "' argument " +
                            std::to_string(i + 1) + " type mismatch: expected '" +
                            sm->paramTypes[i]->name + "', got '" +
                            argTypes[i]->name + "'");
                    }
                }
                return sm->returnType;
            }
        }

        auto* structType = typeRegistry_.lookup(structName);
        if (!structType) {
            error(expr, "unknown type '" + structName + "'");
            return nullptr;
        }
        if (structType->kind != TypeKind::Struct) {
            error(expr, "'" + structName + "' is not a struct type");
            return nullptr;
        }

        auto* sm = findMethodInChain(structType, methodName);
        if ((!sm || !sm->isStatic) && !(sm && sm->isStatic)) {
            // Fallback: trait impl static methods
            auto* tm = findMethodInTraits(structType, methodName);
            if (tm && tm->isStatic) {
                if (argTypes.size() != tm->paramTypes.size()) {
                    error(expr, "static method '" + structName + "::" + methodName +
                        "' expects " + std::to_string(tm->paramTypes.size()) +
                        " arguments, got " + std::to_string(argTypes.size()));
                }
                return tm->returnType;
            }
            if (auto* argList = smc->argList())
                for (auto* argExpr : argList->expression())
                    resolveExprType(argExpr);
            error(expr, "struct '" + structName + "' has no static method '" + methodName + "'");
            return nullptr;
        }

        if (argTypes.size() != sm->paramTypes.size()) {
            error(expr, "static method '" + structName + "::" + methodName +
                "' expects " + std::to_string(sm->paramTypes.size()) +
                " arguments " + formatParamTypes(sm->paramTypes) +
                ", got " + std::to_string(argTypes.size()));
        } else {
            for (size_t i = 0; i < argTypes.size(); i++) {
                if (!argTypes[i] || !sm->paramTypes[i]) continue;
                if (!isAssignable(sm->paramTypes[i], argTypes[i])) {
                    error(expr, "static method '" + structName + "::" +
                        methodName + "' argument " +
                        std::to_string(i + 1) + " type mismatch: expected '" +
                        sm->paramTypes[i]->name + "', got '" +
                        argTypes[i]->name + "'");
                }
            }
        }
        applyCallOwnershipEffects(structName + "::" + methodName, argExprs, expr);
        return sm->returnType;
    }

    // ── Generic function call: max<int32>(a, b) ─────────────────────
    if (auto* gfc = dynamic_cast<LucisParser::GenericFnCallExprContext*>(expr)) {
        auto funcName = gfc->IDENTIFIER()->getText();
        auto typeParamSpecs = gfc->typeSpec();

        // Resolve type arguments
        std::vector<const TypeInfo*> typeArgs;
        for (auto* ts : typeParamSpecs) {
            unsigned dims = 0;
            auto* argTI = resolveTypeSpecInContext(ts, dims);
            if (!argTI) return nullptr;
            typeArgs.push_back(argTI);
        }

        // Find the generic function template
        auto funcIt = genericFuncTemplates_.find(funcName);
        if (funcIt == genericFuncTemplates_.end()) {
            error(expr, "'" + funcName + "' is not a generic function");
            if (auto* argList = gfc->argList())
                for (auto* a : argList->expression()) resolveExprType(a);
            return nullptr;
        }

        // Validate argument count and types before instantiation
        if (auto* pl = funcIt->second.decl->paramList()) {
            auto params = pl->param();
            size_t argCount = gfc->argList() ? gfc->argList()->expression().size() : 0;
            if (argCount != params.size()) {
                error(expr, "'" + funcName + "' expects " +
                       std::to_string(params.size()) + " argument(s), got " +
                       std::to_string(argCount));
                // Still resolve arg types to avoid cascading errors
                if (auto* al = gfc->argList())
                    for (auto* a : al->expression()) resolveExprType(a);
                return instantiateGenericFunc(funcName, funcIt->second, typeArgs, expr);
            }
            auto args = gfc->argList()->expression();
                for (size_t i = 0; i < args.size() && i < params.size(); i++) {
                    auto* argType = resolveExprType(args[i]);
                    if (!argType) continue;
                    unsigned pDims = 0;
                    // Build substitution map for parameter type resolution
                    std::unordered_map<std::string, const TypeInfo*> subst;
                    for (size_t j = 0; j < funcIt->second.typeParams.size() && j < typeArgs.size(); j++)
                        subst[funcIt->second.typeParams[j]] = typeArgs[j];
                    auto* paramType = resolveTypeSpecWithSubst(params[i]->typeSpec(), subst, pDims);
                    if (paramType && !isAssignable(paramType, argType))
                        error(args[i], "type mismatch: expected '" + paramType->name +
                                       "', got '" + argType->name + "'");
                }
            }

        // Instantiate (type-check the body with concrete types)
        return instantiateGenericFunc(funcName, funcIt->second, typeArgs, expr);
    }

    // ── Generic static method call: Node<int32>::create(42) ─────────
    if (auto* gsmc = dynamic_cast<LucisParser::GenericStaticMethodCallExprContext*>(expr)) {
        auto ids = gsmc->IDENTIFIER();
        auto structBaseName = ids[0]->getText();
        auto methodName = ids[1]->getText();
        auto typeParamSpecs = gsmc->typeSpec();

        // Resolve type arguments
        std::vector<const TypeInfo*> typeArgs;
        for (auto* ts : typeParamSpecs) {
            unsigned dims = 0;
            auto* argTI = resolveTypeSpecInContext(ts, dims);
            if (!argTI) return nullptr;
            typeArgs.push_back(argTI);
        }

        auto enumIt = genericEnumTemplates_.find(structBaseName);
        if (enumIt != genericEnumTemplates_.end()) {
            auto* enumType = instantiateGenericEnum(structBaseName, enumIt->second, typeArgs, expr);
            if (!enumType) return nullptr;

            auto* variantInfo = findEnumVariantInfo(enumType, methodName);
            if (!variantInfo) {
                error(expr, "enum '" + enumType->name + "' has no variant '" + methodName +
                             "'; enums do not support static methods via 'extend'");
                return enumType;
            }
            if (variantInfo->payloadKind == EnumPayloadKind::Named) {
                error(expr, "variant '" + enumType->name + "::" + methodName +
                             "' uses named payload; use braces instead of parentheses");
                return enumType;
            }

            std::vector<const TypeInfo*> argTypes;
            if (auto* argList = gsmc->argList()) {
                for (auto* argExpr : argList->expression())
                    argTypes.push_back(resolveExprType(argExpr));
            }

            if (argTypes.size() != variantInfo->payloadFields.size()) {
                error(expr, "variant '" + enumType->name + "::" + methodName +
                             "' expects " + std::to_string(variantInfo->payloadFields.size()) +
                             " argument(s), got " + std::to_string(argTypes.size()));
                return enumType;
            }
            auto args = gsmc->argList() ? gsmc->argList()->expression()
                                        : std::vector<LucisParser::ExpressionContext*>{};
            for (size_t i = 0; i < argTypes.size(); i++) {
                auto* expected = variantInfo->payloadFields[i].typeInfo;
                bool literalToVecOk = false;
                if (expected && expected->kind == TypeKind::Extended &&
                    expected->extendedKind == "Vec" &&
                    i < args.size() &&
                    dynamic_cast<LucisParser::ArrayLitExprContext*>(args[i])) {
                    literalToVecOk = true;
                    auto* arr = dynamic_cast<LucisParser::ArrayLitExprContext*>(args[i]);
                    for (auto* e : arr->expression()) {
                        auto* et = resolveExprType(e);
                        if (et && expected->elementType &&
                            !isAssignable(expected->elementType, et)) {
                            error(e, "element type mismatch: expected '" +
                                         expected->elementType->name + "', got '" +
                                         et->name + "'");
                            literalToVecOk = false;
                        }
                    }
                }
                if (argTypes[i] && expected &&
                    !literalToVecOk &&
                    !isAssignable(expected, argTypes[i])) {
                    error(expr, "variant '" + enumType->name + "::" + methodName +
                                 "' argument " + std::to_string(i + 1) +
                                 " type mismatch: expected '" + expected->name +
                                 "', got '" + argTypes[i]->name + "'");
                }
            }
            return enumType;
        }

        // Find the generic struct template
        auto structIt = genericStructTemplates_.find(structBaseName);
        if (structIt == genericStructTemplates_.end()) {
            error(expr, "'" + structBaseName + "' is not a generic struct");
            if (auto* argList = gsmc->argList())
                for (auto* a : argList->expression()) resolveExprType(a);
            return nullptr;
        }

        // Instantiate the struct (registers methods)
        auto* instanceTI = instantiateGenericStruct(structBaseName, structIt->second,
                                                     typeArgs, expr);
        if (!instanceTI) return nullptr;

        auto mangledName = mangleGenericName(structBaseName, typeArgs);

        // Find the method in the instantiated struct (walks parent chain)
        auto* sm = findMethodInChain(instanceTI, methodName);
        if (sm && sm->isStatic) {
            if (auto* argList = gsmc->argList())
                for (auto* a : argList->expression()) resolveExprType(a);
            return sm->returnType;
        }
        auto* tm = findMethodInTraits(instanceTI, methodName);
        if (tm && tm->isStatic) {
            if (auto* argList = gsmc->argList())
                for (auto* a : argList->expression()) resolveExprType(a);
            return tm->returnType;
        }

        error(expr, "struct '" + mangledName + "' has no static method '" + methodName + "'");
        return nullptr;
    }

    // ── Qualified generic call: lucis::unsafe::va_arg<int32>(ptr) ──────
    if (auto* gqfc = dynamic_cast<LucisParser::GenericQualifiedFnCallExprContext*>(expr)) {
        auto ids = gqfc->IDENTIFIER();
        if (ids.size() < 2) {
            error(expr, "invalid qualified generic call");
            return nullptr;
        }

        // Resolve type arguments
        auto typeParamSpecs = gqfc->typeSpec();
        std::vector<const TypeInfo*> typeArgs;
        for (auto* ts : typeParamSpecs) {
            unsigned dims = 0;
            auto* argTI = resolveTypeSpecInContext(ts, dims);
            if (!argTI) return nullptr;
            typeArgs.push_back(argTI);
        }

        // ── Intrinsic check: lucis::unsafe::va_arg<T>(ptr) ─────────
        if (IntrinsicRegistry::isIntrinsicPrefix(ids[0]->getText())) {
            // Build path: all identifiers except the last = namespace path
            std::vector<std::string> idTexts;
            for (auto* id : ids) idTexts.push_back(id->getText());

            std::string ns, funcName;
            if (!IntrinsicRegistry::parseIntrinsicPath(idTexts, ns, funcName)) {
                error(expr, "invalid intrinsic qualified call");
                if (auto* argList = gqfc->argList())
                    for (auto* a : argList->expression()) resolveExprType(a);
                return nullptr;
            }

            auto* intrinsic = intrinsicRegistry_.lookup(ns, funcName);
            if (!intrinsic || !intrinsic->isGeneric) {
                error(expr, "unknown generic intrinsic '" + ns + "::" + funcName + "'");
                if (auto* argList = gqfc->argList())
                    for (auto* a : argList->expression()) resolveExprType(a);
                return nullptr;
            }

            // Validate type args: must have at least 1
            if (typeArgs.empty()) {
                error(expr, "generic intrinsic '" + ns + "::" + funcName +
                      "' expects at least 1 type argument, got 0");
                if (auto* argList = gqfc->argList())
                    for (auto* a : argList->expression()) resolveExprType(a);
                return nullptr;
            }

            // Validate argument count
            auto argExprs = gqfc->argList()
                ? gqfc->argList()->expression()
                : std::vector<LucisParser::ExpressionContext*>{};

            size_t fixedCount = intrinsic->params.size();
            if (intrinsic->isVariadic) {
                if (argExprs.size() < fixedCount) {
                    error(expr, "generic intrinsic '" + ns + "::" + funcName +
                          "' expects at least " + std::to_string(fixedCount) +
                          " argument(s), got " + std::to_string(argExprs.size()));
                    return nullptr;
                }
            } else if (argExprs.size() != fixedCount) {
                error(expr, "generic intrinsic '" + ns + "::" + funcName + "' expects " +
                      std::to_string(fixedCount) + " argument(s), got " +
                      std::to_string(argExprs.size()));
                return nullptr;
            }

            // Validate argument types (skip _any)
            for (size_t i = 0; i < std::min(argExprs.size(), fixedCount); i++) {
                auto* argType = resolveExprType(argExprs[i]);
                auto& expected = intrinsic->params[i].type;
                if (expected != "_any" && argType) {
                    auto* expectedType = typeRegistry_.lookup(expected);
                    if (!canWidenTo(argType, expectedType)) {
                        error(argExprs[i], "generic intrinsic '" + ns + "::" + funcName +
                              "' argument " + std::to_string(i + 1) + ": expected '" +
                              expected + "', got '" + argType->name + "'");
                    }
                }
            }

            // Return type: "_any" resolves to the designated type arg
            if (intrinsic->returnType == "_any" && !typeArgs.empty()) {
                size_t idx = intrinsic->returnTypeArgIndex;
                if (idx < typeArgs.size())
                    return typeArgs[idx];
            }
            return resolveBuiltinReturnType(intrinsic->returnType);
        }

        // Not an intrinsic — fall through to generic struct lookup
        error(expr, "qualified generic call is only supported for intrinsics");
        if (auto* argList = gqfc->argList())
            for (auto* a : argList->expression()) resolveExprType(a);
        return nullptr;
    }

    // ── Generic struct literal: Node<int32> { value: 42, next: null } ─
    if (auto* gsl = dynamic_cast<LucisParser::GenericStructLitExprContext*>(expr)) {
        auto baseName = gsl->IDENTIFIER(0)->getText();
        auto typeParamSpecs = gsl->typeSpec();

        // Resolve type arguments
        std::vector<const TypeInfo*> typeArgs;
        for (auto* ts : typeParamSpecs) {
            unsigned dims = 0;
            auto* argTI = resolveTypeSpecInContext(ts, dims);
            if (!argTI) return nullptr;
            typeArgs.push_back(argTI);
        }

        // Find template and instantiate
        auto structIt = genericStructTemplates_.find(baseName);
        auto unionIt = genericUnionTemplates_.find(baseName);
        if (structIt == genericStructTemplates_.end() &&
            unionIt == genericUnionTemplates_.end()) {
            error(expr, "'" + baseName + "' is not a generic struct or union");
            return nullptr;
        }

        const TypeInfo* instanceTI = nullptr;
        if (structIt != genericStructTemplates_.end()) {
            instanceTI = instantiateGenericStruct(baseName, structIt->second,
                                                  typeArgs, expr);
        } else {
            instanceTI = instantiateGenericUnion(baseName, unionIt->second,
                                                 typeArgs, expr);
        }
        if (!instanceTI) return nullptr;

        // Validate field initializers
        auto mangledName = mangleGenericName(baseName, typeArgs);
        auto ids = gsl->IDENTIFIER();
        auto fieldExprs = gsl->expression();
        std::unordered_map<std::string, const TypeInfo*> fieldTypeMap;
        for (auto& f : instanceTI->fields)
            fieldTypeMap[f.name] = f.typeInfo;

        if (instanceTI->kind == TypeKind::Union && fieldExprs.size() != 1) {
            error(expr, "union '" + mangledName +
                         "' literal must initialize exactly 1 field, got " +
                         std::to_string(fieldExprs.size()));
        }

        // ids[0] is the type name, ids[1..n] are field names
        for (size_t i = 0; i < fieldExprs.size(); i++) {
            auto fieldName = ids[i + 1]->getText();
            auto* exprType = resolveExprType(fieldExprs[i]);
            auto ftIt = fieldTypeMap.find(fieldName);
            if (ftIt == fieldTypeMap.end()) {
                error(expr, "unknown field '" + fieldName + "' in '" + mangledName + "'");
            } else if (exprType && !isAssignable(ftIt->second, exprType)) {
                error(expr, "field '" + fieldName + "' type mismatch: expected '" +
                           ftIt->second->name + "', got '" + exprType->name + "'");
            }
        }

        return instanceTI;
    }

    // ── Generic struct positional init: Node<int32> { expr, expr, ... } ─
    if (auto* gspi = dynamic_cast<LucisParser::GenericStructPosInitExprContext*>(expr)) {
        auto baseName = gspi->IDENTIFIER()->getText();
        auto typeParamSpecs = gspi->typeSpec();

        std::vector<const TypeInfo*> typeArgs;
        for (auto* ts : typeParamSpecs) {
            unsigned dims = 0;
            auto* argTI = resolveTypeSpecInContext(ts, dims);
            if (!argTI) return nullptr;
            typeArgs.push_back(argTI);
        }

        auto structIt = genericStructTemplates_.find(baseName);
        auto unionIt = genericUnionTemplates_.find(baseName);
        if (structIt == genericStructTemplates_.end() &&
            unionIt == genericUnionTemplates_.end()) {
            error(expr, "'" + baseName + "' is not a generic struct or union");
            return nullptr;
        }

        const TypeInfo* instanceTI = nullptr;
        if (structIt != genericStructTemplates_.end()) {
            instanceTI = instantiateGenericStruct(baseName, structIt->second,
                                                  typeArgs, expr);
        } else {
            instanceTI = instantiateGenericUnion(baseName, unionIt->second,
                                                 typeArgs, expr);
        }
        if (!instanceTI) return nullptr;

        auto mangledName = mangleGenericName(baseName, typeArgs);
        auto fieldExprs = gspi->expression();

        if (instanceTI->kind == TypeKind::Union && fieldExprs.size() != 1) {
            error(expr, "union '" + mangledName +
                         "' literal must initialize exactly 1 field, got " +
                         std::to_string(fieldExprs.size()));
        }

        for (size_t i = 0; i < fieldExprs.size(); i++) {
            if (i >= instanceTI->fields.size()) break;
            auto& sf = instanceTI->fields[i];
            auto* exprType = resolveExprType(fieldExprs[i]);
            if (exprType && sf.typeInfo &&
                !isAssignable(sf.typeInfo, exprType)) {
                error(expr, "field '" + sf.name + "' type mismatch: expected '" +
                           sf.typeInfo->name + "', got '" + exprType->name + "'");
            }
        }

        return instanceTI;
    }



    // ── Generic enum named variant literal: Enum<T>::Variant { ... } ─────
    if (auto* genv = dynamic_cast<LucisParser::GenericEnumNamedVariantExprContext*>(expr)) {
        auto ids = genv->IDENTIFIER();
        auto baseName = ids[0]->getText();
        auto variantName = ids[1]->getText();

        std::vector<const TypeInfo*> typeArgs;
        for (auto* ts : genv->typeSpec()) {
            unsigned dims = 0;
            auto* argTI = resolveTypeSpecInContext(ts, dims);
            if (!argTI) return nullptr;
            typeArgs.push_back(argTI);
        }

        auto enumIt = genericEnumTemplates_.find(baseName);
        if (enumIt == genericEnumTemplates_.end()) {
            error(expr, "'" + baseName + "' is not a generic enum");
            return nullptr;
        }

        auto* enumType = instantiateGenericEnum(baseName, enumIt->second, typeArgs, expr);
        if (!enumType) return nullptr;

        auto* variantInfo = findEnumVariantInfo(enumType, variantName);
        if (!variantInfo) {
            error(expr, "enum '" + enumType->name + "' has no variant '" + variantName + "'");
            return enumType;
        }
        if (variantInfo->payloadKind != EnumPayloadKind::Named) {
            error(expr, "variant '" + enumType->name + "::" + variantName +
                         "' does not use named payload");
            return enumType;
        }

        auto fieldExprs = genv->expression();
        std::unordered_set<std::string> provided;
        for (size_t i = 0; i < fieldExprs.size(); i++) {
            auto fieldName = ids[i + 2]->getText();
            if (!provided.insert(fieldName).second) {
                error(expr, "duplicate payload field '" + fieldName +
                             "' in variant '" + enumType->name + "::" + variantName + "'");
                continue;
            }

            const FieldInfo* expectedField = nullptr;
            for (const auto& field : variantInfo->payloadFields) {
                if (field.name == fieldName) {
                    expectedField = &field;
                    break;
                }
            }
            if (!expectedField) {
                error(expr, "unknown payload field '" + fieldName +
                             "' in variant '" + enumType->name + "::" + variantName + "'");
                continue;
            }

            auto* exprType = resolveExprType(fieldExprs[i]);
            if (exprType && expectedField->typeInfo &&
                !isAssignable(expectedField->typeInfo, exprType)) {
                error(expr, "payload field '" + fieldName +
                             "' type mismatch: expected '" + expectedField->typeInfo->name +
                             "', got '" + exprType->name + "'");
            }
        }

        if (provided.size() != variantInfo->payloadFields.size()) {
            for (const auto& field : variantInfo->payloadFields) {
                if (!provided.count(field.name)) {
                    error(expr, "missing payload field '" + field.name +
                                 "' in variant '" + enumType->name + "::" + variantName + "'");
                }
            }
        }

        return enumType;
    }

    // ── Generic enum positional variant: Enum<T>::Variant { expr, ... } ─
    if (auto* gepv = dynamic_cast<LucisParser::GenericEnumPosVariantExprContext*>(expr)) {
        auto ids = gepv->IDENTIFIER();
        auto baseName = ids[0]->getText();
        auto variantName = ids[1]->getText();

        std::vector<const TypeInfo*> typeArgs;
        for (auto* ts : gepv->typeSpec()) {
            unsigned dims = 0;
            auto* argTI = resolveTypeSpecInContext(ts, dims);
            if (!argTI) return nullptr;
            typeArgs.push_back(argTI);
        }

        auto enumIt = genericEnumTemplates_.find(baseName);
        if (enumIt == genericEnumTemplates_.end()) {
            error(expr, "'" + baseName + "' is not a generic enum");
            return nullptr;
        }

        auto* enumType = instantiateGenericEnum(baseName, enumIt->second, typeArgs, expr);
        if (!enumType) return nullptr;

        auto* variantInfo = findEnumVariantInfo(enumType, variantName);
        if (!variantInfo) {
            error(expr, "enum '" + enumType->name + "' has no variant '" + variantName + "'");
            return enumType;
        }

        auto fieldExprs = gepv->expression();
        if (fieldExprs.size() != variantInfo->payloadFields.size()) {
            error(expr, "variant '" + enumType->name + "::" + variantName +
                         "' expects " + std::to_string(variantInfo->payloadFields.size()) +
                         " payload fields, got " + std::to_string(fieldExprs.size()));
        }

        for (size_t i = 0; i < fieldExprs.size(); i++) {
            if (i >= variantInfo->payloadFields.size()) break;
            const auto& expectedField = variantInfo->payloadFields[i];
            auto* exprType = resolveExprType(fieldExprs[i]);
            if (exprType && expectedField.typeInfo &&
                !isAssignable(expectedField.typeInfo, exprType)) {
                error(expr, "payload field '" + expectedField.name +
                             "' type mismatch: expected '" + expectedField.typeInfo->name +
                             "', got '" + exprType->name + "'");
            }
        }

        return enumType;
    }

    // ── Enum access: Enum::Variant ──────────────────────────────────
    // Also supports module member access: `module::CONST` / `module::fn`
    // when the first identifier is an imported module.
    if (auto* ea = dynamic_cast<LucisParser::EnumAccessExprContext*>(expr)) {
        auto ids = ea->IDENTIFIER();
        auto enumName = ids[0]->getText();
        auto variant = ids[1]->getText();

        auto enumIt = userImports_.find(enumName);
        if (enumIt != userImports_.end()) {
            auto exported = importedSymbolName(variant);
            if (moduleRegistry_) {
                auto* sym = moduleRegistry_->findSymbol(enumIt->second, exported);
                if (sym && sym->kind == ExportedSymbol::Constant && sym->decl) {
                    unsigned memberDims = 0;
                    return resolveTypeSpec(
                        static_cast<LucisParser::ConstDeclaratorContext*>(
                            sym->decl)->typeSpec(),
                        memberDims);
                }
            }
        }

        auto* enumType = typeRegistry_.lookup(enumName);
        if (!enumType && userImports_.count(enumName)) {
            // Imported enum under a local alias.
            auto exported = importedSymbolName(enumName);
            enumType = typeRegistry_.lookup(exported);
        }
        if (!enumType) {
            error(expr, "unknown enum type '" + enumName + "'");
            return nullptr;
        }
        if (enumType->kind != TypeKind::Enum) {
            error(expr, "'" + enumName + "' is not an enum type");
            return nullptr;
        }

        auto* variantInfo = findEnumVariantInfo(enumType, variant);
        if (!variantInfo) {
            error(expr, "enum '" + enumName +
                             "' has no variant '" + variant + "'");
        } else if (!variantInfo->payloadFields.empty()) {
            error(expr, "variant '" + enumName + "::" + variant +
                         "' requires payload construction");
        }
        return enumType;
    }

    // ── Generic enum access: Enum<T>::Variant ───────────────────────
    if (auto* gea = dynamic_cast<LucisParser::GenericEnumAccessExprContext*>(expr)) {
        auto ids = gea->IDENTIFIER();
        auto baseName = ids[0]->getText();
        auto variantName = ids[1]->getText();

        std::vector<const TypeInfo*> typeArgs;
        for (auto* ts : gea->typeSpec()) {
            unsigned dims = 0;
            auto* argTI = resolveTypeSpecInContext(ts, dims);
            if (!argTI) return nullptr;
            typeArgs.push_back(argTI);
        }

        auto enumIt = genericEnumTemplates_.find(baseName);
        if (enumIt == genericEnumTemplates_.end()) {
            error(expr, "'" + baseName + "' is not a generic enum");
            return nullptr;
        }

        auto* enumType = instantiateGenericEnum(baseName, enumIt->second, typeArgs, expr);
        if (!enumType) return nullptr;

        auto* variantInfo = findEnumVariantInfo(enumType, variantName);
        if (!variantInfo) {
            error(expr, "enum '" + enumType->name + "' has no variant '" + variantName + "'");
        } else if (!variantInfo->payloadFields.empty()) {
            error(expr, "variant '" + enumType->name + "::" + variantName +
                         "' requires payload construction");
        }
        return enumType;
    }

    // ── Address-of: &var ────────────────────────────────────────────
    if (auto* addr = dynamic_cast<LucisParser::AddrOfExprContext*>(expr)) {
        auto* innerType = resolveExprType(addr->expression());
        if (!innerType) return nullptr;
        // Mark the variable as used if it's a simple identifier
        if (auto* ident = dynamic_cast<LucisParser::IdentExprContext*>(addr->expression())) {
            auto it = locals_.find(ident->IDENTIFIER()->getText());
            if (it != locals_.end()) it->second.used = true;
        }
        return getPointerType(innerType);
    }

    // ── Dereference: *expr ──────────────────────────────────────────
    if (auto* deref = dynamic_cast<LucisParser::DerefExprContext*>(expr)) {
        auto* operand = resolveExprType(deref->expression());
        if (operand && operand->kind != TypeKind::Pointer) {
            error(expr, "cannot dereference non-pointer type '" +
                             operand->name + "'");
            return nullptr;
        }
        if (operand && operand->pointeeType)
            return operand->pointeeType;
        return nullptr;
    }

    // ── Bang-init: vec!{ 1, 2, 3 }, map!{ "a": 1 }, set!{ 1, 2, 3 } ─
    if (auto* bi = dynamic_cast<LucisParser::BangInitExprContext*>(expr)) {
        // Determine the base type name
        std::string baseName;
        if (bi->VEC())       baseName = "Vec";
        else if (bi->MAP())  baseName = "Map";
        else if (bi->SET())  baseName = "Set";
        else if (bi->IDENTIFIER()) baseName = bi->IDENTIFIER()->getText();

        if (baseName.empty()) {
            error(expr, "bang-initializer requires a type name before '!'");
            ;
            return nullptr;
        }

        // Look up the extended type descriptor
        auto* extDesc = extTypeRegistry_.lookup(baseName);
        if (!extDesc) {
            error(expr, "type '" + baseName + "' does not support bang-initialization");
            ;
            return nullptr;
        }

        auto entries = bi->bangEntry();

        if (extDesc->genericArity == 1) {
            // Vec<T> or Set<T> — each entry is a value
            const TypeInfo* elemType = nullptr;

            if (!entries.empty()) {
                auto* firstEntry = entries[0];
                if (firstEntry->COLON()) {
                    error(firstEntry, "'" + baseName + "' initializer does not accept key:value pairs");
                    ;
                    return nullptr;
                }
                elemType = resolveExprType(firstEntry->expression(0));
                if (!elemType) { ; return nullptr; }

                for (size_t i = 1; i < entries.size(); i++) {
                    auto* entry = entries[i];
                    if (entry->COLON()) {
                        error(entry, "'" + baseName + "' initializer does not accept key:value pairs");
                        ;
                        return nullptr;
                    }
                    auto* et = resolveExprType(entry->expression(0));
                    if (et && !isAssignable(elemType, et))
                        error(entry, "element type mismatch: expected '" +
                                     elemType->name + "', got '" + et->name + "'");
                }
            }

            if (!elemType) {
                // Empty initializer — return base type for error detection
                auto ti = std::make_unique<TypeInfo>();
                ti->name = baseName;
                ti->kind = TypeKind::Extended;
                ti->bitWidth = 0;
                ti->isSigned = false;
                ti->extendedKind = baseName;
                const TypeInfo* raw = ti.get();
                dynamicTypes_.push_back(std::move(ti));
                ;
                return raw;
            }

            std::string fullName = baseName + "<" + elemType->name + ">";
            for (auto& dt : dynamicTypes_) {
                if (dt->name == fullName) { ; return dt.get(); }
            }
            auto ti = std::make_unique<TypeInfo>();
            ti->name = fullName;
            ti->kind = TypeKind::Extended;
            ti->bitWidth = 0;
            ti->isSigned = false;
            ti->builtinSuffix = elemType->builtinSuffix;
            ti->elementType = elemType;
            ti->extendedKind = baseName;
            const TypeInfo* raw = ti.get();
            dynamicTypes_.push_back(std::move(ti));
            ;
            return raw;

        } else if (extDesc->genericArity == 2) {
            // Map<K,V> — each entry is a key:value pair
            const TypeInfo* keyType = nullptr;
            const TypeInfo* valType = nullptr;

            for (size_t i = 0; i < entries.size(); i++) {
                auto* entry = entries[i];
                if (!entry->COLON()) {
                    error(entry, "'" + baseName + "' initializer requires key:value pairs");
                    ;
                    return nullptr;
                }
                auto* kt = resolveExprType(entry->expression(0));
                auto* vt = resolveExprType(entry->expression(1));
                if (!kt || !vt) { ; return nullptr; }

                if (i == 0) {
                    keyType = kt;
                    valType = vt;
                } else {
                    if (!isAssignable(keyType, kt))
                        error(entry, "key type mismatch: expected '" +
                                     keyType->name + "', got '" + kt->name + "'");
                    if (!isAssignable(valType, vt))
                        error(entry, "value type mismatch: expected '" +
                                     valType->name + "', got '" + vt->name + "'");
                }
            }

            if (!keyType || !valType) {
                // Empty Map — return base type for error detection
                auto ti = std::make_unique<TypeInfo>();
                ti->name = baseName;
                ti->kind = TypeKind::Extended;
                ti->bitWidth = 0;
                ti->isSigned = false;
                ti->extendedKind = baseName;
                const TypeInfo* raw = ti.get();
                dynamicTypes_.push_back(std::move(ti));
                ;
                return raw;
            }

            std::string fullName = baseName + "<" + keyType->name + ", " + valType->name + ">";
            for (auto& dt : dynamicTypes_) {
                if (dt->name == fullName) { ; return dt.get(); }
            }
            auto ti = std::make_unique<TypeInfo>();
            ti->name = fullName;
            ti->kind = TypeKind::Extended;
            ti->bitWidth = 0;
            ti->isSigned = false;
            ti->builtinSuffix = keyType->builtinSuffix + "_" + valType->builtinSuffix;
            ti->keyType = keyType;
            ti->valueType = valType;
            ti->extendedKind = baseName;
            const TypeInfo* raw = ti.get();
            dynamicTypes_.push_back(std::move(ti));
            ;
            return raw;
        } else {
            error(expr, "type '" + baseName + "' is not a recognized collection type");
            ;
            return nullptr;
        }
    }

    // ── Array literal: [expr, expr, ...] ────────────────────────────
    if (auto* arr = dynamic_cast<LucisParser::ArrayLitExprContext*>(expr)) {
        auto elements = arr->expression();
        if (elements.empty()) return nullptr;

        auto* firstType = resolveExprType(elements[0]);
        for (size_t i = 1; i < elements.size(); i++) {
            auto* elemType = resolveExprType(elements[i]);
            if (firstType && elemType && !isAssignable(firstType, elemType)) {
                error(expr, "array element type mismatch: expected '" +
                                 firstType->name + "', got '" +
                                 elemType->name + "'");
            }
            // Nested literals must agree on inner length — a ragged array
            // (different inner sizes) would otherwise generate invalid IR.
            auto* innerA =
                dynamic_cast<LucisParser::ArrayLitExprContext*>(elements[0]);
            auto* innerB =
                dynamic_cast<LucisParser::ArrayLitExprContext*>(elements[i]);
            if (innerA && innerB &&
                innerA->expression().size() != innerB->expression().size()) {
                error(expr, "nested array literal size mismatch: expected " +
                                 std::to_string(innerA->expression().size()) +
                                 ", got " +
                                 std::to_string(innerB->expression().size()));
            }
        }
        return firstType;
    }

    // ── List comprehension: [expr | for type x in iterable if cond] ─
    if (auto* lc = dynamic_cast<LucisParser::ListCompExprContext*>(expr)) {
        // Register the loop variable temporarily
        unsigned varDims = 0;
        auto* varType = resolveTypeSpec(lc->typeSpec(), varDims);
        auto  varName = lc->IDENTIFIER()->getText();

        auto prev = locals_.find(varName);
        auto hadPrev = prev != locals_.end();
        VarInfo prevInfo;
        if (hadPrev) prevInfo = prev->second;

        if (varType)
            locals_[varName] = { varType, varDims, {}, true, true, nullptr };

        // Resolve iterable expression
        resolveExprType(lc->expression(1));

        // Resolve optional filter
        auto exprs = lc->expression();
        if (exprs.size() > 2)
            resolveExprType(exprs[2]);

        // Resolve the value expression — its type is the element type
        auto* elemType = resolveExprType(lc->expression(0));

        // Restore scope
        if (hadPrev)
            locals_[varName] = prevInfo;
        else
            locals_.erase(varName);

        ;
        return elemType;
    }

    ;
    return nullptr;
}

// ═══════════════════════════════════════════════════════════════════════
//  Top-level declaration checks
// ═══════════════════════════════════════════════════════════════════════



unsigned Checker::resolveArrayLitDims(LucisParser::ArrayLitExprContext* arrLit) {
    if (!arrLit || arrLit->expression().empty()) return 1;
    unsigned maxInner = 0;
    for (auto* elem : arrLit->expression()) {
        if (auto* inner = dynamic_cast<LucisParser::ArrayLitExprContext*>(elem)) {
            unsigned innerDims = resolveArrayLitDims(inner);
            if (innerDims > maxInner) maxInner = innerDims;
        }
    }
    if (maxInner > 0)
        return maxInner + 1;
    return 1;
}



std::vector<unsigned> Checker::extractArraySizesFromSpec(LucisParser::TypeSpecContext* spec) {
    std::vector<unsigned> sizes;
    while (spec && spec->LBRACKET()) {
        if (spec->INT_LIT()) {
            sizes.push_back(static_cast<unsigned>(std::stoul(spec->INT_LIT()->getText())));
        } else if (spec->IDENTIFIER()) {
            auto identName = spec->IDENTIFIER()->getText();
            auto it = compileTimeValues_.find(identName);
            if (it != compileTimeValues_.end() && it->second.kind() == ComptimeValue::Kind::Int) {
                sizes.push_back(static_cast<unsigned>(it->second.asInt()));
            }
        }
        spec = spec->typeSpec().empty() ? nullptr : spec->typeSpec(0);
    }
    return sizes;
}



unsigned Checker::resolveExprArrayDims(LucisParser::ExpressionContext* expr) {
    if (!expr) return 0;
    if (auto* id = dynamic_cast<LucisParser::IdentExprContext*>(expr)) {
        auto it = locals_.find(id->IDENTIFIER()->getText());
        if (it != locals_.end())
            return it->second.arrayDims;
    }
    if (auto* idx = dynamic_cast<LucisParser::IndexExprContext*>(expr)) {
        unsigned baseDims = resolveExprArrayDims(idx->expression(0));
        return baseDims > 0 ? baseDims - 1 : 0;
    }
    if (auto* mc = dynamic_cast<LucisParser::MethodCallExprContext*>(expr)) {
        unsigned baseDims = resolveExprArrayDims(mc->expression());
        if (baseDims == 0) return 0;

        auto methodName = mc->IDENTIFIER()->getText();
        auto* md = methodRegistry_.lookupArrayMethod(methodName);
        if (!md) return 0;

        if (md->returnType == "_self") return baseDims;
        if (md->returnType == "_elem") return baseDims > 0 ? baseDims - 1 : 0;
        return 0;
    }
    if (auto* paren = dynamic_cast<LucisParser::ParenExprContext*>(expr))
        return resolveExprArrayDims(paren->expression());
    if (auto* arrLit = dynamic_cast<LucisParser::ArrayLitExprContext*>(expr))
        return resolveArrayLitDims(arrLit);
    if (auto* deref = dynamic_cast<LucisParser::DerefExprContext*>(expr))
        return resolveExprArrayDims(deref->expression());
    if (auto* cu = dynamic_cast<LucisParser::CatchUnwrapExprContext*>(expr)) {
        auto* sourceType = resolveExprType(cu->expression());
        UnwrapCatchPatternInfo pattern;
        std::string reason;
        if (!classifyUnwrapCatchEnum(sourceType, pattern, reason))
            return 0;
        if (!pattern.okVariant || pattern.okVariant->payloadFields.empty())
            return 0;
        return pattern.okVariant->payloadFields[0].arrayDims;
    }
    if (auto* pe = dynamic_cast<LucisParser::PropagateExprContext*>(expr)) {
        auto* sourceType = resolveExprType(pe->expression());
        UnwrapCatchPatternInfo pattern;
        std::string reason;
        if (!classifyUnwrapCatchEnum(sourceType, pattern, reason))
            return 0;
        if (!pattern.okVariant || pattern.okVariant->payloadFields.empty())
            return 0;
        return pattern.okVariant->payloadFields[0].arrayDims;
    }
    if (auto* fa = dynamic_cast<LucisParser::FieldAccessExprContext*>(expr)) {
        auto* baseType = resolveExprType(fa->expression());
        if (baseType && baseType->kind == TypeKind::Pointer && baseType->pointeeType)
            baseType = baseType->pointeeType;
        if (baseType && (baseType->kind == TypeKind::Struct || baseType->kind == TypeKind::Union)) {
            auto fieldName = fa->IDENTIFIER()->getText();
            for (auto& field : baseType->fields) {
                if (field.name == fieldName)
                    return field.arrayDims;
            }
        }
        return 0;
    }
    if (dynamic_cast<LucisParser::ArrayLitExprContext*>(expr))
        return 1;
    return 0;
}



const TypeInfo* Checker::tryResolveQualifiedType(antlr4::ParserRuleContext* ctx,
                                                const std::string& first,
                                                const std::string& second) {
    // Try as module::type
    if (moduleRegistry_ && moduleRegistry_->hasModule(first)) {
        auto* sym = moduleRegistry_->findSymbol(first, second);
        if (sym) {
            if (auto* ti = typeRegistry_.lookup(second))
                return ti;
        }
    }

    // Try as enum_type::variant (e.g. Shape::Circle)
    if (auto* enumTI = typeRegistry_.lookup(first)) {
        if (enumTI->kind == TypeKind::Enum) {
            for (const auto& variant : enumTI->enumVariantInfos) {
                if (variant.name == second)
                    return enumTI;
            }
        }
    }

    error(ctx, "'" + first + "::" + second + "' is not a known type or enum variant");
    return nullptr;
}


