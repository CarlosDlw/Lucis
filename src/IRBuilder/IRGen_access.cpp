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



std::any IRGen::visitIndexExpr(LucisParser::IndexExprContext* ctx) {
        SET_DBG_LOC(ctx);
    // Collect all chained indices: arr[i][j][k] → varName + [i, j, k]
    std::vector<LucisParser::ExpressionContext*> indexExprs;
    std::string varName;

    auto* current = static_cast<LucisParser::ExpressionContext*>(ctx);
    while (auto* idxCtx = dynamic_cast<LucisParser::IndexExprContext*>(current)) {
        indexExprs.push_back(idxCtx->expression(1)); // the index
        current = idxCtx->expression(0);              // the base
    }

    auto* identBase = dynamic_cast<LucisParser::IdentExprContext*>(current);

    // Check for user-defined opIndex on struct
    if (!indexExprs.empty()) {
        auto* baseTI = resolveExprTypeInfo(current);
        if (baseTI && baseTI->kind == TypeKind::Struct) {
            if (auto* opEntry = findOperatorEntryInChain(baseTI, "opIndex")) {
                auto* idxVal = castValue(visit(indexExprs[0]));
                if (identBase) {
                    auto it = locals_.find(identBase->IDENTIFIER()->getText());
                    if (it != locals_.end()) {
                        return static_cast<llvm::Value*>(
                            builder_->CreateCall(opEntry->fn, {it->second.alloca, idxVal}));
                    }
                }
                auto* baseVal = castValue(visit(current));
                auto* temp = builder_->CreateAlloca(baseVal->getType(), nullptr, "opidx_base");
                builder_->CreateStore(baseVal, temp);
                return static_cast<llvm::Value*>(
                    builder_->CreateCall(opEntry->fn, {temp, idxVal}));
            }
        }
    }


    if (!identBase) {
        std::reverse(indexExprs.begin(), indexExprs.end());

        // Generic base expression indexing, e.g. self.input[idx].
        if (!indexExprs.empty()) {
            auto* idxVal = castValue(visit(indexExprs[0]));
            auto* i64Ty = llvm::Type::getInt64Ty(*context_);

            auto* baseTI = resolveExprTypeInfo(current);
            auto* baseVal = castValue(visit(current));

            // Map<K,V> index: baseExpr[key] → lucis_map_get_<suffix> or set
            if (baseTI && baseTI->kind == TypeKind::Extended && baseTI->extendedKind == "Map") {
                auto* ptrTy = llvm::PointerType::getUnqual(*context_);
                auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
                auto* keyLLTy = baseTI->keyType->toLLVMType(*context_, module_->getDataLayout());
                auto* valLLTy = baseTI->valueType->toLLVMType(*context_, module_->getDataLayout());
                auto suffix = baseTI->builtinSuffix;

                // If baseVal is not already a pointer (e.g. loaded map value),
                // allocate it to get an address for the C function call
                llvm::Value* mapPtr = baseVal;
                if (!mapPtr->getType()->isPointerTy()) {
                    auto* alloca = builder_->CreateAlloca(mapPtr->getType());
                    builder_->CreateStore(mapPtr, alloca);
                    mapPtr = alloca;
                }

                // Cast key if needed
                if (idxVal->getType() != keyLLTy) {
                    if (idxVal->getType()->isIntegerTy() && keyLLTy->isIntegerTy())
                        idxVal = builder_->CreateIntCast(idxVal, keyLLTy, baseTI->keyType->isSigned);
                }

                bool isRawValue = suffix.size() > 4 && suffix.substr(suffix.size() - 3) == "raw";
                if (isRawValue) {
                    auto* voidTy = llvm::Type::getVoidTy(*context_);
                    auto* valAlloca = builder_->CreateAlloca(valLLTy, nullptr, "map_val_out");
                    auto callee = declareBuiltin(
                        "lucis_map_get_" + suffix, voidTy, {ptrTy, keyLLTy, ptrTy});
                    builder_->CreateCall(callee, {mapPtr, idxVal, valAlloca});
                    return static_cast<llvm::Value*>(builder_->CreateLoad(valLLTy, valAlloca, "map_get_val"));
                } else {
                    auto callee = declareBuiltin(
                        "lucis_map_get_" + suffix, valLLTy, {ptrTy, keyLLTy});
                    auto* result = builder_->CreateCall(callee, {mapPtr, idxVal}, "map_get");
                    return static_cast<llvm::Value*>(result);
                }
            }
            // Vec<T> index: baseExpr[idx] -> lucis_vec_at_<suffix>
            if (baseTI && baseTI->kind == TypeKind::Extended && baseTI->extendedKind == "Vec") {
                auto* ptrTy = llvm::PointerType::getUnqual(*context_);
                auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
                llvm::Value* vecPtr = baseVal;
                if (!vecPtr->getType()->isPointerTy()) {
                    auto* alloca = builder_->CreateAlloca(vecPtr->getType());
                    builder_->CreateStore(vecPtr, alloca);
                    vecPtr = alloca;
                }
                auto* elemLLTy = baseTI->elementType->toLLVMType(*context_, module_->getDataLayout());
                auto suffix = getVecSuffix(baseTI->elementType);
                auto* idx = idxVal;
                if (idx->getType() != usizeTy)
                    idx = builder_->CreateIntCast(idx, usizeTy, false);
                if (suffix == "raw") {
                    auto& dl = module_->getDataLayout();
                    auto elemSz = dl.getTypeAllocSize(elemLLTy);
                    auto* elemSzVal = llvm::ConstantInt::get(usizeTy, elemSz);
                    auto ptrFn = declareBuiltin("lucis_vec_ptr_raw", ptrTy, {ptrTy, usizeTy, usizeTy});
                    auto* elemPtr = builder_->CreateCall(ptrFn, {vecPtr, idx, elemSzVal}, "vec_elem_ptr");
                    auto* load = builder_->CreateLoad(elemLLTy, elemPtr, "vec_at");
                    if (baseTI->elementType && baseTI->elementType->isVolatile)
                        load->setVolatile(true);
                    return static_cast<llvm::Value*>(load);
                }
                auto callee = declareBuiltin("lucis_vec_at_" + suffix, elemLLTy, {ptrTy, usizeTy});
                auto* result = builder_->CreateCall(callee, {vecPtr, idx}, "vec_at");
                return static_cast<llvm::Value*>(result);
            }

            // string[index] -> char
            if (baseTI && baseTI->kind == TypeKind::String && baseVal->getType()->isStructTy()) {
                auto* ptrTy = llvm::PointerType::getUnqual(*context_);
                auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
                auto* i8Ty = llvm::Type::getInt8Ty(*context_);
                auto* strPtr = builder_->CreateExtractValue(baseVal, 0, "str_ptr");
                auto* idxU = (idxVal->getType() == usizeTy)
                                 ? idxVal
                                 : builder_->CreateIntCast(idxVal, usizeTy, false);
                auto* elemPtr = builder_->CreateGEP(i8Ty, strPtr, idxU, "str_elem_ptr");
                return static_cast<llvm::Value*>(builder_->CreateLoad(i8Ty, elemPtr, "str_elem"));
            }

            // pointer[index] -> *load of pointee
            if (baseTI && baseTI->kind == TypeKind::Pointer && baseTI->pointeeType) {
                bool isArrayVar = (resolveExprArrayDims(current) > 0);
                auto* elemTy = isArrayVar
                    ? baseTI->toLLVMType(*context_, module_->getDataLayout())
                    : baseTI->pointeeType->toLLVMType(*context_, module_->getDataLayout());
                auto* ptrBase = baseVal;
                if (!ptrBase->getType()->isPointerTy()) {
                    std::cerr << "lucis: invalid pointer index base expression\n";
                    return static_cast<llvm::Value*>(
                        llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
                }
                auto* elemPtr = builder_->CreateGEP(elemTy, ptrBase, idxVal, "idx_elem_ptr");
                auto* load = builder_->CreateLoad(elemTy, elemPtr, "idx_elem");
                if (baseTI->pointeeType && baseTI->pointeeType->isVolatile)
                    load->setVolatile(true);
                return static_cast<llvm::Value*>(load);
            }

            // Slice {ptr, len} indexing: expr returning []T, e.g., dots[i].arr[j]
            if (baseVal && baseVal->getType()->isStructTy()) {
                auto* st = llvm::dyn_cast<llvm::StructType>(baseVal->getType());
                if (st && st->getNumElements() == 2 &&
                    st->getElementType(0)->isPointerTy() &&
                    st->getElementType(1)->isIntegerTy()) {
                    auto* dataPtr = builder_->CreateExtractValue(baseVal, 0, "slice_ptr");
                    auto* lenVal  = builder_->CreateExtractValue(baseVal, 1, "slice_len");
                    auto* i64Ty = llvm::Type::getInt64Ty(*context_);
                    auto* idx = castValue(visit(indexExprs[0]));
                    if (idx->getType() != i64Ty)
                        idx = builder_->CreateIntCast(idx, i64Ty, true);
                    // Bounds check
                    auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
                    auto* idxU = builder_->CreateIntCast(idx, usizeTy, false, "bnd.idx");
                    auto* lenU = builder_->CreateIntCast(lenVal, usizeTy, false, "bnd.len");
                    emitBoundsCheck(idxU, lenU, "slice_idx");
                    // Get element type from the expression's type info
                    auto* elemTI = resolveExprTypeInfo(current);
                    if (!elemTI) {
                        std::cerr << "lucis: cannot resolve element type for slice index\n";
                        return static_cast<llvm::Value*>(
                            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
                    }
                    auto* elemTy = elemTI->toLLVMType(*context_, module_->getDataLayout());
                    auto* elemPtr = builder_->CreateGEP(
                        elemTy, dataPtr, idx, "slice_elem_ptr");
                    auto* load = builder_->CreateLoad(elemTy, elemPtr, "slice_val");
                    if (elemTI && elemTI->isVolatile) load->setVolatile(true);
                    return static_cast<llvm::Value*>(load);
                }
            }

            // Array-value indexing: chained field/expr returning array, e.g., m.hidden.bias[1]
            if (baseVal && baseVal->getType()->isArrayTy()) {
                auto* zero = llvm::ConstantInt::get(i64Ty, 0);
                auto* tmp = builder_->CreateAlloca(baseVal->getType(), nullptr, "arr_val_tmp");
                builder_->CreateStore(baseVal, tmp);
                auto* arrTyCheck = llvm::cast<llvm::ArrayType>(baseVal->getType());
                auto* arrLenVal = llvm::ConstantInt::get(i64Ty, arrTyCheck->getNumElements());
                emitBoundsCheck(idxVal, arrLenVal, "arr_val_idx");
                auto* elemPtr = builder_->CreateInBoundsGEP(
                    baseVal->getType(), tmp, {zero, idxVal}, "arr_val_elem");
                auto* elemTy = llvm::cast<llvm::ArrayType>(baseVal->getType())->getElementType();
                for (size_t ai = 1; ai < indexExprs.size(); ai++) {
                    auto* nextIdx = castValue(visit(indexExprs[ai]));
                    if (nextIdx->getType() != i64Ty)
                        nextIdx = builder_->CreateIntCast(nextIdx, i64Ty, true);
                    elemPtr = builder_->CreateInBoundsGEP(
                        elemTy, elemPtr, {zero, nextIdx}, "arr_val_elem");
                    elemTy = llvm::cast<llvm::ArrayType>(elemTy)->getElementType();
                }
                return static_cast<llvm::Value*>(
                    builder_->CreateLoad(elemTy, elemPtr, "arr_val"));
            }
        }

        // ── Field-access base: e.g. cell.chars[0]  or  ts.buf[i] ─────
        auto* fieldBase = dynamic_cast<LucisParser::FieldAccessExprContext*>(current);
        if (fieldBase) {
            auto fieldName = fieldBase->IDENTIFIER()->getText();
            auto* innerIdent = dynamic_cast<LucisParser::IdentExprContext*>(
                fieldBase->expression());
            if (innerIdent) {
                auto structVar = innerIdent->IDENTIFIER()->getText();
                auto lit = locals_.find(structVar);
                if (lit != locals_.end() && lit->second.typeInfo) {
                    auto* baseTI = lit->second.typeInfo;
                    // Auto-dereference pointer: *Buffer → Buffer
                    if (baseTI->kind == TypeKind::Pointer && baseTI->pointeeType)
                        baseTI = baseTI->pointeeType;
                    auto* structTI = baseTI;
                    int fieldIdx = -1;
                    const TypeInfo* fieldTI = nullptr;
                    for (size_t f = 0; f < structTI->fields.size(); f++) {
                        if (structTI->fields[f].name == fieldName) {
                            fieldIdx = static_cast<int>(f);
                            fieldTI = structTI->fields[f].typeInfo;
                            break;
                        }
                    }
                    if (fieldIdx >= 0 && fieldTI) {
                        // Compute base pointer: load if variable is a pointer to struct
                        auto* ptrTy = llvm::PointerType::getUnqual(*context_);
                        auto* baseAlloca = lit->second.alloca;
                        auto* baseAllocatedTy = baseAlloca->getAllocatedType();
                        llvm::Value* basePtr = baseAlloca;
                        if (lit->second.typeInfo->kind == TypeKind::Pointer &&
                            lit->second.typeInfo->pointeeType) {
                            auto* loadedPtr = builder_->CreateLoad(ptrTy, baseAlloca, structVar + "_ptr");
                            basePtr = loadedPtr;
                            baseAllocatedTy = structTI->toLLVMType(*context_, module_->getDataLayout());
                        }

                        auto* fieldGep = builder_->CreateStructGEP(
                            baseAllocatedTy, basePtr, fieldIdx,
                            structVar + "_" + fieldName + "_ptr");
                        const auto& fi = structTI->fields[fieldIdx];

                        // Build the LLVM array type from FieldInfo (handles [N]T fields)
                        llvm::Type* arrTy = nullptr;
                        if (fi.arrayDims > 0 && !fi.arraySizes.empty()) {
                            arrTy = fieldTI->toLLVMType(*context_, module_->getDataLayout());
                            for (auto it = fi.arraySizes.rbegin(); it != fi.arraySizes.rend(); ++it)
                                arrTy = llvm::ArrayType::get(arrTy, *it);
                        } else if (fieldTI->arraySize > 0 && fieldTI->elementType) {
                            arrTy = fieldTI->toLLVMType(*context_, module_->getDataLayout());
                        }
                        if (arrTy && llvm::isa<llvm::ArrayType>(arrTy)) {
                            auto* i64Ty = llvm::Type::getInt64Ty(*context_);
                            auto* idx = castValue(visit(indexExprs[0]));
                            if (idx->getType() != i64Ty)
                                idx = builder_->CreateIntCast(idx, i64Ty, true);
                            auto* arrTyCheck = llvm::cast<llvm::ArrayType>(arrTy);
                            auto* arrLenVal = llvm::ConstantInt::get(i64Ty, arrTyCheck->getNumElements());
                            emitBoundsCheck(idx, arrLenVal, "field_arr_idx");
                            auto* elemGep = builder_->CreateInBoundsGEP(
                                arrTy, fieldGep,
                                { llvm::ConstantInt::get(i64Ty, 0), idx },
                                fieldName + "_elem");
                            auto* elemTy = llvm::cast<llvm::ArrayType>(arrTy)->getElementType();
                            // Handle chained indices for multi-dimensional arrays
                            for (size_t ai = 1; ai < indexExprs.size(); ai++) {
                                auto* nextIdx = castValue(visit(indexExprs[ai]));
                                if (nextIdx->getType() != i64Ty)
                                    nextIdx = builder_->CreateIntCast(nextIdx, i64Ty, true);
                                elemGep = builder_->CreateInBoundsGEP(
                                    elemTy, elemGep,
                                    { llvm::ConstantInt::get(i64Ty, 0), nextIdx },
                                    fieldName + "_elem");
                                elemTy = llvm::cast<llvm::ArrayType>(elemTy)->getElementType();
                            }
                            return static_cast<llvm::Value*>(
                                builder_->CreateLoad(elemTy, elemGep, fieldName + "_val"));
                        }

                        // Vec<T>/Set<T>/Map<K,V> field: buf.data[i]
                        if (fieldTI && fieldTI->kind == TypeKind::Extended) {
                            auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);

                            if (fieldTI->extendedKind == "Map") {
                                auto* keyLLTy = fieldTI->keyType->toLLVMType(*context_, module_->getDataLayout());
                                auto* valLLTy = fieldTI->valueType->toLLVMType(*context_, module_->getDataLayout());
                                auto suffix = fieldTI->builtinSuffix;

                                auto* keyVal = castValue(visit(indexExprs[0]));
                                if (keyVal->getType() != keyLLTy) {
                                    if (keyVal->getType()->isIntegerTy() && keyLLTy->isIntegerTy())
                                        keyVal = builder_->CreateIntCast(keyVal, keyLLTy, fieldTI->keyType->isSigned);
                                }

                                // Check if value type is raw (struct) - suffix ends with "_raw"
                                bool isRawValue = suffix.size() > 4 && suffix.substr(suffix.size() - 3) == "raw";

                                if (isRawValue) {
                                    // For raw struct values, lucis_map_get_*_raw returns void and uses out-param
                                    auto* voidTy = llvm::Type::getVoidTy(*context_);
                                    auto* valAlloca = builder_->CreateAlloca(valLLTy, nullptr, "map_val_out");
                                    auto callee = declareBuiltin("lucis_map_get_" + suffix, voidTy, {ptrTy, keyLLTy, ptrTy});
                                    builder_->CreateCall(callee, {fieldGep, keyVal, valAlloca});
                                    return static_cast<llvm::Value*>(builder_->CreateLoad(valLLTy, valAlloca, "map_get_val"));
                                } else {
                                    auto callee = declareBuiltin("lucis_map_get_" + suffix, valLLTy, {ptrTy, keyLLTy});
                                    auto* result = builder_->CreateCall(callee, {fieldGep, keyVal}, "map_get");
                                    return static_cast<llvm::Value*>(result);
                                }
                            }

                            auto* idx = castValue(visit(indexExprs[0]));
                            if (idx->getType() != usizeTy)
                                idx = builder_->CreateIntCast(idx, usizeTy, false);

                            auto* elemLLTy = fieldTI->elementType->toLLVMType(*context_, module_->getDataLayout());
                            auto suffix = getVecSuffix(fieldTI->elementType);
                            if (suffix == "raw") {
                                auto& dl = module_->getDataLayout();
                                auto elemSz = dl.getTypeAllocSize(elemLLTy);
                                auto* elemSzVal = llvm::ConstantInt::get(usizeTy, elemSz);
                                auto ptrFn = declareBuiltin("lucis_vec_ptr_raw", ptrTy, {ptrTy, usizeTy, usizeTy});
                                auto* elemPtr = builder_->CreateCall(ptrFn, {fieldGep, idx, elemSzVal}, "vec_elem_ptr");
                                return static_cast<llvm::Value*>(builder_->CreateLoad(elemLLTy, elemPtr, "vec_at"));
                            }
                            auto callee = declareBuiltin("lucis_vec_at_" + suffix, elemLLTy, {ptrTy, usizeTy});
                            auto* result = builder_->CreateCall(callee, {fieldGep, idx}, "vec_at");
                            return static_cast<llvm::Value*>(result);
                        }
                    }
                }
            }
        }

        // ── Arrow-access base: e.g. ts->buf[i] ──────────────────────
        auto* arrowBase = dynamic_cast<LucisParser::ArrowAccessExprContext*>(current);
        if (arrowBase) {
            auto fieldName = arrowBase->IDENTIFIER()->getText();
            auto* innerIdent = dynamic_cast<LucisParser::IdentExprContext*>(
                arrowBase->expression());
            if (innerIdent) {
                auto ptrVar = innerIdent->IDENTIFIER()->getText();
                auto pit = locals_.find(ptrVar);
                if (pit != locals_.end() && pit->second.typeInfo &&
                    pit->second.typeInfo->kind == TypeKind::Pointer &&
                    pit->second.typeInfo->pointeeType) {
                    auto* structTI = pit->second.typeInfo->pointeeType;
                    int fieldIdx = -1;
                    const TypeInfo* fieldTI = nullptr;
                    for (size_t f = 0; f < structTI->fields.size(); f++) {
                        if (structTI->fields[f].name == fieldName) {
                            fieldIdx = static_cast<int>(f);
                            fieldTI = structTI->fields[f].typeInfo;
                            break;
                        }
                    }
                    if (fieldIdx >= 0 && fieldTI) {
                        const auto& fi = structTI->fields[fieldIdx];
                        llvm::Type* arrTy = nullptr;
                        if (fi.arrayDims > 0 && !fi.arraySizes.empty()) {
                            arrTy = fieldTI->toLLVMType(*context_, module_->getDataLayout());
                            for (auto it = fi.arraySizes.rbegin(); it != fi.arraySizes.rend(); ++it)
                                arrTy = llvm::ArrayType::get(arrTy, *it);
                        } else if (fieldTI->arraySize > 0 && fieldTI->elementType) {
                            arrTy = fieldTI->toLLVMType(*context_, module_->getDataLayout());
                        }
                        if (arrTy && llvm::isa<llvm::ArrayType>(arrTy)) {
                            auto* ptrTy = llvm::PointerType::getUnqual(*context_);
                            auto* structTy = structTI->toLLVMType(*context_, module_->getDataLayout());
                            auto* ptrVal = builder_->CreateLoad(ptrTy, pit->second.alloca, ptrVar + "_ptr");
                            auto* fieldGep = builder_->CreateStructGEP(
                                structTy, ptrVal, fieldIdx,
                                ptrVar + "_" + fieldName + "_ptr");
                            auto* i64Ty = llvm::Type::getInt64Ty(*context_);
                            auto* idx = castValue(visit(indexExprs[0]));
                            if (idx->getType() != i64Ty)
                                idx = builder_->CreateIntCast(idx, i64Ty, true);
                            auto* elemGep = builder_->CreateInBoundsGEP(
                                arrTy, fieldGep,
                                { llvm::ConstantInt::get(i64Ty, 0), idx },
                                fieldName + "_elem");
                            auto* elemTy = llvm::cast<llvm::ArrayType>(arrTy)->getElementType();
                            for (size_t ai = 1; ai < indexExprs.size(); ai++) {
                                auto* nextIdx = castValue(visit(indexExprs[ai]));
                                if (nextIdx->getType() != i64Ty)
                                    nextIdx = builder_->CreateIntCast(nextIdx, i64Ty, true);
                                elemGep = builder_->CreateInBoundsGEP(
                                    elemTy, elemGep,
                                    { llvm::ConstantInt::get(i64Ty, 0), nextIdx },
                                    fieldName + "_elem");
                                elemTy = llvm::cast<llvm::ArrayType>(elemTy)->getElementType();
                            }
                            return static_cast<llvm::Value*>(
                                builder_->CreateLoad(elemTy, elemGep, fieldName + "_val"));
                        }

                        // Vec<T>/Set<T>/Map<K,V> field: ts->buf[i]
                        if (fieldTI && fieldTI->kind == TypeKind::Extended) {
                            auto* ptrTy = llvm::PointerType::getUnqual(*context_);
                            auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
                            auto* structTy = structTI->toLLVMType(*context_, module_->getDataLayout());
                            auto* ptrVal = builder_->CreateLoad(ptrTy, pit->second.alloca, ptrVar + "_ptr");
                            auto* fieldGep = builder_->CreateStructGEP(
                                structTy, ptrVal, fieldIdx,
                                ptrVar + "_" + fieldName + "_ptr");

                            if (fieldTI->extendedKind == "Map") {
                                auto* keyLLTy = fieldTI->keyType->toLLVMType(*context_, module_->getDataLayout());
                                auto* valLLTy = fieldTI->valueType->toLLVMType(*context_, module_->getDataLayout());
                                auto suffix = fieldTI->builtinSuffix;

                                auto* keyVal = castValue(visit(indexExprs[0]));
                                if (keyVal->getType() != keyLLTy) {
                                    if (keyVal->getType()->isIntegerTy() && keyLLTy->isIntegerTy())
                                        keyVal = builder_->CreateIntCast(keyVal, keyLLTy, fieldTI->keyType->isSigned);
                                }

                                // Check if value type is raw (struct) - suffix ends with "_raw"
                                bool isRawValue = suffix.size() > 4 && suffix.substr(suffix.size() - 3) == "raw";

                                if (isRawValue) {
                                    // For raw struct values, lucis_map_get_*_raw returns void and uses out-param
                                    auto* voidTy = llvm::Type::getVoidTy(*context_);
                                    auto* valAlloca = builder_->CreateAlloca(valLLTy, nullptr, "map_val_out");
                                    auto callee = declareBuiltin("lucis_map_get_" + suffix, voidTy, {ptrTy, keyLLTy, ptrTy});
                                    builder_->CreateCall(callee, {fieldGep, keyVal, valAlloca});
                                    return static_cast<llvm::Value*>(builder_->CreateLoad(valLLTy, valAlloca, "map_get_val"));
                                } else {
                                    auto callee = declareBuiltin("lucis_map_get_" + suffix, valLLTy, {ptrTy, keyLLTy});
                                    auto* result = builder_->CreateCall(callee, {fieldGep, keyVal}, "map_get");
                                    return static_cast<llvm::Value*>(result);
                                }
                            }

                            auto* idx = castValue(visit(indexExprs[0]));
                            if (idx->getType() != usizeTy)
                                idx = builder_->CreateIntCast(idx, usizeTy, false);

                            auto* elemLLTy = fieldTI->elementType->toLLVMType(*context_, module_->getDataLayout());
                            auto suffix = getVecSuffix(fieldTI->elementType);
                            if (suffix == "raw") {
                                auto& dl = module_->getDataLayout();
                                auto elemSz = dl.getTypeAllocSize(elemLLTy);
                                auto* elemSzVal = llvm::ConstantInt::get(usizeTy, elemSz);
                                auto ptrFn = declareBuiltin("lucis_vec_ptr_raw", ptrTy, {ptrTy, usizeTy, usizeTy});
                                auto* elemPtr = builder_->CreateCall(ptrFn, {fieldGep, idx, elemSzVal}, "vec_elem_ptr");
                                return static_cast<llvm::Value*>(builder_->CreateLoad(elemLLTy, elemPtr, "vec_at"));
                            }
                            auto callee = declareBuiltin("lucis_vec_at_" + suffix, elemLLTy, {ptrTy, usizeTy});
                            auto* result = builder_->CreateCall(callee, {fieldGep, idx}, "vec_at");
                            return static_cast<llvm::Value*>(result);
                        }
                    }
                }
            }
        }

        std::cerr << "lucis: invalid index base expression\n";
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }
    varName = identBase->IDENTIFIER()->getText();

    // Reverse since we collected from outermost to innermost
    std::reverse(indexExprs.begin(), indexExprs.end());

    // ── Variadic parameter index access ─────────────────────────────
    auto vit = variadicParams_.find(varName);
    if (vit != variadicParams_.end()) {
        auto* ptrTy = llvm::PointerType::getUnqual(*context_);
        auto* i64Ty = llvm::Type::getInt64Ty(*context_);
        auto* dataPtr = builder_->CreateLoad(ptrTy, vit->second.dataPtr, "var_data");
        if (!vit->second.elementType) {
            std::cerr << "lucis: internal error: variadic param missing elementType\n";
            return static_cast<llvm::Value*>(llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
        }
        auto* elemTy = vit->second.elementType->toLLVMType(*context_, module_->getDataLayout());
        auto* idx = castValue(visit(indexExprs[0]));
        if (idx->getType() != i64Ty)
            idx = builder_->CreateIntCast(idx, i64Ty, true);
        auto* gep = builder_->CreateGEP(elemTy, dataPtr, idx, varName + "_elem");
        return static_cast<llvm::Value*>(builder_->CreateLoad(elemTy, gep));
    }

    auto it = locals_.find(varName);
    if (it == locals_.end()) {
        std::cerr << "lucis: undefined variable '" << varName << "'\n";
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }

    // ── Extended type (Vec<T>, Map<K,V>) index access ──────────────────
    if (it->second.typeInfo && it->second.typeInfo->kind == TypeKind::Extended) {
        auto* ti = it->second.typeInfo;
        auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
        auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);

        if (ti->extendedKind == "Map") {
            // Map<K,V> subscript: m[key] → lucis_map_get_<suffix>
            auto* keyLLTy = ti->keyType->toLLVMType(*context_, module_->getDataLayout());
            auto* valLLTy = ti->valueType->toLLVMType(*context_, module_->getDataLayout());
            auto suffix = ti->builtinSuffix;

            auto* keyVal = castValue(visit(indexExprs[0]));
            if (keyVal->getType() != keyLLTy) {
                if (keyVal->getType()->isIntegerTy() && keyLLTy->isIntegerTy())
                    keyVal = builder_->CreateIntCast(keyVal, keyLLTy, ti->keyType->isSigned);
            }

            // Check if value type is raw (struct) - suffix ends with "_raw"
            bool isRawValue = suffix.size() > 4 && suffix.substr(suffix.size() - 3) == "raw";

            if (isRawValue) {
                // For raw struct values, lucis_map_get_*_raw returns void and uses out-param
                auto* voidTy = llvm::Type::getVoidTy(*context_);
                auto* valAlloca = builder_->CreateAlloca(valLLTy, nullptr, "map_val_out");
                auto callee = declareBuiltin(
                    "lucis_map_get_" + suffix, voidTy, { ptrTy, keyLLTy, ptrTy });
                builder_->CreateCall(callee,
                    { it->second.alloca, keyVal, valAlloca });
                return static_cast<llvm::Value*>(builder_->CreateLoad(valLLTy, valAlloca, "map_get_val"));
            } else {
                auto callee = declareBuiltin(
                    "lucis_map_get_" + suffix, valLLTy, { ptrTy, keyLLTy });
                auto* result = builder_->CreateCall(callee,
                    { it->second.alloca, keyVal }, "map_get");
                return static_cast<llvm::Value*>(result);
            }
        }

        // Vec<T> subscript: v[i] → lucis_vec_at_<suffix> (or ptr_raw for struct)
        auto* elemLLTy = ti->elementType->toLLVMType(
            *context_, module_->getDataLayout());
        auto suffix = getVecSuffix(ti->elementType);

        auto* idx = castValue(visit(indexExprs[0]));
        if (idx->getType() != usizeTy)
            idx = builder_->CreateIntCast(idx, usizeTy, false);

        if (suffix == "raw") {
            auto& dl     = module_->getDataLayout();
            auto  elemSz = dl.getTypeAllocSize(elemLLTy);
            auto* elemSzVal = llvm::ConstantInt::get(usizeTy, elemSz);
            auto ptrFn = declareBuiltin("lucis_vec_ptr_raw", ptrTy,
                                        {ptrTy, usizeTy, usizeTy});
            auto* elemPtr = builder_->CreateCall(ptrFn,
                { it->second.alloca, idx, elemSzVal }, "vec_elem_ptr");
            auto* val = builder_->CreateLoad(elemLLTy, elemPtr, "vec_at");
            return static_cast<llvm::Value*>(val);
        }

        auto callee = declareBuiltin(
            "lucis_vec_at_" + suffix, elemLLTy, { ptrTy, usizeTy });
        auto* result = builder_->CreateCall(callee,
            { it->second.alloca, idx }, "vec_at");
        return static_cast<llvm::Value*>(result);
    }

    auto* alloca    = it->second.alloca;
    auto* allocType = alloca->getAllocatedType();
    auto* ti = it->second.typeInfo;

    // ── Slice-like array index access ([]T lowered as {ptr,len}) ────
    if (it->second.arrayDims > 0 && allocType->isStructTy()) {
        auto* sliceTy = llvm::dyn_cast<llvm::StructType>(allocType);
        if (sliceTy && sliceTy->getNumElements() == 2 &&
            sliceTy->getElementType(0)->isPointerTy() &&
            sliceTy->getElementType(1)->isIntegerTy()) {
            auto* i64Ty = llvm::Type::getInt64Ty(*context_);
            auto* elemTI = it->second.typeInfo;
            if (!elemTI) {
                std::cerr << "lucis: missing element type for slice index '" << varName << "'\n";
                return static_cast<llvm::Value*>(
                    llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
            }
            auto* elemTy = elemTI->toLLVMType(*context_, module_->getDataLayout());

            auto* sliceVal = builder_->CreateLoad(allocType, alloca, varName + "_slice");
            auto* dataPtr = builder_->CreateExtractValue(sliceVal, 0, varName + "_slice_ptr");
            auto* idx = castValue(visit(indexExprs[0]));
            if (idx->getType() != i64Ty)
                idx = builder_->CreateIntCast(idx, i64Ty, true);

            auto* elemPtr = builder_->CreateGEP(elemTy, dataPtr, idx, varName + "_slice_elem_ptr");
            return static_cast<llvm::Value*>(
                builder_->CreateLoad(elemTy, elemPtr, varName + "_slice_elem"));
        }
    }

    // ── Pointer index access (ptr[i]) ─────────────────────────────
    if (ti && ti->kind == TypeKind::Pointer && ti->pointeeType) {
        auto* ptrTy  = llvm::PointerType::getUnqual(*context_);
        auto* elemTy = ti->pointeeType->toLLVMType(*context_, module_->getDataLayout());
        auto* i64Ty  = llvm::Type::getInt64Ty(*context_);

        // Load the pointer value from the alloca
        auto* ptrVal = builder_->CreateLoad(ptrTy, alloca, varName + "_ptr");

        // GEP with the first index
        auto* idx = castValue(visit(indexExprs[0]));
        if (idx->getType() != i64Ty)
            idx = builder_->CreateIntCast(idx, i64Ty, true);

        auto* gep = builder_->CreateGEP(elemTy, ptrVal, idx, varName + "_elem");
        return static_cast<llvm::Value*>(builder_->CreateLoad(elemTy, gep));
    }

    // ── String index access (str[i] → char) ─────────────────────────
    if (ti && ti->kind == TypeKind::String && it->second.arrayDims == 0) {
        auto* i8Ty    = llvm::Type::getInt8Ty(*context_);
        auto* i64Ty   = llvm::Type::getInt64Ty(*context_);
        auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
        // Load the string struct { ptr, len }
        auto* strVal = builder_->CreateLoad(allocType, alloca, varName + "_str");
        // Extract the data pointer (field 0)
        auto* strPtr  = builder_->CreateExtractValue(strVal, 0, varName + "_ptr");
        auto* idx = castValue(visit(indexExprs[0]));
        if (idx->getType() != usizeTy)
            idx = builder_->CreateIntCast(idx, usizeTy, false);
        auto* elemPtr = builder_->CreateGEP(i8Ty, strPtr, idx, varName + "_char_ptr");
        return static_cast<llvm::Value*>(builder_->CreateLoad(i8Ty, elemPtr, varName + "_char"));
    }

    // ── Array index access (original path) ──────────────────────────
    if (!allocType->isArrayTy()) {
        std::cerr << "lucis: cannot index non-array variable '" << varName << "'\n";
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }

    auto* i64Ty     = llvm::Type::getInt64Ty(*context_);

    std::vector<llvm::Value*> gepIndices;
    gepIndices.push_back(llvm::ConstantInt::get(i64Ty, 0));
    for (auto* idxExpr : indexExprs) {
        auto* idx = castValue(visit(idxExpr));
        if (idx->getType() != i64Ty)
            idx = builder_->CreateIntCast(idx, i64Ty, true);
        gepIndices.push_back(idx);
    }

    auto* gep = builder_->CreateGEP(allocType, alloca, gepIndices, varName + "_elem");

    // Determine element type by unwrapping array types
    llvm::Type* elemTy = allocType;
    for (size_t i = 0; i < indexExprs.size(); i++) {
        auto* arrTy = llvm::dyn_cast<llvm::ArrayType>(elemTy);
        if (!arrTy) {
            std::cerr << "lucis: too many indices for variable '" << varName << "'\n";
            return static_cast<llvm::Value*>(
                llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
        }
        elemTy = arrTy->getElementType();
    }

    return static_cast<llvm::Value*>(builder_->CreateLoad(elemTy, gep));
}



std::any IRGen::visitStructLitExpr(LucisParser::StructLitExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto identifiers = ctx->IDENTIFIER();
    auto typeName = identifiers.size() > 0 ? identifiers[0]->getText() : "";
    auto* ti = typeRegistry_.lookup(typeName);
    if (!ti || (ti->kind != TypeKind::Struct && ti->kind != TypeKind::Union)) {
        std::cerr << "lucis: unknown struct/union type '" << typeName << "'\n";
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }

    // ── Union literal: Name { field: value } (exactly one field) ────
    if (ti->kind == TypeKind::Union) {
        auto* unionTy = ti->toLLVMType(*context_, module_->getDataLayout());
        auto* alloca = builder_->CreateAlloca(unionTy, nullptr, typeName + "_tmp");

        auto exprs = ctx->expression();
        if (!exprs.empty()) {
            auto fieldName = identifiers.size() > 1 ? identifiers[1]->getText() : "";
            const TypeInfo* fieldTI = nullptr;
            for (auto& f : ti->fields) {
                if (f.name == fieldName) { fieldTI = f.typeInfo; break; }
            }
            if (fieldTI) {
                auto* val = castValue(visit(exprs[0]));
                auto* fieldLLTy = fieldTI->toLLVMType(*context_, module_->getDataLayout());
                if (val->getType() != fieldLLTy) {
                    if (val->getType()->isIntegerTy() && fieldLLTy->isIntegerTy())
                        val = builder_->CreateIntCast(val, fieldLLTy, fieldTI->isSigned);
                    else if (val->getType()->isFloatingPointTy() && fieldLLTy->isFloatingPointTy()) {
                        if (val->getType()->getPrimitiveSizeInBits() > fieldLLTy->getPrimitiveSizeInBits())
                            val = builder_->CreateFPTrunc(val, fieldLLTy);
                        else
                            val = builder_->CreateFPExt(val, fieldLLTy);
                    }
                }
                // Store: cast union pointer to field pointer type, then store
                builder_->CreateStore(val, alloca);
            }
        }

        return static_cast<llvm::Value*>(
            builder_->CreateLoad(unionTy, alloca, typeName + "_val"));
    }

    // ── Struct literal ──────────────────────────────────────────────
    auto* structTy = ti->toLLVMType(*context_, module_->getDataLayout());
    llvm::Value* agg = llvm::ConstantAggregateZero::get(structTy);

    // Parse field initializers: Name { field1: expr1, field2: expr2 }
    auto exprs = ctx->expression();
    for (size_t i = 0; i < exprs.size(); i++) {
        auto fieldName = identifiers[i + 1]->getText();

        // Find field index
        int fieldIdx = -1;
        for (size_t f = 0; f < ti->fields.size(); f++) {
            if (ti->fields[f].name == fieldName) {
                fieldIdx = static_cast<int>(f);
                break;
            }
        }
        if (fieldIdx < 0) {
            std::cerr << "lucis: '" << typeName
                      << "' has no field '" << fieldName << "'\n";
            continue;
        }

        auto* fieldTI = ti->fields[fieldIdx].typeInfo;
        auto* fieldTy = fieldTI->toLLVMType(*context_, module_->getDataLayout());
        llvm::Value* val = nullptr;

        if (fieldTI && fieldTI->kind == TypeKind::Extended &&
            fieldTI->extendedKind == "Vec") {
            if (auto* arrLit = dynamic_cast<LucisParser::ArrayLitExprContext*>(exprs[i])) {
                val = buildVecValueFromArrayLiteral(arrLit, fieldTI,
                                                    typeName + "_" + fieldName + "_vec");
            }
        }

        if (!val && fieldTI && fieldTI->kind == TypeKind::Extended &&
                  fieldTI->extendedKind == "Map") {
            if (auto* arrLit = dynamic_cast<LucisParser::ArrayLitExprContext*>(exprs[i])) {
                if (arrLit->expression().empty()) {
                    // Empty map literal: initialize empty map
                    auto* mapTy = getOrCreateMapStructType();
                    auto* ptrTy = llvm::PointerType::getUnqual(*context_);
                    auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
                    auto suffix = fieldTI->builtinSuffix;
                    auto* alloca = builder_->CreateAlloca(mapTy, nullptr, typeName + "_" + fieldName + "_map");
                    if (suffix.size() > 4 && suffix.substr(suffix.size() - 3) == "raw") {
                        auto* valLLTy = fieldTI->valueType->toLLVMType(*context_, module_->getDataLayout());
                        auto valSz = module_->getDataLayout().getTypeAllocSize(valLLTy);
                        auto initFn = declareBuiltin("lucis_map_init_" + suffix,
                            llvm::Type::getVoidTy(*context_), { ptrTy, usizeTy });
                        builder_->CreateCall(initFn, { alloca, llvm::ConstantInt::get(usizeTy, valSz) });
                    } else {
                        auto initFn = declareBuiltin("lucis_map_init_" + suffix,
                            llvm::Type::getVoidTy(*context_), { ptrTy });
                        builder_->CreateCall(initFn, { alloca });
                    }
                    val = builder_->CreateLoad(mapTy, alloca, typeName + "_" + fieldName + "_map");
                }
            }
        }

        if (!val)
            val = castValue(visit(exprs[i]));

        // Cast if needed
        if (val->getType() != fieldTy) {
            if (val->getType()->isIntegerTy() && fieldTy->isIntegerTy())
                val = builder_->CreateIntCast(val, fieldTy, fieldTI->isSigned);
            else if (val->getType()->isFloatingPointTy() && fieldTy->isFloatingPointTy()) {
                if (val->getType()->getPrimitiveSizeInBits() > fieldTy->getPrimitiveSizeInBits())
                    val = builder_->CreateFPTrunc(val, fieldTy);
                else
                    val = builder_->CreateFPExt(val, fieldTy);
            }
        }

        agg = builder_->CreateInsertValue(agg, val, {static_cast<unsigned>(fieldIdx)});
    }

    // Consume owned string field values (move semantics)
    for (auto* e : exprs) consumeExprIfOwnedLocal(e);

    return static_cast<llvm::Value*>(agg);
}

// ── Struct positional init: Name { expr, expr, ... } ─────────────────



std::any IRGen::visitStructPosInitExpr(LucisParser::StructPosInitExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto typeName = ctx->IDENTIFIER()->getText();
    auto* ti = typeRegistry_.lookup(typeName);
    if (!ti || (ti->kind != TypeKind::Struct && ti->kind != TypeKind::Union)) {
        std::cerr << "lucis: unknown struct/union type '" << typeName << "'\n";
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }

    auto exprs = ctx->expression();

    // ── Union positional init ────────────────────────────────────
    if (ti->kind == TypeKind::Union) {
        auto* unionTy = ti->toLLVMType(*context_, module_->getDataLayout());
        auto* alloca = builder_->CreateAlloca(unionTy, nullptr, typeName + "_tmp");
        if (!exprs.empty()) {
            auto* fieldTI = ti->fields[0].typeInfo;
            if (fieldTI) {
                auto* val = castValue(visit(exprs[0]));
                auto* fieldLLTy = fieldTI->toLLVMType(*context_, module_->getDataLayout());
                if (val->getType() != fieldLLTy) {
                    if (val->getType()->isIntegerTy() && fieldLLTy->isIntegerTy())
                        val = builder_->CreateIntCast(val, fieldLLTy, fieldTI->isSigned);
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
            builder_->CreateLoad(unionTy, alloca, typeName + "_val"));
    }

    // ── Struct positional init ─────────────────────────────────────
    auto* structTy = ti->toLLVMType(*context_, module_->getDataLayout());
    llvm::Value* agg = llvm::ConstantAggregateZero::get(structTy);

    for (size_t i = 0; i < exprs.size(); i++) {
        if (i >= ti->fields.size()) break;

        auto& field = ti->fields[i];
        auto* fieldTI = field.typeInfo;
        auto* fieldTy = buildFieldLLVMType(fieldTI, field.arrayDims, field.arraySizes);
        llvm::Value* val = nullptr;

        if (fieldTI && fieldTI->kind == TypeKind::Extended &&
            fieldTI->extendedKind == "Vec") {
            if (auto* arrLit = dynamic_cast<LucisParser::ArrayLitExprContext*>(exprs[i])) {
                val = buildVecValueFromArrayLiteral(
                    arrLit, fieldTI, typeName + "_" + field.name + "_vec");
            }
        }

        if (!val)
            val = castValue(visit(exprs[i]));

        if (val->getType() != fieldTy) {
            if (val->getType()->isIntegerTy() && fieldTy->isIntegerTy())
                val = builder_->CreateIntCast(val, fieldTy, fieldTI->isSigned);
            else if (val->getType()->isFloatingPointTy() && fieldTy->isFloatingPointTy()) {
                if (val->getType()->getPrimitiveSizeInBits() > fieldTy->getPrimitiveSizeInBits())
                    val = builder_->CreateFPTrunc(val, fieldTy);
                else
                    val = builder_->CreateFPExt(val, fieldTy);
            } else if (field.arrayDims > 0 && field.arraySizes.empty() &&
                       val->getType()->isArrayTy() && fieldTy->isStructTy() &&
                       llvm::cast<llvm::StructType>(fieldTy)->getNumElements() == 2) {
                val = lowerArrayToSlice(val, fieldTy);
            }
        }

        agg = builder_->CreateInsertValue(agg, val, {static_cast<unsigned>(i)});
    }

    for (auto* e : ctx->expression()) consumeExprIfOwnedLocal(e);

    return static_cast<llvm::Value*>(agg);
}



std::any IRGen::visitFieldAccessExpr(LucisParser::FieldAccessExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto fieldName = ctx->IDENTIFIER()->getText();

    // The base expression should resolve to a variable with a struct type
    auto* baseExpr = ctx->expression();

    // ── Bitstruct field read: load + shift + mask ─────────────────
    {
        const TypeInfo* bitTI = nullptr;
        llvm::Value* basePtr = nullptr;
        if (auto* id = dynamic_cast<LucisParser::IdentExprContext*>(baseExpr)) {
            auto it = locals_.find(id->IDENTIFIER()->getText());
            if (it != locals_.end()) {
                auto* ti = it->second.typeInfo;
                if (ti->kind == TypeKind::Pointer && ti->pointeeType &&
                    ti->pointeeType->kind == TypeKind::BitStruct) {
                    bitTI = ti->pointeeType;
                    auto* ptrTy = llvm::PointerType::getUnqual(*context_);
                    basePtr = builder_->CreateLoad(ptrTy, it->second.alloca, id->getText() + "_ptr");
                } else if (ti->kind == TypeKind::BitStruct) {
                    bitTI = ti;
                    basePtr = it->second.alloca;
                }
            }
        }
        // Determine if volatile — check the variable's type qualifier
        bool isBitVol = false;
        if (auto* id = dynamic_cast<LucisParser::IdentExprContext*>(baseExpr)) {
            auto it = locals_.find(id->IDENTIFIER()->getText());
            if (it != locals_.end() && it->second.typeInfo)
                isBitVol = it->second.typeInfo->isVolatile;
        }

        if (bitTI && basePtr) {
            for (auto& bf : bitTI->bitFields) {
                if (bf.name == fieldName && !bf.isPadding) {
                    auto* i32Ty = llvm::Type::getInt32Ty(*context_);
                    auto* backTy = bitTI->backingType->toLLVMType(*context_, module_->getDataLayout());
                    auto* rawVal = builder_->CreateLoad(backTy, basePtr, bitTI->name + "_raw");
                    if (isBitVol) llvm::cast<llvm::LoadInst>(rawVal)->setVolatile(true);

                    // Shift right by offset
                    llvm::Value* shifted = rawVal;
                    if (bf.offset > 0) {
                        shifted = builder_->CreateLShr(rawVal,
                            llvm::ConstantInt::get(backTy, bf.offset), "bf_shift");
                    }
                    // Mask
                    auto* masked = builder_->CreateAnd(shifted,
                        llvm::ConstantInt::get(backTy, (1ULL << bf.width) - 1), "bf_mask");
                    // Cast to target width
                    auto* resultTy = bf.width <= 8 ? llvm::Type::getInt8Ty(*context_)
                                  : bf.width <= 16 ? llvm::Type::getInt16Ty(*context_)
                                  : bf.width <= 32 ? llvm::Type::getInt32Ty(*context_)
                                  : llvm::Type::getInt64Ty(*context_);
                    auto* result = builder_->CreateIntCast(masked, resultTy, false, "bf_val");
                    return static_cast<llvm::Value*>(result);
                }
            }
        }
    }

    // ── Case 1: base is a bare variable (e.g. myStruct.field) ──────
    auto* identBase = dynamic_cast<LucisParser::IdentExprContext*>(baseExpr);
    if (identBase) {
        auto varName = identBase->IDENTIFIER()->getText();

        // ── Variadic parameter .len access ──────────────────────────
        auto vit = variadicParams_.find(varName);
        if (vit != variadicParams_.end()) {
            if (fieldName == "len") {
                auto* i64Ty = llvm::Type::getInt64Ty(*context_);
                return static_cast<llvm::Value*>(
                    builder_->CreateLoad(i64Ty, vit->second.lenAlloca, "var_len"));
            }
            std::cerr << "lucis: variadic parameter '" << varName
                      << "' has no field '" << fieldName << "'\n";
            return static_cast<llvm::Value*>(
                llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
        }

        auto it = locals_.find(varName);
        if (it == locals_.end()) {
            // Check if it's a top-level const
            {
                auto cit = topLevelConsts_.find(varName);
                if (cit != topLevelConsts_.end()) {
                    auto* global = cit->second.global;
                    auto* structTI = cit->second.typeInfo;
                    if (structTI && (structTI->kind == TypeKind::Struct ||
                                     structTI->kind == TypeKind::Union)) {
                        int fieldIdx = -1;
                        const TypeInfo* fieldTI = nullptr;
                        for (size_t f = 0; f < structTI->fields.size(); f++) {
                            if (structTI->fields[f].name == fieldName) {
                                fieldIdx = static_cast<int>(f);
                                fieldTI = structTI->fields[f].typeInfo;
                                break;
                            }
                        }
                        if (fieldIdx >= 0) {
                            auto* globalTy = global->getValueType();
                            auto* structVal = builder_->CreateLoad(globalTy, global, varName);
                            auto* fieldLLTy = buildFieldLLVMType(fieldTI,
                                structTI->fields[fieldIdx].arrayDims,
                                structTI->fields[fieldIdx].arraySizes);
                            if (structTI->kind == TypeKind::Union) {
                                auto* ptrTy = llvm::PointerType::getUnqual(*context_);
                                auto* ptr = builder_->CreatePointerCast(global, ptrTy);
                                return static_cast<llvm::Value*>(
                                    builder_->CreateLoad(fieldLLTy, ptr, fieldName));
                            }
                            return static_cast<llvm::Value*>(
                                builder_->CreateExtractValue(structVal,
                                    {static_cast<unsigned>(fieldIdx)}, fieldName));
                        }
                    }
                    std::cerr << "lucis: '" << structTI->name
                              << "' has no field '" << fieldName << "'\n";
                    return static_cast<llvm::Value*>(
                        llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
                }
            }

            // Check if it's a C struct macro (e.g. RED.r where RED is a Color macro)
            auto smIt = cStructMacros_.find(varName);
            if (smIt != cStructMacros_.end()) {
                const auto* sm = smIt->second;
                auto* structTy = llvm::StructType::getTypeByName(*context_, sm->structType);
                if (structTy) {
                    // Build the ConstantStruct
                    std::vector<llvm::Constant*> fields;
                    unsigned numElements = structTy->getNumElements();
                    for (unsigned i = 0; i < numElements && i < sm->fieldValues.size(); ++i) {
                        auto* fTy = structTy->getElementType(i);
                        fields.push_back(llvm::ConstantInt::get(
                            fTy, static_cast<uint64_t>(sm->fieldValues[i]), true));
                    }
                    for (unsigned i = static_cast<unsigned>(sm->fieldValues.size());
                         i < numElements; ++i) {
                        fields.push_back(llvm::Constant::getNullValue(
                            structTy->getElementType(i)));
                    }
                    auto* constStruct = llvm::ConstantStruct::get(structTy, fields);

                    // Look up the TypeInfo for the struct
                    auto* structTI = typeRegistry_.lookup(sm->structType);
                    if (structTI) {
                        const TypeInfo* fieldTI = nullptr;
                        int fieldIdx = -1;
                        for (size_t f = 0; f < structTI->fields.size(); f++) {
                            if (structTI->fields[f].name == fieldName) {
                                fieldIdx = static_cast<int>(f);
                                fieldTI = structTI->fields[f].typeInfo;
                                break;
                            }
                        }
                        if (fieldIdx >= 0) {
                            return static_cast<llvm::Value*>(
                                builder_->CreateExtractValue(constStruct,
                                    {static_cast<unsigned>(fieldIdx)}, fieldName));
                        }
                        std::cerr << "lucis: '" << sm->structType
                                  << "' has no field '" << fieldName << "'\n";
                    }
                }
                return static_cast<llvm::Value*>(
                    llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
            }

            std::cerr << "lucis: undefined variable '" << varName << "'\n";
            return static_cast<llvm::Value*>(
                llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
        }

        auto* alloca   = it->second.alloca;
        auto* structTI = it->second.typeInfo;

        // ── .len / .length on string { ptr, len } ───────────────
        if (structTI && structTI->kind == TypeKind::String &&
            (fieldName == "len" || fieldName == "length")) {
            auto* strTy  = alloca->getAllocatedType();
            auto* strVal = builder_->CreateLoad(strTy, alloca, varName + "_str");
            return static_cast<llvm::Value*>(
                builder_->CreateExtractValue(strVal, 1, varName + "_len"));
        }

        // ── .len / .length on Vec<T>/Set<T>/Map<T> ─────────────
        if (structTI && structTI->kind == TypeKind::Extended &&
            (fieldName == "len" || fieldName == "length") &&
            (structTI->extendedKind == "Vec" || structTI->extendedKind == "Set" ||
             structTI->extendedKind == "Map")) {
            auto* extTy  = alloca->getAllocatedType();
            auto* extVal = builder_->CreateLoad(extTy, alloca, varName + "_val");
            return static_cast<llvm::Value*>(
                builder_->CreateExtractValue(extVal, 1, varName + "_len"));
        }

        // Auto-dereference: if variable is a pointer to struct, load it first
        if (structTI && structTI->kind == TypeKind::Pointer && structTI->pointeeType) {
            auto* ptrVal = builder_->CreateLoad(
                llvm::PointerType::getUnqual(*context_), alloca, "selfptr");
            auto* pointeeTI = structTI->pointeeType;
            int fieldIdx = -1;
            const TypeInfo* fieldTI = nullptr;
            for (size_t f = 0; f < pointeeTI->fields.size(); f++) {
                if (pointeeTI->fields[f].name == fieldName) {
                    fieldIdx = static_cast<int>(f);
                    fieldTI = pointeeTI->fields[f].typeInfo;
                    break;
                }
            }
            if (fieldIdx < 0) {
                std::cerr << "lucis: '" << pointeeTI->name
                          << "' has no field '" << fieldName << "'\n";
                return static_cast<llvm::Value*>(
                    llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
            }
            auto* fieldLLTy = buildFieldLLVMType(fieldTI,
                pointeeTI->fields[fieldIdx].arrayDims,
                pointeeTI->fields[fieldIdx].arraySizes);
            auto* structLLTy = llvm::StructType::getTypeByName(*context_, pointeeTI->name);
            if (!structLLTy) {
                // Fallback: look up from LLVM module
                structLLTy = static_cast<llvm::StructType*>(
                    pointeeTI->toLLVMType(*context_, module_->getDataLayout()));
            }
            auto* gep = builder_->CreateStructGEP(
                structLLTy, ptrVal, fieldIdx, fieldName + "_ptr");
            auto* load = builder_->CreateLoad(fieldLLTy, gep, fieldName);
            if (fieldTI && fieldTI->isVolatile) load->setVolatile(true);
            return static_cast<llvm::Value*>(load);
        }

        if (structTI->kind != TypeKind::Struct && structTI->kind != TypeKind::Union) {
            std::cerr << "lucis: '" << varName << "' is not a struct or union\n";
            return static_cast<llvm::Value*>(
                llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
        }

        // Find field
        const TypeInfo* fieldTI = nullptr;
        int fieldIdx = -1;
        for (size_t f = 0; f < structTI->fields.size(); f++) {
            if (structTI->fields[f].name == fieldName) {
                fieldIdx = static_cast<int>(f);
                fieldTI = structTI->fields[f].typeInfo;
                break;
            }
        }
        if (fieldIdx < 0) {
            std::cerr << "lucis: '" << structTI->name
                      << "' has no field '" << fieldName << "'\n";
            return static_cast<llvm::Value*>(
                llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
        }

        auto* fieldLLTy = buildFieldLLVMType(fieldTI,
            structTI->fields[fieldIdx].arrayDims,
            structTI->fields[fieldIdx].arraySizes);

        if (structTI->kind == TypeKind::Union) {
            auto* load = builder_->CreateLoad(fieldLLTy, alloca, fieldName);
            if (fieldTI->isVolatile) load->setVolatile(true);
            return static_cast<llvm::Value*>(load);
        }

        auto* structTy = alloca->getAllocatedType();
        auto* gep = builder_->CreateStructGEP(structTy, alloca, fieldIdx, fieldName + "_ptr");
        auto* load = builder_->CreateLoad(fieldLLTy, gep, fieldName);
        if (fieldTI->isVolatile) load->setVolatile(true);
        return static_cast<llvm::Value*>(load);
    }

    // ── Case 2: base is an indexed array element (e.g. arr[i].field) ──
    auto* indexBase = dynamic_cast<LucisParser::IndexExprContext*>(baseExpr);
    if (indexBase) {
        // Walk down to find the root variable and collect indices
        std::vector<LucisParser::ExpressionContext*> indexExprs;
        auto* current = static_cast<LucisParser::ExpressionContext*>(indexBase);
        while (auto* idxCtx = dynamic_cast<LucisParser::IndexExprContext*>(current)) {
            indexExprs.push_back(idxCtx->expression(1));
            current = idxCtx->expression(0);
        }
        auto* rootIdent = dynamic_cast<LucisParser::IdentExprContext*>(current);
        if (!rootIdent) {
            // Dynamic base (e.g. ptr->field[idx].subfield or fn()[idx].field):
            // evaluate the whole indexed expression, store in a temp alloca,
            // then GEP to the field.
            auto* indexedVal = castValue(visit(baseExpr));
            if (indexedVal && indexedVal->getType()->isStructTy()) {
                auto* baseTI = resolveExprTypeInfo(baseExpr);
            if (baseTI && (baseTI->kind == TypeKind::Struct || baseTI->kind == TypeKind::Union)) {
                const TypeInfo* fieldTI = nullptr;
                    int fieldIdx = -1;
                    for (size_t f = 0; f < baseTI->fields.size(); f++) {
                        if (baseTI->fields[f].name == fieldName) {
                            fieldIdx = static_cast<int>(f);
                            fieldTI = baseTI->fields[f].typeInfo;
                            break;
                        }
                    }
                    if (fieldIdx >= 0) {
                        auto* fieldLLTy = buildFieldLLVMType(fieldTI,
                            baseTI->fields[fieldIdx].arrayDims,
                            baseTI->fields[fieldIdx].arraySizes);
                        auto* tmp = builder_->CreateAlloca(indexedVal->getType(), nullptr, "tmp_idx_field");
                        builder_->CreateStore(indexedVal, tmp);
                        auto* gep = builder_->CreateStructGEP(indexedVal->getType(), tmp, fieldIdx, fieldName + "_ptr");
                        auto* load = builder_->CreateLoad(fieldLLTy, gep, fieldName);
                        if (fieldTI->isVolatile) load->setVolatile(true);
                        return static_cast<llvm::Value*>(load);
                    }
                }
            }
            std::cerr << "lucis: invalid base for indexed field access\n";
            return static_cast<llvm::Value*>(
                llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
        }
        std::reverse(indexExprs.begin(), indexExprs.end());

        auto varName = rootIdent->IDENTIFIER()->getText();

        auto it = locals_.find(varName);
        if (it == locals_.end()) {
            std::cerr << "lucis: undefined variable '" << varName << "'\n";
            return static_cast<llvm::Value*>(
                llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
        }

        auto* alloca   = it->second.alloca;
        auto* allocType = alloca->getAllocatedType();
        auto* structTI = it->second.typeInfo;
        auto* i64Ty    = llvm::Type::getInt64Ty(*context_);

        if (structTI && structTI->kind == TypeKind::Pointer)
            structTI = structTI->pointeeType;

        bool isPointerToAggregate = structTI && structTI->kind == TypeKind::Pointer &&
            structTI->pointeeType &&
            (structTI->pointeeType->kind == TypeKind::Struct ||
             structTI->pointeeType->kind == TypeKind::Union);

        // Collection<T> element: tokens[idx].field / m[key].field — resolve the
        // element through the normal index path (which already handles
        // Vec/Map) and GEP into it, mirroring the dynamic-base branch.
        if (structTI && structTI->kind == TypeKind::Extended &&
            (structTI->extendedKind == "Vec" || structTI->extendedKind == "Map")) {
            const TypeInfo* elemTI = structTI->extendedKind == "Vec"
                ? structTI->elementType
                : structTI->valueType;
            if (elemTI && (elemTI->kind == TypeKind::Struct ||
                           elemTI->kind == TypeKind::Union)) {
                int fieldIdx = -1;
                const TypeInfo* fieldTI = nullptr;
                for (size_t f = 0; f < elemTI->fields.size(); f++) {
                    if (elemTI->fields[f].name == fieldName) {
                        fieldIdx = static_cast<int>(f);
                        fieldTI = elemTI->fields[f].typeInfo;
                        break;
                    }
                }
                if (fieldIdx >= 0) {
                    auto* indexedVal = castValue(visit(baseExpr));
                    if (indexedVal && indexedVal->getType()->isStructTy()) {
                        auto* fieldLLTy = buildFieldLLVMType(fieldTI,
                            elemTI->fields[fieldIdx].arrayDims,
                            elemTI->fields[fieldIdx].arraySizes);
                        auto* tmp = builder_->CreateAlloca(indexedVal->getType(), nullptr, "tmp_coll_field");
                        builder_->CreateStore(indexedVal, tmp);
                        auto* gep = builder_->CreateStructGEP(
                            indexedVal->getType(), tmp, fieldIdx, fieldName + "_ptr");
                        auto* load = builder_->CreateLoad(fieldLLTy, gep, fieldName);
                        if (fieldTI->isVolatile) load->setVolatile(true);
                        return static_cast<llvm::Value*>(load);
                    }
                }
                std::cerr << "lucis: '" << elemTI->name << "' has no field '"
                          << fieldName << "'\n";
                return static_cast<llvm::Value*>(
                    llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
            }
        }

        if (!structTI || ((structTI->kind != TypeKind::Struct && structTI->kind != TypeKind::Union) &&
                          !isPointerToAggregate)) {
            std::cerr << "lucis: elements of '" << varName << "' are not structs\n";
            return static_cast<llvm::Value*>(
                llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
        }

        llvm::Value* elemPtr = nullptr;
        llvm::Type* elemTy = nullptr;

        if (it->second.typeInfo && it->second.typeInfo->kind == TypeKind::Pointer &&
            it->second.typeInfo->pointeeType) {
            auto* ptrTy = llvm::PointerType::getUnqual(*context_);
            auto* basePtr = builder_->CreateLoad(ptrTy, alloca, varName + "_ptr");

            elemTy = it->second.typeInfo->pointeeType->toLLVMType(
                *context_, module_->getDataLayout());
            elemPtr = basePtr;

            for (size_t i = 0; i < indexExprs.size(); i++) {
                auto* idx = castValue(visit(indexExprs[i]));
                if (idx->getType() != i64Ty)
                    idx = builder_->CreateIntCast(idx, i64Ty, true);

                if (auto* arrTy = llvm::dyn_cast<llvm::ArrayType>(elemTy)) {
                    elemPtr = builder_->CreateInBoundsGEP(
                        arrTy,
                        elemPtr,
                        { llvm::ConstantInt::get(i64Ty, 0), idx },
                        varName + "_elem_ptr");
                    elemTy = arrTy->getElementType();
                } else {
                    if (i > 0) {
                        std::cerr << "lucis: too many indices for pointer variable '"
                                  << varName << "'\n";
                        return static_cast<llvm::Value*>(
                            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
                    }
                    elemPtr = builder_->CreateInBoundsGEP(
                        elemTy,
                        elemPtr,
                        idx,
                        varName + "_elem_ptr");
                }
            }
        } else {
            // Build GEP indices for array indexing: [0, idx0, idx1, ...]
            std::vector<llvm::Value*> gepIndices;
            gepIndices.push_back(llvm::ConstantInt::get(i64Ty, 0));
            for (auto* idxExpr : indexExprs) {
                auto* idx = castValue(visit(idxExpr));
                if (idx->getType() != i64Ty)
                    idx = builder_->CreateIntCast(idx, i64Ty, true);
                gepIndices.push_back(idx);
            }

            // GEP to the array element (a struct)
            elemPtr = builder_->CreateGEP(allocType, alloca, gepIndices,
                                          varName + "_elem_ptr");

            // Determine the struct LLVM type (unwrap array types)
            elemTy = allocType;
            for (size_t i = 0; i < indexExprs.size(); i++) {
                auto* arrTy = llvm::dyn_cast<llvm::ArrayType>(elemTy);
                if (!arrTy) {
                    std::cerr << "lucis: too many indices for array variable '"
                              << varName << "'\n";
                    return static_cast<llvm::Value*>(
                        llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
                }
                elemTy = arrTy->getElementType();
            }
        }

        if (isPointerToAggregate) {
            auto* ptrTy = llvm::PointerType::getUnqual(*context_);
            elemPtr = builder_->CreateLoad(ptrTy, elemPtr, varName + "_elem_load");
            structTI = structTI->pointeeType;
            elemTy = structTI->toLLVMType(*context_, module_->getDataLayout());
        }

        // Find the field in the struct
        const TypeInfo* fieldTI = nullptr;
        int fieldIdx = -1;
        for (size_t f = 0; f < structTI->fields.size(); f++) {
            if (structTI->fields[f].name == fieldName) {
                fieldIdx = static_cast<int>(f);
                fieldTI = structTI->fields[f].typeInfo;
                break;
            }
        }
        if (fieldIdx < 0) {
            std::cerr << "lucis: '" << structTI->name
                      << "' has no field '" << fieldName << "'\n";
            return static_cast<llvm::Value*>(
                llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
        }

        auto* fieldLLTy = buildFieldLLVMType(fieldTI,
            structTI->fields[fieldIdx].arrayDims,
            structTI->fields[fieldIdx].arraySizes);

        if (structTI->kind == TypeKind::Union) {
            return static_cast<llvm::Value*>(
                builder_->CreateLoad(fieldLLTy, elemPtr, fieldName));
        }

        auto* fieldGep = builder_->CreateStructGEP(elemTy, elemPtr, fieldIdx,
                                                     fieldName + "_ptr");
        return static_cast<llvm::Value*>(
            builder_->CreateLoad(fieldLLTy, fieldGep, fieldName));
    }

    // ── Case 3: base is another field access (e.g. a.b.c — chained) ──
    auto* fieldBase = dynamic_cast<LucisParser::FieldAccessExprContext*>(baseExpr);
    if (fieldBase) {
        // Evaluate chained field access: get the struct value, then access this field
        // We need the address, not the value. Walk down to find the root and rebuild GEP chain.
        // For now, use the general fallback approach: alloca the value and GEP into it.
        auto* baseVal = castValue(visit(baseExpr));
        if (baseVal->getType()->isStructTy()) {
            // Find the field in the intermediate struct
            auto* baseTI = resolveExprTypeInfo(baseExpr);
            if (baseTI && (baseTI->kind == TypeKind::Struct || baseTI->kind == TypeKind::Union)) {
                const TypeInfo* fieldTI = nullptr;
                int fieldIdx = -1;
                for (size_t f = 0; f < baseTI->fields.size(); f++) {
                    if (baseTI->fields[f].name == fieldName) {
                        fieldIdx = static_cast<int>(f);
                        fieldTI = baseTI->fields[f].typeInfo;
                        break;
                    }
                }
                if (fieldIdx >= 0) {
                    auto* fieldLLTy = buildFieldLLVMType(fieldTI,
                        baseTI->fields[fieldIdx].arrayDims,
                        baseTI->fields[fieldIdx].arraySizes);
                    auto* tmp = builder_->CreateAlloca(baseVal->getType(), nullptr, "chain_tmp");
                    builder_->CreateStore(baseVal, tmp);
                    if (baseTI->kind == TypeKind::Union) {
                        auto* l = builder_->CreateLoad(fieldLLTy, tmp, fieldName);
                        if (fieldTI->isVolatile) l->setVolatile(true);
                        return static_cast<llvm::Value*>(l);
                    }
                    auto* gep = builder_->CreateStructGEP(baseVal->getType(), tmp,
                                                          fieldIdx, fieldName + "_ptr");
                    auto* l = builder_->CreateLoad(fieldLLTy, gep, fieldName);
                    if (fieldTI->isVolatile) l->setVolatile(true);
                    return static_cast<llvm::Value*>(l);
                }
            }
        }
    }

    // ── General fallback: any expression returning a struct or pointer (e.g. method call, chained field) ──
    {
        auto* baseVal = castValue(visit(baseExpr));
        if (!baseVal) {
            std::cerr << "lucis: null base value for field access\n";
            return static_cast<llvm::Value*>(
                llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
        }

        // ── BitStruct field through chained access ────────────────
        {
            auto* baseTI = resolveExprTypeInfo(baseExpr);
            if (baseTI && baseTI->kind == TypeKind::BitStruct) {
                for (auto& bf : baseTI->bitFields) {
                    if (bf.name == fieldName && !bf.isPadding) {
                        auto* backTy = baseTI->backingType->toLLVMType(*context_, module_->getDataLayout());
                        llvm::Value* rawVal = baseVal;
                        if (rawVal->getType() != backTy)
                            rawVal = builder_->CreateTrunc(rawVal, backTy);
                        llvm::Value* shifted = rawVal;
                        if (bf.offset > 0)
                            shifted = builder_->CreateLShr(rawVal, llvm::ConstantInt::get(backTy, bf.offset));
                        auto* masked = builder_->CreateAnd(shifted,
                            llvm::ConstantInt::get(backTy, (1ULL << bf.width) - 1));
                        auto* resultTy = bf.width <= 8 ? llvm::Type::getInt8Ty(*context_)
                                      : bf.width <= 16 ? llvm::Type::getInt16Ty(*context_)
                                      : bf.width <= 32 ? llvm::Type::getInt32Ty(*context_)
                                      : llvm::Type::getInt64Ty(*context_);
                        auto* result = builder_->CreateIntCast(masked, resultTy, false);
                        return static_cast<llvm::Value*>(result);
                    }
                }
            }
        }

        // Pointer to struct: e.g. a.field which is *Struct → access field through pointer
        if (baseVal->getType()->isPointerTy()) {
            auto* baseTI = resolveExprTypeInfo(baseExpr);
            if (baseTI && baseTI->kind == TypeKind::Pointer && baseTI->pointeeType) {
                auto* structTI = baseTI->pointeeType;
                int fieldIdx = -1;
                const TypeInfo* fieldTI = nullptr;
                for (size_t f = 0; f < structTI->fields.size(); f++) {
                    if (structTI->fields[f].name == fieldName) {
                        fieldIdx = static_cast<int>(f);
                        fieldTI = structTI->fields[f].typeInfo;
                        break;
                    }
                }
                if (fieldIdx >= 0) {
                    auto* fieldLLTy = buildFieldLLVMType(fieldTI,
                        structTI->fields[fieldIdx].arrayDims,
                        structTI->fields[fieldIdx].arraySizes);
                    auto* structLLTy = llvm::StructType::getTypeByName(*context_, structTI->name);
                    if (!structLLTy)
                        structLLTy = static_cast<llvm::StructType*>(
                            structTI->toLLVMType(*context_, module_->getDataLayout()));
                    auto* gep = builder_->CreateStructGEP(structLLTy, baseVal, fieldIdx, fieldName + "_ptr");
                    auto* load = builder_->CreateLoad(fieldLLTy, gep, fieldName);
                    if (fieldTI->isVolatile) load->setVolatile(true);
                    return static_cast<llvm::Value*>(load);
                }
            }
        }

        if (baseVal->getType()->isStructTy()) {
            auto* baseTI = resolveExprTypeInfo(baseExpr);

            // .len / .length on string expression
            if (baseTI && baseTI->kind == TypeKind::String &&
                (fieldName == "len" || fieldName == "length")) {
                return static_cast<llvm::Value*>(
                    builder_->CreateExtractValue(baseVal, 1, "expr_len"));
            }

            // .len / .length on Vec<T>/Set<T>/Map<T> expression
            if (baseTI && baseTI->kind == TypeKind::Extended &&
                (fieldName == "len" || fieldName == "length") &&
                (baseTI->extendedKind == "Vec" || baseTI->extendedKind == "Set" ||
                 baseTI->extendedKind == "Map")) {
                return static_cast<llvm::Value*>(
                    builder_->CreateExtractValue(baseVal, 1, "expr_len"));
            }

            if (baseTI && (baseTI->kind == TypeKind::Struct || baseTI->kind == TypeKind::Union)) {
                const TypeInfo* fieldTI = nullptr;
                int fieldIdx = -1;
                for (size_t f = 0; f < baseTI->fields.size(); f++) {
                    if (baseTI->fields[f].name == fieldName) {
                        fieldIdx = static_cast<int>(f);
                        fieldTI = baseTI->fields[f].typeInfo;
                        break;
                    }
                }
                if (fieldIdx >= 0) {
                    auto* fieldLLTy = buildFieldLLVMType(fieldTI,
                        baseTI->fields[fieldIdx].arrayDims,
                        baseTI->fields[fieldIdx].arraySizes);
                    auto* tmp = builder_->CreateAlloca(baseVal->getType(), nullptr, "expr_tmp");
                    builder_->CreateStore(baseVal, tmp);
                    if (baseTI->kind == TypeKind::Union) {
                        return static_cast<llvm::Value*>(
                            builder_->CreateLoad(fieldLLTy, tmp, fieldName));
                    }
                    auto* gep = builder_->CreateStructGEP(baseVal->getType(), tmp,
                                                          fieldIdx, fieldName + "_ptr");
                    return static_cast<llvm::Value*>(
                        builder_->CreateLoad(fieldLLTy, gep, fieldName));
                }
            }
        }
    }

    std::cerr << "lucis: unsupported base expression for field access\n";
    return static_cast<llvm::Value*>(
        llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
}



std::any IRGen::visitArrowAccessExpr(LucisParser::ArrowAccessExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto fieldName = ctx->IDENTIFIER()->getText();

    // Evaluate base expression — should yield a pointer to struct
    auto* ptrVal = castValue(visit(ctx->expression()));

    // Resolve the pointee struct TypeInfo from the expression tree
    auto* exprTI = resolveExprTypeInfo(ctx->expression());
    const TypeInfo* structTI = nullptr;
    if (exprTI && exprTI->kind == TypeKind::Pointer)
        structTI = exprTI->pointeeType;

    if (!structTI || (structTI->kind != TypeKind::Struct && structTI->kind != TypeKind::Union)) {
        std::cerr << "lucis: '->' requires pointer to struct or union\n";
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }

    // Find field
    const TypeInfo* fieldTI = nullptr;
    int fieldIdx = -1;
    for (size_t f = 0; f < structTI->fields.size(); f++) {
        if (structTI->fields[f].name == fieldName) {
            fieldIdx = static_cast<int>(f);
            fieldTI = structTI->fields[f].typeInfo;
            break;
        }
    }
    if (fieldIdx < 0) {
        std::cerr << "lucis: '" << structTI->name
                  << "' has no field '" << fieldName << "'\n";
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }

    auto* fieldLLTy = buildFieldLLVMType(fieldTI,
        structTI->fields[fieldIdx].arrayDims,
        structTI->fields[fieldIdx].arraySizes);

    if (structTI->kind == TypeKind::Union) {
        // Union: all fields at offset 0
        auto* load = builder_->CreateLoad(fieldLLTy, ptrVal, fieldName);
        if (fieldTI && fieldTI->isVolatile) load->setVolatile(true);
        return static_cast<llvm::Value*>(load);
    }

    auto* structTy = structTI->toLLVMType(*context_, module_->getDataLayout());
    auto* gep = builder_->CreateStructGEP(structTy, ptrVal, fieldIdx, fieldName + "_ptr");

    auto* load = builder_->CreateLoad(fieldLLTy, gep, fieldName);
    if (fieldTI && fieldTI->isVolatile) load->setVolatile(true);
    return static_cast<llvm::Value*>(load);
}



llvm::Type* IRGen::buildFieldLLVMType(const TypeInfo* elemTI, unsigned arrayDims,
                                      const std::vector<unsigned>& arraySizes) {
    auto* ty = elemTI->toLLVMType(*context_, module_->getDataLayout());
    if (arrayDims > 0 && !arraySizes.empty()) {
        for (auto it = arraySizes.rbegin(); it != arraySizes.rend(); ++it)
            ty = llvm::ArrayType::get(ty, *it);
    } else if (arrayDims > 0 && arraySizes.empty()) {
        // Unsized array field []T — represent as slice {T*, i64}
        auto* ptrTy = llvm::PointerType::getUnqual(*context_);
        auto* i64Ty = llvm::Type::getInt64Ty(*context_);
        ty = llvm::StructType::get(*context_, {ptrTy, i64Ty});
    }
    return ty;
}



llvm::Type* IRGen::getEnumVariantPayloadType(const EnumVariantInfo& variantInfo) {
    if (variantInfo.payloadFields.empty()) return nullptr;

    auto buildFieldType = [&](const FieldInfo& field) -> llvm::Type* {
        auto* llTy = field.typeInfo->toLLVMType(*context_, module_->getDataLayout());
        if (field.arrayDims == 0) return llTy;
        if (field.arraySizes.empty()) {
            auto* ptrTy = llvm::PointerType::getUnqual(*context_);
            auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
            return llvm::StructType::get(*context_, {ptrTy, usizeTy});
        }

        llvm::Type* arrTy = llTy;
        for (auto it = field.arraySizes.rbegin(); it != field.arraySizes.rend(); ++it)
            arrTy = llvm::ArrayType::get(arrTy, *it);
        return arrTy;
    };

    if (variantInfo.payloadKind == EnumPayloadKind::Tuple &&
        variantInfo.payloadFields.size() == 1) {
        return buildFieldType(variantInfo.payloadFields[0]);
    }

    std::vector<llvm::Type*> fieldTypes;
    for (const auto& field : variantInfo.payloadFields) {
        fieldTypes.push_back(buildFieldType(field));
    }
    return llvm::StructType::get(*context_, fieldTypes);
}



llvm::Value* IRGen::buildEnumVariantValue(const TypeInfo* enumType,
                                          const EnumVariantInfo& variantInfo,
                                          const std::vector<llvm::Value*>& payloadValues) {
    if (!enumType) {
        return llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_));
    }

    // For []T payload fields without explicit [N] sizes, infer concrete sizes
    // from the first constructed payload so enum storage matches runtime value.
    if (variantInfo.payloadFields.size() == 1 && payloadValues.size() == 1) {
        const auto& pf = variantInfo.payloadFields[0];
        if (pf.arrayDims > 0 && pf.arraySizes.empty()) {
            std::vector<unsigned> inferred;
            if (inferArraySizesFromLLVMType(payloadValues[0]->getType(), pf.arrayDims, inferred)) {
                auto* mutEnum = const_cast<TypeInfo*>(enumType);
                if (variantInfo.discriminant < mutEnum->enumVariantInfos.size()) {
                    auto& mutField = mutEnum->enumVariantInfos[variantInfo.discriminant].payloadFields[0];
                    if (mutField.arraySizes.empty()) {
                        mutField.arraySizes = inferred;
                    } else if (mutField.arraySizes != inferred) {
                        std::cerr << "lucis: inconsistent inferred array shape for enum variant '"
                                  << variantInfo.name << "'\n";
                    }
                }
            }
        }
    }

    auto* enumLLTy = enumType->toLLVMType(*context_, module_->getDataLayout());
    auto* tagValue = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_),
                                            variantInfo.discriminant);

    if (!llvm::isa<llvm::StructType>(enumLLTy))
        return tagValue;

    auto* alloca = builder_->CreateAlloca(enumLLTy, nullptr, enumType->name + ".variant");
    auto* tagPtr = builder_->CreateStructGEP(enumLLTy, alloca, 0, "enum_tag_ptr");
    builder_->CreateStore(tagValue, tagPtr);

    if (!variantInfo.payloadFields.empty() && !payloadValues.empty()) {
        auto* payloadPtr = builder_->CreateStructGEP(enumLLTy, alloca, 1, "enum_payload_ptr");
        auto* payloadTy = getEnumVariantPayloadType(variantInfo);

        if (payloadTy) {
            auto fieldLLVMType = [&](const FieldInfo& field) -> llvm::Type* {
                auto* llTy = field.typeInfo->toLLVMType(*context_, module_->getDataLayout());
                if (field.arrayDims == 0) return llTy;
                if (field.arraySizes.empty()) {
                    auto* ptrTy = llvm::PointerType::getUnqual(*context_);
                    auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
                    return llvm::StructType::get(*context_, {ptrTy, usizeTy});
                }
                llvm::Type* arrTy = llTy;
                for (auto it = field.arraySizes.rbegin(); it != field.arraySizes.rend(); ++it)
                    arrTy = llvm::ArrayType::get(arrTy, *it);
                return arrTy;
            };

            auto coerceFieldValue = [&](llvm::Value* in, const FieldInfo& field) -> llvm::Value* {
                auto* expected = fieldLLVMType(field);
                if (in->getType() == expected) return in;

                if (in->getType()->isArrayTy() && expected->isArrayTy()) {
                    auto* tmp = builder_->CreateAlloca(expected, nullptr, "enum_payload_arr_cast");
                    storeArrayElements(in, tmp, expected, field.typeInfo, field.arrayDims);
                    return builder_->CreateLoad(expected, tmp, "enum_payload_arr_cast.val");
                }

                if (in->getType()->isIntegerTy() && expected->isIntegerTy())
                    return builder_->CreateIntCast(in, expected, field.typeInfo->isSigned);

                if (in->getType()->isFloatingPointTy() && expected->isFloatingPointTy()) {
                    if (in->getType()->getPrimitiveSizeInBits() > expected->getPrimitiveSizeInBits())
                        return builder_->CreateFPTrunc(in, expected);
                    return builder_->CreateFPExt(in, expected);
                }

                if (in->getType()->isIntegerTy() && expected->isFloatingPointTy())
                    return builder_->CreateSIToFP(in, expected);

                if (in->getType()->isFloatingPointTy() && expected->isIntegerTy())
                    return builder_->CreateFPToSI(in, expected);

                return in;
            };

            llvm::Value* payloadValue = nullptr;
            if (variantInfo.payloadKind == EnumPayloadKind::Tuple && payloadValues.size() == 1) {
                payloadValue = coerceFieldValue(payloadValues[0], variantInfo.payloadFields[0]);
            } else {
                payloadValue = llvm::UndefValue::get(payloadTy);
                for (size_t i = 0; i < payloadValues.size(); i++) {
                    auto* elem = payloadValues[i];
                    if (i < variantInfo.payloadFields.size())
                        elem = coerceFieldValue(elem, variantInfo.payloadFields[i]);
                    payloadValue = builder_->CreateInsertValue(payloadValue, elem,
                                                              {static_cast<unsigned>(i)});
                }
            }

            auto* typedPayloadPtr = builder_->CreateBitCast(payloadPtr,
                llvm::PointerType::getUnqual(payloadTy), "enum_payload_cast");
            builder_->CreateStore(payloadValue, typedPayloadPtr);
        }
    }

    return builder_->CreateLoad(enumLLTy, alloca, enumType->name + ".value");
}



std::any IRGen::visitEnumAccessExpr(LucisParser::EnumAccessExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto identifiers = ctx->IDENTIFIER();
    auto enumName = identifiers.size() > 0 ? identifiers[0]->getText() : "";
    auto variantName = identifiers.size() > 1 ? identifiers[1]->getText() : "";

    auto* ti = typeRegistry_.lookup(enumName);
    if (!ti || ti->kind != TypeKind::Enum) {
        // Check if it's a C enum — constants are stored in cEnumConstants_
        auto it = cEnumConstants_.find(variantName);
        if (it != cEnumConstants_.end()) {
            return static_cast<llvm::Value*>(
                llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), it->second));
        }
        std::cerr << "lucis: unknown enum type '" << enumName << "'\n";
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }

    const EnumVariantInfo* variantInfo = nullptr;
    for (const auto& info : ti->enumVariantInfos) {
        if (info.name == variantName) {
            variantInfo = &info;
            break;
        }
    }
    if (!variantInfo) {
        std::cerr << "lucis: enum '" << enumName
                  << "' has no variant '" << variantName << "'\n";
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }

    return static_cast<llvm::Value*>(buildEnumVariantValue(ti, *variantInfo, {}));
}



std::any IRGen::visitGenericEnumAccessExpr(LucisParser::GenericEnumAccessExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto ids = ctx->IDENTIFIER();
    auto baseName = ids.size() > 0 ? ids[0]->getText() : "";
    auto variantName = ids.size() > 1 ? ids[1]->getText() : "";

    std::vector<const TypeInfo*> typeArgs;
    for (auto* ts : ctx->typeSpec()) {
        auto* argTI = resolveTypeInfo(ts);
        if (!argTI) argTI = typeRegistry_.lookup("int32");
        typeArgs.push_back(argTI);
    }

    auto enumIt = genericEnumTemplates_.find(baseName);
    if (enumIt == genericEnumTemplates_.end()) {
        std::cerr << "lucis: unknown generic enum '" << baseName << "'\n";
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }

    auto* enumType = instantiateGenericEnum(baseName, enumIt->second, typeArgs);
    if (!enumType) {
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }

    const EnumVariantInfo* variantInfo = nullptr;
    for (const auto& info : enumType->enumVariantInfos) {
        if (info.name == variantName) {
            variantInfo = &info;
            break;
        }
    }
    if (!variantInfo) {
        std::cerr << "lucis: enum '" << enumType->name
                  << "' has no variant '" << variantName << "'\n";
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }

    return static_cast<llvm::Value*>(buildEnumVariantValue(enumType, *variantInfo, {}));
}

// ── Qualified struct/union positional init: LIB::Point { x, y } or enum variant: Shape::Circle { 1, 2 } ─────



std::any IRGen::visitQualifiedStructPosInitExpr(LucisParser::QualifiedStructPosInitExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto first = ctx->IDENTIFIER().size() > 0 ? ctx->IDENTIFIER(0)->getText() : "";
    auto second = ctx->IDENTIFIER().size() > 1 ? ctx->IDENTIFIER(1)->getText() : "";

    // Try as qualified struct/union init: namespace::type
    auto* ti = typeRegistry_.lookup(second);
    if (ti && (ti->kind == TypeKind::Struct || ti->kind == TypeKind::Union)) {
        auto exprs = ctx->expression();

        if (ti->kind == TypeKind::Union) {
            auto* unionTy = ti->toLLVMType(*context_, module_->getDataLayout());
            auto* alloca = builder_->CreateAlloca(unionTy, nullptr, second + "_tmp");
            if (!exprs.empty()) {
                auto* fieldTI = ti->fields[0].typeInfo;
                if (fieldTI) {
                    auto* val = castValue(visit(exprs[0]));
                    auto* fieldLLTy = fieldTI->toLLVMType(*context_, module_->getDataLayout());
                    if (val->getType() != fieldLLTy) {
                        if (val->getType()->isIntegerTy() && fieldLLTy->isIntegerTy())
                            val = builder_->CreateIntCast(val, fieldLLTy, fieldTI->isSigned);
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
                builder_->CreateLoad(unionTy, alloca, second + "_val"));
        }

        auto* structTy = ti->toLLVMType(*context_, module_->getDataLayout());
        llvm::Value* agg = llvm::ConstantAggregateZero::get(structTy);

        for (size_t i = 0; i < exprs.size(); i++) {
            if (i >= ti->fields.size()) break;

            auto& field = ti->fields[i];
            auto* fieldTI = field.typeInfo;
            auto* fieldTy = fieldTI->toLLVMType(*context_, module_->getDataLayout());
            llvm::Value* val = nullptr;

            if (fieldTI && fieldTI->kind == TypeKind::Extended &&
                fieldTI->extendedKind == "Vec") {
                if (auto* arrLit = dynamic_cast<LucisParser::ArrayLitExprContext*>(exprs[i])) {
                    val = buildVecValueFromArrayLiteral(
                        arrLit, fieldTI, second + "_" + field.name + "_vec");
                }
            }

            if (!val)
                val = castValue(visit(exprs[i]));

            // Unsized array field []T — lower [N]T value to {ptr, len} slice
            if (val->getType() != fieldTy) {
                if (val->getType()->isIntegerTy() && fieldTy->isIntegerTy())
                    val = builder_->CreateIntCast(val, fieldTy, fieldTI->isSigned);
                else if (val->getType()->isFloatingPointTy() && fieldTy->isFloatingPointTy()) {
                    if (val->getType()->getPrimitiveSizeInBits() > fieldTy->getPrimitiveSizeInBits())
                        val = builder_->CreateFPTrunc(val, fieldTy);
                    else
                        val = builder_->CreateFPExt(val, fieldTy);
                } else if (field.arrayDims > 0 && field.arraySizes.empty() &&
                           val->getType()->isArrayTy() && fieldTy->isStructTy() &&
                           llvm::cast<llvm::StructType>(fieldTy)->getNumElements() == 2) {
                    val = lowerArrayToSlice(val, fieldTy);
                }
            }

            agg = builder_->CreateInsertValue(agg, val, i, second + "_" + field.name);
        }

        for (auto* e : exprs)
            consumeExprIfOwnedLocal(e);
        return static_cast<llvm::Value*>(agg);
    }

    // Try as enum variant init: enum_type::VariantName { ... }
    ti = typeRegistry_.lookup(first);
    if (ti && ti->kind == TypeKind::Enum) {
        auto variantName = second;
        auto exprs = ctx->expression();

        const EnumVariantInfo* variantInfo = nullptr;
        for (const auto& info : ti->enumVariantInfos) {
            if (info.name == variantName) {
                variantInfo = &info;
                break;
            }
        }
        if (!variantInfo) {
            std::cerr << "lucis: enum '" << ti->name << "' has no variant '" << variantName << "'\n";
            return static_cast<llvm::Value*>(
                llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
        }

        std::vector<llvm::Value*> payloadValues(variantInfo->payloadFields.size());
        for (size_t i = 0; i < exprs.size() && i < payloadValues.size(); i++) {
            auto* argExpr = exprs[i];
            auto* expectedTI = variantInfo->payloadFields[i].typeInfo;
            if (expectedTI && expectedTI->kind == TypeKind::Extended &&
                expectedTI->extendedKind == "Vec") {
                if (auto* arrLit = dynamic_cast<LucisParser::ArrayLitExprContext*>(argExpr)) {
                    payloadValues[i] = buildVecValueFromArrayLiteral(
                        arrLit, expectedTI, "enum_pos_vec_payload");
                } else {
                    payloadValues[i] = castValue(visit(argExpr));
                }
            } else {
                payloadValues[i] = castValue(visit(argExpr));
            }
        }

        auto* enumValue = buildEnumVariantValue(ti, *variantInfo, payloadValues);
        for (auto* payloadExpr : exprs)
            consumeExprIfOwnedLocal(payloadExpr);
        return static_cast<llvm::Value*>(enumValue);
    }

    std::cerr << "lucis: unknown type '" << first << "::" << second << "'\n";
    return static_cast<llvm::Value*>(
        llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
}

// ── Qualified struct/union named init: LIB::Point { x: 10, y: 20 } or enum variant: Shape::Circle { r: 4.0 } ─



std::any IRGen::visitQualifiedStructNamedInitExpr(LucisParser::QualifiedStructNamedInitExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto first = ctx->IDENTIFIER().size() > 0 ? ctx->IDENTIFIER(0)->getText() : "";
    auto second = ctx->IDENTIFIER().size() > 1 ? ctx->IDENTIFIER(1)->getText() : "";

    // Try as qualified struct/union init: namespace::type
    auto* ti = typeRegistry_.lookup(second);
    if (ti && (ti->kind == TypeKind::Struct || ti->kind == TypeKind::Union)) {
        auto ids = ctx->IDENTIFIER();
        auto exprs = ctx->expression();

        auto* structTy = ti->toLLVMType(*context_, module_->getDataLayout());
        llvm::Value* agg = llvm::ConstantAggregateZero::get(structTy);

        for (size_t i = 0; i < exprs.size(); i++) {
            auto fieldName = ids[i + 2]->getText();
            int fieldIdx = -1;
            for (int j = 0; j < (int)ti->fields.size(); j++) {
                if (ti->fields[j].name == fieldName) { fieldIdx = j; break; }
            }
            if (fieldIdx < 0) continue;

            auto* fieldTI = ti->fields[fieldIdx].typeInfo;
            auto* fieldTy = fieldTI->toLLVMType(*context_, module_->getDataLayout());
            auto* val = castValue(visit(exprs[i]));

            if (val->getType() != fieldTy) {
                if (val->getType()->isIntegerTy() && fieldTy->isIntegerTy())
                    val = builder_->CreateIntCast(val, fieldTy, fieldTI->isSigned);
                else if (val->getType()->isFloatingPointTy() && fieldTy->isFloatingPointTy()) {
                    if (val->getType()->getPrimitiveSizeInBits() > fieldTy->getPrimitiveSizeInBits())
                        val = builder_->CreateFPTrunc(val, fieldTy);
                    else
                        val = builder_->CreateFPExt(val, fieldTy);
                }
            }

            agg = builder_->CreateInsertValue(agg, val, fieldIdx, second + "_" + fieldName);
        }

        for (auto* e : exprs)
            consumeExprIfOwnedLocal(e);
        return static_cast<llvm::Value*>(agg);
    }

    // Try as enum variant init: enum_type::VariantName { ... }
    ti = typeRegistry_.lookup(first);
    if (ti && ti->kind == TypeKind::Enum) {
        auto variantName = second;
        auto ids = ctx->IDENTIFIER();
        auto exprs = ctx->expression();

        const EnumVariantInfo* variantInfo = nullptr;
        for (const auto& info : ti->enumVariantInfos) {
            if (info.name == variantName) {
                variantInfo = &info;
                break;
            }
        }
        if (!variantInfo) {
            std::cerr << "lucis: enum '" << ti->name << "' has no variant '" << variantName << "'\n";
            return static_cast<llvm::Value*>(
                llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
        }

        std::vector<llvm::Value*> payloadValues(variantInfo->payloadFields.size());
        for (size_t i = 0; i < exprs.size(); i++) {
            auto fieldName = ids[i + 2]->getText();
            for (size_t fieldIdx = 0; fieldIdx < variantInfo->payloadFields.size(); fieldIdx++) {
                if (variantInfo->payloadFields[fieldIdx].name == fieldName) {
                    auto* argExpr = exprs[i];
                    auto* expectedTI = variantInfo->payloadFields[fieldIdx].typeInfo;
                    if (expectedTI && expectedTI->kind == TypeKind::Extended &&
                        expectedTI->extendedKind == "Vec") {
                        if (auto* arrLit = dynamic_cast<LucisParser::ArrayLitExprContext*>(argExpr)) {
                            payloadValues[fieldIdx] = buildVecValueFromArrayLiteral(
                                arrLit, expectedTI, "enum_named_vec_payload");
                            break;
                        }
                    }
                    payloadValues[fieldIdx] = castValue(visit(argExpr));
                    break;
                }
            }
        }

        auto* enumValue = buildEnumVariantValue(ti, *variantInfo, payloadValues);
        for (auto* payloadExpr : exprs)
            consumeExprIfOwnedLocal(payloadExpr);
        return static_cast<llvm::Value*>(enumValue);
    }

    std::cerr << "lucis: unknown type '" << first << "::" << second << "'\n";
    return static_cast<llvm::Value*>(
        llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
}



std::any IRGen::visitGenericEnumNamedVariantExpr(LucisParser::GenericEnumNamedVariantExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto ids = ctx->IDENTIFIER();
    auto baseName = ids.size() > 0 ? ids[0]->getText() : "";
    auto variantName = ids.size() > 1 ? ids[1]->getText() : "";

    std::vector<const TypeInfo*> typeArgs;
    for (auto* ts : ctx->typeSpec()) {
        auto* argTI = resolveTypeInfo(ts);
        if (!argTI) argTI = typeRegistry_.lookup("int32");
        typeArgs.push_back(argTI);
    }

    auto enumIt = genericEnumTemplates_.find(baseName);
    if (enumIt == genericEnumTemplates_.end()) {
        std::cerr << "lucis: unknown generic enum '" << baseName << "'\n";
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }

    auto* enumType = instantiateGenericEnum(baseName, enumIt->second, typeArgs);
    if (!enumType) {
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }

    const EnumVariantInfo* variantInfo = nullptr;
    for (const auto& info : enumType->enumVariantInfos) {
        if (info.name == variantName) {
            variantInfo = &info;
            break;
        }
    }
    if (!variantInfo) {
        std::cerr << "lucis: enum '" << enumType->name
                  << "' has no variant '" << variantName << "'\n";
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }

    std::vector<llvm::Value*> payloadValues(variantInfo->payloadFields.size());
    for (size_t i = 0; i < ctx->expression().size(); i++) {
        auto fieldName = ids[i + 2]->getText();
        for (size_t fieldIdx = 0; fieldIdx < variantInfo->payloadFields.size(); fieldIdx++) {
            if (variantInfo->payloadFields[fieldIdx].name == fieldName) {
                auto* argExpr = ctx->expression(i);
                auto* expectedTI = variantInfo->payloadFields[fieldIdx].typeInfo;
                if (expectedTI && expectedTI->kind == TypeKind::Extended &&
                    expectedTI->extendedKind == "Vec") {
                    if (auto* arrLit = dynamic_cast<LucisParser::ArrayLitExprContext*>(argExpr)) {
                        payloadValues[fieldIdx] = buildVecValueFromArrayLiteral(
                            arrLit, expectedTI, "gen_enum_named_vec_payload");
                        break;
                    }
                }
                payloadValues[fieldIdx] = castValue(visit(argExpr));
                break;
            }
        }
    }

    auto* enumValue = buildEnumVariantValue(enumType, *variantInfo, payloadValues);
    for (auto* payloadExpr : ctx->expression())
        consumeExprIfOwnedLocal(payloadExpr);
    return static_cast<llvm::Value*>(enumValue);
}

// ── Generic enum positional variant: Enum<T>::Variant { expr, ... } ──────



std::any IRGen::visitGenericEnumPosVariantExpr(LucisParser::GenericEnumPosVariantExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto ids = ctx->IDENTIFIER();
    auto baseName = ids[0]->getText();
    auto variantName = ids[1]->getText();

    std::vector<const TypeInfo*> typeArgs;
    for (auto* ts : ctx->typeSpec()) {
        auto* argTI = resolveTypeInfo(ts);
        if (!argTI) argTI = typeRegistry_.lookup("int32");
        typeArgs.push_back(argTI);
    }

    auto enumIt = genericEnumTemplates_.find(baseName);
    if (enumIt == genericEnumTemplates_.end()) {
        std::cerr << "lucis: unknown generic enum '" << baseName << "'\n";
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }

    auto* enumType = instantiateGenericEnum(baseName, enumIt->second, typeArgs);
    if (!enumType) {
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }

    const EnumVariantInfo* variantInfo = nullptr;
    for (const auto& info : enumType->enumVariantInfos) {
        if (info.name == variantName) {
            variantInfo = &info;
            break;
        }
    }
    if (!variantInfo) {
        std::cerr << "lucis: enum '" << enumType->name
                  << "' has no variant '" << variantName << "'\n";
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }

    std::vector<llvm::Value*> payloadValues(variantInfo->payloadFields.size());
    for (size_t i = 0; i < ctx->expression().size() && i < variantInfo->payloadFields.size(); i++) {
        auto* argExpr = ctx->expression(i);
        auto* expectedTI = variantInfo->payloadFields[i].typeInfo;
        if (expectedTI && expectedTI->kind == TypeKind::Extended &&
            expectedTI->extendedKind == "Vec") {
            if (auto* arrLit = dynamic_cast<LucisParser::ArrayLitExprContext*>(argExpr)) {
                payloadValues[i] = buildVecValueFromArrayLiteral(
                    arrLit, expectedTI, "gen_enum_pos_vec_payload");
                continue;
            }
        }
        payloadValues[i] = castValue(visit(argExpr));
    }

    auto* enumValue = buildEnumVariantValue(enumType, *variantInfo, payloadValues);
    for (auto* payloadExpr : ctx->expression())
        consumeExprIfOwnedLocal(payloadExpr);
    return static_cast<llvm::Value*>(enumValue);
}

// ── Static method call: Struct::method(args) ────────────────────────────────


std::any IRGen::visitStaticMethodCallExpr(
        LucisParser::StaticMethodCallExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto ids = ctx->IDENTIFIER();
    if (ids.size() < 2) {
        std::cerr << "lucis: invalid static call expression\n";
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }

    // ── Intrinsic call: lucis::core::trap() ────────────────────────────
    {
        std::vector<std::string> idTexts;
        for (auto* id : ids)
            idTexts.push_back(id->getText());

        if (IntrinsicRegistry::isIntrinsicPrefix(idTexts[0])) {
            std::string ns, funcName;
            if (!IntrinsicRegistry::parseIntrinsicPath(idTexts, ns, funcName)) {
                std::cerr << "lucis: invalid intrinsic path: expected "
                             "'lucis::namespace::function'\n";
                return static_cast<llvm::Value*>(
                    llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
            }

            auto* intrinsic = intrinsicRegistry_.lookup(ns, funcName);
            if (!intrinsic) {
                std::cerr << "lucis: unknown intrinsic '" << ns << "::"
                          << funcName << "'\n";
                return static_cast<llvm::Value*>(
                    llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
            }

            // Visit arguments
            std::vector<llvm::Value*> args;
            std::vector<LucisParser::ExpressionContext*> argExprs;
            if (auto* argList = ctx->argList()) {
                argExprs = argList->expression();
                for (auto* argExpr : argExprs)
                    args.push_back(castValue(visit(argExpr)));
            }

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

            // Lower based on lowering kind
            switch (intrinsic->lowering.kind) {
            case IntrinsicFunction::Lowering::LLVMIntrinsic: {
                // Declare the LLVM intrinsic and call it
                auto* voidTy = llvm::Type::getVoidTy(*context_);
                auto callee = declareBuiltin(
                    intrinsic->lowering.intrinsicName, voidTy, {});
                builder_->CreateCall(callee, {});
                // Execution never continues past a trap
                builder_->CreateUnreachable();
                break;
            }
            case IntrinsicFunction::Lowering::BuiltinCall: {
                auto* voidTy = llvm::Type::getVoidTy(*context_);
                auto callee = declareBuiltin(
                    "lucis_" + intrinsic->lowering.intrinsicName, voidTy, {});
                builder_->CreateCall(callee, args);
                break;
            }
            case IntrinsicFunction::Lowering::InlineIR: {
                std::vector<const TypeInfo*> emptyTypeArgs;
                return intrinsic->lowering.emitIR(
                    *builder_, module_, *context_,
                    typeRegistry_, args, emptyTypeArgs);
            }
            }

            // Consume owned arguments
            for (auto* e : argExprs)
                consumeExprIfOwnedLocal(e);

            return static_cast<llvm::Value*>(
                llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
        }
    }
    // ── End intrinsic call check ─────────────────────────────────────

    auto structName = ids.front()->getText();
    auto methodName = ids.back()->getText();

    auto emitStdModuleCall = [&](const std::string& modulePath) -> llvm::Value* {
        ImportResolver moduleResolver;
        moduleResolver.addImport(modulePath, methodName);
        if (!moduleResolver.isImported(methodName)) {
            std::cerr << "lucis: module '" << modulePath
                      << "' does not export callable symbol '" << methodName << "'\n";
            return llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_));
        }

        if (methodName == "print" || methodName == "println" ||
            methodName == "eprint" || methodName == "eprintln") {
            std::vector<llvm::Value*> args;
            std::vector<LucisParser::ExpressionContext*> argExprs;
            if (auto* argList = ctx->argList()) {
                for (auto* exprCtx : argList->expression()) {
                    argExprs.push_back(exprCtx);
                    args.push_back(castValue(visit(exprCtx)));
                }
            }

            auto* ptrTy = llvm::PointerType::getUnqual(*context_);
            auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);

            for (size_t ai = 0; ai < args.size(); ai++) {
                auto* arg = args[ai];
                auto* argType = arg->getType();
                if (ai < argExprs.size() && resolveExprArrayDims(argExprs[ai]) > 0) {
                    std::cerr << "lucis: function '" << methodName
                              << "' does not accept array arguments directly; "
                                 "use '.toString()' or '.join(...)' before printing\n";
                    continue;
                }

                const TypeInfo* argTypeInfo = nullptr;
                if (auto* load = llvm::dyn_cast<llvm::LoadInst>(arg)) {
                    auto* src = load->getPointerOperand();
                    for (auto& [vname, info] : locals_) {
                        if (info.alloca == src) { argTypeInfo = info.typeInfo; break; }
                    }
                    if (argTypeInfo && argTypeInfo->kind == TypeKind::Union)
                        argTypeInfo = nullptr;
                }

                bool isString = (argTypeInfo && argTypeInfo->kind == TypeKind::String)
                             || (!argTypeInfo && argType->isStructTy());
                if (isString) {
                    auto cFuncName = moduleResolver.resolve(methodName, "str");
                    auto* strPtr = builder_->CreateExtractValue(arg, 0, "str_ptr");
                    auto* strLen = builder_->CreateExtractValue(arg, 1, "str_len");
                    auto callee = declareBuiltin(cFuncName,
                                                 llvm::Type::getVoidTy(*context_),
                                                 { ptrTy, usizeTy });
                    builder_->CreateCall(callee, { strPtr, strLen });
                    continue;
                }

                std::string suffix;
                if (argTypeInfo && !argTypeInfo->builtinSuffix.empty())
                                                       suffix = argTypeInfo->builtinSuffix;
                else if (argType->isIntegerTy(1))      suffix = "bool";
                else if (argType->isIntegerTy(8))      suffix = "i8";
                else if (argType->isIntegerTy(16))     suffix = "i16";
                else if (argType->isIntegerTy(32))     suffix = "i32";
                else if (argType->isIntegerTy(64))     suffix = "i64";
                else if (argType->isIntegerTy(128))    suffix = "i128";
                else if (argType->isFloatTy())         suffix = "f32";
                else if (argType->isDoubleTy())        suffix = "f64";
                else if (argType->isPointerTy())       suffix = "ptr";
                else                                   suffix = "i32";

                auto cFuncName = moduleResolver.resolve(methodName, suffix);
                llvm::Value* callArg = arg;
                llvm::Type* paramType = argType;
                if (argType->isIntegerTy(256)) {
                    if (suffix == "i128")
                        paramType = llvm::Type::getInt128Ty(*context_);
                    else if (suffix == "i64")
                        paramType = llvm::Type::getInt64Ty(*context_);
                    else
                        paramType = llvm::Type::getInt32Ty(*context_);
                    callArg = builder_->CreateTrunc(arg, paramType);
                }

                auto callee = declareBuiltin(cFuncName,
                                             llvm::Type::getVoidTy(*context_),
                                             { paramType });
                builder_->CreateCall(callee, { callArg });
            }

            return llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_));
        }

        if (methodName == "sprintf") {
            auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
            auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
            auto* strTy   = typeRegistry_.lookup("string")
                                ->toLLVMType(*context_, module_->getDataLayout());

            std::vector<llvm::Value*> args;
            if (auto* argList = ctx->argList()) {
                for (auto* exprCtx : argList->expression())
                    args.push_back(castValue(visit(exprCtx)));
            }

            if (args.empty()) {
                std::cerr << "lucis: sprintf requires at least a format string\n";
                return static_cast<llvm::Value*>(
                    llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
            }

            auto* fmtPtr = builder_->CreateExtractValue(args[0], 0, "fmt_ptr");
            auto* fmtLen = builder_->CreateExtractValue(args[0], 1, "fmt_len");

            auto* sliceTy = llvm::StructType::get(*context_, {ptrTy, usizeTy});
            auto* i64Ty = llvm::Type::getInt64Ty(*context_);

            llvm::Value* argsPtr;
            llvm::Value* argsCount;

            if (args.size() <= 1) {
                argsPtr   = llvm::ConstantPointerNull::get(ptrTy);
                argsCount = llvm::ConstantInt::get(usizeTy, 0);
            } else {
                size_t numArgs = args.size() - 1;
                auto* arrTy = llvm::ArrayType::get(sliceTy, numArgs);
                auto* arrAlloca = builder_->CreateAlloca(arrTy, nullptr, "sprintf.args");
                auto* zero = llvm::ConstantInt::get(i64Ty, 0);

                for (size_t i = 0; i < numArgs; i++) {
                    auto* arg = args[i + 1];
                    auto* argTy = arg->getType();

                    if (argTy != sliceTy) {
                        llvm::Value* converted = nullptr;
                        if (argTy->isIntegerTy(1)) {
                            auto* trueStr  = builder_->CreateGlobalString("true", ".str.true", 0, module_);
                            auto* falseStr = builder_->CreateGlobalString("false", ".str.false", 0, module_);
                            auto* trueLen  = llvm::ConstantInt::get(usizeTy, 4);
                            auto* falseLen = llvm::ConstantInt::get(usizeTy, 5);
                            auto* ptr = builder_->CreateSelect(arg, trueStr, falseStr);
                            auto* len = builder_->CreateSelect(arg, trueLen, falseLen);
                            converted = llvm::UndefValue::get(sliceTy);
                            converted = builder_->CreateInsertValue(converted, ptr, 0);
                            converted = builder_->CreateInsertValue(converted, len, 1);
                        } else if (argTy->isIntegerTy()) {
                            auto* ext = builder_->CreateIntCast(arg, i64Ty, true, "icast");
                            auto fn = declareBuiltin("lucis_itoa", sliceTy, {i64Ty});
                            converted = builder_->CreateCall(fn, {ext}, "itoa");
                        } else if (argTy->isFloatingPointTy()) {
                            auto* dblTy = llvm::Type::getDoubleTy(*context_);
                            auto* ext = argTy->isFloatTy()
                                ? builder_->CreateFPExt(arg, dblTy, "fext")
                                : arg;
                            auto fn = declareBuiltin("lucis_ftoa", sliceTy, {dblTy});
                            converted = builder_->CreateCall(fn, {ext}, "ftoa");
                        } else if (argTy->isIntegerTy(8)) {
                            auto* buf = builder_->CreateAlloca(
                                llvm::Type::getInt8Ty(*context_), nullptr, "chbuf");
                            builder_->CreateStore(arg, buf);
                            converted = llvm::UndefValue::get(sliceTy);
                            converted = builder_->CreateInsertValue(converted, buf, 0);
                            converted = builder_->CreateInsertValue(converted,
                                llvm::ConstantInt::get(usizeTy, 1), 1);
                        } else {
                            converted = arg;
                        }
                        arg = converted;
                    }

                    auto* idx = llvm::ConstantInt::get(i64Ty, i);
                    auto* gep = builder_->CreateGEP(arrTy, arrAlloca, {zero, idx});
                    builder_->CreateStore(arg, gep);
                }

                argsPtr = builder_->CreateGEP(arrTy, arrAlloca,
                             {zero, llvm::ConstantInt::get(i64Ty, 0)}, "args_ptr");
                argsCount = llvm::ConstantInt::get(usizeTy, numArgs);
            }

            auto callee = declareBuiltin("lucis_sprintf", strTy,
                {ptrTy, usizeTy, ptrTy, usizeTy});
            auto* ret = builder_->CreateCall(callee,
                {fmtPtr, fmtLen, argsPtr, argsCount}, "sprintf");

            auto* retPtr = builder_->CreateExtractValue(ret, 0, "sprintf_ptr");
            auto* retLen = builder_->CreateExtractValue(ret, 1, "sprintf_len");
            llvm::Value* strStruct = llvm::UndefValue::get(strTy);
            strStruct = builder_->CreateInsertValue(strStruct, retPtr, 0);
            strStruct = builder_->CreateInsertValue(strStruct, retLen, 1);
            return strStruct;
        }

        return nullptr;
    };

    if (ids.size() > 2) {
        auto rootName = ids.front()->getText();
        auto rootImport = userImports_.find(rootName);
        if (rootImport == userImports_.end() || rootImport->second != rootName) {
            std::cerr << "lucis: namespace root '" << rootName
                      << "' is not imported; add 'use " << rootName << ";'\n";
            return static_cast<llvm::Value*>(
                llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
        }

        std::string modulePath;
        for (size_t i = 0; i + 1 < ids.size(); ++i) {
            if (!modulePath.empty()) modulePath += "::";
            modulePath += ids[i]->getText();
        }

        if (!ImportResolver::isStdModule(modulePath)) {
            // Qualified static method call on user type: LIB::User::new(args)
            if (ids.size() == 3) {
                auto nsName = ids[0]->getText();
                auto typeName = ids[1]->getText();
                auto methodName2 = ids[2]->getText();

                auto* ti = typeRegistry_.lookup(typeName);
                if (!ti || (ti->kind != TypeKind::Struct && ti->kind != TypeKind::Union)) {
                    std::cerr << "lucis: '" << nsName << "::" << typeName
                              << "' does not support static methods\n";
                    return static_cast<llvm::Value*>(
                        llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
                }

                auto smIt = staticStructMethods_.find(typeName);
                if (smIt == staticStructMethods_.end()) {
                    std::cerr << "lucis: type '" << nsName << "::" << typeName
                              << "' has no static methods\n";
                    return static_cast<llvm::Value*>(
                        llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
                }

                auto mIt = smIt->second.find(methodName2);
                if (mIt == smIt->second.end()) {
                    std::cerr << "lucis: type '" << nsName << "::" << typeName
                              << "' has no static method '" << methodName2 << "'\n";
                    return static_cast<llvm::Value*>(
                        llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
                }

                auto* fn = mIt->second.fn;
                std::vector<llvm::Value*> callArgs;
                std::vector<LucisParser::ExpressionContext*> argExprs;
                if (auto* argList = ctx->argList()) {
                    argExprs = argList->expression();
                    auto* fnType = fn->getFunctionType();
                    size_t paramIdx = 0;
                    for (auto* argExpr : argExprs) {
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
                    for (auto* e : argExprs) consumeExprIfOwnedLocal(e);
                    return static_cast<llvm::Value*>(
                        llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
                }
                auto* result = builder_->CreateCall(fn, callArgs, "qualified_static_call");
                for (auto* e : argExprs) consumeExprIfOwnedLocal(e);
                return static_cast<llvm::Value*>(result);
            }

            std::cerr << "lucis: unsupported qualified static call '"
                      << modulePath << "::" << methodName << "'\n";
            return static_cast<llvm::Value*>(
                llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
        }

        if (auto* moduleResult = emitStdModuleCall(modulePath)) {
            return static_cast<llvm::Value*>(moduleResult);
        }
    }

    auto importIt = userImports_.find(structName);
    if (importIt != userImports_.end()) {
        if (ImportResolver::isStdModule(importIt->second)) {
            if (auto* moduleResult = emitStdModuleCall(importIt->second)) {
                return static_cast<llvm::Value*>(moduleResult);
            }
        } else if (moduleRegistry_) {
            auto* sym = moduleRegistry_->findSymbol(importIt->second, methodName);
            if (sym && sym->kind == ExportedSymbol::Function) {
                auto* funcDecl = static_cast<LucisParser::FunctionDeclContext*>(sym->decl);
                auto mangledName = methodName;
                if (sym->isStdlib)
                    mangledName = ModuleRegistry::mangle(importIt->second, methodName);
                if (!module_->getFunction(mangledName))
                    declareExternFunction(mangledName, funcDecl);

                auto* fn = module_->getFunction(mangledName);
                if (!fn) {
                    std::cerr << "lucis: module '" << importIt->second
                              << "' does not export callable symbol '" << methodName << "'\n";
                    return static_cast<llvm::Value*>(
                        llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
                }

                std::vector<llvm::Value*> callArgs;
                std::vector<LucisParser::ExpressionContext*> modArgExprs;
                if (auto* argList = ctx->argList()) {
                    modArgExprs = argList->expression();
                    for (auto* argExpr : modArgExprs)
                        callArgs.push_back(castValue(visit(argExpr)));
                }

                if (fn->getReturnType()->isVoidTy()) {
                    builder_->CreateCall(fn, callArgs);
                    for (auto* e : modArgExprs) consumeExprIfOwnedLocal(e);
                    return static_cast<llvm::Value*>(
                        llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
                }
                auto* modResult = builder_->CreateCall(fn, callArgs, "static_call");
                for (auto* e : modArgExprs) consumeExprIfOwnedLocal(e);
                return static_cast<llvm::Value*>(modResult);
            }
        }
    }

    std::vector<const TypeInfo*> argTypes;
    if (auto* argList = ctx->argList()) {
        for (auto* argExpr : argList->expression())
            argTypes.push_back(resolveExprTypeInfo(argExpr));
    }

    if (!typeRegistry_.lookup(structName))
        syncTypeFromSemanticDB(structName);
    if (auto* enumType = typeRegistry_.lookup(structName);
        enumType && enumType->kind == TypeKind::Enum) {
        const EnumVariantInfo* variantInfo = nullptr;
        for (const auto& info : enumType->enumVariantInfos) {
            if (info.name == methodName) {
                variantInfo = &info;
                break;
            }
        }
        if (!variantInfo) {
            std::cerr << "lucis: enum '" << structName
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
                            arrLit, expectedTI, "enum_tuple_vec_payload"));
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

    // Generic enum constructor inside instantiated generic function
    // (e.g. Result::Ok(sum) with active currentGenericSubst_ for T).
    auto genEnumIt = genericEnumTemplates_.find(structName);
    if (genEnumIt != genericEnumTemplates_.end() && !currentGenericSubst_.empty()) {
        std::vector<const TypeInfo*> inferredArgs;
        for (const auto& tp : genEnumIt->second.typeParams) {
            auto it = currentGenericSubst_.find(tp);
            if (it == currentGenericSubst_.end() || !it->second) {
                std::cerr << "lucis: cannot infer type arguments for generic enum '"
                          << structName << "'\n";
                return static_cast<llvm::Value*>(
                    llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
            }
            inferredArgs.push_back(it->second);
        }

        auto* enumType = instantiateGenericEnum(structName, genEnumIt->second, inferredArgs);
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
                            arrLit, expectedTI, "inferred_enum_tuple_vec_payload"));
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

    auto extIt = genericExtendTemplates_.find(structName);
    auto structIt = genericStructTemplates_.find(structName);
    if (extIt != genericExtendTemplates_.end() && structIt != genericStructTemplates_.end()) {
        for (auto* method : extIt->second.decl->extendMethod()) {
            if (method->AMPERSAND()) continue;
            if (method->name->getText() != methodName) continue;

            std::vector<LucisParser::ParamContext*> formalParams;
            if (auto* paramList = method->paramList())
                formalParams = paramList->param();

            auto inferred = inferGenericTypeArgs(extIt->second.typeParams, formalParams, argTypes);
            if (!inferred) break;

            auto* instanceTI = instantiateGenericStruct(structName, structIt->second, *inferred);
            if (!instanceTI) break;

            auto smIt = staticStructMethods_.find(instanceTI->name);
            if (smIt == staticStructMethods_.end()) break;

            auto mIt = smIt->second.find(methodName);
            if (mIt == smIt->second.end()) break;

            auto* fn = mIt->second.fn;
            std::vector<llvm::Value*> callArgs;
            std::vector<LucisParser::ExpressionContext*> genArgExprs;
            if (auto* argList = ctx->argList()) {
                genArgExprs = argList->expression();
                auto* fnType = fn->getFunctionType();
                size_t paramIdx = 0;
                for (auto* argExpr : genArgExprs) {
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
                for (auto* e : genArgExprs) consumeExprIfOwnedLocal(e);
                return static_cast<llvm::Value*>(
                    llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
            }
            auto* genResult = builder_->CreateCall(fn, callArgs, "static_call");
            for (auto* e : genArgExprs) consumeExprIfOwnedLocal(e);
            return static_cast<llvm::Value*>(genResult);
        }
    }

    auto smIt = staticStructMethods_.find(structName);
    if (smIt != staticStructMethods_.end()) {
        auto mIt = smIt->second.find(methodName);
        if (mIt != smIt->second.end()) {
            auto* fn = mIt->second.fn;
            // Collect arguments
            std::vector<llvm::Value*> callArgs;
            std::vector<LucisParser::ExpressionContext*> argExprs;
            if (auto* argList = ctx->argList()) {
                argExprs = argList->expression();
                auto* fnType = fn->getFunctionType();
                size_t paramIdx = 0;
                for (auto* argExpr : argExprs) {
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
            auto* genResult = builder_->CreateCall(fn, callArgs, "static_call");
            return static_cast<llvm::Value*>(genResult);
        }
    }

    // Trait impl static method fallback
    const TypeInfo* structTI = typeRegistry_.lookup(structName);
    if (structTI) {
        for (const auto& traitName : structTI->implementedTraits) {
            std::string fnName = mangleImplMethodName(traitName, structName, methodName);
            auto it = loweredImplMethods_.find(fnName);
            if (it == loweredImplMethods_.end()) continue;
            auto* fn = it->second;
            std::vector<llvm::Value*> callArgs;
            if (auto* argList = ctx->argList()) {
                for (auto* argExpr : argList->expression())
                    callArgs.push_back(castValue(visit(argExpr)));
            }
            auto* retTy = fn->getReturnType();
            if (retTy->isVoidTy()) {
                builder_->CreateCall(fn, callArgs);
                return static_cast<llvm::Value*>(
                    llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
            }
            auto* genResult = builder_->CreateCall(fn, callArgs, "trait_static");
            return static_cast<llvm::Value*>(genResult);
        }
    }

    std::cerr << "lucis: struct '" << structName << "' has no static methods\n";
    return static_cast<llvm::Value*>(
        llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
}



std::any IRGen::visitNullLitExpr(LucisParser::NullLitExprContext* /*ctx*/) {
    return static_cast<llvm::Value*>(
        llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*context_)));
}



std::any IRGen::visitAddrOfExpr(LucisParser::AddrOfExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto* innerExpr = ctx->expression();

    // Case 1: &variable
    if (auto* ident = dynamic_cast<LucisParser::IdentExprContext*>(innerExpr)) {
        auto varName = ident->IDENTIFIER()->getText();
        auto it = locals_.find(varName);
        if (it == locals_.end()) {
            std::cerr << "lucis: undefined variable '" << varName << "'\n";
            return static_cast<llvm::Value*>(
                llvm::UndefValue::get(llvm::PointerType::getUnqual(*context_)));
        }
        return static_cast<llvm::Value*>(it->second.alloca);
    }

    // Case 2: &base[i] — address of indexed element for array or pointer bases
    if (auto* idx = dynamic_cast<LucisParser::IndexExprContext*>(innerExpr)) {
        std::vector<LucisParser::ExpressionContext*> indexExprs;
        auto* current = static_cast<LucisParser::ExpressionContext*>(idx);
        while (auto* idxCtx = dynamic_cast<LucisParser::IndexExprContext*>(current)) {
            indexExprs.push_back(idxCtx->expression(1));
            current = idxCtx->expression(0);
        }
        auto* identBase = dynamic_cast<LucisParser::IdentExprContext*>(current);
        if (identBase) {
            auto varName = identBase->IDENTIFIER()->getText();
            auto it = locals_.find(varName);
            if (it != locals_.end()) {
                auto* alloca   = it->second.alloca;
                auto* allocType = alloca->getAllocatedType();
                auto* ti       = it->second.typeInfo;
                auto* i64Ty    = llvm::Type::getInt64Ty(*context_);

                std::reverse(indexExprs.begin(), indexExprs.end());

                if (ti && ti->kind == TypeKind::Pointer && ti->pointeeType) {
                    auto* ptrTy = llvm::PointerType::getUnqual(*context_);
                    auto* ptrVal = builder_->CreateLoad(ptrTy, alloca, varName + "_ptr");
                    auto* elemTy = ti->pointeeType->toLLVMType(*context_, module_->getDataLayout());
                    auto* idxVal = castValue(visit(indexExprs[0]));
                    if (idxVal->getType() != i64Ty)
                        idxVal = builder_->CreateIntCast(idxVal, i64Ty, true);

                    auto* elemPtr = builder_->CreateGEP(elemTy, ptrVal, idxVal,
                                                         "addr_of_elem");
                    return static_cast<llvm::Value*>(elemPtr);
                }

                std::vector<llvm::Value*> gepIndices;
                gepIndices.push_back(llvm::ConstantInt::get(i64Ty, 0));
                for (auto* idxExpr : indexExprs) {
                    auto* idxVal = castValue(visit(idxExpr));
                    if (idxVal->getType() != i64Ty)
                        idxVal = builder_->CreateIntCast(idxVal, i64Ty, true);
                    gepIndices.push_back(idxVal);
                }

                auto* elemPtr = builder_->CreateGEP(allocType, alloca, gepIndices,
                                                     "addr_of_elem");
                return static_cast<llvm::Value*>(elemPtr);
            }
        }
    }

    // Case 3: &s.field — address of struct field
    if (auto* field = dynamic_cast<LucisParser::FieldAccessExprContext*>(innerExpr)) {
        auto fieldName = field->IDENTIFIER()->getText();
        auto* base = field->expression();
        auto* ident = dynamic_cast<LucisParser::IdentExprContext*>(base);
        if (ident) {
            auto varName = ident->IDENTIFIER()->getText();
            auto it = locals_.find(varName);
            if (it != locals_.end() && it->second.typeInfo) {
                auto* structTI = it->second.typeInfo;
                int fieldIdx = -1;
                for (size_t i = 0; i < structTI->fields.size(); i++) {
                    if (structTI->fields[i].name == fieldName) {
                        fieldIdx = static_cast<int>(i);
                        break;
                    }
                }
                if (fieldIdx >= 0) {
                    auto* structTy = it->second.alloca->getAllocatedType();
                    auto* gep = builder_->CreateStructGEP(structTy, it->second.alloca,
                                                          static_cast<unsigned>(fieldIdx),
                                                          "addr_of_field");
                    return static_cast<llvm::Value*>(gep);
                }
            }
        }
    }
    // Case 4: &ptr->field — address of struct field via pointer
    if (auto* arrow = dynamic_cast<LucisParser::ArrowAccessExprContext*>(innerExpr)) {
        auto fieldName = arrow->IDENTIFIER()->getText();
        auto* base = arrow->expression();
        auto* ptrVal = castValue(visit(base));
        auto* exprTI = resolveExprTypeInfo(base);
        if (exprTI && exprTI->kind == TypeKind::Pointer && exprTI->pointeeType) {
            auto* structTI = exprTI->pointeeType;
            int fieldIdx = -1;
            for (size_t i = 0; i < structTI->fields.size(); i++) {
                if (structTI->fields[i].name == fieldName) {
                    fieldIdx = static_cast<int>(i);
                    break;
                }
            }
            if (fieldIdx >= 0) {
                auto* structLLTy = llvm::StructType::getTypeByName(*context_, structTI->name);
                if (!structLLTy)
                    structLLTy = static_cast<llvm::StructType*>(
                        structTI->toLLVMType(*context_, module_->getDataLayout()));
                auto* gep = builder_->CreateStructGEP(structLLTy, ptrVal,
                                                      static_cast<unsigned>(fieldIdx),
                                                      "addr_of_arrow_field");
                return static_cast<llvm::Value*>(gep);
            }
        }
    }
    std::cerr << "lucis: unsupported expression for address-of operator\n";
    return static_cast<llvm::Value*>(
        llvm::UndefValue::get(llvm::PointerType::getUnqual(*context_)));
}



std::any IRGen::visitDerefExpr(LucisParser::DerefExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto* ptrVal = castValue(visit(ctx->expression()));

    // Find pointee type: first try AST-level resolution, then fall back to IR
    const TypeInfo* pointeeTI = nullptr;
    auto* exprTI = resolveExprTypeInfo(ctx->expression());
    if (exprTI && exprTI->kind == TypeKind::Pointer) {
        pointeeTI = exprTI->pointeeType;
    }

    // Fallback: inspect the LLVM IR value
    if (!pointeeTI) {
        if (auto* load = llvm::dyn_cast<llvm::LoadInst>(ptrVal)) {
            auto* src = load->getPointerOperand();
            for (auto& [vname, info] : locals_) {
                if (info.alloca == src && info.typeInfo->kind == TypeKind::Pointer) {
                    pointeeTI = info.typeInfo->pointeeType;
                    break;
                }
            }
        }
    }

    if (!pointeeTI) {
        std::cerr << "lucis: cannot dereference non-pointer expression\n";
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }

    auto* pointeeLLVM = pointeeTI->toLLVMType(*context_, module_->getDataLayout());
    auto* load = builder_->CreateLoad(pointeeLLVM, ptrVal, "deref");
    if (pointeeTI->isVolatile) load->setVolatile(true);
    return static_cast<llvm::Value*>(load);
}


