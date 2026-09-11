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



static bool typeSpecMentionsGenericParam(
    LucisParser::TypeSpecContext* typeSpec,
    const std::unordered_set<std::string>& genericParams) {
    if (!typeSpec) return false;

    if (!typeSpec->LBRACKET() && !typeSpec->STAR() && !typeSpec->LT() &&
        !typeSpec->VEC() && !typeSpec->MAP() && !typeSpec->SET() &&
        !typeSpec->TUPLE() && !typeSpec->AUTO() && !typeSpec->fnTypeSpec()) {
        return genericParams.count(typeSpec->getText()) > 0;
    }

    for (auto* inner : typeSpec->typeSpec()) {
        if (typeSpecMentionsGenericParam(inner, genericParams))
            return true;
    }
    return false;
}



std::string Checker::mangleGenericName(const std::string& baseName,
                                        const std::vector<const TypeInfo*>& typeArgs) {
    std::string name = baseName;
    for (auto* arg : typeArgs) {
        name += "__";
        // Replace characters that are not valid in identifiers
        for (char c : arg->name) {
            if (c == '<' || c == '>' || c == ' ' || c == ',')
                name += '_';
            else
                name += c;
        }
    }
    return name;
}



const TypeInfo* Checker::resolveTypeSpecWithSubst(
    LucisParser::TypeSpecContext* typeSpec,
    const std::unordered_map<std::string, const TypeInfo*>& subst,
    unsigned& arrayDims) {
    // If the typeSpec is a bare IDENTIFIER that matches a type param, substitute it
    if (!typeSpec->LBRACKET() && !typeSpec->STAR() && !typeSpec->LT() &&
        !typeSpec->VEC() && !typeSpec->MAP() && !typeSpec->SET() &&
        !typeSpec->TUPLE() && !typeSpec->AUTO() && !typeSpec->fnTypeSpec()) {
        auto name = typeSpec->getText();
        auto it = subst.find(name);
        if (it != subst.end()) {
            arrayDims = 0;
            return it->second;
        }
    }
    // For pointer types, substitute into the inner type
    if (typeSpec->STAR() && typeSpec->typeSpec().size() == 1) {
        unsigned innerDims = 0;
        auto* inner = resolveTypeSpecWithSubst(typeSpec->typeSpec(0), subst, innerDims);
        if (!inner) return nullptr;
        // Preserve pointer-to-array shape under generic substitution as well.
        arrayDims = innerDims;
        return getPointerType(inner);
    }
    // For array types, substitute into the element type
    if (typeSpec->LBRACKET() && typeSpec->typeSpec().size() >= 1) {
        arrayDims = 0;
        auto* outerSpec = typeSpec;
        while (outerSpec && outerSpec->LBRACKET()) {
            arrayDims++;
            outerSpec = outerSpec->typeSpec().empty() ? nullptr : outerSpec->typeSpec(0);
        }
        unsigned elemDims = 0;
        auto* elemType = resolveTypeSpecWithSubst(outerSpec, subst, elemDims);
        return elemType;
    }
    // For generic instantiations like Node<T> where T is a type param
    if (typeSpec->LT()) {
        std::string innerBaseName;
        if (typeSpec->VEC()) innerBaseName = "Vec";
        else if (typeSpec->MAP()) innerBaseName = "Map";
        else if (typeSpec->SET()) innerBaseName = "Set";
        else if (typeSpec->IDENTIFIER()) innerBaseName = typeSpec->IDENTIFIER()->getText();
        if (innerBaseName.empty())
            return resolveTypeSpec(typeSpec, arrayDims);

        // Resolve all type args with substitution
        auto typeArgSpecs = typeSpec->typeSpec();
        std::vector<const TypeInfo*> resolvedArgs;
        for (auto* argSpec : typeArgSpecs) {
            unsigned argDims = 0;
            auto* argTI = resolveTypeSpecWithSubst(argSpec, subst, argDims);
            if (!argTI) return nullptr;
            resolvedArgs.push_back(argTI);
        }

        // Check if it's a user-defined generic struct
        auto structIt = genericStructTemplates_.find(innerBaseName);
        if (structIt != genericStructTemplates_.end()) {
            arrayDims = 0;
            return instantiateGenericStruct(innerBaseName, structIt->second,
                                             resolvedArgs, typeSpec);
        }
        auto unionIt = genericUnionTemplates_.find(innerBaseName);
        if (unionIt != genericUnionTemplates_.end()) {
            arrayDims = 0;
            return instantiateGenericUnion(innerBaseName, unionIt->second,
                                           resolvedArgs, typeSpec);
        }
        auto enumIt = genericEnumTemplates_.find(innerBaseName);
        if (enumIt != genericEnumTemplates_.end()) {
            arrayDims = 0;
            return instantiateGenericEnum(innerBaseName, enumIt->second,
                                          resolvedArgs, typeSpec);
        }

        // Built-in/generic extended types (Vec/Map/Set/Task/etc.) with substituted args
        if (auto* extDesc = extTypeRegistry_.lookup(innerBaseName)) {
            if (extDesc->genericArity == 1) {
                if (resolvedArgs.size() != 1) {
                    error(typeSpec, "'" + innerBaseName + "' expects 1 type parameter, got " +
                                        std::to_string(resolvedArgs.size()));
                    return nullptr;
                }
                auto* elemType = resolvedArgs[0];
                if (!elemType) return nullptr;
                if (elemType->kind == TypeKind::Extended &&
                    (innerBaseName == "Vec" || innerBaseName == "Map" || innerBaseName == "Set")) {
                    error(typeSpec, "nested collection types are not supported: '" +
                                        innerBaseName + "<" + elemType->name + ">'");
                    return nullptr;
                }

                auto fullName = innerBaseName + "<" + elemType->name + ">";
                for (auto& dt : dynamicTypes_) {
                    if (dt->name == fullName) {
                        arrayDims = 0;
                        return dt.get();
                    }
                }
                auto ti = std::make_unique<TypeInfo>();
                ti->name = fullName;
                ti->kind = TypeKind::Extended;
                ti->bitWidth = 0;
                ti->isSigned = false;
                ti->builtinSuffix = elemType->builtinSuffix;
                ti->elementType = elemType;
                ti->extendedKind = innerBaseName;
                const TypeInfo* raw = ti.get();
                dynamicTypes_.push_back(std::move(ti));
                arrayDims = 0;
                return raw;
            }

            if (extDesc->genericArity == 2) {
                if (resolvedArgs.size() != 2) {
                    error(typeSpec, "'" + innerBaseName + "' expects 2 type parameters, got " +
                                        std::to_string(resolvedArgs.size()));
                    return nullptr;
                }
                auto* keyType = resolvedArgs[0];
                auto* valType = resolvedArgs[1];
                if (!keyType || !valType) return nullptr;
                if (keyType->kind == TypeKind::Extended || valType->kind == TypeKind::Extended) {
                    error(typeSpec, "nested collection types are not supported for '" +
                                        innerBaseName + "'");
                    return nullptr;
                }

                auto fullName = innerBaseName + "<" + keyType->name + ", " + valType->name + ">";
                for (auto& dt : dynamicTypes_) {
                    if (dt->name == fullName) {
                        arrayDims = 0;
                        return dt.get();
                    }
                }
                auto ti = std::make_unique<TypeInfo>();
                ti->name = fullName;
                ti->kind = TypeKind::Extended;
                ti->bitWidth = 0;
                ti->isSigned = false;
                ti->builtinSuffix = keyType->builtinSuffix + "_" + valType->builtinSuffix;
                ti->keyType = keyType;
                ti->valueType = valType;
                ti->extendedKind = innerBaseName;
                const TypeInfo* raw = ti.get();
                dynamicTypes_.push_back(std::move(ti));
                arrayDims = 0;
                return raw;
            }
        }

        // Otherwise fall back to normal resolution (built-in generics, etc.)
    }
    // Function type: fn(params) -> ret — resolve child types with substitution
    if (typeSpec->fnTypeSpec()) {
        auto* fnSpec = typeSpec->fnTypeSpec();
        auto specs = fnSpec->typeSpec();
        if (specs.empty()) return nullptr;
        unsigned retDims = 0;
        auto* retType = resolveTypeSpecWithSubst(specs.back(), subst, retDims);
        if (!retType) return nullptr;
        std::vector<const TypeInfo*> paramTypes;
        for (size_t i = 0; i + 1 < specs.size(); i++) {
            unsigned pDims = 0;
            auto* pType = resolveTypeSpecWithSubst(specs[i], subst, pDims);
            if (!pType) return nullptr;
            paramTypes.push_back(pType);
        }
        return makeFunctionType(retType, paramTypes);
    }
    // Default: normal resolution (no substitution needed for this node)
    return resolveTypeSpec(typeSpec, arrayDims);
}



const TypeInfo* Checker::resolveTypeParamConstraint(const std::string& constraintName,
                                                      antlr4::ParserRuleContext* ctx) {
    // Supported constraints: numeric, integer, float, bool, string, any
    // Return nullptr for "any".
    if (constraintName == "any" || constraintName == "Any") return nullptr;

    // Constraint aliases that are not concrete language types in TypeRegistry.
    static const TypeInfo numericConstraint = [] {
        TypeInfo ti;
        ti.name = "numeric";
        ti.kind = TypeKind::Integer;
        return ti;
    }();
    static const TypeInfo integerConstraint = [] {
        TypeInfo ti;
        ti.name = "integer";
        ti.kind = TypeKind::Integer;
        return ti;
    }();
    static const TypeInfo floatConstraint = [] {
        TypeInfo ti;
        ti.name = "float";
        ti.kind = TypeKind::Float;
        return ti;
    }();
    static const TypeInfo signedConstraint = [] {
        TypeInfo ti;
        ti.name = "signed";
        ti.kind = TypeKind::Integer;
        ti.isSigned = true;
        return ti;
    }();
    static const TypeInfo unsignedConstraint = [] {
        TypeInfo ti;
        ti.name = "unsigned";
        ti.kind = TypeKind::Integer;
        ti.isSigned = false;
        return ti;
    }();
    static const TypeInfo boolConstraint = [] {
        TypeInfo ti;
        ti.name = "bool";
        ti.kind = TypeKind::Bool;
        return ti;
    }();
    static const TypeInfo stringConstraint = [] {
        TypeInfo ti;
        ti.name = "string";
        ti.kind = TypeKind::String;
        return ti;
    }();
    if (constraintName == "numeric" || constraintName == "Numeric")
        return &numericConstraint;
    if (constraintName == "integer" || constraintName == "Integer")
        return &integerConstraint;
    if (constraintName == "float" || constraintName == "Float")
        return &floatConstraint;
    if (constraintName == "signed" || constraintName == "Signed")
        return &signedConstraint;
    if (constraintName == "unsigned" || constraintName == "Unsigned")
        return &unsignedConstraint;
    if (constraintName == "bool" || constraintName == "Bool")
        return &boolConstraint;
    if (constraintName == "string" || constraintName == "String")
        return &stringConstraint;

    // Try user-defined traits via TraitRegistry
    if (traitRegistry_.lookup(constraintName)) {
        static TypeInfo traitConstraint;
        traitConstraint.name = constraintName;
        traitConstraint.kind = TypeKind::Trait;
        return &traitConstraint;
    }

    // Cross-module lazy fallback: the constraint trait may live in a module
    // reached transitively (e.g. a generic imported from another module).
    if (moduleRegistry_) {
        ensureExternalTypeRegistered(constraintName);
        std::string canonical = importedSymbolName(constraintName);
        const TraitInfo* lazyTrait = traitRegistry_.lookup(constraintName);
        if (!lazyTrait && canonical != constraintName)
            lazyTrait = traitRegistry_.lookup(canonical);
        if (lazyTrait) {
            static TypeInfo traitConstraint;
            traitConstraint.name = lazyTrait->name;
            traitConstraint.kind = TypeKind::Trait;
            return &traitConstraint;
        }
    }

    auto* ti = typeRegistry_.lookup(constraintName);
    if (!ti) {
        error(ctx, "unknown type constraint '" + constraintName + "'");
        return nullptr;
    }
    return ti;
}



bool Checker::satisfiesConstraint(const TypeInfo* typeArg,
                                   const TypeInfo* constraint,
                                   const std::string& paramName,
                                   antlr4::ParserRuleContext* ctx) {
    if (!constraint) return true; // no constraint = any type is OK
    // Simple constraint check: typeArg must be the same kind or compatible
    if (constraint->name == "numeric") {
        if (typeArg->kind != TypeKind::Integer && typeArg->kind != TypeKind::Float) {
            error(ctx, "type argument for '" + paramName + "' must be numeric, got '" +
                       typeArg->name + "'");
            return false;
        }
    } else if (constraint->name == "integer") {
        if (typeArg->kind != TypeKind::Integer) {
            error(ctx, "type argument for '" + paramName + "' must be an integer, got '" +
                       typeArg->name + "'");
            return false;
        }
    } else if (constraint->name == "float") {
        if (typeArg->kind != TypeKind::Float) {
            error(ctx, "type argument for '" + paramName + "' must be a float, got '" +
                       typeArg->name + "'");
            return false;
        }
    } else if (constraint->name == "signed") {
        if (typeArg->kind != TypeKind::Integer || !typeArg->isSigned) {
            error(ctx, "type argument for '" + paramName + "' must be a signed integer, got '" +
                       typeArg->name + "'");
            return false;
        }
    } else if (constraint->name == "unsigned") {
        if (typeArg->kind != TypeKind::Integer || typeArg->isSigned) {
            error(ctx, "type argument for '" + paramName + "' must be an unsigned integer, got '" +
                       typeArg->name + "'");
            return false;
        }
    } else if (constraint->name == "bool") {
        if (typeArg->kind != TypeKind::Bool) {
            error(ctx, "type argument for '" + paramName + "' must be a bool, got '" +
                       typeArg->name + "'");
            return false;
        }
    } else if (constraint->name == "string") {
        if (typeArg->kind != TypeKind::String) {
            error(ctx, "type argument for '" + paramName + "' must be a string, got '" +
                       typeArg->name + "'");
            return false;
        }
    } else if (constraint->kind == TypeKind::Trait) {
        // Delegate to TraitRegistry for user-defined and built-in traits
        auto* trait = traitRegistry_.lookup(constraint->name);
        if (!trait) {
            error(ctx, "unknown trait constraint '" + constraint->name + "'");
            return false;
        }
        if (!typeSatisfiesTraitConstraint(typeArg, trait, paramName, ctx))
            return false;
    }
    return true;
}



const TypeInfo* Checker::instantiateGenericStruct(
    const std::string& baseName,
    const GenericStructTemplate& tmpl,
    const std::vector<const TypeInfo*>& typeArgs,
    antlr4::ParserRuleContext* ctx) {

    if (typeArgs.size() != tmpl.typeParams.size()) {
        error(ctx, "generic struct '" + baseName + "' expects " +
                   std::to_string(tmpl.typeParams.size()) + " type parameter(s), got " +
                   std::to_string(typeArgs.size()));
        return nullptr;
    }

    auto mangledName = mangleGenericName(baseName, typeArgs);

    // Check dynamic type cache first (already instantiated)
    for (auto& dt : dynamicTypes_) {
        if (dt->name == mangledName)
            return dt.get();
    }
    // Check type registry BEFORE cycle detection — allows self-referencing
    // structs (e.g. LinkedListNode<T> → *LinkedListNode<T>) to return the
    // partially-registered skeleton instead of erroring on the cycle.
    if (auto* existing = typeRegistry_.lookup(mangledName))
        return existing;

    // Cycle detection: skeleton is NOT yet in registry (first call), so
    // this is a true cycle with no fallback — should not happen since
    // skeleton is registered before field processing below.
    if (instantiatingGenerics_.count(mangledName)) {
        return typeRegistry_.lookup("int32");
    }
    instantiatingGenerics_.insert(mangledName);

    // Build substitution map: T → int32, K → string, etc.
    std::unordered_map<std::string, const TypeInfo*> subst;
    for (size_t i = 0; i < tmpl.typeParams.size(); i++) {
        // Validate constraint if present
        auto* tpCtx = tmpl.decl->typeParamList()->typeParam(i);
        if (tpCtx->COLON()) {
            // Has constraint: T: numeric
            auto constraintName = tpCtx->IDENTIFIER(1)->getText();
            auto* constraint = resolveTypeParamConstraint(constraintName, ctx);
            if (!satisfiesConstraint(typeArgs[i], constraint, tmpl.typeParams[i], ctx)) {
                instantiatingGenerics_.erase(mangledName);
                return nullptr;
            }
        }
        subst[tmpl.typeParams[i]] = typeArgs[i];
    }

    // Register skeleton for self-referencing fields (e.g., *Node<T> inside Node<T>)
    TypeInfo skeleton;
    skeleton.name = mangledName;
    skeleton.kind = TypeKind::Struct;
    skeleton.bitWidth = 0;
    skeleton.isSigned = false;
    skeleton.isGenericInstance = true;
    skeleton.genericBaseName = baseName;
    skeleton.typeParamNames = tmpl.typeParams;
    skeleton.typeArgs = typeArgs;
    skeleton.genericStructDecl = static_cast<antlr4::ParserRuleContext*>(tmpl.decl);
    typeRegistry_.registerType(skeleton);

    // Instantiate fields with substitution
    TypeInfo ti = skeleton;
    std::unordered_set<std::string> seen;
    for (auto* field : tmpl.decl->structField()) {
        unsigned fieldDims = 0;
        auto* fieldTI = resolveTypeSpecWithSubst(field->typeSpec(), subst, fieldDims);
        if (!fieldTI) {
            error(field, "cannot resolve field type in generic struct '" + baseName + "'");
            instantiatingGenerics_.erase(mangledName);
            return nullptr;
        }
        auto fieldName = field->IDENTIFIER()->getText();
        if (!seen.insert(fieldName).second) {
            error(field, "duplicate field '" + fieldName +
                         "' in generic struct '" + baseName + "'");
            instantiatingGenerics_.erase(mangledName);
            continue;
        }
        ti.fields.push_back({ fieldName, fieldTI, fieldDims });
    }

    // Register concrete type (updates the skeleton registered above)
    typeRegistry_.registerType(std::move(ti));
    const TypeInfo* result = typeRegistry_.lookup(mangledName);

    // Self resolves to the concrete generic type in extend blocks
    subst["Self"] = result;

    // Update self-referencing pointer type to point to the now-complete struct
    // Note: getPointerType stores pointers in dynamicTypes_, not typeRegistry_
    {
        auto ptrName = "*" + mangledName;
        for (auto& dt : dynamicTypes_) {
            if (dt->kind == TypeKind::Pointer && dt->name == ptrName &&
                dt->pointeeType != result) {
                dt->pointeeType = result;
            }
        }
    }

    // If there's a generic extend block for this struct, instantiate methods too
    auto extendIt = genericExtendTemplates_.find(baseName);
    if (extendIt != genericExtendTemplates_.end()) {
        auto& extTmpl = extendIt->second;
        for (auto* method : extTmpl.decl->extendMethod()) {
            // ── Operator overload in generic extend ──
            if (auto* opDecl = method->operatorDecl()) {
                auto internalName = opInternalName(opDecl->operatorName());

                unsigned retDims = 0;
                auto* retType = resolveTypeSpecWithSubst(opDecl->typeSpec(), subst, retDims);
                if (!retType) continue;

                OperatorInfo info;
                info.internalName = internalName;
                info.symbol = opSymbol(opDecl->operatorName());
                info.returnType = retType;
                info.isInstance = (opDecl->AMPERSAND() != nullptr);

                std::vector<LucisParser::ParamContext*> params;
                if (info.isInstance) {
                    params = opDecl->param();
                } else if (auto* pl = opDecl->paramList()) {
                    params = pl->param();
                }

                for (auto* param : params) {
                    unsigned pDims = 0;
                    auto* pType = resolveTypeSpecWithSubst(param->typeSpec(), subst, pDims);
                    if (!pType) continue;
                    info.paramTypes.push_back(pType);
                }

                operatorMethods_[mangledName].push_back(std::move(info));

                // ── Check operator body return types ──
                {
                    auto savedLocals = std::move(locals_);
                    auto savedSubst = std::move(activeTypeSubst_);
                    activeTypeSubst_ = subst;
                    if (info.isInstance) {
                        auto* ptrType = getPointerType(result);
                        locals_["self"] = {ptrType, 0, {}, true, true, nullptr};
                    }
                    for (auto* param : params) {
                        auto paramName = param->IDENTIFIER()->getText();
                        unsigned pDims = 0;
                        auto* pType = resolveTypeSpecWithSubst(param->typeSpec(), subst, pDims);
                        if (!pType) continue;
                        locals_[paramName] = {pType, pDims, {}, true, true, nullptr};
                    }
                    auto* prevRet = currentReturnType_;
                    currentReturnType_ = retType;
                    checkBlock(opDecl->block(), retType);
                    currentReturnType_ = prevRet;
                    activeTypeSubst_ = std::move(savedSubst);
                    locals_ = std::move(savedLocals);
                }
                continue;
            }

            auto methodName = method->name->getText();

            unsigned retDims = 0;
            auto* retType = resolveTypeSpecWithSubst(method->typeSpec(), subst, retDims);
            if (!retType) continue;

            StructMethodInfo info;
            info.name = methodName;
            info.returnType = retType;
            info.isStatic = (method->AMPERSAND() == nullptr);

            std::vector<LucisParser::ParamContext*> params;
            if (info.isStatic) {
                if (auto* pl = method->paramList())
                    params = pl->param();
            } else {
                params = method->param();
            }

            for (auto* param : params) {
                unsigned pDims = 0;
                auto* pType = resolveTypeSpecWithSubst(param->typeSpec(), subst, pDims);
                if (!pType) continue;
                info.paramTypes.push_back(pType);
            }

            structMethods_[mangledName].push_back(std::move(info));

            // ── Check method body return paths ──
            if (retType->kind != TypeKind::Void) {
                if (!blockAlwaysReturns(method->block())) {
                    error(method, "method '" + baseName + "." + methodName +
                                  "' must return a value of type '" + retType->name +
                                  "' on all code paths");
                }
            }
        }
    }

    // `impl Trait for Box<int32>` blocks only become addressable now that the
    // concrete instance exists.
    ensureGenericInstanceAttachments(baseName, mangledName);

    instantiatingGenerics_.erase(mangledName);
    return result;
}



const TypeInfo* Checker::instantiateGenericUnion(
    const std::string& baseName,
    const GenericUnionTemplate& tmpl,
    const std::vector<const TypeInfo*>& typeArgs,
    antlr4::ParserRuleContext* ctx) {

    if (typeArgs.size() != tmpl.typeParams.size()) {
        error(ctx, "generic union '" + baseName + "' expects " +
                   std::to_string(tmpl.typeParams.size()) + " type parameter(s), got " +
                   std::to_string(typeArgs.size()));
        return nullptr;
    }

    auto mangledName = mangleGenericName(baseName, typeArgs);

    for (auto& dt : dynamicTypes_) {
        if (dt->name == mangledName)
            return dt.get();
    }
    if (auto* existing = typeRegistry_.lookup(mangledName))
        return existing;

    if (instantiatingGenerics_.count(mangledName)) {
        error(ctx, "recursive generic instantiation detected for '" + mangledName + "'");
        return nullptr;
    }
    instantiatingGenerics_.insert(mangledName);

    std::unordered_map<std::string, const TypeInfo*> subst;
    for (size_t i = 0; i < tmpl.typeParams.size(); i++) {
        auto* tpCtx = tmpl.decl->typeParamList()->typeParam(i);
        if (tpCtx->COLON()) {
            auto constraintName = tpCtx->IDENTIFIER(1)->getText();
            auto* constraint = resolveTypeParamConstraint(constraintName, ctx);
            if (!satisfiesConstraint(typeArgs[i], constraint, tmpl.typeParams[i], ctx)) {
                instantiatingGenerics_.erase(mangledName);
                return nullptr;
            }
        }
        subst[tmpl.typeParams[i]] = typeArgs[i];
    }

    TypeInfo skeleton;
    skeleton.name = mangledName;
    skeleton.kind = TypeKind::Union;
    skeleton.bitWidth = 0;
    skeleton.isSigned = false;
    skeleton.isGenericInstance = true;
    skeleton.genericBaseName = baseName;
    skeleton.typeParamNames = tmpl.typeParams;
    skeleton.typeArgs = typeArgs;
    skeleton.genericStructDecl = static_cast<antlr4::ParserRuleContext*>(tmpl.decl);
    typeRegistry_.registerType(skeleton);

    TypeInfo ti = skeleton;
    std::unordered_set<std::string> seen;
    for (auto* field : tmpl.decl->unionField()) {
        unsigned fieldDims = 0;
        auto* fieldTI = resolveTypeSpecWithSubst(field->typeSpec(), subst, fieldDims);
        if (!fieldTI) {
            error(field, "cannot resolve field type in generic union '" + baseName + "'");
            instantiatingGenerics_.erase(mangledName);
            return nullptr;
        }
        auto fieldName = field->IDENTIFIER()->getText();
        if (!seen.insert(fieldName).second) {
            error(field, "duplicate field '" + fieldName +
                         "' in generic union '" + baseName + "'");
            instantiatingGenerics_.erase(mangledName);
            continue;
        }
        ti.fields.push_back({ fieldName, fieldTI, fieldDims });
    }

    typeRegistry_.registerType(std::move(ti));

    // `impl Trait for Gen<args>` blocks now have a concrete instance to bind to.
    ensureGenericInstanceAttachments(baseName, mangledName);

    instantiatingGenerics_.erase(mangledName);
    return typeRegistry_.lookup(mangledName);
}



const TypeInfo* Checker::instantiateGenericEnum(
    const std::string& baseName,
    const GenericEnumTemplate& tmpl,
    const std::vector<const TypeInfo*>& typeArgs,
    antlr4::ParserRuleContext* ctx) {

    if (typeArgs.size() != tmpl.typeParams.size()) {
        error(ctx, "generic enum '" + baseName + "' expects " +
                   std::to_string(tmpl.typeParams.size()) + " type parameter(s), got " +
                   std::to_string(typeArgs.size()));
        return nullptr;
    }

    auto mangledName = mangleGenericName(baseName, typeArgs);

    for (auto& dt : dynamicTypes_) {
        if (dt->name == mangledName)
            return dt.get();
    }
    if (auto* existing = typeRegistry_.lookup(mangledName))
        return existing;

    if (instantiatingGenerics_.count(mangledName)) {
        error(ctx, "recursive generic instantiation detected for '" + mangledName + "'");
        return nullptr;
    }
    instantiatingGenerics_.insert(mangledName);

    std::unordered_map<std::string, const TypeInfo*> subst;
    for (size_t i = 0; i < tmpl.typeParams.size(); i++) {
        auto* tpCtx = tmpl.decl->typeParamList()->typeParam(i);
        if (tpCtx->COLON()) {
            auto constraintName = tpCtx->IDENTIFIER(1)->getText();
            auto* constraint = resolveTypeParamConstraint(constraintName, ctx);
            if (!satisfiesConstraint(typeArgs[i], constraint, tmpl.typeParams[i], ctx)) {
                instantiatingGenerics_.erase(mangledName);
                return nullptr;
            }
        }
        subst[tmpl.typeParams[i]] = typeArgs[i];
    }

    TypeInfo skeleton;
    skeleton.name = mangledName;
    skeleton.kind = TypeKind::Enum;
    skeleton.bitWidth = 32;
    skeleton.isSigned = false;
    skeleton.builtinSuffix = "i32";
    skeleton.isGenericInstance = true;
    skeleton.genericBaseName = baseName;
    skeleton.typeParamNames = tmpl.typeParams;
    skeleton.typeArgs = typeArgs;
    skeleton.genericEnumDecl = static_cast<antlr4::ParserRuleContext*>(tmpl.decl);
    typeRegistry_.registerType(skeleton);

    TypeInfo ti = skeleton;
    std::unordered_set<std::string> seen;
    for (auto* variantDecl : tmpl.decl->enumVariant()) {
        auto variantName = variantDecl->IDENTIFIER()->getText();
        if (!seen.insert(variantName).second) {
            error(variantDecl, "duplicate enum variant '" + variantName +
                               "' in generic enum '" + baseName + "'");
            continue;
        }

        ti.enumVariants.push_back(variantName);

        EnumVariantInfo info;
        info.name = variantName;
        info.discriminant = static_cast<unsigned>(ti.enumVariantInfos.size());
        info.isError = hasAttribute(variantDecl->attributeList(), "error");

        if (variantDecl->LPAREN()) {
            info.payloadKind = EnumPayloadKind::Tuple;
            auto payloadTypes = variantDecl->typeSpec();
            for (size_t i = 0; i < payloadTypes.size(); i++) {
                unsigned fieldDims = 0;
                auto* fieldTI = resolveTypeSpecWithSubst(payloadTypes[i], subst, fieldDims);
                if (!fieldTI) {
                    error(payloadTypes[i], "cannot resolve payload type in generic enum '" +
                                           baseName + "'");
                    instantiatingGenerics_.erase(mangledName);
                    return nullptr;
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
                          baseName + "::" + variantName + "' or switch to 'vec<T>'");
                    instantiatingGenerics_.erase(mangledName);
                    return nullptr;
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
                                       "' in enum variant '" + baseName + "::" + variantName + "'");
                    instantiatingGenerics_.erase(mangledName);
                    return nullptr;
                }

                unsigned fieldDims = 0;
                auto* fieldTI = resolveTypeSpecWithSubst(payloadField->typeSpec(), subst, fieldDims);
                if (!fieldTI) {
                    error(payloadField, "cannot resolve payload type in generic enum '" +
                                        baseName + "'");
                    instantiatingGenerics_.erase(mangledName);
                    return nullptr;
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
                          baseName + "::" + variantName + "' or switch to 'vec<T>'");
                    instantiatingGenerics_.erase(mangledName);
                    return nullptr;
                }

                info.payloadFields.push_back({fieldName, fieldTI, fieldDims, fieldSizes});
            }
        }

        ti.enumVariantInfos.push_back(std::move(info));
    }

    typeRegistry_.registerType(std::move(ti));

    // `impl Trait for Gen<args>` blocks now have a concrete instance to bind to.
    ensureGenericInstanceAttachments(baseName, mangledName);

    instantiatingGenerics_.erase(mangledName);
    return typeRegistry_.lookup(mangledName);
}



const TypeInfo* Checker::instantiateGenericFunc(
    const std::string& baseName,
    const GenericFuncTemplate& tmpl,
    const std::vector<const TypeInfo*>& typeArgs,
    antlr4::ParserRuleContext* ctx) {

    if (typeArgs.size() != tmpl.typeParams.size()) {
        error(ctx, "generic function '" + baseName + "' expects " +
                   std::to_string(tmpl.typeParams.size()) + " type parameter(s), got " +
                   std::to_string(typeArgs.size()));
        return nullptr;
    }

    auto mangledName = mangleGenericName(baseName, typeArgs);

    // Already instantiated? Return the function's return type.
    if (functions_.count(mangledName)) {
        auto* ft = functions_[mangledName];
        return ft ? ft->returnType : nullptr;
    }

    // Cycle detection
    if (instantiatingGenerics_.count(mangledName)) {
        error(ctx, "recursive generic function instantiation detected for '" + mangledName + "'");
        return nullptr;
    }
    instantiatingGenerics_.insert(mangledName);

    // Build substitution map
    std::unordered_map<std::string, const TypeInfo*> subst;
    for (size_t i = 0; i < tmpl.typeParams.size(); i++) {
        auto* tpCtx = tmpl.decl->typeParamList()->typeParam(i);
        if (tpCtx->COLON()) {
            auto constraintName = tpCtx->IDENTIFIER(1)->getText();
            auto* constraint = resolveTypeParamConstraint(constraintName, ctx);
            if (!satisfiesConstraint(typeArgs[i], constraint, tmpl.typeParams[i], ctx)) {
                instantiatingGenerics_.erase(mangledName);
                return nullptr;
            }
        }
        subst[tmpl.typeParams[i]] = typeArgs[i];
    }

    // Resolve return type with substitution
    unsigned retDims = 0;
    auto* retType = resolveTypeSpecWithSubst(tmpl.decl->typeSpec(), subst, retDims);
    if (!retType) {
        instantiatingGenerics_.erase(mangledName);
        return nullptr;
    }

    // Resolve parameter types with substitution
    std::vector<const TypeInfo*> paramTypes;
    bool isVariadic = false;
    bool isTypedVariadic = false;
    const TypeInfo* variadicElemType = nullptr;
    if (auto* paramList = tmpl.decl->paramList()) {
        for (auto* param : paramList->param()) {
            if (param->SPREAD() && !param->typeSpec()) {
                // Untyped variadic ... — skip, no type to add
                isVariadic = true;
                break;
            }
            unsigned pDims = 0;
            auto* pType = resolveTypeSpecWithSubst(param->typeSpec(), subst, pDims);
            if (!pType) {
                instantiatingGenerics_.erase(mangledName);
                return nullptr;
            }
            paramTypes.push_back(pType);
            if (param->SPREAD()) {
                // Typed variadic: T ...name
                isVariadic = true;
                isTypedVariadic = true;
                variadicElemType = pType;
            }
        }
    }

    auto* funcType = makeFunctionType(retType, paramTypes, isVariadic, variadicElemType);
    functions_[mangledName] = funcType;

    // Check the function body with the substituted locals
    // We save/restore locals_ since this is a nested instantiation
    auto savedLocals = locals_;
    auto savedTypeSubst = activeTypeSubst_;
    activeTypeSubst_ = subst;
    locals_.clear();
    if (auto* paramList = tmpl.decl->paramList()) {
        for (auto* param : paramList->param()) {
            if (param->SPREAD() && !param->IDENTIFIER())
                continue;
            auto paramName = param->IDENTIFIER()->getText();
            unsigned pDims = 0;
            auto* pType = resolveTypeSpecWithSubst(param->typeSpec(), subst, pDims);
            if (!pType) continue;
            if (param->SPREAD())
                pDims = 1;
            locals_[paramName] = { pType, pDims, {}, true, true, nullptr };
        }
    }

    // Temporarily add type params as dummy locals so references inside the body resolve
    // (they'll be substituted by resolveTypeSpec when body expressions are checked)
    // We don't need this — body checking happens via resolveTypeSpec normally since
    // subst is only used for type specs, not for body expression checking.
    // The body is checked with the concrete types already registered.
    checkBlock(tmpl.decl->block(), retType);

    locals_ = savedLocals;
    activeTypeSubst_ = savedTypeSubst;
    instantiatingGenerics_.erase(mangledName);

    // Return the function's return type (not the function type itself)
    return retType;
}



std::optional<std::vector<const TypeInfo*>> Checker::inferGenericTypeArgs(
    const std::string& displayName,
    const std::vector<std::string>& typeParams,
    LucisParser::TypeParamListContext* typeParamList,
    const std::vector<LucisParser::ParamContext*>& formalParams,
    const std::vector<const TypeInfo*>& argTypes,
    antlr4::ParserRuleContext* ctx) {

    if (formalParams.size() != argTypes.size())
        return std::nullopt;

    std::unordered_set<std::string> genericParamSet(typeParams.begin(), typeParams.end());
    std::unordered_map<std::string, const TypeInfo*> inferred;

    for (size_t i = 0; i < formalParams.size(); i++) {
        auto* formalParam = formalParams[i];
        auto* actualType = argTypes[i];
        if (!formalParam || !formalParam->typeSpec())
            continue;

        if (!typeSpecMentionsGenericParam(formalParam->typeSpec(), genericParamSet))
            continue;

        bool emittedSpecificError = false;
        if (!unifyGenericTypeArg(formalParam->typeSpec(), actualType, genericParamSet,
                                 inferred, ctx, displayName, emittedSpecificError)) {
            if (!emittedSpecificError) {
                error(ctx, "cannot infer generic type parameter(s) for '" +
                             displayName + "' from argument " +
                             std::to_string(i + 1) + " of type '" +
                             (actualType ? actualType->name : "<unknown>") + "'");
            }
            return std::nullopt;
        }
    }

    std::vector<const TypeInfo*> typeArgs;
    for (size_t i = 0; i < typeParams.size(); i++) {
        auto it = inferred.find(typeParams[i]);
        if (it == inferred.end()) {
            error(ctx, "cannot infer generic type parameter '" + typeParams[i] +
                         "' for '" + displayName + "'");
            return std::nullopt;
        }

        if (typeParamList && i < typeParamList->typeParam().size()) {
            auto* tpCtx = typeParamList->typeParam(i);
            if (tpCtx->COLON()) {
                auto constraintName = tpCtx->IDENTIFIER(1)->getText();
                auto* constraint = resolveTypeParamConstraint(constraintName, ctx);
                if (!satisfiesConstraint(it->second, constraint, typeParams[i], ctx))
                    return std::nullopt;
            }
        }

        typeArgs.push_back(it->second);
    }

    return typeArgs;
}



bool Checker::unifyGenericTypeArg(
    LucisParser::TypeSpecContext* formalType,
    const TypeInfo* actualType,
    const std::unordered_set<std::string>& genericParams,
    std::unordered_map<std::string, const TypeInfo*>& inferred,
    antlr4::ParserRuleContext* ctx,
    const std::string& displayName,
    bool& emittedSpecificError) {

    if (!formalType) return true;
    if (!typeSpecMentionsGenericParam(formalType, genericParams)) return true;
    if (!actualType) return false;

    if (!formalType->LBRACKET() && !formalType->STAR() && !formalType->LT() &&
        !formalType->VEC() && !formalType->MAP() && !formalType->SET() &&
        !formalType->TUPLE() && !formalType->AUTO() && !formalType->fnTypeSpec()) {
        auto name = formalType->getText();
        if (genericParams.count(name)) {
            auto it = inferred.find(name);
            if (it == inferred.end()) {
                inferred[name] = actualType;
                return true;
            }
            if (it->second != actualType && it->second->name != actualType->name) {
                emittedSpecificError = true;
                error(ctx, "ambiguous generic inference for '" + displayName +
                             "': '" + name + "' was inferred as both '" +
                             it->second->name + "' and '" + actualType->name + "'");
                return false;
            }
        }
        return true;
    }

    if (formalType->STAR() && formalType->typeSpec().size() == 1) {
        if (actualType->kind != TypeKind::Pointer || !actualType->pointeeType)
            return false;
        return unifyGenericTypeArg(formalType->typeSpec(0), actualType->pointeeType,
                                   genericParams, inferred, ctx, displayName,
                                   emittedSpecificError);
    }

    if (formalType->VEC()) {
        if (actualType->kind != TypeKind::Extended || actualType->extendedKind != "Vec" ||
            !actualType->elementType || formalType->typeSpec().size() != 1)
            return false;
        return unifyGenericTypeArg(formalType->typeSpec(0), actualType->elementType,
                                   genericParams, inferred, ctx, displayName,
                                   emittedSpecificError);
    }

    if (formalType->SET()) {
        if (actualType->kind != TypeKind::Extended || actualType->extendedKind != "Set" ||
            !actualType->elementType || formalType->typeSpec().size() != 1)
            return false;
        return unifyGenericTypeArg(formalType->typeSpec(0), actualType->elementType,
                                   genericParams, inferred, ctx, displayName,
                                   emittedSpecificError);
    }

    if (formalType->MAP()) {
        if (actualType->kind != TypeKind::Extended || actualType->extendedKind != "Map" ||
            !actualType->keyType || !actualType->valueType || formalType->typeSpec().size() != 2)
            return false;
        return unifyGenericTypeArg(formalType->typeSpec(0), actualType->keyType,
                                   genericParams, inferred, ctx, displayName,
                                   emittedSpecificError)
            && unifyGenericTypeArg(formalType->typeSpec(1), actualType->valueType,
                                   genericParams, inferred, ctx, displayName,
                                   emittedSpecificError);
    }

    if (formalType->TUPLE()) {
        if (actualType->kind != TypeKind::Tuple ||
            actualType->tupleElements.size() != formalType->typeSpec().size())
            return false;
        for (size_t i = 0; i < formalType->typeSpec().size(); i++) {
            if (!unifyGenericTypeArg(formalType->typeSpec(i), actualType->tupleElements[i],
                                     genericParams, inferred, ctx, displayName,
                                     emittedSpecificError))
                return false;
        }
        return true;
    }

    if (formalType->LT()) {
        auto baseName = formalType->IDENTIFIER()->getText();
        std::vector<const TypeInfo*> actualArgs;

        if (actualType->isGenericInstance && actualType->genericBaseName == baseName) {
            actualArgs = actualType->typeArgs;
        } else if (actualType->kind == TypeKind::Extended && actualType->extendedKind == baseName) {
            if (formalType->typeSpec().size() == 1 && actualType->elementType) {
                actualArgs.push_back(actualType->elementType);
            } else if (formalType->typeSpec().size() == 2 && actualType->keyType && actualType->valueType) {
                actualArgs.push_back(actualType->keyType);
                actualArgs.push_back(actualType->valueType);
            } else {
                return false;
            }
        } else {
            return false;
        }

        if (actualArgs.size() != formalType->typeSpec().size())
            return false;

        for (size_t i = 0; i < formalType->typeSpec().size(); i++) {
            if (!unifyGenericTypeArg(formalType->typeSpec(i), actualArgs[i], genericParams,
                                     inferred, ctx, displayName, emittedSpecificError))
                return false;
        }
        return true;
    }

    return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Phase 1: SemanticDB parallel population
// ═══════════════════════════════════════════════════════════════════════════════


