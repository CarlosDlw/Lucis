#include "types/TypeInfo.h"

#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/LLVMContext.h>

static llvm::Type* fieldToLLVMType(const FieldInfo& field,
                                   llvm::LLVMContext& ctx,
                                   const llvm::DataLayout& dl) {
    auto* baseTy = field.typeInfo->toLLVMType(ctx, dl);
    if (field.arrayDims == 0) return baseTy;

    // []T in enum payloads — checker rejects this at compile time.
    // Represent as {T*, i64} slice as a safe fallback.
    if (field.arraySizes.empty()) {
        return llvm::StructType::get(ctx, {
            llvm::PointerType::getUnqual(ctx),
            dl.getIntPtrType(ctx)
        });
    }

    llvm::Type* arrTy = baseTy;
    for (auto it = field.arraySizes.rbegin(); it != field.arraySizes.rend(); ++it) {
        arrTy = llvm::ArrayType::get(arrTy, *it);
    }
    return arrTy;
}

static llvm::Type* buildEnumVariantPayloadType(const EnumVariantInfo& variant,
                                               llvm::LLVMContext& ctx,
                                               const llvm::DataLayout& dl) {
    if (variant.payloadFields.empty()) return nullptr;

    if (variant.payloadKind == EnumPayloadKind::Tuple && variant.payloadFields.size() == 1)
        return fieldToLLVMType(variant.payloadFields[0], ctx, dl);

    std::vector<llvm::Type*> fieldTypes;
    for (const auto& field : variant.payloadFields)
        fieldTypes.push_back(fieldToLLVMType(field, ctx, dl));
    return llvm::StructType::get(ctx, fieldTypes);
}

static bool enumHasPayload(const TypeInfo& typeInfo) {
    for (const auto& variant : typeInfo.enumVariantInfos) {
        if (!variant.payloadFields.empty()) return true;
    }
    return false;
}

llvm::Type* TypeInfo::toLLVMType(llvm::LLVMContext& ctx,
                                 const llvm::DataLayout& dl) const {
    // Fixed-size inline array: [N x elemTy]
    if (arraySize > 0 && elementType) {
        auto* elemTy = elementType->toLLVMType(ctx, dl);
        return llvm::ArrayType::get(elemTy, arraySize);
    }

    // Multi-dim array type alias: [N][M]T → [N x [M x elemTy]]
    if (!arraySizes.empty() && elementType) {
        auto* elemTy = elementType->toLLVMType(ctx, dl);
        llvm::Type* arrTy = elemTy;
        for (auto it = arraySizes.rbegin(); it != arraySizes.rend(); ++it)
            arrTy = llvm::ArrayType::get(arrTy, *it);
        return arrTy;
    }

    switch (kind) {
    case TypeKind::Bool:
        return llvm::Type::getInt1Ty(ctx);

    case TypeKind::Char:
        return llvm::Type::getInt8Ty(ctx);

    case TypeKind::Integer:
        if (bitWidth == 0)
            return dl.getIntPtrType(ctx); // isize / usize
        return llvm::Type::getIntNTy(ctx, bitWidth);

    case TypeKind::Float:
        if (bitWidth == 128)
            return llvm::Type::getFP128Ty(ctx);
        if (bitWidth == 80)
            return llvm::Type::getX86_FP80Ty(ctx);
        if (bitWidth == 64)
            return llvm::Type::getDoubleTy(ctx);
        return llvm::Type::getFloatTy(ctx);

    case TypeKind::Void:
        return llvm::Type::getVoidTy(ctx);

    case TypeKind::String:
        // String is a struct { ptr (to uint8 data), usize (len) }
        return llvm::StructType::get(ctx, {
            llvm::PointerType::getUnqual(ctx),
            dl.getIntPtrType(ctx)
        });

    case TypeKind::Struct: {
        // Volatile qualifier does not change the LLVM type
        auto lookupName = name;
        if (lookupName.rfind("volatile ", 0) == 0)
            lookupName = lookupName.substr(9);
        auto* st = llvm::StructType::getTypeByName(ctx, lookupName);
        return st;
    }

    case TypeKind::Union: {
        // Volatile qualifier does not change the LLVM type
        auto lookupName = name;
        if (lookupName.rfind("volatile ", 0) == 0)
            lookupName = lookupName.substr(9);
        auto* st = llvm::StructType::getTypeByName(ctx, lookupName);
        return st;
    }

    case TypeKind::Enum: {
        if (!enumHasPayload(*this))
            return llvm::Type::getInt32Ty(ctx);

        auto* existing = llvm::StructType::getTypeByName(ctx, name);
        if (existing) return existing;

        llvm::Type* storageTy = nullptr;
        llvm::TypeSize maxSize = llvm::TypeSize::getFixed(0);
        llvm::Align maxAlign(1);

        for (const auto& variant : enumVariantInfos) {
            auto* payloadTy = buildEnumVariantPayloadType(variant, ctx, dl);
            if (!payloadTy) continue;

            auto size = dl.getTypeAllocSize(payloadTy);
            auto align = dl.getABITypeAlign(payloadTy);
            if (!storageTy || size > maxSize || (size == maxSize && align > maxAlign)) {
                storageTy = payloadTy;
                maxSize = size;
                maxAlign = align;
            }
        }

        if (!storageTy)
            return llvm::Type::getInt32Ty(ctx);

        return llvm::StructType::create(ctx,
            {llvm::Type::getInt32Ty(ctx), storageTy}, name);
    }

    case TypeKind::Pointer:
        return llvm::PointerType::getUnqual(ctx);

    case TypeKind::VAList: {
        // va_list is target-dependent. On x86_64 Linux/Darwin it's a struct
        // of 4 fields (gp_off, fp_off, overflow_area, reg_save_area).
        // On other targets it's often just a pointer.
        // LLVM's va_start/va_arg intrinsics expect the target-specific va_list type.
        auto* existing = llvm::StructType::getTypeByName(ctx, "struct.__va_list_tag");
        if (existing) return existing;

        // Fallback: create a generic byte buffer that is large enough for most ABIs (24-32 bytes)
        // This is a safety measure; the real type should ideally be queried from the target.
        return llvm::StructType::create(ctx, {
            llvm::Type::getInt32Ty(ctx), // gp_offset
            llvm::Type::getInt32Ty(ctx), // fp_offset
            llvm::PointerType::getUnqual(ctx), // overflow_arg_area
            llvm::PointerType::getUnqual(ctx)  // reg_save_area
        }, "struct.__va_list_tag");
    }

    case TypeKind::Closure:
    case TypeKind::Function:
        // Function values are represented as opaque pointers (function pointers)
        // Closures are fat pointers: {fn_ptr, context_ptr}
        if (kind == TypeKind::Closure) {
            auto* ptrTy = llvm::PointerType::getUnqual(ctx);
            return llvm::StructType::get(ctx, {ptrTy, ptrTy});
        }
        return llvm::PointerType::getUnqual(ctx);

    case TypeKind::Tuple: {
        // Tuple is an anonymous struct: { T1, T2, ... }
        std::vector<llvm::Type*> elemTypes;
        for (auto* elem : tupleElements)
            elemTypes.push_back(elem->toLLVMType(ctx, dl));
        return llvm::StructType::get(ctx, elemTypes);
    }

    case TypeKind::Extended: {
        if (extendedKind == "Task" || extendedKind == "Mutex") {
            return llvm::PointerType::getUnqual(ctx);
        }
        if (extendedKind == "Map") {
            auto* existing = llvm::StructType::getTypeByName(ctx, "lucis_map_header");
            if (existing) return existing;
            auto* ptrTy   = llvm::PointerType::getUnqual(ctx);
            auto* usizeTy = dl.getIntPtrType(ctx);
            return llvm::StructType::create(ctx, {
                ptrTy, ptrTy, ptrTy, ptrTy,
                usizeTy, usizeTy, usizeTy, usizeTy
            }, "lucis_map_header");
        }
        if (extendedKind == "Set") {
            auto* existing = llvm::StructType::getTypeByName(ctx, "lucis_set_header");
            if (existing) return existing;
            auto* ptrTy   = llvm::PointerType::getUnqual(ctx);
            auto* usizeTy = dl.getIntPtrType(ctx);
            return llvm::StructType::create(ctx, {
                ptrTy, ptrTy, ptrTy,
                usizeTy, usizeTy, usizeTy
            }, "lucis_set_header");
        }
        // All Vec<T> variants share the same LLVM struct: { ptr, usize, usize }
        auto* existing = llvm::StructType::getTypeByName(ctx, "lucis_vec_header");
        if (existing) return existing;
        return llvm::StructType::create(ctx, {
            llvm::PointerType::getUnqual(ctx),
            dl.getIntPtrType(ctx),
            dl.getIntPtrType(ctx)
        }, "lucis_vec_header");
    }
    }

    return llvm::Type::getInt32Ty(ctx); // unreachable fallback
}
