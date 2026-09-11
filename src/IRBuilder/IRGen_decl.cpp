#include "IRBuilder/IRGen.h"
#include "IRBuilder/IRGen_helpers.h"
#include "imports/UseParser.h"

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



std::any IRGen::visitStructDecl(LucisParser::StructDeclContext* ctx) {
        SET_DBG_LOC(ctx);
    // Generic struct template — register as template, do NOT emit LLVM struct yet
    if (auto* tpl = ctx->typeParamList()) {
        GenericStructTemplate tmpl;
        for (auto* tp : tpl->typeParam())
            tmpl.typeParams.push_back(tp->IDENTIFIER(0)->getText());
        tmpl.decl = ctx;
        auto name = ctx->IDENTIFIER()->getText();
        genericStructTemplates_[name] = std::move(tmpl);
        return {};
    }

    auto structName = ctx->IDENTIFIER()->getText();

    if (auto* existing = typeRegistry_.lookup(structName)) {
        bool alreadyComplete = existing->kind == TypeKind::Struct && !existing->fields.empty();
        bool isSkeleton = existing->kind == TypeKind::Struct && existing->fields.empty();
        if (alreadyComplete) return {};
        if (!isSkeleton)
            return {};
    }

    // Create opaque LLVM struct first (enables self-referencing pointer fields)
    auto* structType = llvm::StructType::getTypeByName(*context_, structName);
    if (!structType)
        structType = llvm::StructType::create(*context_, structName);
    if (!structType->isOpaque())
        return {};

    // Register skeleton in TypeRegistry so resolveTypeInfo can find it
    TypeInfo ti;
    ti.name = structName;
    ti.kind = TypeKind::Struct;
    ti.bitWidth = 0;
    ti.isSigned = false;
    if (!typeRegistry_.lookup(structName))
        typeRegistry_.registerType(ti);

    // Collect field types
    std::vector<llvm::Type*> fieldTypes;
    // ── Parent struct inheritance ─────────────────────────────────────
    if (ctx->COLON() && ctx->typeSpec()) {
        auto* parentTI = resolveTypeInfo(ctx->typeSpec());
        if (!parentTI || parentTI->kind != TypeKind::Struct || parentTI->fields.empty()) {
            std::cerr << "lucis: parent struct not resolved for struct '" << structName << "'\n";
            return {};
        }
        ti.parentType = parentTI;

        auto* parentLL = llvm::StructType::getTypeByName(*context_, parentTI->name);
        if (!parentLL) {
            parentLL = llvm::StructType::create(*context_, parentTI->name);
        }
        if (parentLL->isOpaque()) {
            return {};
        }
        for (unsigned i = 0; i < parentLL->getNumElements(); i++)
            fieldTypes.push_back(parentLL->getElementType(i));
        for (auto& pf : parentTI->fields)
            ti.fields.push_back(pf);
    }

    for (auto* field : ctx->structField()) {
        auto* fieldTI = resolveTypeInfo(field->typeSpec());
        if (!fieldTI) {
            std::cerr << "lucis: cannot resolve field type in struct '" + structName + "'\n";
            continue;
        }
        // Extract array dims/sizes from the typeSpec (e.g. [256]Token → dims=1, sizes={256})
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
            // Unsized array field []T — represent as slice {T*, i64}
            auto* ptrTy = llvm::PointerType::getUnqual(*context_);
            auto* i64Ty = llvm::Type::getInt64Ty(*context_);
            fieldLLTy = llvm::StructType::get(*context_, {ptrTy, i64Ty});
        }
        fieldTypes.push_back(fieldLLTy);
        if (field->IDENTIFIER()) {
            ti.fields.push_back({ field->IDENTIFIER()->getText(), fieldTI, fieldDims, fieldSizes });
        }
    }

    // Check for #[repr(packed)] — no padding between fields
    bool isPacked = hasReprPacked(ctx->attributeList());

    // Set the struct body now that all fields are resolved
    structType->setBody(fieldTypes, isPacked);

    // Update registry with full type info (fields populated)
    typeRegistry_.registerType(std::move(ti));
    return {};
}



std::any IRGen::visitUnionDecl(LucisParser::UnionDeclContext* ctx) {
        SET_DBG_LOC(ctx);
    if (auto* tpl = ctx->typeParamList()) {
        GenericUnionTemplate tmpl;
        for (auto* tp : tpl->typeParam())
            tmpl.typeParams.push_back(tp->IDENTIFIER(0)->getText());
        tmpl.decl = ctx;
        auto name = ctx->IDENTIFIER()->getText();
        genericUnionTemplates_[name] = std::move(tmpl);
        return {};
    }

    auto unionName = ctx->IDENTIFIER()->getText();
    if (auto* existing = typeRegistry_.lookup(unionName)) {
        bool alreadyComplete = existing->kind == TypeKind::Union && !existing->fields.empty();
        bool isSkeleton = existing->kind == TypeKind::Union && existing->fields.empty();
        if (alreadyComplete) return {};
        if (!isSkeleton)
            return {};
    }

    // Collect field types and compute the max field size
    std::vector<FieldInfo> fieldInfos;
    uint64_t maxSize = 0;

    for (auto* field : ctx->unionField()) {
        auto* fieldTI = resolveTypeInfo(field->typeSpec());
        if (!fieldTI) {
            std::cerr << "lucis: cannot resolve field type in union\n";
            continue;
        }
        auto* fieldLLTy = fieldTI->toLLVMType(*context_, module_->getDataLayout());
        uint64_t fieldSize = module_->getDataLayout().getTypeAllocSize(fieldLLTy);
        if (fieldSize > maxSize) maxSize = fieldSize;
        if (field->IDENTIFIER()) {
            fieldInfos.push_back({ field->IDENTIFIER()->getText(), fieldTI });
        }
    }

    if (maxSize == 0) maxSize = 1;

    // Create LLVM struct with a single [maxSize x i8] body
    auto* i8Ty = llvm::Type::getInt8Ty(*context_);
    auto* bodyTy = llvm::ArrayType::get(i8Ty, maxSize);
    auto* unionType = llvm::StructType::getTypeByName(*context_, unionName);
    if (!unionType) {
        unionType = llvm::StructType::create(*context_, { bodyTy }, unionName);
    } else if (unionType->isOpaque()) {
        unionType->setBody({ bodyTy });
    }
    (void)unionType;

    // Register in TypeRegistry
    TypeInfo ti;
    ti.name = unionName;
    ti.kind = TypeKind::Union;
    ti.bitWidth = 0;
    ti.isSigned = false;
    ti.fields = std::move(fieldInfos);
    typeRegistry_.registerType(std::move(ti));

    return {};
}



std::any IRGen::visitEnumDecl(LucisParser::EnumDeclContext* ctx) {
        SET_DBG_LOC(ctx);
    auto enumName = ctx->IDENTIFIER()->getText();

    if (auto* tpl = ctx->typeParamList()) {
        GenericEnumTemplate tmpl;
        for (auto* tp : tpl->typeParam())
            tmpl.typeParams.push_back(tp->IDENTIFIER(0)->getText());
        tmpl.decl = ctx;
        genericEnumTemplates_[enumName] = std::move(tmpl);
        return {};
    }

    TypeInfo ti;
    ti.name = enumName;
    ti.kind = TypeKind::Enum;
    ti.bitWidth = 32;
    ti.isSigned = false;
    ti.builtinSuffix = "i32";

    unsigned nextDiscriminant = 0;
    for (auto* variantDecl : ctx->enumVariant()) {
        auto variantName = variantDecl->IDENTIFIER()->getText();
        ti.enumVariants.push_back(variantName);

        EnumVariantInfo info;
        info.name = variantName;
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
                auto* fieldTI = resolveTypeInfo(payloadTypes[i]);
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
                auto* fieldTI = resolveTypeInfo(payloadField->typeSpec());
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
    return {};
}



std::any IRGen::visitBitstructDecl(LucisParser::BitstructDeclContext* ctx) {
        SET_DBG_LOC(ctx);
    auto name = ctx->IDENTIFIER()->getText();

    // Resolve backing type
    bool isAuto = ctx->typeSpec() && ctx->typeSpec()->AUTO() != nullptr;
    const TypeInfo* backType = nullptr;
    if (!isAuto)
        backType = resolveTypeInfo(ctx->typeSpec());
    if (!backType && !isAuto) return {};

    TypeInfo ti;
    ti.name = name;
    ti.kind = TypeKind::BitStruct;
    ti.bitWidth = 0;
    ti.isSigned = false;

    // First pass: compute total bits for auto backing type
    unsigned totalBits = 0;
    unsigned runningOffset = 0;
    if (isAuto) {
        for (auto* field : ctx->bitstructField()) {
            auto ints = field->INT_LIT();
            if (ints.size() >= 1)
                totalBits += std::stoul(ints.back()->getText());
        }
        unsigned backBits = 0;
        if (totalBits <= 8) backBits = 8;
        else if (totalBits <= 16) backBits = 16;
        else if (totalBits <= 32) backBits = 32;
        else if (totalBits <= 64) backBits = 64;
        else if (totalBits <= 128) backBits = 128;
        else return {};
        auto typeName = "uint" + std::to_string(backBits);
        backType = typeRegistry_.lookup(typeName);
        if (!backType) return {};
    }
    ti.backingType = backType;
    ti.bitWidth = backType->bitWidth == 0 ? 64 : backType->bitWidth;
    for (auto* field : ctx->bitstructField()) {
        auto ints = field->INT_LIT();
        auto* id = field->IDENTIFIER();

        if (ints.size() == 2) {
            unsigned width = std::stoul(ints[1]->getText());
            BitFieldInfo bfi;
            bfi.offset = runningOffset;
            bfi.width = width;
            bfi.isPadding = true;
            ti.bitFields.push_back(bfi);
            runningOffset += width;
        } else if (ints.size() == 1 && id) {
            unsigned width = std::stoul(ints[0]->getText());
            BitFieldInfo bfi;
            bfi.name = id->getText();
            bfi.offset = runningOffset;
            bfi.width = width;
            bfi.isPadding = false;
            ti.bitFields.push_back(bfi);
            runningOffset += width;
        }
    }

    typeRegistry_.registerType(std::move(ti));
    return {};
}

// ═══════════════════════════════════════════════════════════════════════
//  Operator internal name helpers
// ═══════════════════════════════════════════════════════════════════════



std::any IRGen::visitExtendDecl(LucisParser::ExtendDeclContext* ctx) {
        SET_DBG_LOC(ctx);
    auto structName = ctx->IDENTIFIER()->getText();

    // Generic extend template — register but don't emit yet
    if (auto* tpl = ctx->typeParamList()) {
        GenericExtendTemplate tmpl;
        for (auto* tp : tpl->typeParam())
            tmpl.typeParams.push_back(tp->IDENTIFIER(0)->getText());
        tmpl.decl = ctx;
        genericExtendTemplates_[structName] = std::move(tmpl);
        return {};
    }

    auto* structTI = typeRegistry_.lookup(structName);
    if (!structTI || structTI->kind != TypeKind::Struct) {
        std::cerr << "lucis: cannot extend unknown struct '" << structName << "'\n";
        return {};
    }

    auto* structLLTy = structTI->toLLVMType(*context_, module_->getDataLayout());
    auto* ptrTy = llvm::PointerType::getUnqual(*context_);

     struct ExtendMethodLoweringInfo {
        LucisParser::ExtendMethodContext* method;
        LucisParser::BlockContext* block;
        bool isStatic;
        std::string methodName;
        std::vector<LucisParser::ParamContext*> params;
        std::vector<const TypeInfo*> paramTIs;
        std::vector<llvm::Type*> paramLLTypes;
        const TypeInfo* retTI = nullptr;
        llvm::Type* retLLTy = nullptr;
        llvm::Function* fn = nullptr;
    };

    std::vector<ExtendMethodLoweringInfo> loweringInfos;
    loweringInfos.reserve(ctx->extendMethod().size());

    // Pass 1: declare every method before lowering any body.
    // This allows forward calls between methods inside the same extend block.
    for (auto* method : ctx->extendMethod()) {
        // ── Operator overload ──
        if (auto* opDecl = method->operatorDecl()) {
            auto internalName = opInternalName(opDecl->operatorName());
            auto* retTI = resolveTypeInfo(opDecl->typeSpec());
            if (!retTI) return {};
            auto* retLLTy = retTI->toLLVMType(*context_, module_->getDataLayout());

            bool isInstance = (opDecl->AMPERSAND() != nullptr);

            std::vector<llvm::Type*> paramLLTypes;
            std::vector<const TypeInfo*> paramTIs;

            if (isInstance)
                paramLLTypes.push_back(ptrTy);

            std::vector<LucisParser::ParamContext*> params;
            if (isInstance) {
                params = opDecl->param();
            } else if (auto* pl = opDecl->paramList()) {
                params = pl->param();
            }

            for (auto* param : params) {
                auto* pTI = resolveTypeInfo(param->typeSpec());
                if (!pTI) pTI = typeRegistry_.lookup("int32");
                paramTIs.push_back(pTI);
                paramLLTypes.push_back(pTI->toLLVMType(*context_, module_->getDataLayout()));
            }

            auto baseName = structName + "__" + internalName;
            auto funcName = baseName + "_" + std::to_string(paramLLTypes.size());
            auto* fnType = llvm::FunctionType::get(retLLTy, paramLLTypes, false);
            auto* fn = module_->getFunction(funcName);
            if (!fn) {
                fn = llvm::Function::Create(
                    fnType, llvm::Function::ExternalLinkage, funcName, module_);
            }

            operatorMethods_[structName][internalName].push_back({ fn, retTI });

            loweringInfos.push_back(ExtendMethodLoweringInfo{
                method,
                opDecl->block(),
                !isInstance,
                internalName,
                std::move(params),
                std::move(paramTIs),
                std::move(paramLLTypes),
                retTI,
                retLLTy,
                fn
            });
            continue;
        }

        auto methodName = method->name->getText();
        auto* retTI = resolveTypeInfo(method->typeSpec());
        if (!retTI) return {};
        auto* retLLTy = retTI->toLLVMType(*context_, module_->getDataLayout());

        bool isStatic = (method->AMPERSAND() == nullptr);

        std::vector<llvm::Type*> paramLLTypes;
        std::vector<const TypeInfo*> paramTIs;

        if (!isStatic)
            paramLLTypes.push_back(ptrTy); // &self as first param

        std::vector<LucisParser::ParamContext*> params;
        if (isStatic) {
            if (auto* pl = method->paramList())
                params = pl->param();
        } else {
            params = method->param();
        }

        for (auto* param : params) {
            auto* pTI = resolveTypeInfo(param->typeSpec());
            if (!pTI) pTI = typeRegistry_.lookup("int32");
            paramTIs.push_back(pTI);
            paramLLTypes.push_back(pTI->toLLVMType(*context_, module_->getDataLayout()));
        }

        auto funcName = structName + "__" + methodName;
        auto* fnType = llvm::FunctionType::get(retLLTy, paramLLTypes, false);
        auto* fn = module_->getFunction(funcName);
        if (!fn) {
            fn = llvm::Function::Create(
                fnType, llvm::Function::ExternalLinkage, funcName, module_);
        }

        if (isStatic)
            staticStructMethods_[structName][methodName] = { fn, retTI };
        else
            structMethods_[structName][methodName] = { fn, retTI };

        if (methodName == "drop" && !isStatic && params.empty()) {
            if (auto* mutTI = typeRegistry_.lookupMutable(structName))
                mutTI->dropTracked = true;
        }

        loweringInfos.push_back(ExtendMethodLoweringInfo{
            method,
            method->block(),
            isStatic,
            methodName,
            std::move(params),
            std::move(paramTIs),
            std::move(paramLLTypes),
            retTI,
            retLLTy,
            fn
        });
    }

    // Pass 2: lower method bodies.
    for (auto& info : loweringInfos) {
        auto* method = info.method;
        auto* fn = info.fn;
        bool isStatic = info.isStatic;
        auto* retLLTy = info.retLLTy;

        // Save state
        auto* savedFunc = currentFunction_;
        auto  savedLocals = locals_;
        auto* savedBB = builder_->GetInsertBlock();
        currentFunction_ = fn;
        locals_.clear();

        // Create entry block
        auto* entry = llvm::BasicBlock::Create(*context_, "entry", fn);
        builder_->SetInsertPoint(entry);

        if (!isStatic) {
            // Set up &self parameter
            auto* selfArg = fn->getArg(0);
            selfArg->setName("self");
            auto* selfAlloca = builder_->CreateAlloca(ptrTy, nullptr, "self");
            builder_->CreateStore(selfArg, selfAlloca);

            // Create pointer-to-struct TypeInfo for `self`
            auto ptrTIName = "*" + structName;
            if (!typeRegistry_.lookup(ptrTIName)) {
                TypeInfo ptrTI;
                ptrTI.name = ptrTIName;
                ptrTI.kind = TypeKind::Pointer;
                ptrTI.bitWidth = 0;
                ptrTI.isSigned = false;
                ptrTI.builtinSuffix = "ptr";
                ptrTI.pointeeType = structTI;
                typeRegistry_.registerType(std::move(ptrTI));
            }
            locals_["self"] = { selfAlloca, typeRegistry_.lookup(ptrTIName), 0 };
        }

        // Set up parameters
        size_t argOffset = isStatic ? 0 : 1;
        for (size_t i = 0; i < info.params.size(); i++) {
            auto paramName = info.params[i]->IDENTIFIER()->getText();
            auto* arg = fn->getArg(i + argOffset);
            arg->setName(paramName);
            auto* paramLLTy = info.paramLLTypes[i + argOffset];
            auto* alloca = builder_->CreateAlloca(paramLLTy, nullptr, paramName);
            builder_->CreateStore(arg, alloca);
            locals_[paramName] = { alloca, info.paramTIs[i], 0, /*isParam=*/true };
        }

        // Visit the method body statements directly
        auto* body = info.block ? info.block : method->block();
        auto savedRetType = currentFnReturnType_;
        currentFnReturnType_ = info.retTI;
        if (body) {
            for (auto* stmt : body->statement()) {
                visit(stmt);
            }
        }
        currentFnReturnType_ = savedRetType;

        // If no terminator, add implicit return
        if (!builder_->GetInsertBlock()->getTerminator()) {
            if (retLLTy->isVoidTy())
                builder_->CreateRetVoid();
            else
                builder_->CreateRet(llvm::UndefValue::get(retLLTy));
        }

        // Restore state
        currentFunction_ = savedFunc;
        locals_ = savedLocals;
        if (savedBB)
            builder_->SetInsertPoint(savedBB);
    }

    return {};
}

// ── Trait declaration ─────────────────────────────────────────────────────────



std::any IRGen::visitTraitDecl(LucisParser::TraitDeclContext* ctx)
{
    auto name = ctx->IDENTIFIER()->getText();

    // Handle generic trait — store as template
    if (auto* tpl = ctx->typeParamList()) {
        GenericTraitTemplate tmpl;
        for (auto* tp : tpl->typeParam())
            tmpl.typeParams.push_back(tp->IDENTIFIER(0)->getText());
        tmpl.decl = ctx;
        genericTraitTemplates_[name] = std::move(tmpl);
        return {};
    }

    // Register non-generic trait in IRGen's trait registry
    TraitInfo ti;
    ti.name = name;
    ti.modulePath = currentModulePath_;
    ti.decl = ctx;

    for (auto* method : ctx->traitMethod()) {
        TraitMethodInfo mi;
        mi.name = method->name->getText();
        mi.hasSelfReceiver = (method->AMPERSAND() != nullptr);
        mi.isStatic = !mi.hasSelfReceiver;

        unsigned retDims = 0;
        mi.returnType = resolveTypeInfo(method->typeSpec());

        if (mi.isStatic) {
            if (auto* pl = method->paramList()) {
                for (auto* p : pl->param()) {
                    mi.paramTypes.push_back(resolveTypeInfo(p->typeSpec()));
                }
            }
        } else {
            for (auto* p : method->param()) {
                mi.paramTypes.push_back(resolveTypeInfo(p->typeSpec()));
            }
        }

        ti.methods.push_back(mi);
    }

    traitRegistry_.registerTrait(std::move(ti));
    return {};
}

// ── Impl declaration ──────────────────────────────────────────────────────────



std::any IRGen::visitImplDecl(LucisParser::ImplDeclContext* ctx)
{
    // Handle generic impl — store as template
    if (auto* tpl = ctx->typeParamList()) {
        GenericImplTemplate tmpl;
        for (auto* tp : tpl->typeParam())
            tmpl.typeParams.push_back(tp->IDENTIFIER(0)->getText());
        tmpl.decl = ctx;
        auto implTypeName = ctx->typeSpec(1)->getText();
        genericImplTemplates_[implTypeName] = std::move(tmpl);
        return {};
    }

    // Resolve the implementing type
    const TypeInfo* implTI = resolveTypeInfo(ctx->typeSpec(1));
    if (!implTI) return {};

    // Resolve the trait via the type system (will find it in traitRegistry_)
    const TypeInfo* traitTI = resolveTypeInfo(ctx->typeSpec(0));
    if (!traitTI || traitTI->kind != TypeKind::Trait) return {};

    const TraitInfo* trait = traitRegistry_.lookup(traitTI->name);
    TraitInfo genericStorage;
    if (!trait && genericTraitTemplates_.count(traitTI->name)) {
        auto& gtmpl = genericTraitTemplates_[traitTI->name];
        genericStorage.name = traitTI->name;
        std::unordered_map<std::string, const TypeInfo*> subst;
        if (gtmpl.typeParams.size() == ctx->typeSpec(0)->typeSpec().size()) {
            for (size_t i = 0; i < gtmpl.typeParams.size(); ++i)
                subst[gtmpl.typeParams[i]] = resolveTypeInfo(ctx->typeSpec(0)->typeSpec(i));
        }
        for (auto* m : gtmpl.decl->traitMethod()) {
            TraitMethodInfo mi;
            mi.name = m->name->getText();
            mi.hasSelfReceiver = (m->AMPERSAND() != nullptr);
            mi.isStatic = !mi.hasSelfReceiver;
            mi.returnType = subst.empty() ? resolveTypeInfo(m->typeSpec())
                                          : resolveTypeInfoWithSubst(m->typeSpec(), subst);
            auto addParams = [&](auto* pl, auto& params) {
                if (!pl) return;
                for (auto* p : pl->param()) {
                    params.push_back(subst.empty() ? resolveTypeInfo(p->typeSpec())
                                                   : resolveTypeInfoWithSubst(p->typeSpec(), subst));
                }
            };
            if (mi.isStatic) addParams(m->paramList(), mi.paramTypes);
            else for (auto* p : m->param()) mi.paramTypes.push_back(
                subst.empty() ? resolveTypeInfo(p->typeSpec())
                              : resolveTypeInfoWithSubst(p->typeSpec(), subst));
            genericStorage.methods.push_back(mi);
        }
        trait = &genericStorage;
    }
    if (!trait) return {};

    // Register ImplEntry
    ImplEntry entry;
    entry.traitName = trait->name;
    entry.typeName = implTI->name;
    entry.implType = implTI;
    entry.decl = ctx;
    entry.sourceModulePath = currentModulePath_;
    traitImpls_[entry.traitName].push_back(std::move(entry));

    // Register impl in trait registry
    ImplInfo implInfo;
    implInfo.traitName = trait->name;
    implInfo.typeName = implTI->name;
    implInfo.decl = ctx;
    implInfo.modulePath = currentModulePath_;
    traitRegistry_.registerImpl(std::move(implInfo));

    // Mark the type as implementing this trait
    auto* mutableTI = typeRegistry_.lookupMutable(implTI->name);
    if (mutableTI)
        mutableTI->implementedTraits.insert(trait->name);

    // Generate LLVM functions for each impl method
    for (auto* method : ctx->implMethod()) {
        auto methodName = method->name->getText();
        bool isStatic = (method->AMPERSAND() == nullptr);

        auto mangledFnName = mangleImplMethodName(
            trait->name, implTI->name, methodName);

        // A forward declaration may already exist (created by
        // declareImplMethods during cross-file registration). Reuse it and
        // lower the body into it; only skip when a body already exists.
        auto loweredIt = loweredImplMethods_.find(mangledFnName);
        if (loweredIt != loweredImplMethods_.end() &&
            loweredIt->second && !loweredIt->second->empty())
            continue;

        // Resolve return type
        const TypeInfo* retTI = resolveTypeInfo(method->typeSpec());

        // Build LLVM function type
        std::vector<llvm::Type*> paramLLTypes;

        // Instance methods: first param is &self (pointer to impl type)
        if (!isStatic) {
            auto* llvmType = implTI->toLLVMType(*context_, module_->getDataLayout());
            if (!llvmType) continue;
            auto* ptrTy = llvm::PointerType::getUnqual(llvmType);
            paramLLTypes.push_back(ptrTy);
        }

        // Method params
        if (isStatic) {
            if (auto* pl = method->paramList()) {
                for (auto* p : pl->param()) {
                    if (p->SPREAD()) continue;
                    unsigned dims = 0;
                    const TypeInfo* pTI = resolveTypeInfo(p->typeSpec());
                    if (!pTI) continue;
                    auto* pLL = pTI->toLLVMType(*context_, module_->getDataLayout());
                    if (pLL) paramLLTypes.push_back(pLL);
                }
            }
        } else {
            for (auto* p : method->param()) {
                if (p->SPREAD()) continue;
                unsigned dims = 0;
                const TypeInfo* pTI = resolveTypeInfo(p->typeSpec());
                if (!pTI) continue;
                auto* pLL = pTI->toLLVMType(*context_, module_->getDataLayout());
                if (pLL) paramLLTypes.push_back(pLL);
            }
        }

        llvm::Type* retLLType = llvm::Type::getVoidTy(*context_);
        if (retTI && retTI->kind != TypeKind::Void) {
            retLLType = retTI->toLLVMType(*context_, module_->getDataLayout());
            if (!retLLType) retLLType = llvm::Type::getVoidTy(*context_);
        }

        llvm::Function* fn;
        if (loweredIt != loweredImplMethods_.end() && loweredIt->second) {
            // Reuse the forward-declared function.
            fn = loweredIt->second;
            auto* fnType = llvm::FunctionType::get(retLLType, paramLLTypes, false);
            if (fn->getFunctionType() != fnType) {
                // Signature mismatch (should not happen — the checker
                // validated the impl) — create a fresh definition.
                fn = llvm::Function::Create(
                    fnType, llvm::Function::ExternalLinkage, mangledFnName, module_);
            }
        } else {
            auto* fnType = llvm::FunctionType::get(retLLType, paramLLTypes, false);
            fn = llvm::Function::Create(
                fnType, llvm::Function::ExternalLinkage, mangledFnName, module_);
            loweredImplMethods_[mangledFnName] = fn;
        }

        // Lower the method body
        auto* prevFn = currentFunction_;
        auto* prevBB = builder_->GetInsertBlock();
        currentFunction_ = fn;

        auto* entryBB = llvm::BasicBlock::Create(*context_, "entry", fn);
        builder_->SetInsertPoint(entryBB);

        // Save/restore locals
        auto savedLocals = locals_;
        locals_.clear();

        // Set up self for instance methods
        size_t argIdx = 0;
        if (!isStatic) {
            auto* selfArg = fn->getArg(0);
            selfArg->setName("self");
            auto ptrTIName = "*" + implTI->name;
            if (!typeRegistry_.lookup(ptrTIName)) {
                TypeInfo ptrTI;
                ptrTI.name = ptrTIName;
                ptrTI.kind = TypeKind::Pointer;
                ptrTI.pointeeType = implTI;
                typeRegistry_.registerType(std::move(ptrTI));
            }
            auto* selfPtrTI = typeRegistry_.lookup(ptrTIName);
            auto* selfAlloca = builder_->CreateAlloca(selfArg->getType(), nullptr, "self");
            builder_->CreateStore(selfArg, selfAlloca);
            locals_["self"] = {selfAlloca, selfPtrTI, 0};
            argIdx = 1;
        }

        // Register params as locals
        if (isStatic) {
            if (auto* pl = method->paramList()) {
                for (auto* p : pl->param()) {
                    if (p->SPREAD()) continue;
                    auto paramName = p->IDENTIFIER()->getText();
                    unsigned dims = 0;
                    const TypeInfo* pTI = resolveTypeInfo(p->typeSpec());
                    if (!pTI) continue;
                    auto* arg = fn->getArg(argIdx++);
                    arg->setName(paramName);
                    auto* alloca = builder_->CreateAlloca(pTI->toLLVMType(*context_, module_->getDataLayout()), nullptr, paramName);
                    builder_->CreateStore(arg, alloca);
                    locals_[paramName] = {alloca, pTI, dims};
                }
            }
        } else {
            for (auto* p : method->param()) {
                if (p->SPREAD()) continue;
                auto paramName = p->IDENTIFIER()->getText();
                unsigned dims = 0;
                const TypeInfo* pTI = resolveTypeInfo(p->typeSpec());
                if (!pTI) continue;
                auto* arg = fn->getArg(argIdx++);
                arg->setName(paramName);
                auto* alloca = builder_->CreateAlloca(pTI->toLLVMType(*context_, module_->getDataLayout()), nullptr, paramName);
                builder_->CreateStore(arg, alloca);
                locals_[paramName] = {alloca, pTI, dims};
            }
        }

        // Visit method body
        auto savedRetType = currentFnReturnType_;
        currentFnReturnType_ = retTI;
        for (auto* stmt : method->block()->statement()) {
            visit(stmt);
        }
        currentFnReturnType_ = savedRetType;

        // Implicit ret void if needed
        if (!builder_->GetInsertBlock()->getTerminator()) {
            if (fn->getReturnType()->isVoidTy())
                builder_->CreateRetVoid();
            else
                builder_->CreateRet(llvm::UndefValue::get(fn->getReturnType()));
        }

        // Restore
        locals_ = savedLocals;
        currentFunction_ = prevFn;
        if (prevBB) builder_->SetInsertPoint(prevBB);
    }

    return {};
}



std::any IRGen::visitTypeAliasDecl(LucisParser::TypeAliasDeclContext* ctx) {
        SET_DBG_LOC(ctx);
    auto name = ctx->IDENTIFIER()->getText();

    // Skip if already registered (e.g., from a previous pass)
    if (typeRegistry_.lookup(name))
        return {};

    auto* typeSpecCtx = ctx->typeSpec();
    if (auto* fnSpec = typeSpecCtx->fnTypeSpec()) {
        TypeInfo ti;
        ti.name = name;
        ti.kind = TypeKind::Function;
        ti.bitWidth = 0;
        ti.isSigned = false;
        ti.builtinSuffix = "ptr";

        // All typeSpecs from fnTypeSpec: params are [0..n-2], return is [n-1]
        auto specs = fnSpec->typeSpec();
        ti.returnType = resolveTypeInfo(specs.back());
        if (!ti.returnType) ti.returnType = typeRegistry_.lookup("int32");

        for (size_t i = 0; i + 1 < specs.size(); i++) {
            auto* pti = resolveTypeInfo(specs[i]);
            if (!pti) pti = typeRegistry_.lookup("int32");
            ti.paramTypes.push_back(pti);
        }

        typeRegistry_.registerType(std::move(ti));
    } else {
        // Check if the target is an array type: [N][M]...T
        auto* spec = typeSpecCtx;
        std::vector<unsigned> sizes;
        while (!spec->typeSpec().empty() && spec->INT_LIT()) {
            sizes.push_back(std::stoul(spec->INT_LIT()->getText()));
            spec = spec->typeSpec(0);
        }

        auto* baseTI = resolveTypeInfo(typeSpecCtx);
        if (!baseTI) return {};
        TypeInfo ti = *baseTI;
        ti.name = name;

        if (!sizes.empty()) {
            // Array type alias: store dimensions and base element type
            ti.arraySizes = std::move(sizes);
            ti.elementType = baseTI;
        }

        typeRegistry_.registerType(std::move(ti));
    }

    return {};
}



const TypeInfo* IRGen::findMethodReturnInChain(const TypeInfo* ti,
                                                const std::string& name) const {
    for (auto* t = ti; t; t = t->parentType) {
        auto smIt = structMethods_.find(t->name);
        if (smIt == structMethods_.end()) continue;
        auto mIt = smIt->second.find(name);
        if (mIt != smIt->second.end())
            return mIt->second.returnType;
    }
    return nullptr;
}



IRGen::MethodEntry* IRGen::findMethodEntryInChain(const TypeInfo* ti,
                                                    const std::string& name) {
    for (auto* t = ti; t; t = t->parentType) {
        auto smIt = structMethods_.find(t->name);
        if (smIt == structMethods_.end()) continue;
        auto mIt = smIt->second.find(name);
        if (mIt != smIt->second.end())
            return &mIt->second;
    }
    return nullptr;
}



IRGen::MethodEntry* IRGen::findOperatorEntryInChain(const TypeInfo* ti,
                                                      const std::string& internalName) {
    for (auto* t = ti; t; t = t->parentType) {
        auto smIt = operatorMethods_.find(t->name);
        if (smIt == operatorMethods_.end()) continue;
        auto mIt = smIt->second.find(internalName);
        if (mIt != smIt->second.end() && !mIt->second.empty())
            return &mIt->second[0];
    }
    return nullptr;
}

// ── Forward-declare a user function (signature only, no body) ───────────


void IRGen::forwardDeclareFunction(LucisParser::FunctionDeclContext* ctx) {
    if (!ctx) return;
    if (!irDeclActive(ctx->attributeList(), targetTriple_, debugOutput_)) return;
    // Generic function templates are not forward-declared — only instantiations are
    if (ctx->typeParamList()) {
        if (ctx->IDENTIFIER().empty()) {
            std::cerr << "lucis: function must have a name\n";
            return;
        }
        auto funcName = ctx->name->getText();
        GenericFuncTemplate tmpl;
        for (auto* tp : ctx->typeParamList()->typeParam()) {
            if (tp->IDENTIFIER().empty()) continue;
            tmpl.typeParams.push_back(tp->IDENTIFIER(0)->getText());
        }
        tmpl.decl = ctx;
        genericFuncTemplates_[funcName] = std::move(tmpl);
        return;
    }

    auto* retInfo    = resolveTypeInfo(ctx->typeSpec());
    if (!retInfo) return;
    auto retArrayDims = countArrayDims(ctx->typeSpec());
    auto* returnType = retInfo->toLLVMType(*context_, module_->getDataLayout());
    if (ctx->IDENTIFIER().empty()) {
        std::cerr << "lucis: function must have a name\n";
        return;
    }
    auto  funcName   = ctx->name->getText();

    bool isMainWithArgs = (funcName == "main" && ctx->paramList() != nullptr);

    // Collect parameter types
    std::vector<llvm::Type*> paramTypes;
    int variadicIdx = -1;
    bool isVarArg = false;
    if (auto* params = ctx->paramList()) {
        auto paramList = params->param();
        auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
        for (size_t i = 0; i < paramList.size(); i++) {
            auto* param = paramList[i];

            // Untyped variadic: ...
            if (param->SPREAD() && !param->typeSpec()) {
                isVarArg = true;
                break;
            }

            auto* pInfo = resolveTypeInfo(param->typeSpec());
            auto pDims  = countArrayDims(param->typeSpec());
            if (param->SPREAD()) {
                variadicIdx = static_cast<int>(i);
                paramTypes.push_back(llvm::PointerType::getUnqual(*context_));
                paramTypes.push_back(llvm::Type::getInt64Ty(*context_));
            } else if (pDims > 0) {
                auto* ptrTy = llvm::PointerType::getUnqual(*context_);
                auto* sliceTy = llvm::StructType::get(*context_, {ptrTy, usizeTy});
                paramTypes.push_back(sliceTy);
            } else {
                paramTypes.push_back(pInfo->toLLVMType(*context_, module_->getDataLayout()));
            }
        }
    }

    std::string emitName = funcName;
    if (isMainWithArgs || ((testMode_ || fuzzMode_) && funcName == "main")) {
        emitName = "lucis_user_main";
    } else if (funcName != "main") {
        if (isStdlib_)
            emitName = ModuleRegistry::mangle(currentModulePath_, funcName);
    }

    // Skip if already declared (e.g. from cross-file registration)
    if (module_->getFunction(emitName))
        return;

    auto* funcType = llvm::FunctionType::get(returnType, paramTypes, isVarArg);
    auto* fn = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, emitName, module_);
    if (isVarArg) {
        fn->addFnAttr("frame-pointer", "all");
    }

    // Cache return TypeInfo for resolveExprTypeInfo
    fnReturnTypes_[emitName] = retInfo;
    fnReturnArrayDims_[emitName] = retArrayDims;

    // Register call target name
    callTargetMap_[funcName] = emitName;

    // Register variadic function info (only for typed variadics)
    if (variadicIdx >= 0 && ctx->paramList()) {
        auto* vParam = ctx->paramList()->param(variadicIdx);
        auto* vInfo = resolveTypeInfo(vParam->typeSpec());
        variadicFunctions_[emitName] = {
            static_cast<size_t>(variadicIdx), vInfo
        };
    }
}

std::any IRGen::visitFunctionDecl(LucisParser::FunctionDeclContext* ctx) {
    // Generic function templates are handled on-demand at call sites
    if (ctx->typeParamList()) return {};
    if (!irDeclActive(ctx->attributeList(), targetTriple_, debugOutput_)) return {};

    // Comptime functions are not compiled to runtime code
    if (ctx->COMPTIME()) return {};

    auto* retInfo    = resolveTypeInfo(ctx->typeSpec());
    if (!retInfo) return {};
    auto* returnType = retInfo->toLLVMType(*context_, module_->getDataLayout());
    if (ctx->IDENTIFIER().empty()) {
        std::cerr << "lucis: function must have a name\n";
        return {};
    }
    auto  funcName   = ctx->name->getText();

    // ── Special handling: main(Vec<string> args) ────────────────────────
    // When main has parameters, we generate:
    //   1) lucis_user_main(ptr %args) — the user's actual code
    //   2) main(i32 %argc, ptr %argv) — a C-ABI wrapper that builds the Vec
    bool isMainWithArgs = (funcName == "main" && ctx->paramList() != nullptr);

    // Collect parameter types (variadic param → ptr + i64 pair)
    std::vector<llvm::Type*> paramTypes;
    int variadicIdx = -1; // index of the variadic param in the grammar param list
    bool isVarArg = false;
    if (auto* params = ctx->paramList()) {
        auto paramList = params->param();
        auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
        for (size_t i = 0; i < paramList.size(); i++) {
            auto* param = paramList[i];

            // Untyped variadic: ...
            if (param->SPREAD() && !param->typeSpec()) {
                isVarArg = true;
                break;
            }

            auto* pInfo = resolveTypeInfo(param->typeSpec());
            auto pDims  = countArrayDims(param->typeSpec());
            if (param->SPREAD()) {
                // Variadic: emit as (ptr, i64) for the packed array
                variadicIdx = static_cast<int>(i);
                paramTypes.push_back(llvm::PointerType::getUnqual(*context_));
                paramTypes.push_back(llvm::Type::getInt64Ty(*context_));
            } else if (pDims > 0) {
                // []T-like parameter ABI: {ptr, len}
                auto* ptrTy = llvm::PointerType::getUnqual(*context_);
                auto* sliceTy = llvm::StructType::get(*context_, {ptrTy, usizeTy});
                paramTypes.push_back(sliceTy);
            } else {
                paramTypes.push_back(pInfo->toLLVMType(*context_, module_->getDataLayout()));
            }
        }
    }

    // If main has args, emit the user's code under a different name
    std::string emitName = funcName;
    bool isEntry = hasAttribute(ctx->attributeList(), "entry");
    bool noMangle = hasAttribute(ctx->attributeList(), "no_mangle") || isEntry;
    bool isExport = hasAttribute(ctx->attributeList(), "export") || isEntry;
    if (isEntry) entryPoint_ = funcName;
    if (isMainWithArgs || ((testMode_ || fuzzMode_) && funcName == "main")) {
        emitName = "lucis_user_main";
        if ((testMode_ || fuzzMode_) && funcName == "main")
            std::cerr << "lucis: [" << (fuzzMode_ ? "fuzz" : "test")
                      << "] ignoring user 'main' (the harness provides its own)\n";
    } else if (funcName != "main" && !noMangle) {
        bool isStdlib = (currentFile_.rfind("/usr/", 0) == 0);
        if (isStdlib)
            emitName = ModuleRegistry::mangle(currentModulePath_, funcName);
    }

    // Register call target name
    callTargetMap_[funcName] = emitName;

    // Reuse forward-declared function, or create if not yet declared
    llvm::Function* func = module_->getFunction(emitName);
    if (!func) {
        auto* funcType = llvm::FunctionType::get(returnType, paramTypes, isVarArg);
        auto linkage = isExport ? llvm::Function::ExternalLinkage
                                : llvm::Function::ExternalLinkage;
        func = llvm::Function::Create(
            funcType, linkage, emitName, module_);
    }

    // Name each parameter for readable IR
    if (auto* params = ctx->paramList()) {
        auto paramList = params->param();
        size_t llvmIdx = 0;
        for (size_t i = 0; i < paramList.size(); i++) {
            // Skip untyped variadic ... (no identifier to name)
            if (paramList[i]->SPREAD() && !paramList[i]->IDENTIFIER())
                continue;
            auto pName = paramList[i]->IDENTIFIER()->getText();
            if (paramList[i]->SPREAD()) {
                func->getArg(llvmIdx)->setName(pName + ".ptr");
                func->getArg(llvmIdx + 1)->setName(pName + ".len");
                llvmIdx += 2;
            } else {
                func->getArg(llvmIdx)->setName(pName);
                llvmIdx++;
            }
        }
    }

    // ── Apply function attributes ────────────────────────────────────
    {
        auto* attrs = ctx->attributeList();
        // #[link_section("name")]
        auto linkSect = getAttributeStringArg(attrs, "link_section");
        if (!linkSect.empty())
            func->setSection(linkSect);
        // #[inline(always)] / #[inline(never)]
        if (hasAttribute(attrs, "inline")) {
            for (auto* a : attrs->attribute()) {
                if (a->IDENTIFIER() && a->IDENTIFIER()->getText() == "inline") {
                    if (auto* argList = a->attrArgList()) {
                        for (auto* arg : argList->attrArg()) {
                            if (arg->IDENTIFIER()) {
                                auto val = arg->IDENTIFIER()->getText();
                                if (val == "always")
                                    func->addFnAttr(llvm::Attribute::AlwaysInline);
                                else if (val == "never")
                                    func->addFnAttr(llvm::Attribute::NoInline);
                            }
                        }
                    }
                }
            }
        }
        // #[cold]
        if (hasAttribute(attrs, "cold"))
            func->addFnAttr(llvm::Attribute::Cold);
        // #[hot]
        if (hasAttribute(attrs, "hot"))
            func->addFnAttr(llvm::Attribute::InlineHint);
        // #[noreturn]
        if (hasAttribute(attrs, "noreturn"))
            func->addFnAttr(llvm::Attribute::NoReturn);
        // #[optimize(speed)] / #[optimize(size)]
        if (hasAttribute(attrs, "optimize")) {
            for (auto* a : attrs->attribute()) {
                if (a->IDENTIFIER() && a->IDENTIFIER()->getText() == "optimize") {
                    if (auto* argList = a->attrArgList()) {
                        for (auto* arg : argList->attrArg()) {
                            if (arg->IDENTIFIER()) {
                                auto val = arg->IDENTIFIER()->getText();
                                if (val == "size")
                                    func->addFnAttr(llvm::Attribute::OptimizeForSize);
                                else if (val == "speed")
                                    func->addFnAttr("optimize-for-speed");
                            }
                        }
                    }
                }
            }
        }
        // #[used] — prevent dead-stripping
        if (hasAttribute(attrs, "used")) {
            func->setVisibility(llvm::GlobalValue::DefaultVisibility);
            llvm::appendToUsed(*module_, {func});
        }
        // #[naked] — no prologue/epilogue
        if (hasAttribute(attrs, "naked"))
            func->addFnAttr(llvm::Attribute::Naked);
        // #[no_stack_probe] — disable stack probing
        if (hasAttribute(attrs, "no_stack_probe"))
            func->addFnAttr("probe-stack", "0");
    }

    currentFunction_ = func;
    currentFnReturnType_ = retInfo;
    locals_.clear(); // each function has its own scope
    labels_.clear();
    variadicParams_.clear();
    deferStack_.clear();

    // Register variadic function info for call-site packing (only for typed variadics)
    if (variadicIdx >= 0 && ctx->paramList()) {
        auto* vParam = ctx->paramList()->param(variadicIdx);
        auto* vInfo = resolveTypeInfo(vParam->typeSpec());
        variadicFunctions_[emitName] = {
            static_cast<size_t>(variadicIdx), vInfo
        };
    }

    // Create entry block first, then store params as allocas
    auto* entry = llvm::BasicBlock::Create(*context_, "entry", func);
    builder_->SetInsertPoint(entry);

    // ── Debug info: create DISubprogram for this function ──
    if (dbgBuilder_) {
        auto* fnType = dbgBuilder_->createSubroutineType(
            dbgBuilder_->getOrCreateTypeArray({}));
        unsigned funcLine = ctx->getStart() ? ctx->getStart()->getLine() : 0;
        currentDbgScope_ = dbgBuilder_->createFunction(
            dbgFile_, funcName, emitName,
            dbgFile_, funcLine,
            fnType,
            funcLine,
            llvm::DINode::FlagZero,
            llvm::DISubprogram::SPFlagDefinition);
        func->setSubprogram(currentDbgScope_);
        // Set debug location for entry block (param allocas etc.)
        builder_->SetCurrentDebugLocation(
            llvm::DILocation::get(*context_, funcLine, 0, currentDbgScope_));
    }

    if (auto* params = ctx->paramList()) {
        auto paramList = params->param();
        size_t llvmIdx = 0;
        for (size_t i = 0; i < paramList.size(); i++) {
            auto* param = paramList[i];

            // Skip untyped variadic ... (no identifier/type to store)
            if (param->SPREAD() && !param->typeSpec())
                continue;

            auto* pInfo = resolveTypeInfo(param->typeSpec());
            if (!pInfo) pInfo = typeRegistry_.lookup("int32");
            auto pDims  = countArrayDims(param->typeSpec());
            auto  pName = param->IDENTIFIER()->getText();

            if (param->SPREAD()) {
                // Variadic: store ptr and len in separate allocas
                auto* ptrTy = llvm::PointerType::getUnqual(*context_);
                auto* i64Ty = llvm::Type::getInt64Ty(*context_);

                auto* ptrAlloca = builder_->CreateAlloca(ptrTy, nullptr, pName + ".ptr");
                auto* lenAlloca = builder_->CreateAlloca(i64Ty, nullptr, pName + ".len");
                builder_->CreateStore(func->getArg(llvmIdx), ptrAlloca);
                builder_->CreateStore(func->getArg(llvmIdx + 1), lenAlloca);

                variadicParams_[pName] = { ptrAlloca, lenAlloca, pInfo };
                llvmIdx += 2;
            } else {
                llvm::Type* pType = nullptr;
                if (pDims > 0) {
                    auto* ptrTy = llvm::PointerType::getUnqual(*context_);
                    auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
                    pType = llvm::StructType::get(*context_, {ptrTy, usizeTy});
                } else {
                    pType = pInfo->toLLVMType(*context_, module_->getDataLayout());
                }
                auto* alloca = builder_->CreateAlloca(pType, nullptr, pName);
                builder_->CreateStore(func->getArg(llvmIdx), alloca);
                locals_[pName] = { alloca, pInfo, pDims, /*isParam=*/true };
                { unsigned dbgLine = ctx->getStart() ? ctx->getStart()->getLine() : 0;
                    emitDbgDeclare(alloca, pName, pInfo, dbgLine, llvmIdx + 1); }
                llvmIdx++;
            }
        }
    }

    // Visit block body (statements only, skip block's own entry creation)
    for (auto* stmt : ctx->block()->statement()) {
        visit(stmt);
    }

    // If no terminator, emit deferred/auto cleanups and add implicit return
    if (!builder_->GetInsertBlock()->getTerminator()) {
        emitAllCleanups();
        if (func->getReturnType()->isVoidTy())
            builder_->CreateRetVoid();
        else
            builder_->CreateRet(llvm::UndefValue::get(func->getReturnType()));
    }

    // ── #[interrupt] — generate wrapper with LLVM X86_INTR convention ──
    if (hasAttribute(ctx->attributeList(), "interrupt")) {
        auto innerName = emitName + "_inner";
        func->setName(innerName);
        func->setLinkage(llvm::GlobalValue::InternalLinkage);
        func->setCallingConv(llvm::CallingConv::X86_INTR);

        int vectorNum = -1;
        for (auto* a : ctx->attributeList()->attribute()) {
            if (a->IDENTIFIER() && a->IDENTIFIER()->getText() == "interrupt") {
                if (auto* argList = a->attrArgList()) {
                    for (auto* arg : argList->attrArg()) {
                        if (arg->IDENTIFIER() && arg->IDENTIFIER()->getText() == "vector" &&
                            arg->attrArg() && arg->attrArg()->INT_LIT())
                            vectorNum = static_cast<int>(
                                std::stol(arg->attrArg()->INT_LIT()->getText()));
                    }
                }
            }
        }

        auto* voidTy = llvm::Type::getVoidTy(*context_);
        auto* wrapperType = llvm::FunctionType::get(voidTy, false);
        auto* wrapper = llvm::Function::Create(
            wrapperType, llvm::Function::ExternalLinkage, emitName, module_);
        wrapper->setCallingConv(llvm::CallingConv::X86_INTR);

        auto* entryBB = llvm::BasicBlock::Create(*context_, "entry", wrapper);
        builder_->SetInsertPoint(entryBB);
        auto* innerFunc = module_->getFunction(innerName);
        if (innerFunc)
            builder_->CreateCall(innerFunc, {});
        builder_->CreateRetVoid();

        if (vectorNum >= 0)
            interruptTable_.push_back({vectorNum, wrapper});
    }

    // Reset debug scope for next function
    currentDbgScope_ = nullptr;

    currentFunction_ = nullptr;

    // ── Generate the real main() wrapper ────────────────────────────────
    // (Skipped in test/fuzz mode: the harness provides its own `main`.)
    if (isMainWithArgs && !testMode_ && !fuzzMode_) {
        auto* i32Ty   = llvm::Type::getInt32Ty(*context_);
        auto* ptrTy   = llvm::PointerType::getUnqual(*context_);

        // Determine which kind of main wrapper based on parameter type:
        //   main(Vec<string> x)  → existing wrapper with lucis_args_init (copies strings)
        //   main([]cstring x)    → new zero-copy wrapper (wraps argv as slice)
        bool isCSliceMain = false;
        if (auto* params = ctx->paramList()) {
            auto paramList = params->param();
            if (paramList.size() == 1) {
                auto* param = paramList[0];
                auto* pInfo = resolveTypeInfo(param->typeSpec());
                unsigned pDims = countArrayDims(param->typeSpec());
                // []cstring: pDims == 1 && element type is *char
                if (pDims == 1 && pInfo && pInfo->kind == TypeKind::Pointer &&
                    pInfo->pointeeType && pInfo->pointeeType->name == "char") {
                    isCSliceMain = true;
                }
            }
        }

        if (isCSliceMain) {
            // ── Zero-copy wrapper for main([]cstring argv) ─────────────
            // The user's code (lucis_user_main) takes a slice {ptr, i64}.
            // The C entry point main(i32 argc, ptr argv) wraps argv into
            // that slice without copying any strings.
            auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
            auto* sliceTy = llvm::StructType::get(*context_, {ptrTy, usizeTy});

            // main(i32 %argc, ptr %argv) → i32
            auto* wrapperType = llvm::FunctionType::get(i32Ty, {i32Ty, ptrTy}, false);
            auto* wrapperFn = llvm::Function::Create(
                wrapperType, llvm::Function::ExternalLinkage, "main", module_);

            auto* wrapperEntry = llvm::BasicBlock::Create(*context_, "entry", wrapperFn);
            builder_->SetInsertPoint(wrapperEntry);

            auto* argc = wrapperFn->getArg(0);
            auto* argv = wrapperFn->getArg(1);
            argc->setName("argc");
            argv->setName("argv");

            // Build slice { char** data, usize len } from { argv, argc }
            auto* sliceAlloca = builder_->CreateAlloca(sliceTy, nullptr, "argv_slice");

            auto* dataGEP = builder_->CreateStructGEP(sliceTy, sliceAlloca, 0);
            builder_->CreateStore(argv, dataGEP);

            auto* argcExt = builder_->CreateIntCast(argc, usizeTy, false, "argc_usize");
            auto* lenGEP = builder_->CreateStructGEP(sliceTy, sliceAlloca, 1);
            builder_->CreateStore(argcExt, lenGEP);

            // Load slice value and call lucis_user_main({ptr, i64})
            auto* sliceVal = builder_->CreateLoad(sliceTy, sliceAlloca, "argv_slice");
            auto* retVal = builder_->CreateCall(func, {sliceVal}, "ret");

            builder_->CreateRet(retVal);
        } else {
            // ── Vec<string> wrapper (existing code) ───────────────────
            auto* voidTy  = llvm::Type::getVoidTy(*context_);
            auto* vecTy   = getOrCreateVecStructType();

            // main(i32 %argc, ptr %argv) → i32
            auto* wrapperType = llvm::FunctionType::get(i32Ty, {i32Ty, ptrTy}, false);
            auto* wrapperFn   = llvm::Function::Create(
                wrapperType, llvm::Function::ExternalLinkage, "main", module_);

            auto* wrapperEntry = llvm::BasicBlock::Create(*context_, "entry", wrapperFn);
            builder_->SetInsertPoint(wrapperEntry);

            auto* argc = wrapperFn->getArg(0);
            auto* argv = wrapperFn->getArg(1);
            argc->setName("argc");
            argv->setName("argv");

            // Alloca a Vec<string> on the stack
            auto* vecAlloca = builder_->CreateAlloca(vecTy, nullptr, "args");

            // Call lucis_args_init(&args, argc, argv)
            auto argsInit = declareBuiltin("lucis_args_init", voidTy,
                                           {ptrTy, i32Ty, ptrTy});
            builder_->CreateCall(argsInit, {vecAlloca, argc, argv});

            // Call lucis_user_main(args_vec_struct)
            // The user function takes the vec struct by value (loaded from alloca)
            auto* vecVal = builder_->CreateLoad(vecTy, vecAlloca, "args_vec");
            auto* retVal = builder_->CreateCall(func, {vecVal}, "ret");

            // Vec is passed by value to lucis_user_main — ownership transferred.
            // Do NOT free here; cleanup happens inside lucis_user_main via auto-drop.
            builder_->CreateRet(retVal);
        }
    }

    return {};
}



std::any IRGen::visitBlock(LucisParser::BlockContext* ctx) {
        SET_DBG_LOC(ctx);
    auto* entry = llvm::BasicBlock::Create(*context_, "entry", currentFunction_);
    builder_->SetInsertPoint(entry);

    for (auto* stmt : ctx->statement()) {
        visit(stmt);
    }

    return {};
}

// use std::log::println;   →  register in ImportResolver

std::any IRGen::visitUseRoot(LucisParser::UseRootContext* ctx) {
        SET_DBG_LOC(ctx);
    if (!irDeclActive(ctx->attributeList(), targetTriple_, debugOutput_)) return {};
    auto bindings = parseUseDecl(ctx);
    for (auto& b : bindings) {
        if (b.kind != UseBinding::Kind::Module) continue;
        // Module root import: userImports_[local] = module path.
        userImports_[b.localName] = ModuleRegistry::usePathToModulePath(b.modulePath);
    }
    return {};
}

// use std::log::println [as local];   →  register in ImportResolver

std::any IRGen::visitUseItem(LucisParser::UseItemContext* ctx) {
        SET_DBG_LOC(ctx);
    if (!irDeclActive(ctx->attributeList(), targetTriple_, debugOutput_)) return {};
    auto bindings = parseUseDecl(ctx);
    for (auto& b : bindings) {
        if (b.kind != UseBinding::Kind::Symbol) continue;
        userImportNames_[b.localName] = b.exportedName;
        auto fullModPath = ModuleRegistry::usePathToModulePath(
            b.modulePath + "::" + b.exportedName);
        bool isModuleImport =
            ImportResolver::isStdModule(fullModPath) ||
            (moduleRegistry_ && moduleRegistry_->hasModule(fullModPath));
        if (ImportResolver::isStdModule(b.modulePath)) {
            imports_.addImport(b.modulePath, b.exportedName, b.localName);
            // stdlib symbols may also be defined in stdlib .lc modules
            // (e.g. stdio::printf); keep the module binding for those.
            if (!moduleRegistry_ || !moduleRegistry_->hasModule(fullModPath))
                userImports_[b.localName] =
                    ModuleRegistry::usePathToModulePath(b.modulePath);
        } else if (isModuleImport) {
            userImports_[b.localName] = fullModPath;
        } else {
            userImports_[b.localName] = ModuleRegistry::usePathToModulePath(b.modulePath);
        }
    }
    return {};
}

// use std::log::{ print as output, println };

std::any IRGen::visitUseGroup(LucisParser::UseGroupContext* ctx) {
        SET_DBG_LOC(ctx);
    if (!irDeclActive(ctx->attributeList(), targetTriple_, debugOutput_)) return {};
    auto bindings = parseUseDecl(ctx);
    for (auto& b : bindings) {
        if (b.kind != UseBinding::Kind::Symbol) continue;
        userImportNames_[b.localName] = b.exportedName;
        if (ImportResolver::isStdModule(b.modulePath)) {
            imports_.addImport(b.modulePath, b.exportedName, b.localName);
            if (!moduleRegistry_ || !moduleRegistry_->hasModule(
                    ModuleRegistry::usePathToModulePath(
                        b.modulePath + "::" + b.exportedName)))
                userImports_[b.localName] =
                    ModuleRegistry::usePathToModulePath(b.modulePath);
        } else {
            auto modPath = ModuleRegistry::usePathToModulePath(b.modulePath);
            userImports_[b.localName] = modPath;
        }
    }
    return {};
}

// use Response<int32>::*;


std::any IRGen::visitUseEnumWildcard(LucisParser::UseEnumWildcardContext* ctx) {
        SET_DBG_LOC(ctx);
    if (!irDeclActive(ctx->attributeList(), targetTriple_, debugOutput_)) return {};
    auto* ti = resolveTypeInfo(ctx->typeSpec());
    if (!ti || ti->kind != TypeKind::Enum) return {};
    for (const auto& vi : ti->enumVariantInfos) {
        enumVariantImports_.try_emplace(vi.name, InjectedEnumVariant{ ti, &vi });
    }
    return {};
}

// ═══════════════════════════════════════════════════════════════════════
//  Cross-file symbol registration (namespace support)
// ═══════════════════════════════════════════════════════════════════════



void IRGen::registerCrossFileSymbols(LucisParser::ProgramContext* ctx) {
    if (!moduleRegistry_) return;

    // Phase 2 note: SemanticDB is available via semanticDB_ for type lookups.
    // The syncTypeFromSemanticDB() method can convert Decl→TypeInfo when needed.
    // Full migration to SemanticDB as primary source is deferred to avoid
    // disrupting the existing cross-file resolution pipeline.

    // Populate userImports_ from preamble use declarations before any
    // cross-file symbol processing (visitUseItem/visitUseGroup run later
    // during the main AST traversal, but we need the import map now).
    if (ctx) {
        for (auto* pd : ctx->preambleDecl()) {
            auto* ud = pd->useDecl();
            if (!ud) continue;
            auto* attrs = [&]() -> LucisParser::AttributeListContext* {
                if (auto* r = dynamic_cast<LucisParser::UseRootContext*>(ud)) return r->attributeList();
                if (auto* i = dynamic_cast<LucisParser::UseItemContext*>(ud)) return i->attributeList();
                if (auto* g = dynamic_cast<LucisParser::UseGroupContext*>(ud)) return g->attributeList();
                if (auto* e = dynamic_cast<LucisParser::UseEnumWildcardContext*>(ud)) return e->attributeList();
                return nullptr;
            }();
            if (!irDeclActive(attrs, targetTriple_, debugOutput_)) continue;
            auto bindings = parseUseDecl(ud);
            for (auto& b : bindings) {
                if (b.kind == UseBinding::Kind::Module) {
                    // use std::log;  →  namespace binding
                    userImports_[b.localName] = ModuleRegistry::usePathToModulePath(b.modulePath);
                } else if (b.kind == UseBinding::Kind::Symbol) {
                    userImportNames_[b.localName] = b.exportedName;
                    auto fullModPath = ModuleRegistry::usePathToModulePath(
                        b.modulePath + "::" + b.exportedName);
                    bool isModuleImport =
                        ImportResolver::isStdModule(fullModPath) ||
                        (moduleRegistry_ && moduleRegistry_->hasModule(fullModPath));
                    if (ImportResolver::isStdModule(b.modulePath)) {
                        imports_.addImport(b.modulePath, b.exportedName, b.localName);
                        if (!moduleRegistry_ || !moduleRegistry_->hasModule(fullModPath))
                            userImports_[b.localName] =
                                ModuleRegistry::usePathToModulePath(b.modulePath);
                    } else if (isModuleImport) {
                        userImports_[b.localName] = fullModPath;
                    } else {
                        userImports_[b.localName] =
                            ModuleRegistry::usePathToModulePath(b.modulePath);
                    }
                }
                // EnumWildcard bindings are resolved by visitUseEnumWildcard.
            }
        }
    }

    // Pre-register imported enums so local type aliases referencing them resolve
    for (auto& [symName, sourceNs] : userImports_) {
        auto* sym = moduleRegistry_->findSymbol(sourceNs, importedSymbolName(symName));
        if (!sym || sym->kind != ExportedSymbol::Enum) continue;
        if (!typeRegistry_.lookup(sym->name) && !genericEnumTemplates_.count(sym->name)) {
            auto* enumCtx = dynamic_cast<LucisParser::EnumDeclContext*>(sym->decl);
            if (enumCtx) visitEnumDecl(enumCtx);
        }
    }

    // Pre-register imported structs with inherited parents in dependency order
    {
        struct Pending {
            LucisParser::StructDeclContext* decl;
            std::string ns;
        };
        std::unordered_map<std::string, Pending> pending;
        for (auto& [symName, sourceNs] : userImports_) {
            auto* sym = moduleRegistry_->findSymbol(sourceNs, importedSymbolName(symName));
            if (!sym || sym->kind != ExportedSymbol::Struct) continue;
            // Key by the EXPORTED name: skeletons, LLVM struct types and the
            // final registration all use the exported name. Using the local
            // binding (an alias) would create an empty "alias" skeleton and
            // an opaque %alias LLVM type.
            if (!genericStructTemplates_.count(sym->name)) {
                pending[sym->name] = { static_cast<LucisParser::StructDeclContext*>(sym->decl), sourceNs };
            }
        }
        // Add transitive parents
        bool addedParents = true;
        while (addedParents) {
            addedParents = false;
            for (auto& [name, p] : pending) {
                if (p.decl->COLON() && p.decl->typeSpec()) {
                    auto* parentTI = resolveTypeInfo(p.decl->typeSpec());
                    std::string parentName = parentTI ? parentTI->name : "";
                    if (!parentName.empty() && !pending.count(parentName)) {
                        auto* parentSym = moduleRegistry_->findSymbol(p.ns, parentName);
                        if (!parentSym) {
                            for (auto& mod : moduleRegistry_->allModules()) {
                                parentSym = moduleRegistry_->findSymbol(mod, parentName);
                                if (parentSym) break;
                            }
                        }
                        if (parentSym && parentSym->kind == ExportedSymbol::Struct
                            && !genericStructTemplates_.count(parentName)) {
                            pending[parentName] = {
                                static_cast<LucisParser::StructDeclContext*>(parentSym->decl),
                                parentSym->modulePath
                            };
                            addedParents = true;
                        }
                    }
                }
            }
        }
        // Pre-register skeletons
        for (auto& [name, p] : pending) {
            if (!typeRegistry_.lookup(name) && !genericStructTemplates_.count(name)) {
                TypeInfo skeleton;
                skeleton.name = name;
                skeleton.kind = TypeKind::Struct;
                skeleton.bitWidth = 0;
                skeleton.isSigned = false;
                typeRegistry_.registerType(skeleton);
            }
            if (!llvm::StructType::getTypeByName(*context_, name))
                llvm::StructType::create(*context_, name);
        }
        // Discover transitive struct dependencies from field types
        // (e.g. LinkedListNode<T> referenced by LinkedList<T>'s fields)
        {
            std::vector<std::pair<std::string, Pending>> newPendings;
            std::function<void(LucisParser::TypeSpecContext*, const std::string&)> addFieldDeps;
            addFieldDeps = [&](LucisParser::TypeSpecContext* ts, const std::string& ns) {
                if (!ts) return;
                for (auto* inner : ts->typeSpec())
                    addFieldDeps(inner, ns);
                if (!ts->IDENTIFIER()) return;
                auto depName = ts->IDENTIFIER()->getText();
                if (pending.count(depName) || genericStructTemplates_.count(depName))
                    return;
                auto* sym = moduleRegistry_->findSymbol(ns, depName);
                if (!sym || sym->kind != ExportedSymbol::Struct) return;
                newPendings.push_back({depName, {static_cast<LucisParser::StructDeclContext*>(sym->decl), ns}});
            };
            // Collect field deps without modifying pending during iteration
            for (auto& [name, p] : pending)
                for (auto* field : p.decl->structField())
                    addFieldDeps(field->typeSpec(), p.ns);
            // Add newly discovered deps and register skeletons + LLVM opaque types
            for (auto& [depName, dep] : newPendings) {
                if (pending.count(depName) || genericStructTemplates_.count(depName))
                    continue;
                pending[depName] = std::move(dep);
                if (!typeRegistry_.lookup(depName)) {
                    TypeInfo sk;
                    sk.name = depName;
                    sk.kind = TypeKind::Struct;
                    typeRegistry_.registerType(std::move(sk));
                }
                if (!llvm::StructType::getTypeByName(*context_, depName))
                    llvm::StructType::create(*context_, depName);
            }
        }
        // Process in dependency order
        std::unordered_set<std::string> done;
        bool madeProgress = true;
        while (madeProgress) {
            madeProgress = false;
            for (auto& [name, p] : pending) {
                if (done.count(name)) continue;
                if (typeRegistry_.lookup(name) && !llvm::StructType::getTypeByName(*context_, name)) continue;
                if (p.decl->COLON() && p.decl->typeSpec()) {
                    auto* parentTI = resolveTypeInfo(p.decl->typeSpec());
                    if (!parentTI || parentTI->kind != TypeKind::Struct || parentTI->fields.empty())
                        continue;
                }
                // Check field-type dependencies: if any field is a struct whose
                // LLVM body hasn't been set yet, defer this struct. Prevents
                // SIGSEGV in llvm::StructType::setBody when a field type is
                // still opaque (e.g. Terminal has field TerminalState, both in
                // the same cross-file module).
                {
                    bool fieldsReady = true;
                    for (auto* field : p.decl->structField()) {
                        auto* fieldTI = resolveTypeInfo(field->typeSpec());
                        if (!fieldTI || fieldTI->kind != TypeKind::Struct) continue;
                        if (fieldTI->name == name) continue;
                        auto* fieldLL = llvm::StructType::getTypeByName(*context_, fieldTI->name);
                        if (!fieldLL || fieldLL->isOpaque()) {
                            fieldsReady = false;
                            break;
                        }
                    }
                    if (!fieldsReady) continue;
                }
                visitStructDecl(p.decl);
                done.insert(name);
                madeProgress = true;
            }
        }
    }

    // Pre-register local enums/aliases and struct/union skeletons first so
    // external same-namespace declarations can resolve current-file type names.
    if (ctx) {
        for (auto* decl : ctx->topLevelDecl()) {
            if (auto* enumDecl = decl->enumDecl()) {
                auto name = enumDecl->IDENTIFIER()->getText();
                if (!typeRegistry_.lookup(name) && !genericEnumTemplates_.count(name))
                    visitEnumDecl(enumDecl);
            } else if (auto* aliasDecl = decl->typeAliasDecl()) {
                auto name = aliasDecl->IDENTIFIER()->getText();
                if (!typeRegistry_.lookup(name))
                    visitTypeAliasDecl(aliasDecl);
            }
        }

        for (auto* decl : ctx->topLevelDecl()) {
            if (auto* structDecl = decl->structDecl()) {
                auto name = structDecl->IDENTIFIER()->getText();
                if (!typeRegistry_.lookup(name) && !genericStructTemplates_.count(name)) {
                    TypeInfo skeleton;
                    skeleton.name = name;
                    skeleton.kind = TypeKind::Struct;
                    skeleton.bitWidth = 0;
                    skeleton.isSigned = false;
                    typeRegistry_.registerType(std::move(skeleton));
                    if (!llvm::StructType::getTypeByName(*context_, name))
                        llvm::StructType::create(*context_, name);
                }
            } else if (auto* unionDecl = decl->unionDecl()) {
                auto name = unionDecl->IDENTIFIER()->getText();
                if (!typeRegistry_.lookup(name) && !genericUnionTemplates_.count(name)) {
                    TypeInfo skeleton;
                    skeleton.name = name;
                    skeleton.kind = TypeKind::Union;
                    skeleton.bitWidth = 0;
                    skeleton.isSigned = false;
                    typeRegistry_.registerType(std::move(skeleton));
                    if (!llvm::StructType::getTypeByName(*context_, name))
                        llvm::StructType::create(*context_, name);
                }
            }
        }
    }

    auto sameNsSymbols = moduleRegistry_->getExternalSymbols(
        currentModulePath_, currentFile_);

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

            if (depSym->kind == ExportedSymbol::Enum && !typeRegistry_.lookup(baseName)) {
                auto* ed = static_cast<LucisParser::EnumDeclContext*>(depSym->decl);
                visitEnumDecl(ed);
                return;
            }
            auto* existingTI = typeRegistry_.lookup(baseName);
            bool needsVisit = !existingTI ||
                !llvm::StructType::getTypeByName(*context_, baseName);
            if (depSym->kind == ExportedSymbol::Struct && needsVisit) {
                auto* sd = static_cast<LucisParser::StructDeclContext*>(depSym->decl);
                // Register a skeleton first to break self-referential cycles
                {
                    TypeInfo skeleton;
                    skeleton.name = baseName;
                    skeleton.kind = TypeKind::Struct;
                    skeleton.bitWidth = 0;
                    skeleton.isSigned = false;
                    typeRegistry_.registerType(std::move(skeleton));
                    if (!llvm::StructType::getTypeByName(*context_, baseName))
                        llvm::StructType::create(*context_, baseName);
                }
                for (auto* field : sd->structField())
                    self(self, field->typeSpec(), ns);
                // Upgrade skeleton to full type via visitStructDecl
                visitStructDecl(sd);
                return;
            }
            bool needsUnionVisit = !existingTI ||
                !llvm::StructType::getTypeByName(*context_, baseName);
            if (depSym->kind == ExportedSymbol::Union && needsUnionVisit) {
                auto* ud = static_cast<LucisParser::UnionDeclContext*>(depSym->decl);
                // Register a skeleton first to break self-referential cycles
                {
                    TypeInfo skeleton;
                    skeleton.name = baseName;
                    skeleton.kind = TypeKind::Union;
                    skeleton.bitWidth = 0;
                    skeleton.isSigned = false;
                    typeRegistry_.registerType(std::move(skeleton));
                    if (!llvm::StructType::getTypeByName(*context_, baseName))
                        llvm::StructType::create(*context_, baseName);
                }
                for (auto* field : ud->unionField())
                    self(self, field->typeSpec(), ns);
                // Upgrade skeleton to full type via visitUnionDecl
                visitUnionDecl(ud);
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

    // ── Phase 1a: Register enums / type aliases from same namespace ─────
    //    (must come before structs so struct fields can reference them)
    for (auto* sym : sameNsSymbols) {
        if (sym->kind == ExportedSymbol::Enum) {
            auto* enumCtx = dynamic_cast<LucisParser::EnumDeclContext*>(sym->decl);
            if (enumCtx && !typeRegistry_.lookup(sym->name))
                visitEnumDecl(enumCtx);
        } else if (sym->kind == ExportedSymbol::TypeAlias) {
            auto* aliasCtx = dynamic_cast<LucisParser::TypeAliasDeclContext*>(sym->decl);
            if (aliasCtx && !typeRegistry_.lookup(sym->name)) {
                ensureTypeDependencyFromSpec(
                    ensureTypeDependencyFromSpec, aliasCtx->typeSpec(), currentModulePath_);
                visitTypeAliasDecl(aliasCtx);
            }
        }
    }

    // ── Phase 1b: Register structs / unions from same namespace ─────────
    for (auto* sym : sameNsSymbols) {
        if (sym->kind == ExportedSymbol::Struct) {
            auto* structCtx = dynamic_cast<LucisParser::StructDeclContext*>(sym->decl);
            if (structCtx && !typeRegistry_.lookup(sym->name)) {
                for (auto* field : structCtx->structField())
                    ensureTypeDependencyFromSpec(
                        ensureTypeDependencyFromSpec, field->typeSpec(), currentModulePath_);
                visitStructDecl(structCtx);
            }
        } else if (sym->kind == ExportedSymbol::Union) {
            auto* unionCtx = dynamic_cast<LucisParser::UnionDeclContext*>(sym->decl);
            if (unionCtx && !typeRegistry_.lookup(sym->name)) {
                for (auto* field : unionCtx->unionField())
                    ensureTypeDependencyFromSpec(
                        ensureTypeDependencyFromSpec, field->typeSpec(), currentModulePath_);
                visitUnionDecl(unionCtx);
            }
        }
    }

    // ── Phase 1c: Register enums / type aliases from imported namespaces
    for (auto& [symbolName, sourceNs] : userImports_) {
        auto* sym = moduleRegistry_->findSymbol(sourceNs, importedSymbolName(symbolName));
        if (!sym) continue;

        if (sym->kind == ExportedSymbol::Enum) {
            auto* enumCtx = dynamic_cast<LucisParser::EnumDeclContext*>(sym->decl);
            if (enumCtx && !typeRegistry_.lookup(sym->name))
                visitEnumDecl(enumCtx);
        } else if (sym->kind == ExportedSymbol::TypeAlias) {
            auto* aliasCtx = dynamic_cast<LucisParser::TypeAliasDeclContext*>(sym->decl);
            if (aliasCtx && !typeRegistry_.lookup(sym->name)) {
                ensureTypeDependencyFromSpec(
                    ensureTypeDependencyFromSpec, aliasCtx->typeSpec(), sourceNs);
                visitTypeAliasDecl(aliasCtx);
            }
        }
    }

    // ── Phase 1d: Register structs / unions from imported namespaces ────
    for (auto& [symbolName, sourceNs] : userImports_) {
        auto* sym = moduleRegistry_->findSymbol(sourceNs, importedSymbolName(symbolName));
        if (!sym) continue;

        if (sym->kind == ExportedSymbol::Struct) {
            auto* structCtx = dynamic_cast<LucisParser::StructDeclContext*>(sym->decl);
            if (structCtx && !typeRegistry_.lookup(sym->name)) {
                for (auto* field : structCtx->structField())
                    ensureTypeDependencyFromSpec(
                        ensureTypeDependencyFromSpec, field->typeSpec(), sourceNs);
                visitStructDecl(structCtx);
            }
        } else if (sym->kind == ExportedSymbol::Union) {
            auto* unionCtx = dynamic_cast<LucisParser::UnionDeclContext*>(sym->decl);
            if (unionCtx && !typeRegistry_.lookup(sym->name)) {
                for (auto* field : unionCtx->unionField())
                    ensureTypeDependencyFromSpec(
                        ensureTypeDependencyFromSpec, field->typeSpec(), sourceNs);
                visitUnionDecl(unionCtx);
            }
        }
    }

    // ── Phase 1e: Declare extern extend block methods ───────────────────
    //    For same-namespace and imported structs, find their extend blocks
    //    and declare methods as extern functions.
    auto declareExtendMethods = [&](const std::string& ns) {
        auto allSyms = moduleRegistry_->getModuleSymbols(ns);
        for (auto* sym : allSyms) {
            if (sym->kind != ExportedSymbol::ExtendBlock) continue;
            if (sym->sourceFile == currentFile_) continue;
            // Only process extend blocks for types we've registered.
            // Check generic struct templates FIRST — a skeleton may already
            // exist in typeRegistry_ from Phase 0c (line 1643), but we need
            // the generic extend registered in genericExtendTemplates_.
            if (genericStructTemplates_.count(sym->name)) {
                auto* genExtCtx = dynamic_cast<LucisParser::ExtendDeclContext*>(sym->decl);
                if (genExtCtx) {
                    // Process the defining module's stdlib imports so that
                    // method bodies (emitted by instantiateGenericStruct)
                    // can resolve functions like alloc, free, realloc, etc.
                    if (sym->treeAnchor && sym->treeAnchor->tree) {
                        auto* moduleTree = sym->treeAnchor->tree;
                        for (auto* pd : moduleTree->preambleDecl()) {
                            auto* ud = pd->useDecl();
                            if (!ud) continue;
                            for (auto& b : parseUseDecl(ud)) {
                                if (b.kind != UseBinding::Kind::Symbol) continue;
                                if (ImportResolver::isStdModule(b.modulePath))
                                    imports_.addImport(b.modulePath, b.exportedName, b.localName);
                            }
                        }
                    }
                    visitExtendDecl(genExtCtx);
                }
                continue;
            }

            auto structName = sym->name;

            // The type may only be reachable transitively (not imported by
            // name), so load its declaration from this module on demand --
            // otherwise its extend methods would be silently skipped.
            auto* structTI = typeRegistry_.lookup(structName);
            if (!structTI ||
                (structTI->kind == TypeKind::Struct && structTI->fields.empty()) ||
                (structTI->kind == TypeKind::Union && structTI->fields.empty())) {
                if (auto* typeSym = moduleRegistry_->findSymbol(ns, structName)) {
                    if (auto* sCtx = dynamic_cast<LucisParser::StructDeclContext*>(typeSym->decl)) {
                        for (auto* field : sCtx->structField())
                            ensureTypeDependencyFromSpec(
                                ensureTypeDependencyFromSpec, field->typeSpec(), ns);
                        visitStructDecl(sCtx);
                    } else if (auto* uCtx = dynamic_cast<LucisParser::UnionDeclContext*>(typeSym->decl)) {
                        for (auto* field : uCtx->unionField())
                            ensureTypeDependencyFromSpec(
                                ensureTypeDependencyFromSpec, field->typeSpec(), ns);
                        visitUnionDecl(uCtx);
                    }
                }
                structTI = typeRegistry_.lookup(structName);
            }
            if (!structTI || (structTI->kind != TypeKind::Struct &&
                              structTI->kind != TypeKind::Union)) continue;
            if (structTI->kind == TypeKind::Struct && structTI->fields.empty()) continue;

            auto* extCtx = dynamic_cast<LucisParser::ExtendDeclContext*>(sym->decl);
            if (!extCtx) continue;

            for (auto* method : extCtx->extendMethod()) {
                // ── Operator overload ──
                if (auto* opDecl = method->operatorDecl()) {
                    auto internalName = opInternalName(opDecl->operatorName());
                    auto* opRetTI = resolveTypeInfo(opDecl->typeSpec());
                    if (!opRetTI) continue;
                    auto* opRetLLTy = opRetTI->toLLVMType(*context_, module_->getDataLayout());
                    if (!opRetLLTy) continue;

                    bool opIsInstance = (opDecl->AMPERSAND() != nullptr);
                    std::vector<llvm::Type*> opParamLLTypes;
                    if (opIsInstance)
                        opParamLLTypes.push_back(
                            llvm::PointerType::getUnqual(*context_));

                    std::vector<LucisParser::ParamContext*> opParams;
                    if (opIsInstance)
                        opParams = opDecl->param();
                    else if (auto* opl = opDecl->paramList())
                        opParams = opl->param();
                    for (auto* p : opParams) {
                        auto* pTI = resolveTypeInfo(p->typeSpec());
                        opParamLLTypes.push_back(
                            pTI ? pTI->toLLVMType(*context_, module_->getDataLayout())
                                : llvm::Type::getInt32Ty(*context_));
                    }

                    auto opFuncName = structName + "__" + internalName + "_" +
                                      std::to_string(opParamLLTypes.size());
                    if (module_->getFunction(opFuncName)) continue;
                    auto* opFnType = llvm::FunctionType::get(
                        opRetLLTy, opParamLLTypes, false);
                    auto* opFn = llvm::Function::Create(
                        opFnType, llvm::Function::ExternalLinkage, opFuncName, module_);
                    operatorMethods_[structName][internalName].push_back({ opFn, opRetTI });
                    continue;
                }

                auto methodName = method->name->getText();
                auto funcName = structName + "__" + methodName;

                // Skip if already declared
                if (module_->getFunction(funcName)) continue;

                // Pre-register types referenced in method signatures
                // (e.g. ReadFileResult as return type) before resolving them.
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

                auto* retTI = resolveTypeInfo(method->typeSpec());
                if (!retTI) continue;
                auto* retLLTy = retTI->toLLVMType(*context_, module_->getDataLayout());
                if (!retLLTy) continue;

                bool isStatic = (method->AMPERSAND() == nullptr);

                std::vector<llvm::Type*> paramLLTypes;
                if (!isStatic) {
                    paramLLTypes.push_back(
                        llvm::PointerType::getUnqual(*context_)); // &self
                }

                std::vector<LucisParser::ParamContext*> params;
                if (isStatic) {
                    if (auto* pl = method->paramList())
                        params = pl->param();
                } else {
                    params = method->param();
                }

                bool isVarArg = false;
                for (auto* param : params) {
                    // Untyped variadic: ...
                    if (param->SPREAD() && !param->typeSpec()) {
                        isVarArg = true;
                        break;
                    }

                    auto* pTI = resolveTypeInfo(param->typeSpec());
                    paramLLTypes.push_back(
                        pTI ? pTI->toLLVMType(*context_, module_->getDataLayout())
                            : llvm::Type::getInt32Ty(*context_));
                }

                auto* fnType = llvm::FunctionType::get(
                    retLLTy, paramLLTypes, isVarArg);
                if (!fnType) continue;
                auto* fn = llvm::Function::Create(
                    fnType, llvm::Function::ExternalLinkage,
                    funcName, module_);

                if (isStatic)
                    staticStructMethods_[structName][methodName] = { fn, retTI };
                else
                    structMethods_[structName][methodName] = { fn, retTI };
            }
        }
    };

    // Extend blocks from same namespace
    declareExtendMethods(currentModulePath_);

    // Extend blocks from imported namespaces
    std::unordered_set<std::string> processedNs;
    for (auto& [symbolName, sourceNs] : userImports_) {
        if (processedNs.insert(sourceNs).second)
            declareExtendMethods(sourceNs);
    }

    // ── Impl blocks from same and imported namespaces ──────────────────
    auto declareImplMethods = [&](const std::string& ns) {
        auto symbols = moduleRegistry_->getModuleSymbols(ns);
        for (auto* sym : symbols) {
            if (sym->kind != ExportedSymbol::ImplBlock) continue;
            auto* implCtx = dynamic_cast<LucisParser::ImplDeclContext*>(sym->decl);
            if (!implCtx || implCtx->typeParamList()) continue; // skip generic
            // Malformed impl (syntax error, e.g. "impl H" without
            // "for Type") — the parser already reported the error.
            if (implCtx->typeSpec().size() < 2) continue;

            // Validate impl type exists
            const TypeInfo* implTI = resolveTypeInfo(implCtx->typeSpec(1));
            if (!implTI) continue;

            const TypeInfo* traitTI = resolveTypeInfo(implCtx->typeSpec(0));
            if (!traitTI || traitTI->kind != TypeKind::Trait) continue;

            const TraitInfo* trait = traitRegistry_.lookup(traitTI->name);
            if (!trait) continue;

            // Register entry
            ImplEntry entry;
            entry.traitName = trait->name;
            entry.typeName = implTI->name;
            entry.implType = implTI;
            entry.decl = implCtx;
            entry.sourceModulePath = ns;
            traitImpls_[entry.traitName].push_back(std::move(entry));

            // Mark the type as implementing this trait
            auto* ti = typeRegistry_.lookupMutable(implTI->name);
            if (ti) ti->implementedTraits.insert(trait->name);

            // Forward-declare LLVM functions for each impl method
            for (auto* method : implCtx->implMethod()) {
                auto methodName = method->name->getText();
                auto mangledFnName = mangleImplMethodName(
                    trait->name, implTI->name, methodName);

                if (loweredImplMethods_.count(mangledFnName)) continue;

                bool isStatic = (method->AMPERSAND() == nullptr);

                unsigned retDims = 0;
                const TypeInfo* retTI = resolveTypeInfo(method->typeSpec());

                std::vector<llvm::Type*> paramLLTypes;
                if (!isStatic) {
                    auto* llvmType = implTI->toLLVMType(*context_, module_->getDataLayout());
                    if (llvmType) {
                        auto* ptrTy = llvm::PointerType::getUnqual(llvmType);
                        paramLLTypes.push_back(ptrTy);
                    }
                }
                if (isStatic) {
                    if (auto* pl = method->paramList()) {
                        for (auto* p : pl->param()) {
                            if (p->SPREAD()) continue;
                            unsigned dims = 0;
                            const TypeInfo* pTI = resolveTypeInfo(p->typeSpec());
                            if (!pTI) continue;
                            auto* pLL = pTI->toLLVMType(*context_, module_->getDataLayout());
                            if (pLL) paramLLTypes.push_back(pLL);
                        }
                    }
                } else {
                    for (auto* p : method->param()) {
                        if (p->SPREAD()) continue;
                        unsigned dims = 0;
                        const TypeInfo* pTI = resolveTypeInfo(p->typeSpec());
                        if (!pTI) continue;
                        auto* pLL = pTI->toLLVMType(*context_, module_->getDataLayout());
                        if (pLL) paramLLTypes.push_back(pLL);
                    }
                }

                llvm::Type* retLLType = llvm::Type::getVoidTy(*context_);
                if (retTI && retTI->kind != TypeKind::Void)
                    retLLType = retTI->toLLVMType(*context_, module_->getDataLayout());

                auto* fnType = llvm::FunctionType::get(retLLType, paramLLTypes, false);
                auto* fn = llvm::Function::Create(
                    fnType, llvm::Function::ExternalLinkage,
                    mangledFnName, module_);
                loweredImplMethods_[mangledFnName] = fn;
            }
        }
    };

    declareImplMethods(currentModulePath_);
    processedNs.clear();
    for (auto& [symbolName, sourceNs] : userImports_) {
        if (processedNs.insert(sourceNs).second)
            declareImplMethods(sourceNs);
    }

    // ── Phase 2: Declare extern functions from same namespace ──────────
    for (auto* sym : sameNsSymbols) {
        if (sym->kind == ExportedSymbol::Function) {
            auto* funcCtx = dynamic_cast<LucisParser::FunctionDeclContext*>(sym->decl);
            if (!funcCtx) continue;
            ensureFunctionTypeDependencies(funcCtx, currentModulePath_);

            if (funcCtx->typeParamList()) {
                GenericFuncTemplate tmpl;
                for (auto* tp : funcCtx->typeParamList()->typeParam())
                    tmpl.typeParams.push_back(tp->IDENTIFIER(0)->getText());
                tmpl.decl = funcCtx;
                genericFuncTemplates_[sym->name] = std::move(tmpl);
                continue;
            }

            auto mangledName = sym->name;
            if (sym->isStdlib)
                mangledName = ModuleRegistry::mangle(sym->modulePath, sym->name);
            if (!module_->getFunction(mangledName))
                declareExternFunction(mangledName, funcCtx);
            callTargetMap_[sym->name] = mangledName;
        }
    }

    // ── Phase 2b: Declare extern functions from imported namespaces ────
    for (auto& [symbolName, sourceNs] : userImports_) {
        auto* sym = moduleRegistry_->findSymbol(sourceNs, importedSymbolName(symbolName));
        if (!sym || sym->kind != ExportedSymbol::Function) continue;

        auto* funcCtx = dynamic_cast<LucisParser::FunctionDeclContext*>(sym->decl);
        if (!funcCtx) continue;
        ensureFunctionTypeDependencies(funcCtx, sourceNs);

        if (funcCtx->typeParamList()) {
            GenericFuncTemplate tmpl;
            for (auto* tp : funcCtx->typeParamList()->typeParam())
                tmpl.typeParams.push_back(tp->IDENTIFIER(0)->getText());
            tmpl.decl = funcCtx;
            genericFuncTemplates_[symbolName] = std::move(tmpl);
            continue;
        }

        auto mangledName = sym->name;
        if (sym->isStdlib)
            mangledName = ModuleRegistry::mangle(sourceNs, sym->name);
        if (!module_->getFunction(mangledName))
            declareExternFunction(mangledName, funcCtx);
        callTargetMap_[symbolName] = mangledName;
    }

    // ── Phase 3: Register local functions into callTargetMap_ ──────────
    //    (done later in visitFunctionDecl via mangling)
}

// ── Phase 2: Sync type from SemanticDB into local TypeRegistry ────────────────



std::any IRGen::visitTypeSpec(LucisParser::TypeSpecContext* ctx) {
        SET_DBG_LOC(ctx);
    auto* ti = resolveTypeInfo(ctx);
    if (!ti) return static_cast<llvm::Type*>(llvm::Type::getInt32Ty(*context_));
    return static_cast<llvm::Type*>(ti->toLLVMType(*context_, module_->getDataLayout()));
}


