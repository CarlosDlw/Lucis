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


std::string IRGen::mangleGenericName(const std::string& baseName,
                                      const std::vector<const TypeInfo*>& typeArgs) {
    std::string name = baseName;
    for (auto* arg : typeArgs) {
        name += "__";
        for (char c : arg->name) {
            if (c == '<' || c == '>' || c == ' ' || c == ',')
                name += '_';
            else
                name += c;
        }
    }
    return name;
}



std::string IRGen::mangleImplMethodName(const std::string& traitName,
                                          const std::string& typeName,
                                          const std::string& methodName) {
    return "impl_" + traitName + "_" + typeName + "__" + methodName;
}



const TypeInfo* IRGen::resolveTypeInfoWithSubst(
    LucisParser::TypeSpecContext* ctx,
    const std::unordered_map<std::string, const TypeInfo*>& subst) {
    if (!ctx) return typeRegistry_.lookup("int32");

    // Bare IDENTIFIER that is a type param → substitute
    if (!ctx->LBRACKET() && !ctx->STAR() && !ctx->LT() &&
        !ctx->VEC() && !ctx->MAP() && !ctx->SET() &&
        !ctx->TUPLE() && !ctx->AUTO() && !ctx->fnTypeSpec()) {
        auto name = ctx->getText();
        auto it = subst.find(name);
        if (it != subst.end()) return it->second;
    }

    // Pointer: *T
    if (ctx->STAR() && ctx->typeSpec().size() == 1) {
        auto* inner = resolveTypeInfoWithSubst(ctx->typeSpec(0), subst);
        if (!inner) return typeRegistry_.lookup("int32");
        auto ptrName = "*" + inner->name;
        if (auto* existing = typeRegistry_.lookup(ptrName)) return existing;
        TypeInfo ti;
        ti.name = ptrName;
        ti.kind = TypeKind::Pointer;
        ti.pointeeType = inner;
        ti.bitWidth = 0;
        ti.isSigned = false;
        ti.builtinSuffix = "ptr";
        typeRegistry_.registerType(std::move(ti));
        return typeRegistry_.lookup(ptrName);
    }

    // Generic instantiation: Foo<T> → Foo<int32>
    if (ctx->LT()) {
        std::string innerBaseName;
        if (ctx->VEC()) innerBaseName = "Vec";
        else if (ctx->MAP()) innerBaseName = "Map";
        else if (ctx->SET()) innerBaseName = "Set";
        else if (ctx->IDENTIFIER()) innerBaseName = ctx->IDENTIFIER()->getText();
        if (innerBaseName.empty())
            return resolveTypeInfo(ctx);
        std::vector<const TypeInfo*> resolvedArgs;
        for (auto* argSpec : ctx->typeSpec()) {
            auto* argTI = resolveTypeInfoWithSubst(argSpec, subst);
            if (!argTI) argTI = typeRegistry_.lookup("int32");
            resolvedArgs.push_back(argTI);
        }
        auto structIt = genericStructTemplates_.find(innerBaseName);
        if (structIt != genericStructTemplates_.end()) {
            return instantiateGenericStruct(innerBaseName, structIt->second, resolvedArgs);
        }
        auto unionIt = genericUnionTemplates_.find(innerBaseName);
        if (unionIt != genericUnionTemplates_.end()) {
            return instantiateGenericUnion(innerBaseName, unionIt->second, resolvedArgs);
        }
        auto enumIt = genericEnumTemplates_.find(innerBaseName);
        if (enumIt != genericEnumTemplates_.end()) {
            return instantiateGenericEnum(innerBaseName, enumIt->second, resolvedArgs);
        }

        // Only create Extended types for known built-in names
        if (auto* extDesc = extTypeRegistry_.lookup(innerBaseName)) {
            if (resolvedArgs.size() == 1 && extDesc->genericArity == 1) {
                auto* elemTI = resolvedArgs[0];
                if (!elemTI) elemTI = typeRegistry_.lookup("int32");
                auto fullName = innerBaseName + "<" + elemTI->name + ">";
                if (auto* existing = typeRegistry_.lookup(fullName))
                    return existing;
                TypeInfo ti;
                ti.name = fullName;
                ti.kind = TypeKind::Extended;
                ti.bitWidth = 0;
                ti.isSigned = false;
                ti.builtinSuffix = elemTI->builtinSuffix.empty() ? "raw" : elemTI->builtinSuffix;
                ti.elementType = elemTI;
                ti.extendedKind = innerBaseName;
                typeRegistry_.registerType(std::move(ti));
                return typeRegistry_.lookup(fullName);
            } else if (resolvedArgs.size() == 2 && extDesc->genericArity == 2) {
                auto* keyTI = resolvedArgs[0];
                auto* valTI = resolvedArgs[1];
                if (!keyTI) keyTI = typeRegistry_.lookup("int32");
                if (!valTI) valTI = typeRegistry_.lookup("int32");
                auto fullName = innerBaseName + "<" + keyTI->name + ", " + valTI->name + ">";
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
                ti.extendedKind = innerBaseName;
                typeRegistry_.registerType(std::move(ti));
                return typeRegistry_.lookup(fullName);
            }
        }
    }

    // Default: normal resolution
    return resolveTypeInfo(ctx);
}



llvm::StructType* IRGen::ensureGenericStructType(const std::string& mangledName,
                                                   const TypeInfo* instanceTI) {
    // Check if LLVM struct already created
    if (auto* existing = llvm::StructType::getTypeByName(*context_, mangledName))
        return existing;

    // Create the LLVM struct type from instanceTI fields
    auto* structType = llvm::StructType::create(*context_, mangledName);

    std::vector<llvm::Type*> fieldTypes;
    for (auto& field : instanceTI->fields) {
        auto* llTy = buildFieldLLVMType(field.typeInfo,
                                        field.arrayDims,
                                        field.arraySizes);
        fieldTypes.push_back(llTy);
    }
    structType->setBody(fieldTypes);
    return structType;
}



const TypeInfo* IRGen::instantiateGenericStruct(
    const std::string& baseName,
    const GenericStructTemplate& tmpl,
    const std::vector<const TypeInfo*>& typeArgs) {

    auto mangledName = mangleGenericName(baseName, typeArgs);
    const TypeInfo* result = nullptr;

    // Cycle detection — struct references itself via pointer (e.g. LinkedListNode<T> → *LinkedListNode<T>)
    if (instantiatedGenerics_.count(mangledName)) {
        if (auto* existing = typeRegistry_.lookup(mangledName))
            return existing;
        return typeRegistry_.lookup("int32");
    }
    // Always insert before "already existed" check so cycle detection
    // works even for recursive calls during extend method processing.
    instantiatedGenerics_.insert(mangledName);

    // Build substitution map: T → int32, etc. (needed for both first-time and extend methods)
    std::unordered_map<std::string, const TypeInfo*> subst;
    for (size_t i = 0; i < tmpl.typeParams.size() && i < typeArgs.size(); i++)
        subst[tmpl.typeParams[i]] = typeArgs[i];

    // Check if already instantiated and finalized (skip empty skeletons from Checker)
    auto* existing = typeRegistry_.lookup(mangledName);
    if (existing) {
        bool isSkeleton = existing->kind == TypeKind::Struct &&
                          existing->fields.empty() && existing->bitWidth == 0 &&
                          existing->isGenericInstance;
        if (!isSkeleton) {
                ensureGenericStructType(mangledName, existing);
                result = existing;
            }
    }

    if (!result) {
        // First-time instantiation

        // Register skeleton for self-referencing fields
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

        // Create opaque LLVM struct first (enables self-referencing pointer fields)
        auto* structLLTy = llvm::StructType::create(*context_, mangledName);

        // Instantiate fields
        TypeInfo ti = skeleton;
        std::vector<llvm::Type*> fieldTypes;
        for (auto* field : tmpl.decl->structField()) {
            auto* fieldTI = resolveTypeInfoWithSubst(field->typeSpec(), subst);
            if (!fieldTI) fieldTI = typeRegistry_.lookup("int32");
            unsigned fieldDims = 0;
            std::vector<unsigned> fieldSizes;
            {
                auto* spec = field->typeSpec();
                while (spec && spec->LBRACKET()) {
                    fieldDims++;
                    if (spec->INT_LIT())
                        fieldSizes.push_back(static_cast<unsigned>(
                            std::stoul(spec->INT_LIT()->getText())));
                    spec = spec->typeSpec(0);
                }
            }
            llvm::Type* fieldLLTy = fieldTI->toLLVMType(*context_, module_->getDataLayout());
            if (fieldDims > 0 && !fieldSizes.empty()) {
                for (auto it = fieldSizes.rbegin(); it != fieldSizes.rend(); ++it)
                    fieldLLTy = llvm::ArrayType::get(fieldLLTy, *it);
            } else if (fieldDims > 0 && fieldSizes.empty()) {
                auto* ptrTy = llvm::PointerType::getUnqual(*context_);
                auto* i64Ty = llvm::Type::getInt64Ty(*context_);
                fieldLLTy = llvm::StructType::get(*context_, {ptrTy, i64Ty});
            }
            fieldTypes.push_back(fieldLLTy);
            ti.fields.push_back({ field->IDENTIFIER()->getText(), fieldTI, fieldDims, fieldSizes });
        }
        structLLTy->setBody(fieldTypes);

        // Update registry with full type info
        typeRegistry_.registerType(std::move(ti));
        result = typeRegistry_.lookup(mangledName);

        // Update self-referencing pointer type to point to the now-complete struct
        {
            auto ptrName = "*" + mangledName;
            if (auto* existingPtr = typeRegistry_.lookup(ptrName)) {
                if (existingPtr->kind == TypeKind::Pointer && existingPtr->pointeeType != result) {
                    TypeInfo updatedPtr = *existingPtr;
                    updatedPtr.pointeeType = result;
                    typeRegistry_.registerType(std::move(updatedPtr));
                }
            }
        }
    }

    // ★ Self must resolve to the concrete instantiated type, not the opaque generic template
    subst["Self"] = result;

    // Instantiate extend methods for this struct, if any (only once per concrete type)
    if (loweredGenericExtendTypes_.insert(mangledName).second) {
    auto extendIt = genericExtendTemplates_.find(baseName);
    if (extendIt != genericExtendTemplates_.end()) {
        auto& extTmpl = extendIt->second;
        auto* ptrTy = llvm::PointerType::getUnqual(*context_);

        for (auto* method : extTmpl.decl->extendMethod()) {
            // ── Operator overload in generic extend ──
            if (auto* opDecl = method->operatorDecl()) {
                auto internalName = opInternalName(opDecl->operatorName());
                auto* retTI = resolveTypeInfoWithSubst(opDecl->typeSpec(), subst);
                if (!retTI) continue;
                auto* retLLTy = retTI->toLLVMType(*context_, module_->getDataLayout());

                bool isInstance = (opDecl->AMPERSAND() != nullptr);

                std::vector<llvm::Type*> paramLLTypes;
                std::vector<const TypeInfo*> paramTIs;
                if (isInstance) paramLLTypes.push_back(ptrTy);

                std::vector<LucisParser::ParamContext*> params;
                if (isInstance) {
                    params = opDecl->param();
                } else if (auto* pl = opDecl->paramList()) {
                    params = pl->param();
                }

                for (auto* param : params) {
                    auto* pTI = resolveTypeInfoWithSubst(param->typeSpec(), subst);
                    if (!pTI) pTI = typeRegistry_.lookup("int32");
                    paramTIs.push_back(pTI);
                    paramLLTypes.push_back(pTI->toLLVMType(*context_, module_->getDataLayout()));
                }

                auto baseName = mangledName + "__" + internalName;
                auto funcName = baseName + "_" + std::to_string(paramLLTypes.size());
                auto* fnType = llvm::FunctionType::get(retLLTy, paramLLTypes, false);
                auto* fn = module_->getFunction(funcName);
                if (!fn) {
                    fn = llvm::Function::Create(
                        fnType, llvm::Function::ExternalLinkage, funcName, module_);
                }
                operatorMethods_[mangledName][internalName].push_back({ fn, retTI });

                // ── Emit operator body ──
                auto* savedFunc = currentFunction_;
                auto  savedLocals = locals_;
                auto* savedBB = builder_->GetInsertBlock();
                auto  savedSubst = currentGenericSubst_;
                currentGenericSubst_ = subst;
                currentFunction_ = fn;
                locals_.clear();

                auto* entry = llvm::BasicBlock::Create(*context_, "entry", fn);
                builder_->SetInsertPoint(entry);

                if (isInstance) {
                    auto* selfArg = fn->getArg(0);
                    selfArg->setName("self");
                    auto* selfAlloca = builder_->CreateAlloca(ptrTy, nullptr, "self");
                    builder_->CreateStore(selfArg, selfAlloca);
                    auto ptrTIName = "*" + mangledName;
                    if (!typeRegistry_.lookup(ptrTIName)) {
                        TypeInfo ptrTI;
                        ptrTI.name = ptrTIName;
                        ptrTI.kind = TypeKind::Pointer;
                        ptrTI.pointeeType = const_cast<TypeInfo*>(result);
                        typeRegistry_.registerType(std::move(ptrTI));
                    }
                    locals_["self"] = { selfAlloca, typeRegistry_.lookup(ptrTIName), 0 };
                }

                size_t argOff = isInstance ? 1 : 0;
                for (size_t pi = 0; pi < params.size(); pi++) {
                    auto paramName = params[pi]->IDENTIFIER()->getText();
                    auto* arg = fn->getArg(pi + argOff);
                    arg->setName(paramName);
                    auto* alloca = builder_->CreateAlloca(paramLLTypes[pi + argOff], nullptr, paramName);
                    builder_->CreateStore(arg, alloca);
                    locals_[paramName] = { alloca, paramTIs[pi], 0, true };
                }

                if (opDecl->block()) {
                    for (auto* stmt : opDecl->block()->statement())
                        visit(stmt);
                }

                if (!builder_->GetInsertBlock()->getTerminator()) {
                    if (retLLTy->isVoidTy())
                        builder_->CreateRetVoid();
                    else
                        builder_->CreateRet(llvm::UndefValue::get(retLLTy));
                }

                currentFunction_ = savedFunc;
                locals_ = savedLocals;
                currentGenericSubst_ = savedSubst;
                if (savedBB)
                    builder_->SetInsertPoint(savedBB);

                continue;
            }

            auto methodName = method->name->getText();
            auto* retTI = resolveTypeInfoWithSubst(method->typeSpec(), subst);
            auto* retLLTy = retTI->toLLVMType(*context_, module_->getDataLayout());

            bool isStatic = (method->AMPERSAND() == nullptr);

            std::vector<llvm::Type*> paramLLTypes;
            std::vector<const TypeInfo*> paramTIs;
            if (!isStatic) {
                paramLLTypes.push_back(ptrTy); // &self
            }

            std::vector<LucisParser::ParamContext*> params;
            if (isStatic) {
                if (auto* pl = method->paramList()) params = pl->param();
            } else {
                params = method->param();
            }

            for (auto* param : params) {
                auto* pTI = resolveTypeInfoWithSubst(param->typeSpec(), subst);
                if (!pTI) pTI = typeRegistry_.lookup("int32");
                paramTIs.push_back(pTI);
                paramLLTypes.push_back(pTI->toLLVMType(*context_, module_->getDataLayout()));
            }

            auto funcName = mangledName + "__" + methodName;
            auto* fnType = llvm::FunctionType::get(retLLTy, paramLLTypes, false);
            auto* fn = llvm::Function::Create(
                fnType, llvm::Function::ExternalLinkage, funcName, module_);

            if (isStatic)
                staticStructMethods_[mangledName][methodName] = { fn, retTI };
            else
                structMethods_[mangledName][methodName] = { fn, retTI };

            if (methodName == "drop" && !isStatic && params.empty()) {
                if (auto* mutTI = typeRegistry_.lookupMutable(mangledName))
                    mutTI->dropTracked = true;
            }

            // Emit body
            auto* savedFunc = currentFunction_;
            auto  savedLocals = locals_;
            auto* savedBB = builder_->GetInsertBlock();
            auto  savedSubst = currentGenericSubst_;
            currentGenericSubst_ = subst;
            currentFunction_ = fn;
            locals_.clear();

            auto* entry = llvm::BasicBlock::Create(*context_, "entry", fn);
            builder_->SetInsertPoint(entry);

            if (!isStatic) {
                auto* selfArg = fn->getArg(0);
                selfArg->setName("self");
                auto* selfAlloca = builder_->CreateAlloca(ptrTy, nullptr, "self");
                builder_->CreateStore(selfArg, selfAlloca);
                auto ptrTIName = "*" + mangledName;
                if (!typeRegistry_.lookup(ptrTIName)) {
                    TypeInfo ptrTI;
                    ptrTI.name = ptrTIName;
                    ptrTI.kind = TypeKind::Pointer;
                    ptrTI.pointeeType = result;
                    ptrTI.bitWidth = 0;
                    ptrTI.isSigned = false;
                    ptrTI.builtinSuffix = "ptr";
                    typeRegistry_.registerType(std::move(ptrTI));
                }
                locals_["self"] = { selfAlloca, typeRegistry_.lookup(ptrTIName), 0 };
            }

            size_t argOffset = isStatic ? 0 : 1;
            for (size_t i = 0; i < params.size(); i++) {
                auto paramName = params[i]->IDENTIFIER()->getText();
                auto* arg = fn->getArg(i + argOffset);
                arg->setName(paramName);
                auto* paramLLTy = paramLLTypes[i + argOffset];
                auto* alloca = builder_->CreateAlloca(paramLLTy, nullptr, paramName);
                builder_->CreateStore(arg, alloca);
                locals_[paramName] = { alloca, paramTIs[i], 0, true };
            }

            // Register all type params as their concrete types so body can use them
            // (we can't inject them into locals_, but resolveTypeInfo already uses
            //  the substituted types since fields are concrete)

            for (auto* stmt : method->block()->statement())
                visit(stmt);

            if (!builder_->GetInsertBlock()->getTerminator()) {
                if (retLLTy->isVoidTy())
                    builder_->CreateRetVoid();
                else
                    builder_->CreateRet(llvm::UndefValue::get(retLLTy));
            }

            currentFunction_ = savedFunc;
            locals_ = savedLocals;
            currentGenericSubst_ = savedSubst;
            if (savedBB)
                builder_->SetInsertPoint(savedBB);
        }
    }

    } // end if (loweredGenericExtendTypes_)

    // ── Instantiate generic impl blocks for this concrete type ──────────
    if (loweredGenericImplTypes_.insert(mangledName).second) {
        // Find all generic impls whose type name matches this struct's base name
        for (auto& [implTypeName, genImpl] : genericImplTemplates_) {
            if (implTypeName != baseName) continue;

            auto* implCtx = genImpl.decl;
            if (!implCtx) continue;

            // Build substitution map: impl's type params → concrete type args
            std::unordered_map<std::string, const TypeInfo*> implSubst;
            for (size_t i = 0; i < genImpl.typeParams.size() && i < typeArgs.size(); ++i)
                implSubst[genImpl.typeParams[i]] = typeArgs[i];
            implSubst["Self"] = result;

            // Resolve trait type
            const TypeInfo* traitTI = resolveTypeInfo(implCtx->typeSpec(0));
            if (!traitTI || traitTI->kind != TypeKind::Trait) continue;

            const TraitInfo* trait = traitRegistry_.lookup(traitTI->name);
            if (!trait) continue;

            // Register ImplEntry
            ImplEntry entry;
            entry.traitName = trait->name;
            entry.typeName = mangledName;
            entry.implType = result;
            entry.decl = implCtx;
            entry.sourceModulePath = currentModulePath_;
            traitImpls_[entry.traitName].push_back(std::move(entry));

            // Mark the type as implementing this trait
            auto* mutTI = typeRegistry_.lookupMutable(mangledName);
            if (mutTI) mutTI->implementedTraits.insert(trait->name);

            // Generate LLVM functions for each generic impl method
            for (auto* method : implCtx->implMethod()) {
                auto methodName = method->name->getText();
                auto fnName = mangleImplMethodName(
                    trait->name, mangledName, methodName);

                if (loweredImplMethods_.count(fnName)) continue;

                bool isStatic = (method->AMPERSAND() == nullptr);

                unsigned retDims = 0;
                const TypeInfo* retTI = resolveTypeInfoWithSubst(
                    method->typeSpec(), implSubst);

                std::vector<llvm::Type*> paramLLTypes;
                if (!isStatic) {
                    auto* ptrTy = llvm::PointerType::getUnqual(result->toLLVMType(*context_, module_->getDataLayout()));
                    paramLLTypes.push_back(ptrTy);
                }

                if (isStatic) {
                    if (auto* pl = method->paramList()) {
                        for (auto* p : pl->param()) {
                            if (p->SPREAD()) continue;
                            unsigned dims = 0;
                            const TypeInfo* pTI = resolveTypeInfoWithSubst(
                                p->typeSpec(), implSubst);
                            if (!pTI) continue;
                            auto* pLL = pTI->toLLVMType(*context_, module_->getDataLayout());
                            if (pLL) paramLLTypes.push_back(pLL);
                        }
                    }
                } else {
                    for (auto* p : method->param()) {
                        if (p->SPREAD()) continue;
                        unsigned dims = 0;
                        const TypeInfo* pTI = resolveTypeInfoWithSubst(
                            p->typeSpec(), implSubst);
                        if (!pTI) continue;
                        auto* pLL = pTI->toLLVMType(*context_, module_->getDataLayout());
                        if (pLL) paramLLTypes.push_back(pLL);
                    }
                }

                llvm::Type* retLLType = llvm::Type::getVoidTy(*context_);
                if (retTI && retTI->kind != TypeKind::Void)
                    retLLType = retTI->toLLVMType(*context_, module_->getDataLayout());

                auto* fnType = llvm::FunctionType::get(
                    retLLType, paramLLTypes, false);
                auto* fn = llvm::Function::Create(
                    fnType, llvm::Function::ExternalLinkage,
                    fnName, module_);
                loweredImplMethods_[fnName] = fn;

                // Lower body with active substitution
                auto* prevFn = currentFunction_;
                auto* prevBB = builder_->GetInsertBlock();
                currentFunction_ = fn;

                auto* entryBB = llvm::BasicBlock::Create(
                    *context_, "entry", fn);
                builder_->SetInsertPoint(entryBB);

                auto savedLocals = locals_;
                locals_.clear();
                auto savedSubst = currentGenericSubst_;
                currentGenericSubst_ = implSubst;

                size_t argIdx = 0;
                if (!isStatic) {
                    auto* selfArg = fn->getArg(0);
                    selfArg->setName("self");
                    auto ptrTIName = "*" + mangledName;
                    if (!typeRegistry_.lookup(ptrTIName)) {
                        TypeInfo ptrTI;
                        ptrTI.name = ptrTIName;
                        ptrTI.kind = TypeKind::Pointer;
                        ptrTI.pointeeType = result;
                        typeRegistry_.registerType(std::move(ptrTI));
                    }
                    auto* selfPtrTI = typeRegistry_.lookup(ptrTIName);
                    auto* selfAlloca = builder_->CreateAlloca(selfArg->getType(), nullptr, "self");
                    builder_->CreateStore(selfArg, selfAlloca);
                    locals_["self"] = {selfAlloca, selfPtrTI, 0};
                    argIdx = 1;
                }

                if (isStatic) {
                    if (auto* pl = method->paramList()) {
                        for (auto* p : pl->param()) {
                            if (p->SPREAD()) continue;
                            auto paramName = p->IDENTIFIER()->getText();
                            unsigned dims = 0;
                            const TypeInfo* pTI = resolveTypeInfoWithSubst(
                                p->typeSpec(), implSubst);
                            if (!pTI) continue;
                            auto* arg = fn->getArg(argIdx++);
                            arg->setName(paramName);
                            auto* alloca = builder_->CreateAlloca(
                                pTI->toLLVMType(*context_, module_->getDataLayout()), nullptr, paramName);
                            builder_->CreateStore(arg, alloca);
                            locals_[paramName] = {alloca, pTI, dims};
                        }
                    }
                } else {
                    for (auto* p : method->param()) {
                        if (p->SPREAD()) continue;
                        auto paramName = p->IDENTIFIER()->getText();
                        unsigned dims = 0;
                        const TypeInfo* pTI = resolveTypeInfoWithSubst(
                            p->typeSpec(), implSubst);
                        if (!pTI) continue;
                        auto* arg = fn->getArg(argIdx++);
                        arg->setName(paramName);
                        auto* alloca = builder_->CreateAlloca(
                            pTI->toLLVMType(*context_, module_->getDataLayout()), nullptr, paramName);
                        builder_->CreateStore(arg, alloca);
                        locals_[paramName] = {alloca, pTI, dims};
                    }
                }

                auto savedRetType = currentFnReturnType_;
                currentFnReturnType_ = retTI;
                visitBlock(method->block());
                currentFnReturnType_ = savedRetType;

                if (!builder_->GetInsertBlock()->getTerminator()) {
                    if (fn->getReturnType()->isVoidTy())
                        builder_->CreateRetVoid();
                    else
                        builder_->CreateRet(
                            llvm::UndefValue::get(fn->getReturnType()));
                }

                locals_ = savedLocals;
                currentGenericSubst_ = savedSubst;
                currentFunction_ = prevFn;
                if (prevBB) builder_->SetInsertPoint(prevBB);
            }
        }
    }

done:
    instantiatedGenerics_.erase(mangledName);
    return result;
}



const TypeInfo* IRGen::instantiateGenericUnion(
    const std::string& baseName,
    const GenericUnionTemplate& tmpl,
    const std::vector<const TypeInfo*>& typeArgs) {

    auto mangledName = mangleGenericName(baseName, typeArgs);

    if (auto* existing = typeRegistry_.lookup(mangledName)) {
        if (!llvm::StructType::getTypeByName(*context_, mangledName)) {
            uint64_t maxSize = 1;
            for (auto& field : existing->fields) {
                auto* fieldTy = field.typeInfo->toLLVMType(*context_, module_->getDataLayout());
                maxSize = std::max<uint64_t>(maxSize,
                    module_->getDataLayout().getTypeAllocSize(fieldTy));
            }
            auto* i8Ty = llvm::Type::getInt8Ty(*context_);
            auto* bodyTy = llvm::ArrayType::get(i8Ty, maxSize);
            llvm::StructType::create(*context_, { bodyTy }, mangledName);
        }
        return existing;
    }

    if (instantiatedGenerics_.count(mangledName)) {
        std::cerr << "lucis: recursive generic instantiation: " << mangledName << "\n";
        return typeRegistry_.lookup("int32");
    }
    instantiatedGenerics_.insert(mangledName);

    std::unordered_map<std::string, const TypeInfo*> subst;
    for (size_t i = 0; i < tmpl.typeParams.size() && i < typeArgs.size(); i++)
        subst[tmpl.typeParams[i]] = typeArgs[i];

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
    uint64_t maxSize = 1;
    for (auto* field : tmpl.decl->unionField()) {
        auto* fieldTI = resolveTypeInfoWithSubst(field->typeSpec(), subst);
        if (!fieldTI) fieldTI = typeRegistry_.lookup("int32");
        ti.fields.push_back({ field->IDENTIFIER()->getText(), fieldTI });

        auto* fieldLLTy = fieldTI->toLLVMType(*context_, module_->getDataLayout());
        maxSize = std::max<uint64_t>(maxSize,
            module_->getDataLayout().getTypeAllocSize(fieldLLTy));
    }

    auto* i8Ty = llvm::Type::getInt8Ty(*context_);
    auto* bodyTy = llvm::ArrayType::get(i8Ty, maxSize);
    llvm::StructType::create(*context_, { bodyTy }, mangledName);

    typeRegistry_.registerType(std::move(ti));
    instantiatedGenerics_.erase(mangledName);
    return typeRegistry_.lookup(mangledName);
}



const TypeInfo* IRGen::instantiateGenericEnum(
    const std::string& baseName,
    const GenericEnumTemplate& tmpl,
    const std::vector<const TypeInfo*>& typeArgs) {

    auto mangledName = mangleGenericName(baseName, typeArgs);

    if (auto* existing = typeRegistry_.lookup(mangledName)) {
        existing->toLLVMType(*context_, module_->getDataLayout());
        return existing;
    }

    if (instantiatedGenerics_.count(mangledName)) {
        std::cerr << "lucis: recursive generic instantiation: " << mangledName << "\n";
        return typeRegistry_.lookup("int32");
    }
    instantiatedGenerics_.insert(mangledName);

    std::unordered_map<std::string, const TypeInfo*> subst;
    for (size_t i = 0; i < tmpl.typeParams.size() && i < typeArgs.size(); i++)
        subst[tmpl.typeParams[i]] = typeArgs[i];

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
    unsigned nextDiscriminant = 0;
    for (auto* variantDecl : tmpl.decl->enumVariant()) {
        auto variantName = variantDecl->IDENTIFIER()->getText();
        ti.enumVariants.push_back(variantName);

        EnumVariantInfo info;
        info.name = variantName;
        info.isError = false;
        if (auto* al = variantDecl->attributeList()) {
            for (auto* a : al->attribute()) {
                if (a->IDENTIFIER() && a->IDENTIFIER()->getText() == "error") {
                    info.isError = true;
                    break;
                }
            }
        }
        if (variantDecl->ASSIGN()) {
            info.discriminant = static_cast<unsigned>(
                evalEnumDiscExpr(variantDecl->expression()));
            nextDiscriminant = info.discriminant + 1;
        } else {
            info.discriminant = nextDiscriminant++;
        }

        if (variantDecl->LPAREN()) {
            info.payloadKind = EnumPayloadKind::Tuple;
            auto payloadTypes = variantDecl->typeSpec();
            for (size_t i = 0; i < payloadTypes.size(); i++) {
                auto* fieldTI = resolveTypeInfoWithSubst(payloadTypes[i], subst);
                if (!fieldTI) fieldTI = typeRegistry_.lookup("int32");
                unsigned fieldDims = countArrayDims(payloadTypes[i]);
                std::vector<unsigned> fieldSizes;
                auto* spec = payloadTypes[i];
                while (spec && spec->LBRACKET()) {
                    if (spec->INT_LIT())
                        fieldSizes.push_back(static_cast<unsigned>(std::stoul(spec->INT_LIT()->getText())));
                    spec = spec->typeSpec(0);
                }
                info.payloadFields.push_back({"_" + std::to_string(i), fieldTI, fieldDims, fieldSizes});
            }
        } else if (variantDecl->LBRACE()) {
            info.payloadKind = EnumPayloadKind::Named;
            for (auto* payloadField : variantDecl->enumPayloadField()) {
                auto* fieldTI = resolveTypeInfoWithSubst(payloadField->typeSpec(), subst);
                if (!fieldTI) fieldTI = typeRegistry_.lookup("int32");
                unsigned fieldDims = countArrayDims(payloadField->typeSpec());
                std::vector<unsigned> fieldSizes;
                auto* spec = payloadField->typeSpec();
                while (spec && spec->LBRACKET()) {
                    if (spec->INT_LIT())
                        fieldSizes.push_back(static_cast<unsigned>(std::stoul(spec->INT_LIT()->getText())));
                    spec = spec->typeSpec(0);
                }
                info.payloadFields.push_back({payloadField->IDENTIFIER()->getText(), fieldTI, fieldDims, fieldSizes});
            }
        }

        ti.enumVariantInfos.push_back(std::move(info));
    }

    typeRegistry_.registerType(std::move(ti));
    auto* result = typeRegistry_.lookup(mangledName);
    if (result)
        result->toLLVMType(*context_, module_->getDataLayout());
    instantiatedGenerics_.erase(mangledName);
    return result;
}



llvm::Function* IRGen::instantiateGenericFunc(
    const std::string& baseName,
    const GenericFuncTemplate& tmpl,
    const std::vector<const TypeInfo*>& typeArgs) {

    auto mangledName = mangleGenericName(baseName, typeArgs);

    // Already instantiated?
    if (auto* existing = module_->getFunction(mangledName))
        return existing;

    // Cycle detection
    if (instantiatedGenerics_.count(mangledName)) {
        std::cerr << "lucis: recursive generic function instantiation: " << mangledName << "\n";
        return nullptr;
    }
    instantiatedGenerics_.insert(mangledName);

    // Build substitution map
    std::unordered_map<std::string, const TypeInfo*> subst;
    for (size_t i = 0; i < tmpl.typeParams.size() && i < typeArgs.size(); i++)
        subst[tmpl.typeParams[i]] = typeArgs[i];

    // Resolve return type
    auto* retTI = resolveTypeInfoWithSubst(tmpl.decl->typeSpec(), subst);
    if (!retTI) retTI = typeRegistry_.lookup("int32");
    auto retArrayDims = countArrayDims(tmpl.decl->typeSpec());
    auto* retLLTy = retTI->toLLVMType(*context_, module_->getDataLayout());

    // Collect parameter types
    std::vector<llvm::Type*> paramLLTypes;
    std::vector<const TypeInfo*> paramTIs;
    int variadicIdx = -1;
    bool hasTypedVariadic = false;
    auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
    if (auto* paramList = tmpl.decl->paramList()) {
        for (size_t pi = 0; pi < paramList->param().size(); pi++) {
            auto* param = paramList->param(pi);
            // Untyped variadic skip
            if (param->SPREAD() && !param->typeSpec())
                break;
            auto* pTI = resolveTypeInfoWithSubst(param->typeSpec(), subst);
            if (!pTI) pTI = typeRegistry_.lookup("int32");
            paramTIs.push_back(pTI);
            if (param->SPREAD()) {
                // Typed variadic: emit as (ptr, i64) for the packed array
                hasTypedVariadic = true;
                variadicIdx = static_cast<int>(pi);
                paramLLTypes.push_back(llvm::PointerType::getUnqual(*context_));
                paramLLTypes.push_back(llvm::Type::getInt64Ty(*context_));
            } else {
                paramLLTypes.push_back(pTI->toLLVMType(*context_, module_->getDataLayout()));
            }
        }
    }

    auto* fnType = llvm::FunctionType::get(retLLTy, paramLLTypes, false);
    auto* fn = llvm::Function::Create(
        fnType, llvm::Function::ExternalLinkage, mangledName, module_);

    if (hasTypedVariadic && tmpl.decl->paramList()) {
        auto* vParam = tmpl.decl->paramList()->param(variadicIdx);
        auto* vInfo = resolveTypeInfoWithSubst(vParam->typeSpec(), subst);
        variadicFunctions_[mangledName] = {
            static_cast<size_t>(variadicIdx), vInfo
        };
    }

    fnReturnTypes_[mangledName] = retTI;
    fnReturnArrayDims_[mangledName] = retArrayDims;

    // Emit body
    auto* savedFunc = currentFunction_;
    auto  savedLocals = locals_;
    auto* savedBB = builder_->GetInsertBlock();
    auto  savedSubst = currentGenericSubst_;
    currentGenericSubst_ = subst;
    currentFunction_ = fn;
    locals_.clear();

    auto* entry = llvm::BasicBlock::Create(*context_, "entry", fn);
    builder_->SetInsertPoint(entry);

    if (auto* paramList = tmpl.decl->paramList()) {
        auto params = paramList->param();
        size_t llvmIdx = 0;
        for (size_t i = 0; i < params.size(); i++) {
            // Skip untyped variadic ... (no identifier/type to store)
            if (params[i]->SPREAD() && !params[i]->typeSpec())
                continue;

            auto paramName = params[i]->IDENTIFIER()->getText();

            if (params[i]->SPREAD()) {
                // Typed variadic: store ptr and len in separate allocas
                auto* ptrTy = llvm::PointerType::getUnqual(*context_);
                auto* i64Ty = llvm::Type::getInt64Ty(*context_);
                auto* ptrAlloca = builder_->CreateAlloca(ptrTy, nullptr, paramName + ".ptr");
                auto* lenAlloca = builder_->CreateAlloca(i64Ty, nullptr, paramName + ".len");
                builder_->CreateStore(fn->getArg(llvmIdx), ptrAlloca);
                builder_->CreateStore(fn->getArg(llvmIdx + 1), lenAlloca);
                variadicParams_[paramName] = { ptrAlloca, lenAlloca, paramTIs[i] };
                llvmIdx += 2;
            } else {
                auto* arg = fn->getArg(llvmIdx);
                arg->setName(paramName);
                auto* paramLLTy = paramLLTypes[llvmIdx];
                auto* alloca = builder_->CreateAlloca(paramLLTy, nullptr, paramName);
                builder_->CreateStore(arg, alloca);
                auto paramDims = countArrayDims(params[i]->typeSpec());
                locals_[paramName] = { alloca, paramTIs[i], paramDims, true };
                llvmIdx++;
            }
        }
    }

    for (auto* stmt : tmpl.decl->block()->statement())
        visit(stmt);

    if (!builder_->GetInsertBlock()->getTerminator()) {
        if (retLLTy->isVoidTy())
            builder_->CreateRetVoid();
        else
            builder_->CreateRet(llvm::UndefValue::get(retLLTy));
    }

    currentFunction_ = savedFunc;
    locals_ = savedLocals;
    currentGenericSubst_ = savedSubst;
    if (savedBB)
        builder_->SetInsertPoint(savedBB);

    instantiatedGenerics_.erase(mangledName);
    return fn;
}



std::optional<std::vector<const TypeInfo*>> IRGen::inferGenericTypeArgs(
    const std::vector<std::string>& typeParams,
    const std::vector<LucisParser::ParamContext*>& formalParams,
    const std::vector<const TypeInfo*>& argTypes) {

    bool hasTypedVariadic = false;
    if (!formalParams.empty()) {
        auto* last = formalParams.back();
        hasTypedVariadic = last && last->SPREAD() && last->typeSpec();
    }
    size_t fixedCount = hasTypedVariadic ? formalParams.size() - 1 : formalParams.size();

    if (!hasTypedVariadic && formalParams.size() != argTypes.size())
        return std::nullopt;
    if (hasTypedVariadic && argTypes.size() < fixedCount)
        return std::nullopt;

    std::unordered_set<std::string> genericParamSet(typeParams.begin(), typeParams.end());
    std::unordered_map<std::string, const TypeInfo*> inferred;

    // Match fixed params
    for (size_t i = 0; i < fixedCount; i++) {
        auto* formalParam = formalParams[i];
        auto* actualType = argTypes[i];
        if (!formalParam || !formalParam->typeSpec())
            continue;

        if (!typeSpecMentionsGenericParam(formalParam->typeSpec(), genericParamSet))
            continue;

        if (!unifyGenericTypeArg(formalParam->typeSpec(), actualType, genericParamSet,
                                 inferred)) {
            return std::nullopt;
        }
    }

    // Match variadic args against element type
    if (hasTypedVariadic) {
        auto* variadicParam = formalParams.back();
        for (size_t i = fixedCount; i < argTypes.size(); i++) {
            auto* actualType = argTypes[i];
            if (!variadicParam->typeSpec())
                continue;
            if (!typeSpecMentionsGenericParam(variadicParam->typeSpec(), genericParamSet))
                continue;

            if (!unifyGenericTypeArg(variadicParam->typeSpec(), actualType, genericParamSet, inferred))
                return std::nullopt;
        }
    }

    std::vector<const TypeInfo*> typeArgs;
    for (auto& typeParam : typeParams) {
        auto it = inferred.find(typeParam);
        if (it == inferred.end())
            return std::nullopt;
        typeArgs.push_back(it->second);
    }

    return typeArgs;
}



bool IRGen::unifyGenericTypeArg(
    LucisParser::TypeSpecContext* formalType,
    const TypeInfo* actualType,
    const std::unordered_set<std::string>& genericParams,
    std::unordered_map<std::string, const TypeInfo*>& inferred) {

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
            return it->second == actualType || it->second->name == actualType->name;
        }
        return true;
    }

    if (formalType->STAR() && formalType->typeSpec().size() == 1) {
        if (actualType->kind != TypeKind::Pointer || !actualType->pointeeType)
            return false;
        return unifyGenericTypeArg(formalType->typeSpec(0), actualType->pointeeType,
                                   genericParams, inferred);
    }

    if (formalType->VEC()) {
        if (actualType->kind != TypeKind::Extended || actualType->extendedKind != "Vec" ||
            !actualType->elementType || formalType->typeSpec().size() != 1)
            return false;
        return unifyGenericTypeArg(formalType->typeSpec(0), actualType->elementType,
                                   genericParams, inferred);
    }

    if (formalType->SET()) {
        if (actualType->kind != TypeKind::Extended || actualType->extendedKind != "Set" ||
            !actualType->elementType || formalType->typeSpec().size() != 1)
            return false;
        return unifyGenericTypeArg(formalType->typeSpec(0), actualType->elementType,
                                   genericParams, inferred);
    }

    if (formalType->MAP()) {
        if (actualType->kind != TypeKind::Extended || actualType->extendedKind != "Map" ||
            !actualType->keyType || !actualType->valueType || formalType->typeSpec().size() != 2)
            return false;
        return unifyGenericTypeArg(formalType->typeSpec(0), actualType->keyType,
                                   genericParams, inferred)
            && unifyGenericTypeArg(formalType->typeSpec(1), actualType->valueType,
                                   genericParams, inferred);
    }

    if (formalType->TUPLE()) {
        if (actualType->kind != TypeKind::Tuple ||
            actualType->tupleElements.size() != formalType->typeSpec().size())
            return false;
        for (size_t i = 0; i < formalType->typeSpec().size(); i++) {
            if (!unifyGenericTypeArg(formalType->typeSpec(i), actualType->tupleElements[i],
                                     genericParams, inferred))
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
                                     inferred))
                return false;
        }
        return true;
    }

    return true;
}

// ── Generic expression visitors ──────────────────────────────────────────



std::any IRGen::visitGenericFnCallExpr(LucisParser::GenericFnCallExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto funcName = ctx->IDENTIFIER()->getText();

    // Resolve type arguments
    std::vector<const TypeInfo*> typeArgs;
    for (auto* ts : ctx->typeSpec()) {
        auto* argTI = resolveTypeInfo(ts);
        if (!argTI) argTI = typeRegistry_.lookup("int32");
        typeArgs.push_back(argTI);
    }

    // Check if it's a user-defined generic function
    auto funcIt = genericFuncTemplates_.find(funcName);
    llvm::Function* fn = nullptr;
    if (funcIt != genericFuncTemplates_.end()) {
        fn = instantiateGenericFunc(funcName, funcIt->second, typeArgs);
    }

    if (!fn) {
        std::cerr << "lucis: unknown generic function '" << funcName << "'\n";
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }

    // Collect and coerce arguments
    std::vector<llvm::Value*> callArgs;
    if (auto* argList = ctx->argList()) {
        auto* fnType = fn->getFunctionType();
        size_t paramIdx = 0;
        for (auto* argExpr : argList->expression()) {
            auto* argVal = castValue(visit(argExpr));
            if (paramIdx < fnType->getNumParams()) {
                auto* paramTy = fnType->getParamType(paramIdx);
                if (argVal->getType() != paramTy) {
                    if (argVal->getType()->isIntegerTy() && paramTy->isIntegerTy()) {
                        auto* argTI = resolveExprTypeInfo(argExpr);
                        argVal = builder_->CreateIntCast(argVal, paramTy, argTI && argTI->isSigned);
                    } else if (argVal->getType()->isFloatingPointTy() && paramTy->isFloatingPointTy())
                        argVal = builder_->CreateFPCast(argVal, paramTy);
                }
            }
            callArgs.push_back(argVal);
            paramIdx++;
        }
    }

    auto* retTy = fn->getReturnType();
    if (retTy->isVoidTy()) {
        builder_->CreateCall(fn, callArgs);
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }
    return static_cast<llvm::Value*>(
        builder_->CreateCall(fn, callArgs, "generic_call"));
}



std::any IRGen::visitGenericStaticMethodCallExpr(
    LucisParser::GenericStaticMethodCallExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto ids = ctx->IDENTIFIER();
    auto structBaseName = ids[0]->getText();
    auto methodName = ids[1]->getText();

    // Resolve type arguments
    std::vector<const TypeInfo*> typeArgs;
    for (auto* ts : ctx->typeSpec()) {
        auto* argTI = resolveTypeInfo(ts);
        if (!argTI) argTI = typeRegistry_.lookup("int32");
        typeArgs.push_back(argTI);
    }

    auto enumIt = genericEnumTemplates_.find(structBaseName);
    if (enumIt != genericEnumTemplates_.end()) {
        auto* enumType = instantiateGenericEnum(structBaseName, enumIt->second, typeArgs);
        if (!enumType) {
            return static_cast<llvm::Value*>(
                llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
        }

        const EnumVariantInfo* variantInfo = nullptr;
        for (const auto& info : enumType->enumVariantInfos) {
            if (info.name == methodName) {
                variantInfo = &info;
                break;
            }
        }
        if (!variantInfo) {
            std::cerr << "lucis: enum '" << enumType->name
                      << "' has no variant '" << methodName << "'\n";
            return static_cast<llvm::Value*>(
                llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
        }

        std::vector<llvm::Value*> payloadValues;
        if (auto* argList = ctx->argList()) {
            auto args = argList->expression();
            for (size_t i = 0; i < args.size(); ++i) {
                auto* argExpr = args[i];
                const TypeInfo* expectedTI = (i < variantInfo->payloadFields.size())
                    ? variantInfo->payloadFields[i].typeInfo : nullptr;
                if (expectedTI && expectedTI->kind == TypeKind::Extended &&
                    expectedTI->extendedKind == "Vec") {
                    if (auto* arrLit = dynamic_cast<LucisParser::ArrayLitExprContext*>(argExpr)) {
                        payloadValues.push_back(buildVecValueFromArrayLiteral(
                            arrLit, expectedTI, "gen_enum_tuple_vec_payload"));
                        continue;
                    }
                }
                payloadValues.push_back(castValue(visit(argExpr)));
            }
        }
        auto* enumValue = buildEnumVariantValue(enumType, *variantInfo, payloadValues);
        if (auto* argList = ctx->argList()) {
            for (auto* payloadExpr : argList->expression())
                consumeExprIfOwnedLocal(payloadExpr);
        }
        return static_cast<llvm::Value*>(enumValue);
    }

    // Instantiate the generic struct if needed
    auto structIt = genericStructTemplates_.find(structBaseName);
    if (structIt == genericStructTemplates_.end()) {
        std::cerr << "lucis: '" << structBaseName << "' is not a generic struct\n";
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }
    instantiateGenericStruct(structBaseName, structIt->second, typeArgs);

    auto mangledName = mangleGenericName(structBaseName, typeArgs);

    // Find the static method
    auto smIt = staticStructMethods_.find(mangledName);
    if (smIt == staticStructMethods_.end()) {
        std::cerr << "lucis: struct '" << mangledName << "' has no static methods\n";
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }
    auto mIt = smIt->second.find(methodName);
    if (mIt == smIt->second.end()) {
        std::cerr << "lucis: struct '" << mangledName
                  << "' has no static method '" << methodName << "'\n";
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }

    auto* fn = mIt->second.fn;

    // Collect arguments
    std::vector<llvm::Value*> callArgs;
    if (auto* argList = ctx->argList()) {
        auto* fnType = fn->getFunctionType();
        size_t paramIdx = 0;
        for (auto* argExpr : argList->expression()) {
            auto* argVal = castValue(visit(argExpr));
            if (paramIdx < fnType->getNumParams()) {
                auto* paramTy = fnType->getParamType(paramIdx);
                if (argVal->getType() != paramTy) {
                    if (argVal->getType()->isIntegerTy() && paramTy->isIntegerTy())
                        argVal = builder_->CreateIntCast(argVal, paramTy, true);
                    else if (argVal->getType()->isFloatingPointTy() && paramTy->isFloatingPointTy())
                        argVal = builder_->CreateFPCast(argVal, paramTy);
                }
            }
            callArgs.push_back(argVal);
            paramIdx++;
        }
    }

    auto* retTy = fn->getReturnType();
    if (retTy->isVoidTy()) {
        builder_->CreateCall(fn, callArgs);
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }
    return static_cast<llvm::Value*>(
        builder_->CreateCall(fn, callArgs, "generic_static_call"));
}



std::any IRGen::visitGenericQualifiedFnCallExpr(
    LucisParser::GenericQualifiedFnCallExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto ids = ctx->IDENTIFIER();
    if (ids.size() < 2) {
        std::cerr << "lucis: invalid qualified generic call\n";
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }

    // Resolve type arguments
    std::vector<const TypeInfo*> typeArgs;
    for (auto* ts : ctx->typeSpec()) {
        auto* argTI = resolveTypeInfo(ts);
        if (!argTI) argTI = typeRegistry_.lookup("int32");
        typeArgs.push_back(argTI);
    }

    // Check for intrinsic: lucis::unsafe::va_arg<T>(ptr)
    std::vector<std::string> idTexts;
    for (auto* id : ids)
        idTexts.push_back(id->getText());

    // Collect arguments
    std::vector<llvm::Value*> args;
    if (auto* argList = ctx->argList()) {
        for (auto* argExpr : argList->expression())
            args.push_back(castValue(visit(argExpr)));
    }

    if (IntrinsicRegistry::isIntrinsicPrefix(idTexts[0])) {
        std::string ns, funcName;
        if (!IntrinsicRegistry::parseIntrinsicPath(idTexts, ns, funcName)) {
            std::cerr << "lucis: invalid intrinsic qualified generic call\n";
            return static_cast<llvm::Value*>(
                llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
        }

        auto* intrinsic = intrinsicRegistry_.lookup(ns, funcName);
        if (intrinsic && intrinsic->isGeneric) {
            // Apply implicit widening casts for promoted arguments
            for (size_t i = 0; i < args.size() && i < intrinsic->params.size(); i++) {
                auto& expectedTypeName = intrinsic->params[i].type;
                if (expectedTypeName == "_any") continue;
                auto* expectedType = typeRegistry_.lookup(expectedTypeName);
                if (!expectedType) continue;
                auto* llvmExpectedTy = expectedType->toLLVMType(
                    *context_, module_->getDataLayout());
                auto* actualTy = args[i]->getType();
                if (actualTy == llvmExpectedTy) continue;
                
                // Pointer to integer conversion (e.g., ptr -> int64 for syscalls)
                if (actualTy->isPointerTy() && llvmExpectedTy->isIntegerTy()) {
                    args[i] = builder_->CreatePtrToInt(args[i], llvmExpectedTy, "ptr2int");
                }
                // Integer widening
                else if (actualTy->isIntegerTy() && llvmExpectedTy->isIntegerTy() &&
                    actualTy->getIntegerBitWidth() < llvmExpectedTy->getIntegerBitWidth())
                    args[i] = builder_->CreateIntCast(
                        args[i], llvmExpectedTy, expectedType->isSigned, "widen");
                // Float widening
                else if (actualTy->isFloatingPointTy() && llvmExpectedTy->isFloatingPointTy() &&
                         actualTy->getPrimitiveSizeInBits() < llvmExpectedTy->getPrimitiveSizeInBits())
                    args[i] = builder_->CreateFPCast(args[i], llvmExpectedTy, "fwiden");
            }
            return intrinsic->lowering.emitIR(*builder_, module_, *context_,
                                              typeRegistry_, args, typeArgs);
        }

        std::cerr << "lucis: unknown generic intrinsic '" << ns << "::"
                  << funcName << "'\n";
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }

    std::cerr << "lucis: qualified generic call is not supported for non-intrinsics\n";
    return static_cast<llvm::Value*>(
        llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
}



std::any IRGen::visitGenericStructLitExpr(LucisParser::GenericStructLitExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto baseName = ctx->IDENTIFIER(0)->getText();

    // Resolve type arguments
    std::vector<const TypeInfo*> typeArgs;
    for (auto* ts : ctx->typeSpec()) {
        auto* argTI = resolveTypeInfo(ts);
        if (!argTI) argTI = typeRegistry_.lookup("int32");
        typeArgs.push_back(argTI);
    }

    // Ensure the generic aggregate is instantiated
    auto structIt = genericStructTemplates_.find(baseName);
    auto unionIt = genericUnionTemplates_.find(baseName);
    if (structIt == genericStructTemplates_.end() &&
        unionIt == genericUnionTemplates_.end()) {
        std::cerr << "lucis: '" << baseName << "' is not a generic struct or union\n";
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }
    const TypeInfo* instanceTI = nullptr;
    if (structIt != genericStructTemplates_.end())
        instanceTI = instantiateGenericStruct(baseName, structIt->second, typeArgs);
    else
        instanceTI = instantiateGenericUnion(baseName, unionIt->second, typeArgs);
    if (!instanceTI) {
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }

    auto mangledName = mangleGenericName(baseName, typeArgs);
    auto* structLLTy = llvm::StructType::getTypeByName(*context_, mangledName);
    if (!structLLTy && instanceTI->kind == TypeKind::Struct) {
        structLLTy = ensureGenericStructType(mangledName, instanceTI);
    }

    if (instanceTI->kind == TypeKind::Union) {
        if (!structLLTy) {
            std::cerr << "lucis: generic union LLVM type missing for '" << mangledName << "'\n";
            return static_cast<llvm::Value*>(
                llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
        }

        auto* alloca = builder_->CreateAlloca(structLLTy, nullptr, mangledName + ".tmp");
        auto ids = ctx->IDENTIFIER();
        auto exprs = ctx->expression();
        if (!exprs.empty()) {
            auto fieldName = ids[1]->getText();
            const TypeInfo* fieldTI = nullptr;
            for (auto& field : instanceTI->fields) {
                if (field.name == fieldName) {
                    fieldTI = field.typeInfo;
                    break;
                }
            }
            if (!fieldTI) {
                std::cerr << "lucis: unknown field '" << fieldName
                          << "' in generic union literal '" << mangledName << "'\n";
                return static_cast<llvm::Value*>(
                    llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
            }

            auto* val = castValue(visit(exprs[0]));
            auto* fieldLLTy = fieldTI->toLLVMType(*context_, module_->getDataLayout());
            if (val->getType() != fieldLLTy) {
                if (val->getType()->isIntegerTy() && fieldLLTy->isIntegerTy())
                    val = builder_->CreateIntCast(val, fieldLLTy, true);
                else if (val->getType()->isFloatingPointTy() && fieldLLTy->isFloatingPointTy()) {
                    if (val->getType()->getPrimitiveSizeInBits() > fieldLLTy->getPrimitiveSizeInBits())
                        val = builder_->CreateFPTrunc(val, fieldLLTy);
                    else
                        val = builder_->CreateFPExt(val, fieldLLTy);
                }
            }
            builder_->CreateStore(val, alloca);
        }

        return static_cast<llvm::Value*>(
            builder_->CreateLoad(structLLTy, alloca, "generic_union_lit"));
    }

    // Allocate the struct on the stack
    auto* alloca = builder_->CreateAlloca(structLLTy, nullptr, mangledName + ".tmp");

    // Build a map from field name → index
    std::unordered_map<std::string, unsigned> fieldIndex;
    for (unsigned i = 0; i < instanceTI->fields.size(); i++)
        fieldIndex[instanceTI->fields[i].name] = i;

    // Get field initializers from the literal
    auto ids = ctx->IDENTIFIER();      // field names (includes base struct name at [0]? No — it's separate)
    auto exprs = ctx->expression();

    // NOTE: The grammar rule is:
    //   IDENTIFIER LT typeSpec* GT LBRACE (IDENTIFIER COLON expression ...)? RBRACE
    // IDENTIFIER is the struct name (already used for baseName above).
    // The field IDENTIFIER/expression pairs come from the repeated IDENTIFIER COLON expression.
    // In the generated context, IDENTIFIER() returns all IDENTIFIERs including the base name.
    // Field names start at IDENTIFIER(0) (the struct base name is NOT in ctx->IDENTIFIER() for
    // the field list — it's the primary IDENTIFIER before LT).
    // Let's check: the grammar has: IDENTIFIER LT ... GT LBRACE (IDENTIFIER COLON expression)* RBRACE
    // ctx->IDENTIFIER() returns ALL identifiers matched: [0]=struct name, [1..n]=field names.
    // So field names start at index 1.

    // Initialize all fields to zero first
    for (unsigned i = 0; i < instanceTI->fields.size(); i++) {
        auto* fieldTI = instanceTI->fields[i].typeInfo;
        auto* fieldLLTy = fieldTI->toLLVMType(*context_, module_->getDataLayout());
        auto* gep = builder_->CreateStructGEP(structLLTy, alloca, i);
        builder_->CreateStore(llvm::Constant::getNullValue(fieldLLTy), gep);
    }

    // Apply provided field initializers
    for (size_t i = 0; i < exprs.size(); i++) {
        auto fieldName = ids[i + 1]->getText(); // field names start at [1]
        auto it = fieldIndex.find(fieldName);
        if (it == fieldIndex.end()) {
            std::cerr << "lucis: unknown field '" << fieldName
                      << "' in generic struct literal '" << mangledName << "'\n";
            continue;
        }
        auto* gep = builder_->CreateStructGEP(structLLTy, alloca, it->second);
        auto* fieldTI = instanceTI->fields[it->second].typeInfo;
        auto* fieldLLTy = fieldTI->toLLVMType(*context_, module_->getDataLayout());
        llvm::Value* val = nullptr;
        if (fieldTI && fieldTI->kind == TypeKind::Extended &&
            fieldTI->extendedKind == "Vec") {
            if (auto* arrLit = dynamic_cast<LucisParser::ArrayLitExprContext*>(exprs[i])) {
                val = buildVecValueFromArrayLiteral(arrLit, fieldTI,
                                                    mangledName + "_" + fieldName + "_vec");
            }
        }
        if (!val)
            val = castValue(visit(exprs[i]));
        if (val->getType() != fieldLLTy) {
            if (val->getType()->isIntegerTy() && fieldLLTy->isIntegerTy())
                val = builder_->CreateIntCast(val, fieldLLTy, true);
            else if (val->getType()->isPointerTy() && fieldLLTy->isPointerTy())
                ; // ok — opaque pointers
        }
        builder_->CreateStore(val, gep);
    }

    // Return the value by loading the struct
    return static_cast<llvm::Value*>(
        builder_->CreateLoad(structLLTy, alloca, "generic_struct_lit"));
}

// ── Generic struct positional init: Name<T> { expr, expr, ... } ──────



std::any IRGen::visitGenericStructPosInitExpr(LucisParser::GenericStructPosInitExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto baseName = ctx->IDENTIFIER()->getText();

    std::vector<const TypeInfo*> typeArgs;
    for (auto* ts : ctx->typeSpec()) {
        auto* argTI = resolveTypeInfo(ts);
        if (!argTI) argTI = typeRegistry_.lookup("int32");
        typeArgs.push_back(argTI);
    }

    auto structIt = genericStructTemplates_.find(baseName);
    auto unionIt = genericUnionTemplates_.find(baseName);
    if (structIt == genericStructTemplates_.end() &&
        unionIt == genericUnionTemplates_.end()) {
        std::cerr << "lucis: '" << baseName << "' is not a generic struct or union\n";
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }
    const TypeInfo* instanceTI = nullptr;
    if (structIt != genericStructTemplates_.end())
        instanceTI = instantiateGenericStruct(baseName, structIt->second, typeArgs);
    else
        instanceTI = instantiateGenericUnion(baseName, unionIt->second, typeArgs);
    if (!instanceTI) {
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }

    auto mangledName = mangleGenericName(baseName, typeArgs);
    auto* structLLTy = llvm::StructType::getTypeByName(*context_, mangledName);
    if (!structLLTy && instanceTI->kind == TypeKind::Struct) {
        structLLTy = ensureGenericStructType(mangledName, instanceTI);
    }

    auto exprs = ctx->expression();

    if (instanceTI->kind == TypeKind::Union) {
        if (!structLLTy) {
            std::cerr << "lucis: generic union LLVM type missing for '" << mangledName << "'\n";
            return static_cast<llvm::Value*>(
                llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
        }

        auto* alloca = builder_->CreateAlloca(structLLTy, nullptr, mangledName + ".tmp");
        if (!exprs.empty()) {
            auto* fieldTI = instanceTI->fields[0].typeInfo;
            if (fieldTI) {
                auto* val = castValue(visit(exprs[0]));
                auto* fieldLLTy = fieldTI->toLLVMType(*context_, module_->getDataLayout());
                if (val->getType() != fieldLLTy) {
                    if (val->getType()->isIntegerTy() && fieldLLTy->isIntegerTy())
                        val = builder_->CreateIntCast(val, fieldLLTy, true);
                    else if (val->getType()->isFloatingPointTy() && fieldLLTy->isFloatingPointTy()) {
                        if (val->getType()->getPrimitiveSizeInBits() > fieldLLTy->getPrimitiveSizeInBits())
                            val = builder_->CreateFPTrunc(val, fieldLLTy);
                        else
                            val = builder_->CreateFPExt(val, fieldLLTy);
                    }
                }
                builder_->CreateStore(val, alloca);
            }
        }

        return static_cast<llvm::Value*>(
            builder_->CreateLoad(structLLTy, alloca, "generic_union_lit"));
    }

    // Allocate the struct on the stack
    auto* alloca = builder_->CreateAlloca(structLLTy, nullptr, mangledName + ".tmp");

    // Initialize all fields to zero first
    for (unsigned i = 0; i < instanceTI->fields.size(); i++) {
        auto* fieldTI = instanceTI->fields[i].typeInfo;
        auto* fieldLLTy = fieldTI->toLLVMType(*context_, module_->getDataLayout());
        auto* gep = builder_->CreateStructGEP(structLLTy, alloca, i);
        builder_->CreateStore(llvm::Constant::getNullValue(fieldLLTy), gep);
    }

    // Apply provided field initializers by position
    for (size_t i = 0; i < exprs.size() && i < instanceTI->fields.size(); i++) {
        auto* fieldTI = instanceTI->fields[i].typeInfo;
        auto* fieldLLTy = fieldTI->toLLVMType(*context_, module_->getDataLayout());
        auto* gep = builder_->CreateStructGEP(structLLTy, alloca, static_cast<unsigned>(i));
        llvm::Value* val = nullptr;
        if (fieldTI && fieldTI->kind == TypeKind::Extended &&
            fieldTI->extendedKind == "Vec") {
            if (auto* arrLit = dynamic_cast<LucisParser::ArrayLitExprContext*>(exprs[i])) {
                val = buildVecValueFromArrayLiteral(arrLit, fieldTI,
                                                    mangledName + "_pos_vec");
            }
        }
        if (!val)
            val = castValue(visit(exprs[i]));
        if (val->getType() != fieldLLTy) {
            if (val->getType()->isIntegerTy() && fieldLLTy->isIntegerTy())
                val = builder_->CreateIntCast(val, fieldLLTy, true);
            else if (val->getType()->isPointerTy() && fieldLLTy->isPointerTy())
                ; // ok — opaque pointers
        }
        builder_->CreateStore(val, gep);
    }

    return static_cast<llvm::Value*>(
        builder_->CreateLoad(structLLTy, alloca, "generic_struct_lit"));
}

// ── Attribute helpers ───────────────────────────────────────────────


