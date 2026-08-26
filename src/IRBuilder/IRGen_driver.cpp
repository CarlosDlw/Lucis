#include "IRBuilder/IRGen.h"
#include "IRBuilder/IRGen_helpers.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
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



IRGen::IRGen()
    : intrinsicRegistry_(typeRegistry_) {
}



void IRGen::setModuleContext(const ModuleRegistry* registry,
                              const std::string& modulePath,
                              const std::string& currentFile,
                              bool isStdlib) {
    moduleRegistry_     = registry;
    currentModulePath_  = modulePath;
    currentFile_        = currentFile;
    isStdlib_           = isStdlib;
}



void IRGen::setCBindings(const CBindings* bindings) {
    cBindings_ = bindings;
}



bool IRGen::isStructLiteralExpr(LucisParser::ExpressionContext* expr) {
    if (!expr) return false;
    return dynamic_cast<LucisParser::StructLitExprContext*>(expr) != nullptr ||
           dynamic_cast<LucisParser::GenericStructLitExprContext*>(expr) != nullptr;
}

// True when the expression yields a shallow copy of a Vec element
// (at()/first()/last()/subscript). The vec owns the element memory —
// including any string fields — so locals copied from it must not
// auto-free those fields.


bool IRGen::isBorrowedStructValueExpr(LucisParser::ExpressionContext* expr) {
    if (!expr) return false;
    if (auto* mc = dynamic_cast<LucisParser::MethodCallExprContext*>(expr)) {
        auto name = mc->IDENTIFIER()->getText();
        if (name != "at" && name != "first" && name != "last") return false;
        auto* recvTI = resolveExprTypeInfo(mc->expression());
        return recvTI && recvTI->kind == TypeKind::Extended && recvTI->extendedKind == "Vec";
    }
    if (auto* ix = dynamic_cast<LucisParser::IndexExprContext*>(expr)) {
        if (ix->expression().empty()) return false;
        auto* recvTI = resolveExprTypeInfo(ix->expression(0));
        return recvTI && recvTI->kind == TypeKind::Extended && recvTI->extendedKind == "Vec";
    }
    return false;
}



llvm::Function* IRGen::declareCFunction(const std::string& name) {
    // Already declared in this module?
    if (auto* fn = module_->getFunction(name))
        return fn;

    if (!cBindings_) return nullptr;

    auto* cfunc = cBindings_->findFunction(name);
    if (!cfunc) return nullptr;

    auto& dl = module_->getDataLayout();

    // Build original LLVM types
    auto* origRetTy = cfunc->returnType->toLLVMType(*context_, dl);
    std::vector<llvm::Type*> origParamTypes;
    for (auto* pTI : cfunc->paramTypes)
        origParamTypes.push_back(pTI->toLLVMType(*context_, dl));

    // Classify return and params for x86-64 ABI coercion
    CABIInfo abiInfo;
    abiInfo.returnInfo = ABIClassifier::classifyReturn(origRetTy, dl, *context_);
    abiInfo.originalRetType = origRetTy;

    for (auto* pty : origParamTypes) {
        abiInfo.paramInfos.push_back(
            ABIClassifier::classifyParam(pty, dl, *context_));
        abiInfo.originalParamTypes.push_back(pty);
    }

    // Build coerced function type
    llvm::Type* coercedRetTy = origRetTy;
    if (abiInfo.returnInfo.kind == ABIArgKind::Coerce)
        coercedRetTy = abiInfo.returnInfo.coercedType;
    else if (abiInfo.returnInfo.kind == ABIArgKind::Indirect)
        coercedRetTy = llvm::Type::getVoidTy(*context_);

    std::vector<llvm::Type*> coercedParamTypes;

    // Sret: prepend hidden pointer parameter for large struct return
    if (abiInfo.returnInfo.kind == ABIArgKind::Indirect)
        coercedParamTypes.push_back(llvm::PointerType::getUnqual(*context_));

    for (size_t i = 0; i < origParamTypes.size(); i++) {
        auto& pi = abiInfo.paramInfos[i];
        switch (pi.kind) {
        case ABIArgKind::Direct:
            coercedParamTypes.push_back(origParamTypes[i]);
            break;
        case ABIArgKind::Coerce:
            coercedParamTypes.push_back(pi.coercedType);
            break;
        case ABIArgKind::Expand:
            for (auto* t : pi.expandedTypes)
                coercedParamTypes.push_back(t);
            break;
        case ABIArgKind::Indirect:
            coercedParamTypes.push_back(llvm::PointerType::getUnqual(*context_));
            break;
        }
    }

    auto* fnType = llvm::FunctionType::get(coercedRetTy, coercedParamTypes,
                                           cfunc->isVariadic);
    auto* fn = llvm::Function::Create(
        fnType, llvm::Function::ExternalLinkage, name, module_);

    // Add ABI attributes
    unsigned argIdx = 0;
    if (abiInfo.returnInfo.kind == ABIArgKind::Indirect) {
        fn->addParamAttr(0, llvm::Attribute::getWithStructRetType(
            *context_, origRetTy));
        fn->addParamAttr(0, llvm::Attribute::get(
            *context_, llvm::Attribute::NoAlias));
        fn->addParamAttr(0, llvm::Attribute::get(
            *context_, llvm::Attribute::Writable));
        argIdx = 1;
    }

    for (size_t i = 0; i < abiInfo.paramInfos.size(); i++) {
        if (abiInfo.paramInfos[i].kind == ABIArgKind::Indirect) {
            fn->addParamAttr(argIdx, llvm::Attribute::getWithByValType(
                *context_, origParamTypes[i]));
        }
        if (abiInfo.paramInfos[i].kind == ABIArgKind::Expand)
            argIdx += abiInfo.paramInfos[i].expandedTypes.size();
        else
            argIdx++;
    }

    // Store ABI info for call-site coercion
    cabiInfos_[name] = std::move(abiInfo);

    externCFunctions_.insert(name);
    return fn;
}



std::unique_ptr<IRModule> IRGen::generate(LucisParser::ProgramContext* tree,
                                          const std::string& moduleName) {
    auto ctx = std::make_unique<llvm::LLVMContext>();
    auto mod = std::make_unique<llvm::Module>(moduleName, *ctx);
    
    // Set target triple early to avoid LLVM warnings about overriding it later
    auto triple = targetTriple_.empty()
        ? llvm::sys::getDefaultTargetTriple()
        : targetTriple_;
    mod->setTargetTriple(llvm::Triple(triple));

    // Set the data layout from the triple so all size computations during IR
    // generation (vec/map element sizes, struct layouts) use the real target
    // layout. Without this, getDataLayout() falls back to a packed default,
    // producing sizes inconsistent with the backend layout (e.g. {i32, i64}
    // sized 12 instead of 16), which corrupts raw vec elements.
    {
        llvm::InitializeNativeTarget();
        std::string dlErr;
        if (auto* dlTarget = llvm::TargetRegistry::lookupTarget(
                llvm::Triple(triple), dlErr)) {
            llvm::TargetOptions dlOpt;
            std::unique_ptr<llvm::TargetMachine> dlMachine(
                dlTarget->createTargetMachine(llvm::Triple(triple),
                    llvm::sys::getHostCPUName(), llvm::StringRef(""),
                    dlOpt, llvm::Reloc::PIC_));
            if (dlMachine)
                mod->setDataLayout(dlMachine->createDataLayout());
        } else {
            std::cerr << "lucis: lookupTarget failed: " << dlErr << "\n";
        }
    }
    
    llvm::IRBuilder<> builder(*ctx);

    context_ = ctx.get();
    module_  = mod.get();
    builder_ = &builder;
    locals_.clear();
    imports_ = ImportResolver{};
    userImports_.clear();
    userImportNames_.clear();
    callTargetMap_.clear();
    externCFunctions_.clear();
    fnReturnTypes_.clear();
    fnReturnArrayDims_.clear();
    globalBuiltins_ = {"exit", "panic", "assert", "assertMsg",
                        "unreachable", "toInt", "toFloat", "toBool", "toString",
                        "cstr", "fromCStr", "fromCStrCopy", "fromCStrLen", "freeStr"};
    createdDITypes_.clear();

    visitProgram(tree);

    // Test mode: synthesize the test harness `main` in the entry unit.
    if (testMode_ && !testCases_.empty())
        synthesizeTestMain();

    // Fuzz mode: instrument user code with coverage hooks, then expose the
    // target and synthesize the runner `main` (entry unit only).
    if (fuzzMode_) {
        injectFuzzCoverage();
        if (fuzzTarget_)
            synthesizeFuzzMain();
    }

    // Finalize debug info
    if (dbgBuilder_) {
        dbgBuilder_->finalize();
        delete dbgBuilder_;
        dbgBuilder_ = nullptr;
    }

    // Validate the generated IR before handing it off
    std::string verifyErr;
    llvm::raw_string_ostream errStream(verifyErr);
    if (llvm::verifyModule(*module_, &errStream)) {
        std::cerr << "lucis: IR verification failed:\n" << errStream.str() << "\n";
        // Dump IR for debugging
        llvm::errs() << *module_;
        if (dbgBuilder_) { dbgBuilder_->finalize(); delete dbgBuilder_; dbgBuilder_ = nullptr; }
        context_ = nullptr;
        module_  = nullptr;
        builder_ = nullptr;
        return nullptr;
    }

    context_ = nullptr;
    module_  = nullptr;
    builder_ = nullptr;
    dbgFile_ = nullptr;
    dbgCU_ = nullptr;
    currentDbgScope_ = nullptr;

    return std::make_unique<IRModule>(std::move(ctx), std::move(mod));
}


// ── Debug info helpers ──────────────────────────────────────────────────────



llvm::DIType* IRGen::getOrCreateDIType(const TypeInfo* ti) {
    if (!ti || !dbgBuilder_) return nullptr;
    // Check cache first to avoid infinite recursion (e.g. *Node → Node → *Node)
    {
        auto it = createdDITypes_.find(ti->name);
        if (it != createdDITypes_.end()) return it->second;
    }
    
    switch (ti->kind) {
    case TypeKind::Integer: {
        unsigned bitWidth = ti->bitWidth > 0 ? ti->bitWidth : 64;
        unsigned enc = ti->isSigned ? llvm::dwarf::DW_ATE_signed : llvm::dwarf::DW_ATE_unsigned;
        return dbgBuilder_->createBasicType(ti->name, bitWidth, enc);
    }
    case TypeKind::Float: {
        unsigned bitWidth = (ti->name == "float32" || ti->name == "f32") ? 32 : 64;
        return dbgBuilder_->createBasicType(ti->name, bitWidth, llvm::dwarf::DW_ATE_float);
    }
    case TypeKind::Bool:
        return dbgBuilder_->createBasicType("bool", 8, llvm::dwarf::DW_ATE_boolean);
    case TypeKind::Char:
        return dbgBuilder_->createBasicType("char", 8, llvm::dwarf::DW_ATE_signed_char);
    case TypeKind::Pointer: {
        auto* pointeeDITy = ti->pointeeType ? getOrCreateDIType(ti->pointeeType) : nullptr;
        if (!pointeeDITy)
            pointeeDITy = dbgBuilder_->createBasicType("void", 8, llvm::dwarf::DW_ATE_unsigned);
        createdDITypes_[ti->name] = dbgBuilder_->createPointerType(pointeeDITy,
            module_->getDataLayout().getPointerSizeInBits());
        return createdDITypes_[ti->name];
    }
    case TypeKind::Struct: {
        createdDITypes_[ti->name] = nullptr; // mark in-progress to break cycles
        auto* llvmSt = ti->toLLVMType(*context_, module_->getDataLayout());
        if (!llvmSt) return nullptr;
        auto* structTy = llvm::dyn_cast<llvm::StructType>(llvmSt);
        if (!structTy) return nullptr;
        auto& dl = module_->getDataLayout();
        uint64_t sizeBits = dl.getTypeSizeInBits(structTy);
        uint32_t alignBits = dl.getABITypeAlign(structTy).value() * 8;
        
        llvm::SmallVector<llvm::Metadata*, 8> members;
        if (auto* layout = dl.getStructLayout(structTy)) {
            for (unsigned i = 0; i < ti->fields.size() && i < structTy->getNumElements(); i++) {
                auto& field = ti->fields[i];
                auto* fieldDIType = getOrCreateDIType(field.typeInfo);
                if (!fieldDIType) continue;
                uint64_t offsetBits = layout->getElementOffsetInBits(i);
                uint32_t fieldAlign = dl.getABITypeAlign(structTy->getElementType(i)).value() * 8;
                uint64_t fieldSizeBits = dl.getTypeSizeInBits(structTy->getElementType(i));
            auto* member = dbgBuilder_->createMemberType(
                dbgFile_, field.name, dbgFile_, 0,
                fieldSizeBits, fieldAlign, offsetBits,
                llvm::DINode::FlagZero, fieldDIType);
                members.push_back(member);
            }
        }
        auto memberArray = dbgBuilder_->getOrCreateArray(members);
        createdDITypes_[ti->name] = dbgBuilder_->createStructType(
            dbgFile_, ti->name, dbgFile_, 0, sizeBits, alignBits,
            llvm::DINode::FlagZero, nullptr, memberArray);
        return createdDITypes_[ti->name];
    }
    case TypeKind::Enum: {
        // Check if enum has payload (tagged union)
        bool hasPayload = false;
        for (const auto& variant : ti->enumVariantInfos) {
            if (!variant.payloadFields.empty()) { hasPayload = true; break; }
        }
        if (!hasPayload) {
            return dbgBuilder_->createBasicType(ti->name, 32, llvm::dwarf::DW_ATE_unsigned);
        }

        // Tagged union enum — DICompositeType with discriminant + payload union
        createdDITypes_[ti->name] = nullptr; // mark in-progress to break cycles
        auto& dl = module_->getDataLayout();
        auto* llvmEnumTy = ti->toLLVMType(*context_, dl);
        if (!llvmEnumTy) return nullptr;
        auto* enumStructTy = llvm::dyn_cast<llvm::StructType>(llvmEnumTy);
        if (!enumStructTy) return nullptr;
        uint64_t sizeBits = dl.getTypeSizeInBits(enumStructTy);
        uint32_t alignBits = dl.getABITypeAlign(enumStructTy).value() * 8;

        llvm::SmallVector<llvm::Metadata*, 8> members;

        // 1. Discriminant field (i32, always at offset 0)
        auto* discDIType = dbgBuilder_->createBasicType("i32", 32, llvm::dwarf::DW_ATE_unsigned);
        auto* discMember = dbgBuilder_->createMemberType(
            dbgFile_, "tag", dbgFile_, 0,
            32, 32, 0, llvm::DINode::FlagZero, discDIType);
        members.push_back(discMember);

        // 2. Union of variant payloads
        uint64_t storageOffset = 32;
        uint32_t storageAlign = 32;
        if (enumStructTy->getNumElements() > 1) {
            storageAlign = dl.getABITypeAlign(enumStructTy->getStructElementType(1)).value() * 8;
            if (auto* layout = dl.getStructLayout(enumStructTy))
                storageOffset = layout->getElementOffsetInBits(1);
        }
        uint64_t unionSizeBits = sizeBits > storageOffset ? sizeBits - storageOffset : 0;

        llvm::SmallVector<llvm::Metadata*, 8> variantMembers;
        for (const auto& variant : ti->enumVariantInfos) {
            if (variant.payloadFields.empty()) continue;

            // Build DIType for this variant's payload
            llvm::DIType* payloadDITy = nullptr;
            uint64_t variantSizeBits = 0;

            if (variant.payloadKind == EnumPayloadKind::Tuple && variant.payloadFields.size() == 1) {
                // Single-field tuple: payload IS the field type
                payloadDITy = getOrCreateDIType(variant.payloadFields[0].typeInfo);
                if (payloadDITy) {
                    auto* fLLTy = variant.payloadFields[0].typeInfo->toLLVMType(*context_, dl);
                    if (fLLTy) variantSizeBits = dl.getTypeSizeInBits(fLLTy);
                }
            } else {
                // Multi-field payload — create a struct type for the variant
                std::vector<llvm::Type*> fLLTypes;
                for (const auto& f : variant.payloadFields) {
                    fLLTypes.push_back(f.typeInfo->toLLVMType(*context_, dl));
                }
                auto* variantStructTy = llvm::StructType::get(*context_, fLLTypes);
                auto* variantLayout = dl.getStructLayout(variantStructTy);
                variantSizeBits = dl.getTypeSizeInBits(variantStructTy);
                uint32_t variantAlign = dl.getABITypeAlign(variantStructTy).value() * 8;

                llvm::SmallVector<llvm::Metadata*, 8> fieldMembers;
                for (unsigned fi = 0; fi < variant.payloadFields.size(); fi++) {
                    auto& field = variant.payloadFields[fi];
                    auto* fieldDITy = getOrCreateDIType(field.typeInfo);
                    if (!fieldDITy) continue;
                    uint64_t fOffset = variantLayout->getElementOffsetInBits(fi);
                    uint64_t fSize = dl.getTypeSizeInBits(fLLTypes[fi]);
                    uint32_t fAlign = dl.getABITypeAlign(fLLTypes[fi]).value() * 8;
                    auto* member = dbgBuilder_->createMemberType(
                        dbgFile_, field.name, dbgFile_, 0,
                        fSize, fAlign, fOffset,
                        llvm::DINode::FlagZero, fieldDITy);
                    fieldMembers.push_back(member);
                }
                auto fieldArray = dbgBuilder_->getOrCreateArray(fieldMembers);
                payloadDITy = dbgBuilder_->createStructType(
                    dbgFile_, variant.name, dbgFile_, 0,
                    variantSizeBits, variantAlign,
                    llvm::DINode::FlagZero, nullptr, fieldArray);
            }

            if (!payloadDITy) continue;

            auto* variantMember = dbgBuilder_->createMemberType(
                dbgFile_, variant.name, dbgFile_, 0,
                variantSizeBits, storageAlign, 0, // offset 0 within union
                llvm::DINode::FlagZero, payloadDITy);
            variantMembers.push_back(variantMember);
        }

        auto variantArray = dbgBuilder_->getOrCreateArray(variantMembers);
        auto* unionDIType = dbgBuilder_->createUnionType(
            dbgFile_, "payload", dbgFile_, 0,
            unionSizeBits, storageAlign,
            llvm::DINode::FlagZero, variantArray);

        auto* unionMember = dbgBuilder_->createMemberType(
            dbgFile_, "payload", dbgFile_, 0,
            unionSizeBits, storageAlign, storageOffset,
            llvm::DINode::FlagZero, unionDIType);
        members.push_back(unionMember);

        auto memberArray = dbgBuilder_->getOrCreateArray(members);
        createdDITypes_[ti->name] = dbgBuilder_->createStructType(
            dbgFile_, ti->name, dbgFile_, 0, sizeBits, alignBits,
            llvm::DINode::FlagZero, nullptr, memberArray);
        return createdDITypes_[ti->name];
    }
    case TypeKind::String: {
        // string = { ptr (data), usize (len) }
        auto& dl = module_->getDataLayout();
        auto* strLLTy = ti->toLLVMType(*context_, dl);
        auto* strStructTy = llvm::dyn_cast<llvm::StructType>(strLLTy);
        if (!strStructTy || strStructTy->getNumElements() < 2) return nullptr;
        uint64_t sizeBits = dl.getTypeSizeInBits(strStructTy);
        uint32_t alignBits = dl.getABITypeAlign(strStructTy).value() * 8;

        llvm::SmallVector<llvm::Metadata*, 8> members;
        if (auto* layout = dl.getStructLayout(strStructTy)) {
            // data: ptr
            auto* ptrDITy = dbgBuilder_->createPointerType(
                dbgBuilder_->createBasicType("char", 8, llvm::dwarf::DW_ATE_signed_char),
                dl.getPointerSizeInBits());
            auto* dataMember = dbgBuilder_->createMemberType(
                dbgFile_, "data", dbgFile_, 0,
                dl.getTypeSizeInBits(strStructTy->getElementType(0)),
                dl.getABITypeAlign(strStructTy->getElementType(0)).value() * 8,
                layout->getElementOffsetInBits(0),
                llvm::DINode::FlagZero, ptrDITy);
            members.push_back(dataMember);

            // len: usize
            auto* lenDITy = dbgBuilder_->createBasicType(
                "usize", dl.getPointerSizeInBits(), llvm::dwarf::DW_ATE_unsigned);
            auto* lenMember = dbgBuilder_->createMemberType(
                dbgFile_, "len", dbgFile_, 0,
                dl.getTypeSizeInBits(strStructTy->getElementType(1)),
                dl.getABITypeAlign(strStructTy->getElementType(1)).value() * 8,
                layout->getElementOffsetInBits(1),
                llvm::DINode::FlagZero, lenDITy);
            members.push_back(lenMember);
        }

        auto memberArray = dbgBuilder_->getOrCreateArray(members);
        return dbgBuilder_->createStructType(
            dbgFile_, ti->name, dbgFile_, 0, sizeBits, alignBits,
            llvm::DINode::FlagZero, nullptr, memberArray);
    }
    case TypeKind::Void:
        return nullptr;
    case TypeKind::Extended: {
        if (ti->extendedKind == "Set") {
            // Set<T> = { ptr (states), ptr (keys), ptr (hashes), usize (len), usize (cap), usize (key_size) }
            auto& dl = module_->getDataLayout();
            auto* setLLTy = ti->toLLVMType(*context_, dl);
            auto* setStructTy = llvm::dyn_cast<llvm::StructType>(setLLTy);
            if (!setStructTy || setStructTy->getNumElements() < 6) return nullptr;
            uint64_t sizeBits = dl.getTypeSizeInBits(setStructTy);
            uint32_t alignBits = dl.getABITypeAlign(setStructTy).value() * 8;

            // Element type pointer for values
            llvm::DIType* elemDITy = ti->elementType
                ? getOrCreateDIType(ti->elementType) : nullptr;
            if (!elemDITy)
                elemDITy = dbgBuilder_->createBasicType(
                    "void", 8, llvm::dwarf::DW_ATE_unsigned);
            auto* elemPtrDITy = dbgBuilder_->createPointerType(
                elemDITy, dl.getPointerSizeInBits());
            auto* usizeDITy = dbgBuilder_->createBasicType(
                "usize", dl.getPointerSizeInBits(), llvm::dwarf::DW_ATE_unsigned);

            llvm::SmallVector<llvm::Metadata*, 8> members;
            if (auto* layout = dl.getStructLayout(setStructTy)) {
                // Create typed pointers matching the C runtime struct
                auto* uint8PtrDITy = dbgBuilder_->createPointerType(
                    dbgBuilder_->createBasicType("uint8", 8, llvm::dwarf::DW_ATE_unsigned),
                    dl.getPointerSizeInBits());
                auto* uint64PtrDITy = dbgBuilder_->createPointerType(
                    dbgBuilder_->createBasicType("uint64", 64, llvm::dwarf::DW_ATE_unsigned),
                    dl.getPointerSizeInBits());
                const char* fieldNames[6] = {
                    "states", "keys", "hashes",
                    "len", "cap", "key_size"
                };
                llvm::DIType* fieldTypes[6] = {
                    uint8PtrDITy, elemPtrDITy, uint64PtrDITy,
                    usizeDITy, usizeDITy, usizeDITy
                };
                for (unsigned i = 0; i < 6; i++) {
                    auto* member = dbgBuilder_->createMemberType(
                        dbgFile_, fieldNames[i], dbgFile_, 0,
                        dl.getTypeSizeInBits(setStructTy->getElementType(i)),
                        dl.getABITypeAlign(setStructTy->getElementType(i)).value() * 8,
                        layout->getElementOffsetInBits(i),
                        llvm::DINode::FlagZero, fieldTypes[i]);
                    members.push_back(member);
                }
            }
            auto memberArray = dbgBuilder_->getOrCreateArray(members);
            return dbgBuilder_->createStructType(
                dbgFile_, ti->name, dbgFile_, 0, sizeBits, alignBits,
                llvm::DINode::FlagZero, nullptr, memberArray);
        }
        if (ti->extendedKind == "Vec") {
            // Vec<T> = { ptr (data), usize (len), usize (capacity) }
            auto& dl = module_->getDataLayout();
            auto* vecLLTy = ti->toLLVMType(*context_, dl);
            auto* vecStructTy = llvm::dyn_cast<llvm::StructType>(vecLLTy);
            if (!vecStructTy || vecStructTy->getNumElements() < 3) return nullptr;
            uint64_t sizeBits = dl.getTypeSizeInBits(vecStructTy);
            uint32_t alignBits = dl.getABITypeAlign(vecStructTy).value() * 8;

            llvm::SmallVector<llvm::Metadata*, 8> members;
            if (auto* layout = dl.getStructLayout(vecStructTy)) {
                // data: ptr to element type (or void* if unknown)
                llvm::DIType* elemDITy = ti->elementType
                    ? getOrCreateDIType(ti->elementType) : nullptr;
                if (!elemDITy)
                    elemDITy = dbgBuilder_->createBasicType(
                        "void", 8, llvm::dwarf::DW_ATE_unsigned);
                auto* dataDITy = dbgBuilder_->createPointerType(
                    elemDITy, dl.getPointerSizeInBits());
                auto* dataMember = dbgBuilder_->createMemberType(
                    dbgFile_, "data", dbgFile_, 0,
                    dl.getTypeSizeInBits(vecStructTy->getElementType(0)),
                    dl.getABITypeAlign(vecStructTy->getElementType(0)).value() * 8,
                    layout->getElementOffsetInBits(0),
                    llvm::DINode::FlagZero, dataDITy);
                members.push_back(dataMember);

                // len: usize
                auto* lenDITy = dbgBuilder_->createBasicType(
                    "usize", dl.getPointerSizeInBits(), llvm::dwarf::DW_ATE_unsigned);
                auto* lenMember = dbgBuilder_->createMemberType(
                    dbgFile_, "len", dbgFile_, 0,
                    dl.getTypeSizeInBits(vecStructTy->getElementType(1)),
                    dl.getABITypeAlign(vecStructTy->getElementType(1)).value() * 8,
                    layout->getElementOffsetInBits(1),
                    llvm::DINode::FlagZero, lenDITy);
                members.push_back(lenMember);

                // capacity: usize
                auto* capDITy = dbgBuilder_->createBasicType(
                    "usize", dl.getPointerSizeInBits(), llvm::dwarf::DW_ATE_unsigned);
                auto* capMember = dbgBuilder_->createMemberType(
                    dbgFile_, "capacity", dbgFile_, 0,
                    dl.getTypeSizeInBits(vecStructTy->getElementType(2)),
                    dl.getABITypeAlign(vecStructTy->getElementType(2)).value() * 8,
                    layout->getElementOffsetInBits(2),
                    llvm::DINode::FlagZero, capDITy);
                members.push_back(capMember);
            }

            auto memberArray = dbgBuilder_->getOrCreateArray(members);
            return dbgBuilder_->createStructType(
                dbgFile_, ti->name, dbgFile_, 0, sizeBits, alignBits,
                llvm::DINode::FlagZero, nullptr, memberArray);
        }
        if (ti->extendedKind == "Map") {
            // Map<K,V> = { ptr (states), ptr (keys), ptr (values), ptr (hashes),
            //              usize (len), usize (cap), usize (key_size), usize (val_size) }
            auto& dl = module_->getDataLayout();
            auto* mapLLTy = ti->toLLVMType(*context_, dl);
            auto* mapStructTy = llvm::dyn_cast<llvm::StructType>(mapLLTy);
            if (!mapStructTy || mapStructTy->getNumElements() < 8) return nullptr;
            uint64_t sizeBits = dl.getTypeSizeInBits(mapStructTy);
            uint32_t alignBits = dl.getABITypeAlign(mapStructTy).value() * 8;

            // Build reusable DITypes
            auto* uint8PtrDITy = dbgBuilder_->createPointerType(
                dbgBuilder_->createBasicType("uint8", 8, llvm::dwarf::DW_ATE_unsigned),
                dl.getPointerSizeInBits());
            auto* uint64PtrDITy = dbgBuilder_->createPointerType(
                dbgBuilder_->createBasicType("uint64", 64, llvm::dwarf::DW_ATE_unsigned),
                dl.getPointerSizeInBits());
            auto* usizeDITy = dbgBuilder_->createBasicType(
                "usize", dl.getPointerSizeInBits(), llvm::dwarf::DW_ATE_unsigned);

            // Key type pointer
            llvm::DIType* keyDITy = ti->keyType
                ? getOrCreateDIType(ti->keyType) : nullptr;
            if (!keyDITy)
                keyDITy = dbgBuilder_->createBasicType(
                    "void", 8, llvm::dwarf::DW_ATE_unsigned);
            auto* keyPtrDITy = dbgBuilder_->createPointerType(
                keyDITy, dl.getPointerSizeInBits());

            // Value type pointer
            llvm::DIType* valDITy = ti->valueType
                ? getOrCreateDIType(ti->valueType) : nullptr;
            if (!valDITy)
                valDITy = dbgBuilder_->createBasicType(
                    "void", 8, llvm::dwarf::DW_ATE_unsigned);
            auto* valPtrDITy = dbgBuilder_->createPointerType(
                valDITy, dl.getPointerSizeInBits());

            llvm::SmallVector<llvm::Metadata*, 8> members;
            if (auto* layout = dl.getStructLayout(mapStructTy)) {
                const char* fieldNames[8] = {
                    "states", "keys", "values", "hashes",
                    "len", "cap", "key_size", "val_size"
                };
                llvm::DIType* fieldTypes[8] = {
                    uint8PtrDITy, keyPtrDITy, valPtrDITy, uint64PtrDITy,
                    usizeDITy, usizeDITy, usizeDITy, usizeDITy
                };
                for (unsigned i = 0; i < 8; i++) {
                    auto* member = dbgBuilder_->createMemberType(
                        dbgFile_, fieldNames[i], dbgFile_, 0,
                        dl.getTypeSizeInBits(mapStructTy->getElementType(i)),
                        dl.getABITypeAlign(mapStructTy->getElementType(i)).value() * 8,
                        layout->getElementOffsetInBits(i),
                        llvm::DINode::FlagZero, fieldTypes[i]);
                    members.push_back(member);
                }
            }
            auto memberArray = dbgBuilder_->getOrCreateArray(members);
            return dbgBuilder_->createStructType(
                dbgFile_, ti->name, dbgFile_, 0, sizeBits, alignBits,
                llvm::DINode::FlagZero, nullptr, memberArray);
        }
        // Other extended types (Task, Mutex) fall back to opaque
        return dbgBuilder_->createBasicType(ti->name, 64, llvm::dwarf::DW_ATE_unsigned);
    }
    default:
        return dbgBuilder_->createBasicType(ti->name, 64, llvm::dwarf::DW_ATE_unsigned);
    }
}



void IRGen::emitDbgDeclare(llvm::AllocaInst* alloca, const std::string& name,
                           const TypeInfo* ti, unsigned line, unsigned argNo) {
    if (!dbgBuilder_ || !currentDbgScope_ || !alloca) return;
    auto* diType = getOrCreateDIType(ti);
    if (!diType) return;
    
    llvm::DILocalVariable* var;
    if (argNo > 0) {
        var = dbgBuilder_->createParameterVariable(
            currentDbgScope_, name, argNo, dbgFile_, line, diType, true);
    } else {
        var = dbgBuilder_->createAutoVariable(
            currentDbgScope_, name, dbgFile_, line, diType, true);
    }
    
    auto* loc = llvm::DILocation::get(*context_, line, 0, currentDbgScope_);
    dbgBuilder_->insertDeclare(
        alloca, var, dbgBuilder_->createExpression(), loc, builder_->GetInsertBlock());

}

// ── Visitors ────────────────────────────────────────────────────────────────



std::any IRGen::visitProgram(LucisParser::ProgramContext* ctx) {
    // ── Register built-in Error struct type in LLVM ─────────────────────
    {
        auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
        auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
        auto* i32Ty   = llvm::Type::getInt32Ty(*context_);
        auto* i64Ty   = llvm::Type::getInt64Ty(*context_);
        auto* strTy   = llvm::StructType::get(*context_, { ptrTy, usizeTy });
        auto* errorTy = llvm::StructType::create(
            *context_,
            { strTy, strTy, i32Ty, i32Ty, i64Ty, i64Ty },
            "Error");
        (void)errorTy;
    }

    // ── Register C struct types from #include bindings ────────────────
    if (cBindings_) {
        // Phase 1: Create all C structs as opaque LLVM types (forward decls)
        // This ensures all types exist before resolving inter-struct references.
        for (auto& [name, cstruct] : cBindings_->structs()) {
            if (llvm::StructType::getTypeByName(*context_, name))
                continue;
            llvm::StructType::create(*context_, name);
        }

        // Phase 2a: Set bodies for opaque types (unions, bitfield structs)
        // These use a byte-array representation and must be sized first so
        // that structs referencing them can be properly laid out.
        for (auto& [name, cstruct] : cBindings_->structs()) {
            if (!cstruct.fields.empty()) continue;

            auto* structTy = llvm::StructType::getTypeByName(*context_, name);
            if (!structTy || !structTy->isOpaque()) continue;

            if (cstruct.byteSize > 0) {
                auto* i8Ty = llvm::Type::getInt8Ty(*context_);
                auto* arrTy = llvm::ArrayType::get(i8Ty, cstruct.byteSize);
                structTy->setBody({ arrTy });
            }
        }

        // Phase 2b: Set bodies for structs with resolved fields
        for (auto& [name, cstruct] : cBindings_->structs()) {
            if (cstruct.fields.empty()) continue;

            auto* structTy = llvm::StructType::getTypeByName(*context_, name);
            if (!structTy || !structTy->isOpaque()) continue;

            std::vector<llvm::Type*> fieldTypes;
            bool allResolved = true;
            for (auto& field : cstruct.fields) {
                auto* llTy = field.typeInfo->toLLVMType(*context_,
                                module_->getDataLayout());
                if (!llTy) { allResolved = false; break; }
                fieldTypes.push_back(llTy);
            }
            if (!allResolved) continue;

            structTy->setBody(fieldTypes);

            // Also register in our TypeRegistry so resolveTypeInfo works
            TypeInfo ti;
            ti.name = name;
            ti.kind = TypeKind::Struct;
            ti.bitWidth = 0;
            ti.isSigned = false;
            ti.fields = cstruct.fields;
            typeRegistry_.registerType(std::move(ti));
        }  // end struct field body for-loop

    }  // end if (cBindings_)

    // ── Debug info: create compile unit ─────────────────────────────────
    if (emitDebugInfo_) {
        dbgBuilder_ = new llvm::DIBuilder(*module_);
        dbgFile_ = dbgBuilder_->createFile(
            currentFile_, ".");
        dbgCU_ = dbgBuilder_->createCompileUnit(
            llvm::dwarf::DW_LANG_C, dbgFile_, "lucis",
            true, "", 0);
    }

    if (cBindings_) {
        // Register C enum constants as globals
        for (auto& [name, cenum] : cBindings_->enums()) {
            for (auto& [vname, val] : cenum.values) {
                cEnumConstants_[vname] = val;
            }
        }

        // Register C #define constants (integer, float, string)
        for (auto& [name, cmacro] : cBindings_->macros()) {
            if (cmacro.isFloat)
                cFloatConstants_[name] = cmacro.floatValue;
            else if (cmacro.isString)
                cStringConstants_[name] = cmacro.stringValue;
            else
                cEnumConstants_[name] = cmacro.value;
        }

        // Register C #define struct literal macros (e.g. RAYWHITE)
        for (auto& [name, sm] : cBindings_->structMacros()) {
            cStructMacros_[name] = &sm;
        }

        // Declare C global variables as extern globals
        for (auto& [name, gvar] : cBindings_->globals()) {
            auto* llTy = gvar.type->toLLVMType(*context_,
                             module_->getDataLayout());
            if (!llTy) continue;

            auto* gv = new llvm::GlobalVariable(
                *module_, llTy, false,
                llvm::GlobalValue::ExternalLinkage,
                nullptr, name);
            cGlobals_[name] = { gv, gvar.type };
        }

        // Register C typedefs as type aliases
        for (auto& [name, ctdef] : cBindings_->typedefs()) {
            if (!typeRegistry_.lookup(name)) {
                TypeInfo ti = *ctdef.underlying;
                ti.name = name;
                typeRegistry_.registerType(std::move(ti));
            }
        }
    }

    // Process `use` declarations first so all imports are known
    for (auto* pre : ctx->preambleDecl()) {
        if (auto* ud = pre->useDecl()) visit(ud);
    }
    for (auto* tld : ctx->topLevelDecl()) {
        if (auto* ud = tld->useDecl()) visit(ud);
    }

    // Register cross-file symbols (extern declarations, types, etc.)
    registerCrossFileSymbols(ctx);

    // Register type aliases first
    for (auto* decl : ctx->topLevelDecl()) {
        auto* aliasDecl = decl->typeAliasDecl();
        if (!aliasDecl)
            continue;
        auto name = aliasDecl->IDENTIFIER()->getText();
        if (!typeRegistry_.lookup(name))
            visit(decl);
    }
    // Register struct, union, and enum types first
    for (auto* decl : ctx->topLevelDecl()) {
        if (auto* structDecl = decl->structDecl()) {
            auto name = structDecl->IDENTIFIER()->getText();
            auto* existing = typeRegistry_.lookup(name);
            bool needsCompletion =
                existing && existing->kind == TypeKind::Struct && existing->fields.empty();
            if ((!existing || needsCompletion) && !genericStructTemplates_.count(name))
                visit(decl);
        } else if (auto* unionDecl = decl->unionDecl()) {
            auto name = unionDecl->IDENTIFIER()->getText();
            auto* existing = typeRegistry_.lookup(name);
            bool needsCompletion =
                existing && existing->kind == TypeKind::Union && existing->fields.empty();
            if ((!existing || needsCompletion) && !genericUnionTemplates_.count(name))
                visit(decl);
        } else if (auto* enumDecl = decl->enumDecl()) {
            auto name = enumDecl->IDENTIFIER()->getText();
            if (!typeRegistry_.lookup(name) && !genericEnumTemplates_.count(name))
                visit(decl);
        } else if (auto* bitD = decl->bitstructDecl()) {
            auto name = bitD->IDENTIFIER()->getText();
            if (!typeRegistry_.lookup(name))
                visit(decl);
        }
    }
    // Register extern (FFI) function declarations
    // Must come BEFORE extend blocks so methods can call extern functions.
    for (auto* decl : ctx->topLevelDecl()) {
        if (decl->externDecl())
            visit(decl);
    }
    // Collect and process top-level const declarations
    for (auto* decl : ctx->topLevelDecl()) {
        if (auto* constDecl = decl->constDeclStmt())
            visit(constDecl);
    }
    // Forward-declare all user functions (signatures only) BEFORE extend
    // blocks so methods can call module-level functions directly.
    for (auto* decl : ctx->topLevelDecl()) {
        if (decl->functionDecl())
            forwardDeclareFunction(decl->functionDecl());
    }
    // Register struct methods via `extend` blocks
    for (auto* decl : ctx->topLevelDecl()) {
        if (decl->extendDecl())
            visit(decl);
    }
    // Register trait declarations
    for (auto* decl : ctx->topLevelDecl()) {
        if (decl->traitDecl())
            visit(decl);
    }
    // Register impl declarations (must follow trait declarations)
    for (auto* decl : ctx->topLevelDecl()) {
        if (decl->implDecl())
            visit(decl);
    }
    // Process c_macro blocks
    for (auto* decl : ctx->topLevelDecl()) {
        if (auto* cm = decl->cMacroBlock())
            visitCMacroBlock(cm);
    }
    // Process inline assembly blocks
    for (auto* decl : ctx->topLevelDecl()) {
        if (auto* ab = decl->asmBBlock())
            visitAsmBBlock(ab);
    }

    // Then process functions (generate bodies)
    for (auto* decl : ctx->topLevelDecl()) {
        if (decl->functionDecl())
            visit(decl);
    }
    
    // Generate initialization function for top-level consts
    if (!pendingConstDecls_.empty()) {
        generateConstInitFunction();
    }
    
    // ── #[vector_table] — generate interrupt vector table ──────────
    if (!interruptTable_.empty())
        emitVectorTable();
    
    return {};
}



void IRGen::generateConstInitFunction() {
    // Create function: void __lucis_const_init()
    auto* voidTy = llvm::Type::getVoidTy(*context_);
    auto* funcTy = llvm::FunctionType::get(voidTy, {}, false);
    auto* initFunc = llvm::Function::Create(
        funcTy,
        llvm::GlobalValue::PrivateLinkage,
        "__lucis_const_init",
        module_
    );

    // Create entry block
    auto* entryBlock = llvm::BasicBlock::Create(*context_, "entry", initFunc);
    builder_->SetInsertPoint(entryBlock);

    auto* prevFunc = currentFunction_;
    currentFunction_ = initFunc;

    // Evaluate each const declarator and store its value
    for (auto& [decl, global] : pendingConstDecls_) {
        if (!decl->expression()) continue;

        auto* exprVal = std::any_cast<llvm::Value*>(visit(decl->expression()));
        if (!exprVal) continue;

        // Coerce to the global's type if needed
        auto* globalTy = global->getValueType();
        if (exprVal->getType() != globalTy) {
            bool srcSigned = true;
            if (auto* srcTI = resolveExprTypeInfo(decl->expression())) {
                if (srcTI->kind == TypeKind::Integer) srcSigned = srcTI->isSigned;
            }
            exprVal = coerceValueToType(exprVal, globalTy, srcSigned);
        }

        // Store the evaluated value into the global variable
        builder_->CreateStore(exprVal, global);
    }

    // Return from init function
    builder_->CreateRetVoid();

    currentFunction_ = prevFunc;

    // Register with global constructors using appendToGlobalCtors
    llvm::appendToGlobalCtors(*module_, initFunc, 0);
}



void IRGen::emitVectorTable() {
    auto* voidTy = llvm::Type::getVoidTy(*context_);
    auto* fnTy = llvm::FunctionType::get(voidTy, false);
    auto* ptrTy = llvm::PointerType::getUnqual(*context_);
    auto* arrTy = llvm::ArrayType::get(ptrTy, 256);

    std::vector<llvm::Constant*> entries(256,
        llvm::ConstantPointerNull::get(ptrTy));

    for (auto& entry : interruptTable_) {
        if (entry.vector >= 0 && entry.vector < 256 && entry.wrapper)
            entries[entry.vector] = llvm::ConstantExpr::getBitCast(
                entry.wrapper, ptrTy);
    }

    auto* init = llvm::ConstantArray::get(arrTy, entries);
    auto* gv = new llvm::GlobalVariable(*module_, arrTy, true,
        llvm::GlobalValue::ExternalLinkage, init, "vector_table");
    gv->setSection(".isr_vector");
    gv->setAlignment(llvm::MaybeAlign(16));
}



std::any IRGen::visitExternDecl(LucisParser::ExternDeclContext* ctx) {
        SET_DBG_LOC(ctx);
    auto funcName = ctx->IDENTIFIER()->getText();

    // Don't re-declare if already present
    if (module_->getFunction(funcName))
        return {};

    auto* retTI   = resolveTypeInfo(ctx->typeSpec());
    if (!retTI) return {};
    auto* retLLTy = retTI->toLLVMType(*context_, module_->getDataLayout());

    bool isVariadic = (ctx->SPREAD() != nullptr);

    std::vector<llvm::Type*> paramTypes;
    if (auto* paramList = ctx->externParamList()) {
        for (auto* param : paramList->externParam()) {
            auto* pTI   = resolveTypeInfo(param->typeSpec());
            if (!pTI) pTI = typeRegistry_.lookup("int32");
            auto* pLLTy = pTI->toLLVMType(*context_, module_->getDataLayout());
            paramTypes.push_back(pLLTy);
        }
    }

    auto* fnType = llvm::FunctionType::get(retLLTy, paramTypes, isVariadic);
    auto* fn = llvm::Function::Create(
        fnType, llvm::Function::ExternalLinkage, funcName, module_);

    // Mark as an extern C function for call resolution
    externCFunctions_.insert(funcName);

    return {};
}



bool IRGen::syncTypeFromSemanticDB(const std::string& name) {
    if (!semanticDB_) return true;
    if (typeRegistry_.lookup(name)) return true;

    const auto* decl = semanticDB_->lookupAny(name);
    if (!decl) return false;

    switch (decl->kind) {
    case semantic::DeclKind::Struct: {
        const auto* sd = decl->as<semantic::StructDecl>();
        if (!sd) return false;
        TypeInfo ti;
        ti.name = sd->name;
        ti.kind = TypeKind::Struct;
        ti.bitWidth = 0;
        ti.isSigned = false;
        ti.dropTracked = sd->dropTracked;
        ti.moveOnly = sd->moveOnly;
        for (const auto& f : sd->fields) {
            FieldInfo fi;
            fi.name = f.name;
            fi.arrayDims = f.arrayDims;
            fi.arraySizes = f.arraySizes;
            fi.autoFill = f.autoFill;
            if (f.type)
                fi.typeInfo = typeRegistry_.lookup(f.type->name);
            ti.fields.push_back(std::move(fi));
        }
        typeRegistry_.registerType(std::move(ti));
        return true;
    }
    case semantic::DeclKind::Union: {
        const auto* ud = decl->as<semantic::UnionDecl>();
        if (!ud) return false;
        TypeInfo ti;
        ti.name = ud->name;
        ti.kind = TypeKind::Union;
        ti.bitWidth = 0;
        ti.isSigned = false;
        ti.dropTracked = ud->dropTracked;
        ti.moveOnly = ud->moveOnly;
        for (const auto& f : ud->fields) {
            FieldInfo fi;
            fi.name = f.name;
            fi.arrayDims = f.arrayDims;
            fi.arraySizes = f.arraySizes;
            fi.autoFill = f.autoFill;
            if (f.type)
                fi.typeInfo = typeRegistry_.lookup(f.type->name);
            ti.fields.push_back(std::move(fi));
        }
        typeRegistry_.registerType(std::move(ti));
        return true;
    }
    case semantic::DeclKind::Enum: {
        const auto* ed = decl->as<semantic::EnumDecl>();
        if (!ed) return false;
        TypeInfo ti;
        ti.name = ed->name;
        ti.kind = TypeKind::Enum;
        ti.bitWidth = ed->bitWidth;
        ti.isSigned = false;
        ti.builtinSuffix = "i32";
        ti.dropTracked = ed->dropTracked;
        ti.moveOnly = ed->moveOnly;
        for (const auto& v : ed->variants) {
            ti.enumVariants.push_back(v.name);
            EnumVariantInfo evi;
            evi.name = v.name;
            evi.discriminant = v.discriminant;
            switch (v.payloadKind) {
            case semantic::VariantPayloadKind::Unit:  evi.payloadKind = EnumPayloadKind::Unit; break;
            case semantic::VariantPayloadKind::Tuple: evi.payloadKind = EnumPayloadKind::Tuple; break;
            case semantic::VariantPayloadKind::Named: evi.payloadKind = EnumPayloadKind::Named; break;
            }
            for (const auto& pf : v.payloadFields) {
                FieldInfo fi;
                fi.name = pf.name;
                fi.arrayDims = pf.arrayDims;
                fi.arraySizes = pf.arraySizes;
                fi.autoFill = pf.autoFill;
                if (pf.type)
                    fi.typeInfo = typeRegistry_.lookup(pf.type->name);
                evi.payloadFields.push_back(std::move(fi));
            }
            ti.enumVariantInfos.push_back(std::move(evi));
        }
        typeRegistry_.registerType(std::move(ti));
        return true;
    }
    default:
        return false;
    }
}



void IRGen::declareExternFunction(const std::string& mangledName,
                                   LucisParser::FunctionDeclContext* decl) {
    auto* retTI     = resolveTypeInfo(decl->typeSpec());
    if (!retTI) return;
    auto* retLLType = retTI->toLLVMType(*context_, module_->getDataLayout());

    std::vector<llvm::Type*> paramTypes;
    int variadicIdx = -1;
    bool isVarArg = false;

    if (auto* params = decl->paramList()) {
        auto paramList = params->param();
        for (size_t i = 0; i < paramList.size(); i++) {
            auto* param = paramList[i];

            // Untyped variadic: ...
            if (param->SPREAD() && !param->typeSpec()) {
                isVarArg = true;
                break;
            }

            auto* pTI = resolveTypeInfo(param->typeSpec());
            unsigned pDims = countArrayDims(param->typeSpec());
            if (param->SPREAD()) {
                variadicIdx = static_cast<int>(i);
                paramTypes.push_back(llvm::PointerType::getUnqual(*context_));
                paramTypes.push_back(llvm::Type::getInt64Ty(*context_));
            } else if (pDims > 0) {
                // []T param: lower to the { ptr, usize } slice ABI.
                auto* ptrTy = llvm::PointerType::getUnqual(*context_);
                auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
                paramTypes.push_back(llvm::StructType::get(*context_, {ptrTy, usizeTy}));
            } else {
                paramTypes.push_back(pTI ? pTI->toLLVMType(*context_, module_->getDataLayout())
                                         : llvm::Type::getInt32Ty(*context_));
            }
        }
    }

    auto* fnType = llvm::FunctionType::get(retLLType, paramTypes, isVarArg);
    llvm::Function::Create(fnType, llvm::Function::ExternalLinkage,
                           mangledName, module_);

    // Register variadic info under the mangled name (only for typed variadics)
    if (variadicIdx >= 0 && decl->paramList()) {
        auto* vParam = decl->paramList()->param(variadicIdx);
        auto* vInfo = resolveTypeInfo(vParam->typeSpec());
        variadicFunctions_[mangledName] = {
            static_cast<size_t>(variadicIdx), vInfo
        };
    }
}



std::string IRGen::resolveCallTarget(const std::string& name) const {
    auto it = callTargetMap_.find(name);
    if (it != callTargetMap_.end())
        return it->second;
    return name;
}



llvm::Value* IRGen::coerceValueToType(llvm::Value* value,
                                      llvm::Type* targetType,
                                      bool assumeSignedInt) {
    if (!value || !targetType) return value;
    auto* srcType = value->getType();
    if (srcType == targetType) return value;

    if (srcType->isIntegerTy() && targetType->isIntegerTy())
        return builder_->CreateIntCast(value, targetType, assumeSignedInt);

    if (srcType->isFloatingPointTy() && targetType->isFloatingPointTy()) {
        if (srcType->getPrimitiveSizeInBits() > targetType->getPrimitiveSizeInBits())
            return builder_->CreateFPTrunc(value, targetType);
        return builder_->CreateFPExt(value, targetType);
    }

    if (srcType->isIntegerTy() && targetType->isFloatingPointTy())
        return builder_->CreateSIToFP(value, targetType);

    if (srcType->isFloatingPointTy() && targetType->isIntegerTy())
        return builder_->CreateFPToSI(value, targetType);

    if (srcType->isStructTy() && targetType->isStructTy()) {
        auto* srcST = llvm::cast<llvm::StructType>(srcType);
        auto* dstST = llvm::cast<llvm::StructType>(targetType);
        if (srcST->getNumElements() != dstST->getNumElements())
            return value;

        llvm::Value* agg = llvm::UndefValue::get(dstST);
        for (unsigned i = 0; i < srcST->getNumElements(); i++) {
            auto* elem = builder_->CreateExtractValue(value, {i}, "coerce_e");
            auto* coercedElem = coerceValueToType(elem, dstST->getElementType(i), assumeSignedInt);
            if (!coercedElem || coercedElem->getType() != dstST->getElementType(i))
                return value;
            agg = builder_->CreateInsertValue(agg, coercedElem, {i}, "coerce_ins");
        }
        return agg;
    }

    return value;
}

// Convert pointer to intptr for bitwise/shift operations.
// LLVM does not allow bitwise ops directly on pointer types.


llvm::Value* IRGen::ptrToIntIfNeeded(llvm::Value* val) {
    if (val->getType()->isPointerTy()) {
        auto* i64Ty = llvm::Type::getInt64Ty(*context_);
        return builder_->CreatePtrToInt(val, i64Ty, "ptr2int");
    }
     return val;
}

// const NAME = VALUE; or const NAME: TYPE = VALUE;


llvm::ArrayType* IRGen::buildTargetArrayType(llvm::Type* srcTy, llvm::Type* elemTy) {
    auto* srcArr  = llvm::cast<llvm::ArrayType>(srcTy);
    auto  count   = srcArr->getNumElements();
    auto* srcElem = srcArr->getElementType();
    if (srcElem->isArrayTy())
        return llvm::ArrayType::get(buildTargetArrayType(srcElem, elemTy), count);
    return llvm::ArrayType::get(elemTy, count);
}



void IRGen::storeArrayElements(llvm::Value* src, llvm::Value* destPtr,
                                llvm::Type* destArrTy, const TypeInfo* elemTI,
                                unsigned dims) {
    auto* arrTy = llvm::cast<llvm::ArrayType>(destArrTy);
    unsigned count = arrTy->getNumElements();
    auto* i64Ty = llvm::Type::getInt64Ty(*context_);
    auto* zero  = llvm::ConstantInt::get(i64Ty, 0);

    for (unsigned i = 0; i < count; i++) {
        auto* elem = builder_->CreateExtractValue(src, {i});
        auto* idx  = llvm::ConstantInt::get(i64Ty, i);
        auto* gep  = builder_->CreateGEP(destArrTy, destPtr, { zero, idx });

        if (dims > 1) {
            storeArrayElements(elem, gep, arrTy->getElementType(), elemTI, dims - 1);
        } else {
            auto* targetElemTy = arrTy->getElementType();
            if (elem->getType() != targetElemTy) {
                if (elem->getType()->isIntegerTy() && targetElemTy->isIntegerTy())
                    elem = builder_->CreateIntCast(elem, targetElemTy, elemTI->isSigned);
                else if (elem->getType()->isFloatingPointTy() && targetElemTy->isFloatingPointTy()) {
                    if (elem->getType()->getPrimitiveSizeInBits() > targetElemTy->getPrimitiveSizeInBits())
                        elem = builder_->CreateFPTrunc(elem, targetElemTy);
                    else
                        elem = builder_->CreateFPExt(elem, targetElemTy);
                }
            }
            builder_->CreateStore(elem, gep);
        }
    }
}



llvm::Value* IRGen::lowerArrayToSlice(llvm::Value* arrVal, llvm::Type* sliceTy) {
    auto* i64Ty = llvm::Type::getInt64Ty(*context_);
    auto* zero  = llvm::ConstantInt::get(i64Ty, 0);

    llvm::Value* srcAlloca = nullptr;
    if (auto* li = llvm::dyn_cast<llvm::LoadInst>(arrVal))
        srcAlloca = li->getPointerOperand();
    if (!srcAlloca) {
        srcAlloca = builder_->CreateAlloca(arrVal->getType(), nullptr, "arr.slice");
        builder_->CreateStore(arrVal, srcAlloca);
    }

    auto* elemPtr = builder_->CreateGEP(arrVal->getType(), srcAlloca,
                                        {zero, zero}, "arr.slice.ptr");
    auto* ptrFieldTy = llvm::cast<llvm::StructType>(sliceTy)->getElementType(0);
    if (elemPtr->getType() != ptrFieldTy)
        elemPtr = builder_->CreateBitCast(elemPtr, ptrFieldTy, "arr.slice.cast");

    auto* lenFieldTy = llvm::cast<llvm::StructType>(sliceTy)->getElementType(1);
    auto* lenField = llvm::ConstantInt::get(
        llvm::cast<llvm::IntegerType>(lenFieldTy),
        llvm::cast<llvm::ArrayType>(arrVal->getType())->getNumElements());

    llvm::Value* slice = llvm::UndefValue::get(sliceTy);
    slice = builder_->CreateInsertValue(slice, elemPtr, {0}, "arr.slice.ins.ptr");
    slice = builder_->CreateInsertValue(slice, lenField, {1}, "arr.slice.ins.len");

    if (auto* li = llvm::dyn_cast<llvm::LoadInst>(arrVal))
        if (li->use_empty())
            li->eraseFromParent();

    return slice;
}

// Declare (or retrieve an already-declared) external C function in the module.


llvm::FunctionCallee IRGen::declareBuiltin(const std::string&           name,
                                           llvm::Type*                  retType,
                                           llvm::ArrayRef<llvm::Type*>  argTypes) {
    if (auto* existing = module_->getFunction(name)) {
        return { existing->getFunctionType(), existing };
    }
    auto* fnType = llvm::FunctionType::get(retType, argTypes, false);
    return module_->getOrInsertFunction(name, fnType);
}



llvm::StructType* IRGen::getOrCreateVecStructType() {
    auto* existing = llvm::StructType::getTypeByName(*context_, "lucis_vec_header");
    if (existing) return existing;

    auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
    auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
    return llvm::StructType::create(*context_,
        { ptrTy, usizeTy, usizeTy }, "lucis_vec_header");
}



llvm::StructType* IRGen::getOrCreateMapStructType() {
    auto* existing = llvm::StructType::getTypeByName(*context_, "lucis_map_header");
    if (existing) return existing;

    auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
    auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
    return llvm::StructType::create(*context_,
        { ptrTy, ptrTy, ptrTy, ptrTy,
          usizeTy, usizeTy, usizeTy, usizeTy }, "lucis_map_header");
}



llvm::StructType* IRGen::getOrCreateSetStructType() {
    auto* existing = llvm::StructType::getTypeByName(*context_, "lucis_set_header");
    if (existing) return existing;

    auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
    auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
    return llvm::StructType::create(*context_,
        { ptrTy, ptrTy, ptrTy,
          usizeTy, usizeTy, usizeTy }, "lucis_set_header");
}



std::string IRGen::getVecSuffix(const TypeInfo* elemTI) {
    if (!elemTI) return "i32";
    if (elemTI->builtinSuffix.empty()) return "raw";
    if (elemTI->builtinSuffix == "ptr") return "raw";
    return elemTI->builtinSuffix;
}

// ── Safe any_cast helper ────────────────────────────────────────────────────
// Returns nullptr (instead of throwing std::bad_any_cast) when the
// visit result is empty or holds a different type.


llvm::Value* IRGen::castValue(std::any result) {
    if (!result.has_value()) {
        std::cerr << "[IRGen] internal error: visit returned empty result\n";
        return nullptr;
    }
    if (auto* ptr = std::any_cast<llvm::Value*>(&result))
        return *ptr;
    std::cerr << "[IRGen] internal error: visit returned unexpected type\n";
    return nullptr;
}

// ── Defer infrastructure ────────────────────────────────────────────────────



bool IRGen::hasAttribute(LucisParser::AttributeListContext* attrs, const std::string& name) {
    if (!attrs) return false;
    for (auto* a : attrs->attribute()) {
        if (a->IDENTIFIER() && a->IDENTIFIER()->getText() == name)
            return true;
    }
    return false;
}



bool IRGen::hasReprPacked(LucisParser::AttributeListContext* attrs) {
    if (!attrs) return false;
    for (auto* a : attrs->attribute()) {
        if (a->IDENTIFIER() && a->IDENTIFIER()->getText() == "repr") {
            if (auto* argList = a->attrArgList()) {
                for (auto* arg : argList->attrArg()) {
                    if (arg->IDENTIFIER() && arg->IDENTIFIER()->getText() == "packed")
                        return true;
                }
            }
        }
    }
    return false;
}



std::string IRGen::getAttributeStringArg(LucisParser::AttributeListContext* attrs, const std::string& name) {
    if (!attrs) return "";
    for (auto* a : attrs->attribute()) {
        if (a->IDENTIFIER() && a->IDENTIFIER()->getText() == name) {
            if (auto* argList = a->attrArgList()) {
                for (auto* arg : argList->attrArg()) {
                    if (arg->STR_LIT()) {
                        auto raw = arg->STR_LIT()->getText();
                        // Strip surrounding quotes
                        if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"')
                            return raw.substr(1, raw.size() - 2);
                        return raw;
                    }
                    if (arg->C_STR_LIT()) {
                        auto raw = arg->C_STR_LIT()->getText();
                        if (raw.size() >= 3 && raw.front() == 'c' && raw[1] == '"' && raw.back() == '"')
                            return raw.substr(2, raw.size() - 3);
                        return raw;
                    }
                }
            }
        }
    }
    return "";
}

