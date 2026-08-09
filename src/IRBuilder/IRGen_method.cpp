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
llvm::Value* IRGen::resolveMethodReceiverAddress(LucisParser::ExpressionContext* expr) {
    if (!expr)
        return nullptr;
    if (auto* paren = dynamic_cast<LucisParser::ParenExprContext*>(expr))
        return resolveMethodReceiverAddress(paren->expression());
    if (auto* ident = dynamic_cast<LucisParser::IdentExprContext*>(expr)) {
        auto it = locals_.find(ident->IDENTIFIER()->getText());
        if (it == locals_.end() || !it->second.typeInfo)
            return nullptr;
        auto* ti = it->second.typeInfo;
        if (ti->kind == TypeKind::Pointer)
            return builder_->CreateLoad(llvm::PointerType::getUnqual(*context_),
                                        it->second.alloca,
                                        ident->IDENTIFIER()->getText() + "_recvptr");
        return it->second.alloca;
    }
    if (auto* deref = dynamic_cast<LucisParser::DerefExprContext*>(expr))
        return castValue(visit(deref->expression()));
    if (auto* field = dynamic_cast<LucisParser::FieldAccessExprContext*>(expr)) {
        auto* baseExpr = field->expression();
        auto* basePtr = resolveMethodReceiverAddress(baseExpr);
        if (!basePtr)
            return nullptr;
        auto* baseTI = resolveExprTypeInfo(baseExpr);
        if (!baseTI)
            return nullptr;
        if (baseTI->kind == TypeKind::Pointer && baseTI->pointeeType)
            baseTI = baseTI->pointeeType;
        if (!baseTI || (baseTI->kind != TypeKind::Struct && baseTI->kind != TypeKind::Union))
            return nullptr;
        int fieldIdx = -1;
        for (size_t i = 0; i < baseTI->fields.size(); i++) {
            if (baseTI->fields[i].name == field->IDENTIFIER()->getText()) {
                fieldIdx = static_cast<int>(i);
                break;
            }
        }
        if (fieldIdx < 0)
            return nullptr;
        auto* structTy = llvm::dyn_cast<llvm::StructType>(
            baseTI->toLLVMType(*context_, module_->getDataLayout()));
        if (!structTy)
            return nullptr;
        if (baseTI->kind == TypeKind::Union)
            return basePtr;
        return builder_->CreateStructGEP(structTy, basePtr,
                                         static_cast<unsigned>(fieldIdx),
                                         field->IDENTIFIER()->getText() + "_recvptr");
    }
    if (auto* arrow = dynamic_cast<LucisParser::ArrowAccessExprContext*>(expr)) {
        auto* baseExpr = arrow->expression();
        auto* basePtr = resolveMethodReceiverAddress(baseExpr);
        if (!basePtr)
            basePtr = castValue(visit(baseExpr));
        if (!basePtr)
            return nullptr;
        auto* baseTI = resolveExprTypeInfo(baseExpr);
        if (!baseTI)
            return nullptr;
        if (baseTI->kind == TypeKind::Pointer && baseTI->pointeeType)
            baseTI = baseTI->pointeeType;
        if (!baseTI || (baseTI->kind != TypeKind::Struct && baseTI->kind != TypeKind::Union))
            return nullptr;
        int fieldIdx = -1;
        for (size_t i = 0; i < baseTI->fields.size(); i++) {
            if (baseTI->fields[i].name == arrow->IDENTIFIER()->getText()) {
                fieldIdx = static_cast<int>(i);
                break;
            }
        }
        if (fieldIdx < 0)
            return nullptr;
        auto* structTy = llvm::dyn_cast<llvm::StructType>(
            baseTI->toLLVMType(*context_, module_->getDataLayout()));
        if (!structTy)
            return nullptr;
        if (baseTI->kind == TypeKind::Union)
            return basePtr;
        return builder_->CreateStructGEP(structTy, basePtr,
                                         static_cast<unsigned>(fieldIdx),
                                         arrow->IDENTIFIER()->getText() + "_recvptr");
    }
    return nullptr;
}
// ── FFI: extern function declarations ─────────────────────────────────────────
std::any
IRGen::visitMethodCallExpr(LucisParser::MethodCallExprContext* ctx) {
    auto methodName = ctx->IDENTIFIER()->getText(); // X
    // Resolve receiver type info BEFORE evaluating receiver
    const TypeInfo* receiverTI = nullptr;
    unsigned recvArrayDims = 0;
    auto* baseExpr = ctx->expression();
    std::string receiverVarName;
    if (auto* identBase = dynamic_cast<LucisParser::IdentExprContext*>(baseExpr)) {
        receiverVarName = identBase->IDENTIFIER()->getText();
        auto it = locals_.find(receiverVarName);
        if (it != locals_.end()) {
            receiverTI = it->second.typeInfo;
            recvArrayDims = it->second.arrayDims;
        }
    }
    if (receiverVarName.empty()) {
        auto* rootExpr = baseExpr;
        while (auto* paren = dynamic_cast<LucisParser::ParenExprContext*>(rootExpr))
            rootExpr = paren->expression();
        if (auto* deref = dynamic_cast<LucisParser::DerefExprContext*>(rootExpr)) {
            rootExpr = deref->expression();
            while (auto* paren = dynamic_cast<LucisParser::ParenExprContext*>(rootExpr))
                rootExpr = paren->expression();
        }
        if (auto* identBase = dynamic_cast<LucisParser::IdentExprContext*>(rootExpr)) {
            receiverVarName = identBase->IDENTIFIER()->getText();
            auto it = locals_.find(receiverVarName);
            if (it != locals_.end()) {
                receiverTI = it->second.typeInfo;
                recvArrayDims = it->second.arrayDims;
            }
        }
    }
    // Fallback: resolve type for non-identifier expressions (chained calls, etc.)
    if (!receiverTI)
        receiverTI = resolveExprTypeInfo(baseExpr);
    if (recvArrayDims == 0)
        recvArrayDims = resolveExprArrayDims(baseExpr);
    // Auto-dereference: ptr.method() → pointee.method()
    // Exclude *void (no meaningful pointee methods).
    if (receiverTI && receiverTI->kind == TypeKind::Pointer && receiverTI->pointeeType
        && receiverTI->pointeeType->kind != TypeKind::Void)
        receiverTI = receiverTI->pointeeType;
    // ── Variadic parameter method dispatch ───────────────────────────
    if (!receiverVarName.empty()) {
        auto vit = variadicParams_.find(receiverVarName);
        if (vit != variadicParams_.end()) {
            if (methodName == "len") {
                auto* i64Ty = llvm::Type::getInt64Ty(*context_);
                return static_cast<llvm::Value*>(
                    builder_->CreateLoad(i64Ty, vit->second.lenAlloca, "var_len"));
            }
            std::cerr << "lucis: variadic parameter '" << receiverVarName
                      << "' has no method '" << methodName << "'\n";
            return static_cast<llvm::Value*>(
                llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
        }
    }
    // ── Extended type method dispatch ────────────────────────────────
    if (receiverTI && receiverTI->kind == TypeKind::Extended) {
        auto* extDesc = extTypeRegistry_.lookup(receiverTI->extendedKind);
        if (!extDesc) {
            std::cerr << "lucis: unknown extended type '" << receiverTI->extendedKind << "'\n";
            return static_cast<llvm::Value*>(
                llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
        }
        // Find method descriptor
        const MethodDescriptor* desc = nullptr;
        for (auto& md : extDesc->methods) {
            if (md.name == methodName) { desc = &md; break; }
        }
        if (!desc) {
            std::cerr << "lucis: unknown method '" << methodName
                      << "' on type '" << receiverTI->name << "'\n";
            return static_cast<llvm::Value*>(
                llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
        }
        // Collect argument values
        std::vector<llvm::Value*> args;
        std::vector<LucisParser::ExpressionContext*> extArgExprs;
        if (auto* argList = ctx->argList()) {
            extArgExprs = argList->expression();
            for (auto* argExpr : extArgExprs) {
                args.push_back(castValue(visit(argExpr)));
            }
        }
        auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
        auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
        // Determine struct type and suffix based on extended kind
        llvm::StructType* extStructTy;
        std::string suffix;
        if (receiverTI->extendedKind == "Map") {
            extStructTy = getOrCreateMapStructType();
            suffix = receiverTI->builtinSuffix; // "keySuffix_valSuffix"
        } else if (receiverTI->extendedKind == "Set") {
            extStructTy = getOrCreateSetStructType();
            if (receiverTI->elementType)
                suffix = getVecSuffix(receiverTI->elementType);
            else
                suffix = "raw";
        } else {
            extStructTy = getOrCreateVecStructType();
            if (receiverTI->elementType)
                suffix = getVecSuffix(receiverTI->elementType);
            else
                suffix = "raw";
        }
        // Map Lucis method names to C runtime function names
        auto cMethodName = methodName;
        if (receiverTI->extendedKind == "Map" && cMethodName == "insert")
            cMethodName = "set";
        auto cFuncName = extDesc->cPrefix + "_" + cMethodName + "_" + suffix;
        // Get the alloca pointer for the receiver variable
        llvm::Value* recvPtr = resolveMethodReceiverAddress(baseExpr);
        if (!recvPtr) {
            std::cerr << "lucis: cannot get address of extended type for method call\n";
            return static_cast<llvm::Value*>(
                llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
        }
        // ── Map.keys() / Map.values() / Set.values() — out-param returning Vec
        if ((receiverTI->extendedKind == "Map" &&
             (methodName == "keys" || methodName == "values")) ||
            (receiverTI->extendedKind == "Set" && methodName == "values")) {
            auto* vecTy     = getOrCreateVecStructType();
            auto* voidTy    = llvm::Type::getVoidTy(*context_);
            auto* outAlloca = builder_->CreateAlloca(vecTy, nullptr,
                                                     methodName + "_out");
            auto callee = declareBuiltin(cFuncName, voidTy, {ptrTy, ptrTy});
            builder_->CreateCall(callee, {recvPtr, outAlloca});
            auto* result = builder_->CreateLoad(vecTy, outAlloca,
                                                methodName + "_val");
            return static_cast<llvm::Value*>(result);
        }
        // ── Vec.clone() — out-param returning a new Vec of the same type
        if (receiverTI->extendedKind == "Vec" && methodName == "clone") {
            auto* vecTy     = getOrCreateVecStructType();
            auto* voidTy    = llvm::Type::getVoidTy(*context_);
            auto* outAlloca = builder_->CreateAlloca(vecTy, nullptr, "clone_out");
            auto callee = declareBuiltin(cFuncName, voidTy, {ptrTy, ptrTy});
            builder_->CreateCall(callee, {recvPtr, outAlloca});
            auto* result = builder_->CreateLoad(vecTy, outAlloca, "clone_val");
            return static_cast<llvm::Value*>(result);
        }
        // Resolve placeholder LLVM types
        llvm::Type* elemLLTy = nullptr;
        llvm::Type* keyLLTy  = nullptr;
        llvm::Type* valLLTy  = nullptr;
        if (receiverTI->elementType)
            elemLLTy = receiverTI->elementType->toLLVMType(*context_, module_->getDataLayout());
        if (receiverTI->keyType)
            keyLLTy = receiverTI->keyType->toLLVMType(*context_, module_->getDataLayout());
        if (receiverTI->valueType)
            valLLTy = receiverTI->valueType->toLLVMType(*context_, module_->getDataLayout());
        // Resolve return type
        llvm::Type* retTy;
        if (desc->returnType == "void")
            retTy = llvm::Type::getVoidTy(*context_);
        else if (desc->returnType == "_self")
            retTy = extStructTy;
        else if (desc->returnType == "_elem")
            retTy = elemLLTy;
        else if (desc->returnType == "_key")
            retTy = keyLLTy;
        else if (desc->returnType == "_val")
            retTy = valLLTy;
        else if (desc->returnType == "_vec_key" || desc->returnType == "_vec_val")
            retTy = getOrCreateVecStructType();
        else if (desc->returnType == "bool")
            retTy = llvm::Type::getInt1Ty(*context_);
        else if (desc->returnType == "usize")
            retTy = usizeTy;
        else if (desc->returnType == "int64")
            retTy = llvm::Type::getInt64Ty(*context_);
        else if (desc->returnType == "float64")
            retTy = llvm::Type::getDoubleTy(*context_);
        else if (desc->returnType == "string") {
            auto* sPtrTy = llvm::PointerType::getUnqual(*context_);
            retTy = llvm::StructType::get(*context_, { sPtrTy, usizeTy });
        } else {
            auto* rTI = typeRegistry_.lookup(desc->returnType);
            retTy = rTI ? rTI->toLLVMType(*context_, module_->getDataLayout())
                        : llvm::Type::getInt32Ty(*context_);
        }
        // For bool-returning methods in C (return int), use i32 for ABI
        bool boolReturn = (desc->returnType == "bool");
        llvm::Type* cRetTy = boolReturn
            ? llvm::Type::getInt32Ty(*context_) : retTy;
        // Build C function argument list: first arg is always receiver ptr
        std::vector<llvm::Value*> callArgs = { recvPtr };
        std::vector<llvm::Type*> paramLLTypes = { ptrTy };
        for (size_t i = 0; i < desc->paramTypes.size(); i++) {
            auto& pt = desc->paramTypes[i];
            llvm::Type* paramTy;
            // For raw element/value types, C function expects const void*
            bool isRawElem = (pt == "_elem" && suffix == "raw");
            bool isRawVal  = (pt == "_val"  && suffix.size() >= 4 &&
                              suffix.substr(suffix.size() - 3) == "raw");
            if (isRawElem || isRawVal)
                paramTy = ptrTy;
            else if (pt == "_elem")       paramTy = elemLLTy;
            else if (pt == "_key")   paramTy = keyLLTy;
            else if (pt == "_val")   paramTy = valLLTy;
            else if (pt == "_self")  paramTy = ptrTy;
            else if (pt == "usize")  paramTy = usizeTy;
            else if (pt == "int32")  paramTy = llvm::Type::getInt32Ty(*context_);
            else if (pt == "string") {
                auto* sPtrTy = llvm::PointerType::getUnqual(*context_);
                paramTy = llvm::StructType::get(*context_, { sPtrTy, usizeTy });
            } else {
                auto* pTI = typeRegistry_.lookup(pt);
                paramTy = pTI ? pTI->toLLVMType(*context_, module_->getDataLayout())
                              : llvm::Type::getInt32Ty(*context_);
            }
            paramLLTypes.push_back(paramTy);
            // Cast argument if needed
            llvm::Value* argVal = args[i];
            if (isRawElem || isRawVal) {
                // Raw struct: alloca, store value, pass pointer
                llvm::Type* rawTy = isRawElem ? elemLLTy : valLLTy;
                auto* tmp = builder_->CreateAlloca(rawTy);
                builder_->CreateStore(argVal, tmp);
                argVal = tmp;
            } else if (pt == "_self") {
                // _self params expect a pointer to the struct, not the struct
                // by value. Try to get the alloca from the argument expression.
                if (auto* argList = ctx->argList()) {
                    auto* argExpr = argList->expression(i);
                    if (auto* identArg = dynamic_cast<LucisParser::IdentExprContext*>(argExpr)) {
                        auto ait = locals_.find(identArg->IDENTIFIER()->getText());
                        if (ait != locals_.end())
                            argVal = ait->second.alloca;
                    }
                }
            } else if (argVal->getType() != paramTy) {
                if (argVal->getType()->isIntegerTy() && paramTy->isIntegerTy())
                    argVal = builder_->CreateIntCast(argVal, paramTy, false);
            }
            callArgs.push_back(argVal);
            // Vec.push_raw / Vec.init_cap_raw need elem_size after the raw ptr
            if (isRawElem && receiverTI->extendedKind == "Vec") {
                auto& dl    = module_->getDataLayout();
                auto  sz    = dl.getTypeAllocSize(elemLLTy);
                callArgs.push_back(llvm::ConstantInt::get(usizeTy, sz));
                paramLLTypes.push_back(usizeTy);
            }
        }
        auto callee = declareBuiltin(cFuncName, cRetTy, paramLLTypes);
        // ── Vec<T> raw accessors (struct elements) ─────────────────────
        // The C runtime has no lucis_vec_at_raw / first_raw / last_raw
        // (C cannot return arbitrary structs by value). Use
        // lucis_vec_ptr_raw + load, matching the for-in and subscript
        // codegen paths. ptr_raw bounds-checks and aborts on OOB.
        if (receiverTI->extendedKind == "Vec" && suffix == "raw" &&
            (methodName == "at" || methodName == "first" || methodName == "last")) {
            auto& dl = module_->getDataLayout();
            auto* elemSzVal = llvm::ConstantInt::get(usizeTy, dl.getTypeAllocSize(elemLLTy));
            llvm::Value* idx = llvm::ConstantInt::get(usizeTy, 0);
            if (methodName == "at") {
                idx = args[0];
                if (idx->getType() != usizeTy)
                    idx = builder_->CreateIntCast(idx, usizeTy, false);
            } else if (methodName == "last") {
                auto lenCallee = declareBuiltin("lucis_vec_len_raw", usizeTy, {ptrTy});
                auto* len = builder_->CreateCall(lenCallee, {recvPtr}, "vec.len");
                idx = builder_->CreateSub(len,
                    llvm::ConstantInt::get(usizeTy, 1), "vec.last.idx");
            }
            auto ptrCallee = declareBuiltin("lucis_vec_ptr_raw", ptrTy, {ptrTy, usizeTy, usizeTy});
            auto* elemPtr = builder_->CreateCall(ptrCallee, {recvPtr, idx, elemSzVal}, "vec_elem_ptr");
            for (auto* e : extArgExprs) consumeExprIfOwnedLocal(e);
            return static_cast<llvm::Value*>(builder_->CreateLoad(elemLLTy, elemPtr, "vec_at"));
        }
        if (retTy->isVoidTy()) {
            builder_->CreateCall(callee, callArgs);
            for (auto* e : extArgExprs) consumeExprIfOwnedLocal(e);
            return static_cast<llvm::Value*>(
                llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
        }
        auto* result = builder_->CreateCall(callee, callArgs, "ext_method");
        // Convert C int → i1 for bool returns
        if (boolReturn)
            return static_cast<llvm::Value*>(
                builder_->CreateICmpNE(result,
                    llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), 0)));
        for (auto* e : extArgExprs) consumeExprIfOwnedLocal(e);
        return static_cast<llvm::Value*>(result);
    }
    // ── Non-extended method call (original path) ─────────────────────
    // ── Struct method via `extend` block (walks parent chain) ────────
    if (receiverTI && receiverTI->kind == TypeKind::Struct && recvArrayDims == 0) {
        auto* me = findMethodEntryInChain(receiverTI, methodName);
        if (me) {
            auto* fn = me->fn;
                // Collect argument values
                std::vector<llvm::Value*> callArgs;
                // First arg: pointer to the struct instance (&self)
                auto* recvPtr = resolveMethodReceiverAddress(baseExpr);
                if (!recvPtr) {
                    // Temporary receiver (e.g. getArg(0).asString()): the
                    // expression returns a value, but &self expects a pointer.
                    // Store the value in a temporary alloca and pass its ptr.
                    auto* receiverVal = castValue(visit(baseExpr));
                    auto* selfParamTy = fn->getFunctionType()->getParamType(0);
                    if (receiverVal->getType() == selfParamTy) {
                        // Already a pointer — pass as-is.
                        callArgs.push_back(receiverVal);
                    } else {
                        auto* tmpSelf = builder_->CreateAlloca(
                            receiverVal->getType(), nullptr, "tmp_self");
                        builder_->CreateStore(receiverVal, tmpSelf);
                        callArgs.push_back(tmpSelf);
                    }
                } else {
                    callArgs.push_back(recvPtr);
                }
                // Remaining args
                std::vector<LucisParser::ExpressionContext*> extArgExprs;
                if (auto* argList = ctx->argList()) {
                    extArgExprs = argList->expression();
                    auto* fnType = fn->getFunctionType();
                    size_t paramIdx = 1; // skip &self
                    for (auto* argExpr : extArgExprs) {
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
                    for (auto* e : extArgExprs) consumeExprIfOwnedLocal(e);
                    return static_cast<llvm::Value*>(
                        llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
                }
                auto* extResult = builder_->CreateCall(fn, callArgs, "method_result");
                for (auto* e : extArgExprs) consumeExprIfOwnedLocal(e);
                return static_cast<llvm::Value*>(extResult);
            }
            // ── Trait method via `impl` block ──────────────────────────
            for (const auto& traitName : receiverTI->implementedTraits) {
                std::string fnName = mangleImplMethodName(
                    traitName, receiverTI->name, methodName);
                auto it = loweredImplMethods_.find(fnName);
                if (it == loweredImplMethods_.end()) continue;
                auto* fn = it->second;
                std::vector<llvm::Value*> callArgs;
                auto* recvPtr = resolveMethodReceiverAddress(baseExpr);
                if (!recvPtr) {
                    auto* receiverVal = castValue(visit(baseExpr));
                    auto* selfParamTy = fn->getFunctionType()->getParamType(0);
                    if (receiverVal->getType() == selfParamTy) {
                        callArgs.push_back(receiverVal);
                    } else {
                        auto* tmpSelf = builder_->CreateAlloca(
                            receiverVal->getType(), nullptr, "tmp_self");
                        builder_->CreateStore(receiverVal, tmpSelf);
                        callArgs.push_back(tmpSelf);
                    }
                } else {
                    callArgs.push_back(recvPtr);
                }
                size_t paramIdx = 1;
                if (auto* al = ctx->argList()) {
                    for (auto* a : al->expression()) {
                        auto* argVal = castValue(visit(a));
                        if (paramIdx < fn->getFunctionType()->getNumParams()) {
                            auto* paramTy = fn->getFunctionType()->getParamType(paramIdx);
                            auto* argTy = argVal->getType();
                            if (argTy != paramTy) {
                                if (argTy->isIntegerTy() && paramTy->isIntegerTy())
                                    argVal = builder_->CreateIntCast(argVal, paramTy, true);
                                else if (argTy->isFloatingPointTy() && paramTy->isFloatingPointTy())
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
                auto* implResult = builder_->CreateCall(fn, callArgs, "trait_result");
                return static_cast<llvm::Value*>(implResult);
            }
        // ── Struct field function pointer call ─────────────────────────
        for (auto& field : receiverTI->fields) {
            if (field.name == methodName && field.typeInfo) {
                auto* fieldTI = field.typeInfo;
                if (fieldTI->kind == TypeKind::Pointer && fieldTI->pointeeType &&
                    fieldTI->pointeeType->kind == TypeKind::Function)
                    fieldTI = fieldTI->pointeeType;
                if (fieldTI->kind != TypeKind::Function) continue;
                // Get pointer to the struct instance
                auto* structPtr = resolveMethodReceiverAddress(baseExpr);
                llvm::Value* fnPtr = nullptr;
                if (structPtr) {
                    auto* structLLTy = llvm::StructType::getTypeByName(*context_,
                        receiverTI->name);
                    if (!structLLTy)
                        structLLTy = static_cast<llvm::StructType*>(
                            receiverTI->toLLVMType(*context_, module_->getDataLayout()));
                    unsigned fIdx = static_cast<unsigned>(&field - &receiverTI->fields[0]);
                    auto* fieldGEP = builder_->CreateStructGEP(structLLTy, structPtr,
                        fIdx, methodName + "_gep");
                    auto* ptrTy = llvm::PointerType::getUnqual(*context_);
                    fnPtr = builder_->CreateLoad(ptrTy, fieldGEP, methodName + "_fnptr");
                } else {
                    auto* structVal = castValue(visit(baseExpr));
                    unsigned fIdx = static_cast<unsigned>(&field - &receiverTI->fields[0]);
                    fnPtr = builder_->CreateExtractValue(structVal, {fIdx},
                        methodName + "_fnptr");
                }
                // Build LLVM function type from TypeInfo
                auto* retLLVM = fieldTI->returnType->toLLVMType(*context_,
                    module_->getDataLayout());
                std::vector<llvm::Type*> paramLLVM;
                for (auto* pti : fieldTI->paramTypes)
                    paramLLVM.push_back(pti->toLLVMType(*context_,
                        module_->getDataLayout()));
                auto* fnType = llvm::FunctionType::get(retLLVM, paramLLVM,
                    fieldTI->isVariadic);
                // Collect arguments
                std::vector<llvm::Value*> callArgs;
                std::vector<LucisParser::ExpressionContext*> argExprs;
                if (auto* argList = ctx->argList()) {
                    argExprs = argList->expression();
                    for (size_t i = 0; i < argExprs.size(); i++) {
                        auto* argVal = castValue(visit(argExprs[i]));
                        if (i < paramLLVM.size() && argVal->getType() != paramLLVM[i]) {
                            if (argVal->getType()->isIntegerTy() && paramLLVM[i]->isIntegerTy())
                                argVal = builder_->CreateIntCast(argVal, paramLLVM[i], true);
                            else if (argVal->getType()->isFloatingPointTy() &&
                                     paramLLVM[i]->isFloatingPointTy())
                                argVal = builder_->CreateFPCast(argVal, paramLLVM[i]);
                        }
                        callArgs.push_back(argVal);
                    }
                }
                bool isVoidFn = retLLVM && retLLVM->isVoidTy();
                auto* result = isVoidFn
                    ? builder_->CreateCall(
                        llvm::FunctionCallee(fnType, fnPtr), callArgs)
                    : builder_->CreateCall(
                        llvm::FunctionCallee(fnType, fnPtr), callArgs, "fncall");
                for (auto* e : argExprs) consumeExprIfOwnedLocal(e);
                if (isVoidFn)
                    return static_cast<llvm::Value*>(
                        llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
                return static_cast<llvm::Value*>(result);
            }
        }
    }
    // ── String method dispatch ───────────────────────────────────────
    if (receiverTI && receiverTI->kind == TypeKind::String && recvArrayDims == 0) {
        auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
        auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
        auto* i1Ty    = llvm::Type::getInt1Ty(*context_);
        auto* i8Ty    = llvm::Type::getInt8Ty(*context_);
        auto* i32Ty   = llvm::Type::getInt32Ty(*context_);
        auto* i64Ty   = llvm::Type::getInt64Ty(*context_);
        auto* strTy   = llvm::StructType::get(*context_, {ptrTy, usizeTy});
        llvm::Value* strVal = nullptr;
        if (!receiverVarName.empty()) {
            auto recvIt = locals_.find(receiverVarName);
            if (recvIt != locals_.end()) {
                strVal = builder_->CreateLoad(strTy, recvIt->second.alloca,
                                              receiverVarName + "_strval");
            }
        }
        if (!strVal) {
            strVal = castValue(visit(baseExpr));
        }
        if (strVal) {
            auto* strPtr = builder_->CreateExtractValue(strVal, 0, "str_ptr");
            auto* strLen = builder_->CreateExtractValue(strVal, 1, "str_len");
            bool cleanupTempReceiver = !dynamic_cast<LucisParser::IdentExprContext*>(baseExpr) &&
                                       !isBorrowedStringExpr(baseExpr);
            auto cleanupReceiverIfTemp = [&]() {
                if (!cleanupTempReceiver) return;
                auto freeCallee = declareBuiltin("lucis_freeStr", llvm::Type::getVoidTy(*context_),
                                                 {ptrTy, usizeTy});
                builder_->CreateCall(freeCallee, {strPtr, strLen});
                cleanupTempReceiver = false;
            };
            // Collect extra arguments
            std::vector<llvm::Value*> mArgs;
            if (auto* argList = ctx->argList())
                for (auto* e : argList->expression())
                    mArgs.push_back(castValue(visit(e)));
            // Helper: rebuild string struct from C return value { ptr, len }
            auto buildStr = [&](llvm::Value* ret) -> llvm::Value* {
                auto* rPtr = builder_->CreateExtractValue(ret, 0, "sret_ptr");
                auto* rLen = builder_->CreateExtractValue(ret, 1, "sret_len");
                llvm::Value* s = llvm::UndefValue::get(strTy);
                s = builder_->CreateInsertValue(s, rPtr, 0);
                s = builder_->CreateInsertValue(s, rLen, 1);
                return s;
            };
            // Helper: expand string fat-pointer arg into (ptr, len)
            auto extractStr = [&](llvm::Value* sv, std::vector<llvm::Value*>& out) {
                out.push_back(builder_->CreateExtractValue(sv, 0, "arg_ptr"));
                out.push_back(builder_->CreateExtractValue(sv, 1, "arg_len"));
            };
            if (methodName == "len") {
                return static_cast<llvm::Value*>(strLen);
            }
            if (methodName == "ptr") {
                // returns a ptr
                return static_cast<llvm::Value*>(strPtr);
            }
            if (methodName == "isEmpty") {
                auto* zero = llvm::ConstantInt::get(usizeTy, 0);
                return static_cast<llvm::Value*>(
                    builder_->CreateICmpEQ(strLen, zero, "str_empty"));
            }
            if (methodName == "toString") {
                return static_cast<llvm::Value*>(strVal);
            }
            if (methodName == "front") {
                auto* elem = builder_->CreateGEP(i8Ty, strPtr,
                    llvm::ConstantInt::get(usizeTy, 0), "front_ptr");
                return static_cast<llvm::Value*>(builder_->CreateLoad(i8Ty, elem, "front"));
            }
            if (methodName == "back") {
                auto* idx = builder_->CreateSub(strLen,
                    llvm::ConstantInt::get(usizeTy, 1), "back_idx");
                auto* elem = builder_->CreateGEP(i8Ty, strPtr, idx, "back_ptr");
                return static_cast<llvm::Value*>(builder_->CreateLoad(i8Ty, elem, "back"));
            }
            if (methodName == "at") {
                auto* idx = mArgs[0];
                if (idx->getType() != usizeTy)
                    idx = builder_->CreateIntCast(idx, usizeTy, false);
                auto* elem = builder_->CreateGEP(i8Ty, strPtr, idx, "at_ptr");
                return static_cast<llvm::Value*>(builder_->CreateLoad(i8Ty, elem, "at"));
            }
            // (receiver, string) → bool
            if (methodName == "contains" || methodName == "startsWith" ||
                methodName == "endsWith") {
                static const std::unordered_map<std::string, std::string> boolFns = {
                    {"contains",   "lucis_contains"},
                    {"startsWith", "lucis_startsWith"},
                    {"endsWith",   "lucis_endsWith"},
                };
                std::vector<llvm::Value*> callArgs = {strPtr, strLen};
                extractStr(mArgs[0], callArgs);
                auto callee = declareBuiltin(boolFns.at(methodName), i32Ty,
                    {ptrTy, usizeTy, ptrTy, usizeTy});
                auto* r = builder_->CreateCall(callee, callArgs, methodName);
                cleanupReceiverIfTemp();
                return static_cast<llvm::Value*>(
                    builder_->CreateICmpNE(r, llvm::ConstantInt::get(i32Ty, 0)));
            }
            // (receiver, string) → int64
            if (methodName == "indexOf" || methodName == "lastIndexOf") {
                static const std::unordered_map<std::string, std::string> idxFns = {
                    {"indexOf",     "lucis_indexOf"},
                    {"lastIndexOf", "lucis_lastIndexOf"},
                };
                std::vector<llvm::Value*> callArgs = {strPtr, strLen};
                extractStr(mArgs[0], callArgs);
                auto callee = declareBuiltin(idxFns.at(methodName), i64Ty,
                    {ptrTy, usizeTy, ptrTy, usizeTy});
                return static_cast<llvm::Value*>(
                    builder_->CreateCall(callee, callArgs, methodName));
            }
            if (methodName == "count") {
                std::vector<llvm::Value*> callArgs = {strPtr, strLen};
                extractStr(mArgs[0], callArgs);
                auto callee = declareBuiltin("lucis_count", usizeTy,
                    {ptrTy, usizeTy, ptrTy, usizeTy});
                return static_cast<llvm::Value*>(
                    builder_->CreateCall(callee, callArgs, "count"));
            }
            // No-arg string → string transformations
            if (methodName == "toUpper"  || methodName == "toLower"  ||
                methodName == "trim"     || methodName == "trimLeft" ||
                methodName == "trimRight"|| methodName == "reverse") {
                static const std::unordered_map<std::string, std::string> xformFns = {
                    {"toUpper",   "lucis_toUpper"},
                    {"toLower",   "lucis_toLower"},
                    {"trim",      "lucis_trim"},
                    {"trimLeft",  "lucis_trimLeft"},
                    {"trimRight", "lucis_trimRight"},
                    {"reverse",   "lucis_reverse"},
                };
                auto callee = declareBuiltin(xformFns.at(methodName), strTy, {ptrTy, usizeTy});
                auto* r = builder_->CreateCall(callee, {strPtr, strLen}, methodName);
                return static_cast<llvm::Value*>(buildStr(r));
            }
            if (methodName == "substring") {
                auto* start  = mArgs[0];
                auto* length = mArgs[1];
                if (start->getType()  != usizeTy) start  = builder_->CreateIntCast(start,  usizeTy, false);
                if (length->getType() != usizeTy) length = builder_->CreateIntCast(length, usizeTy, false);
                auto callee = declareBuiltin("lucis_substring", strTy,
                    {ptrTy, usizeTy, usizeTy, usizeTy});
                auto* r = builder_->CreateCall(callee, {strPtr, strLen, start, length}, "substring");
                return static_cast<llvm::Value*>(buildStr(r));
            }
            if (methodName == "slice") {
                auto* start = mArgs[0];
                auto* end   = mArgs[1];
                if (start->getType() != i64Ty) start = builder_->CreateIntCast(start, i64Ty, true);
                if (end->getType()   != i64Ty) end   = builder_->CreateIntCast(end,   i64Ty, true);
                auto callee = declareBuiltin("lucis_slice", strTy, {ptrTy, usizeTy, i64Ty, i64Ty});
                auto* r = builder_->CreateCall(callee, {strPtr, strLen, start, end}, "slice");
                return static_cast<llvm::Value*>(buildStr(r));
            }
            if (methodName == "repeat") {
                auto* n = mArgs[0];
                if (n->getType() != usizeTy) n = builder_->CreateIntCast(n, usizeTy, false);
                auto callee = declareBuiltin("lucis_repeat", strTy, {ptrTy, usizeTy, usizeTy});
                auto* r = builder_->CreateCall(callee, {strPtr, strLen, n}, "repeat");
                return static_cast<llvm::Value*>(buildStr(r));
            }
            if (methodName == "padLeft" || methodName == "padRight") {
                std::string fname = (methodName == "padLeft") ? "lucis_padLeft" : "lucis_padRight";
                auto* width = mArgs[0];
                auto* fill  = mArgs[1];
                if (width->getType() != usizeTy) width = builder_->CreateIntCast(width, usizeTy, false);
                auto callee = declareBuiltin(fname, strTy, {ptrTy, usizeTy, usizeTy, i8Ty});
                auto* r = builder_->CreateCall(callee, {strPtr, strLen, width, fill}, methodName);
                return static_cast<llvm::Value*>(buildStr(r));
            }
            if (methodName == "replace" || methodName == "replaceFirst") {
                std::string fname = (methodName == "replace") ? "lucis_replace" : "lucis_replaceFirst";
                std::vector<llvm::Value*> callArgs = {strPtr, strLen};
                extractStr(mArgs[0], callArgs);  // old
                extractStr(mArgs[1], callArgs);  // rep
                auto callee = declareBuiltin(fname, strTy,
                    {ptrTy, usizeTy, ptrTy, usizeTy, ptrTy, usizeTy});
                auto* r = builder_->CreateCall(callee, callArgs, methodName);
                return static_cast<llvm::Value*>(buildStr(r));
            }
            if (methodName == "trimChar") {
                auto* ch = mArgs[0];
                auto callee = declareBuiltin("lucis_trimChar", strTy, {ptrTy, usizeTy, i8Ty});
                auto* r = builder_->CreateCall(callee, {strPtr, strLen, ch}, "trimChar");
                return static_cast<llvm::Value*>(buildStr(r));
            }
            if (methodName == "capitalize") {
                auto callee = declareBuiltin("lucis_capitalize", strTy, {ptrTy, usizeTy});
                auto* r = builder_->CreateCall(callee, {strPtr, strLen}, "capitalize");
                return static_cast<llvm::Value*>(buildStr(r));
            }
            if (methodName == "removePrefix" || methodName == "removeSuffix") {
                std::string fname = (methodName == "removePrefix")
                    ? "lucis_removePrefix" : "lucis_removeSuffix";
                std::vector<llvm::Value*> callArgs = {strPtr, strLen};
                extractStr(mArgs[0], callArgs);
                auto callee = declareBuiltin(fname, strTy,
                    {ptrTy, usizeTy, ptrTy, usizeTy});
                auto* r = builder_->CreateCall(callee, callArgs, methodName);
                return static_cast<llvm::Value*>(buildStr(r));
            }
            if (methodName == "insert") {
                auto* pos = mArgs[0];
                if (pos->getType() != usizeTy)
                    pos = builder_->CreateIntCast(pos, usizeTy, false);
                std::vector<llvm::Value*> callArgs = {strPtr, strLen, pos};
                extractStr(mArgs[1], callArgs);
                auto callee = declareBuiltin("lucis_strInsert", strTy,
                    {ptrTy, usizeTy, usizeTy, ptrTy, usizeTy});
                auto* r = builder_->CreateCall(callee, callArgs, "insert");
                return static_cast<llvm::Value*>(buildStr(r));
            }
            if (methodName == "remove") {
                auto* start = mArgs[0];
                auto* count = mArgs[1];
                if (start->getType() != usizeTy)
                    start = builder_->CreateIntCast(start, usizeTy, false);
                if (count->getType() != usizeTy)
                    count = builder_->CreateIntCast(count, usizeTy, false);
                auto callee = declareBuiltin("lucis_strRemove", strTy,
                    {ptrTy, usizeTy, usizeTy, usizeTy});
                auto* r = builder_->CreateCall(callee, {strPtr, strLen, start, count}, "remove");
                return static_cast<llvm::Value*>(buildStr(r));
            }
            if (methodName == "concat") {
                std::vector<llvm::Value*> callArgs = {strPtr, strLen};
                extractStr(mArgs[0], callArgs);
                auto callee = declareBuiltin("lucis_concat", strTy,
                    {ptrTy, usizeTy, ptrTy, usizeTy});
                auto* r = builder_->CreateCall(callee, callArgs, "concat");
                return static_cast<llvm::Value*>(buildStr(r));
            }
            if (methodName == "compareTo") {
                std::vector<llvm::Value*> callArgs = {strPtr, strLen};
                extractStr(mArgs[0], callArgs);
                auto callee = declareBuiltin("lucis_compareTo", i32Ty,
                    {ptrTy, usizeTy, ptrTy, usizeTy});
                return static_cast<llvm::Value*>(
                    builder_->CreateCall(callee, callArgs, "compareTo"));
            }
            if (methodName == "equalsIgnoreCase") {
                std::vector<llvm::Value*> callArgs = {strPtr, strLen};
                extractStr(mArgs[0], callArgs);
                auto callee = declareBuiltin("lucis_equalsIgnoreCase", i32Ty,
                    {ptrTy, usizeTy, ptrTy, usizeTy});
                auto* r = builder_->CreateCall(callee, callArgs, "eqIgnCase");
                return static_cast<llvm::Value*>(
                    builder_->CreateICmpNE(r, llvm::ConstantInt::get(i32Ty, 0)));
            }
            // String classification methods → bool
            if (methodName == "isNumeric" || methodName == "isAlpha" ||
                methodName == "isAlphaNum" || methodName == "isUpper" ||
                methodName == "isLower" || methodName == "isBlank") {
                static const std::unordered_map<std::string, std::string> clsFns = {
                    {"isNumeric",  "lucis_strIsNumeric"},
                    {"isAlpha",    "lucis_strIsAlpha"},
                    {"isAlphaNum", "lucis_strIsAlphaNum"},
                    {"isUpper",    "lucis_strIsUpper"},
                    {"isLower",    "lucis_strIsLower"},
                    {"isBlank",    "lucis_strIsBlank"},
                };
                auto callee = declareBuiltin(clsFns.at(methodName), i32Ty,
                    {ptrTy, usizeTy});
                auto* r = builder_->CreateCall(callee, {strPtr, strLen}, methodName);
                return static_cast<llvm::Value*>(
                    builder_->CreateICmpNE(r, llvm::ConstantInt::get(i32Ty, 0)));
            }
            if (methodName == "toInt") {
                auto callee = declareBuiltin("lucis_parseInt", i64Ty, {ptrTy, usizeTy});
                return static_cast<llvm::Value*>(
                    builder_->CreateCall(callee, {strPtr, strLen}, "toInt"));
            }
            if (methodName == "toFloat") {
                auto* dblTy = llvm::Type::getDoubleTy(*context_);
                auto callee = declareBuiltin("lucis_parseFloat", dblTy, {ptrTy, usizeTy});
                return static_cast<llvm::Value*>(
                    builder_->CreateCall(callee, {strPtr, strLen}, "toFloat"));
            }
            if (methodName == "toBool") {
                auto callee = declareBuiltin("lucis_strToBool", i32Ty, {ptrTy, usizeTy});
                auto* r = builder_->CreateCall(callee, {strPtr, strLen}, "toBool");
                return static_cast<llvm::Value*>(
                    builder_->CreateICmpNE(r, llvm::ConstantInt::get(i32Ty, 0)));
            }
            if (methodName == "hash") {
                auto* u64Ty = llvm::Type::getInt64Ty(*context_);
                auto callee = declareBuiltin("lucis_hashString", u64Ty, {ptrTy, usizeTy});
                return static_cast<llvm::Value*>(
                    builder_->CreateCall(callee, {strPtr, strLen}, "hash"));
            }
            // split, join, chars, bytes, lines, words return arrays/vecs
            // These use out-param Vec pattern
            if (methodName == "split") {
                auto* vecTy   = getOrCreateVecStructType();
                auto* voidTy  = llvm::Type::getVoidTy(*context_);
                auto* outAlloca = builder_->CreateAlloca(vecTy, nullptr, "split_out");
                std::vector<llvm::Value*> callArgs = {outAlloca, strPtr, strLen};
                extractStr(mArgs[0], callArgs);
                auto callee = declareBuiltin("lucis_split", voidTy,
                    {ptrTy, ptrTy, usizeTy, ptrTy, usizeTy});
                builder_->CreateCall(callee, callArgs);
                return static_cast<llvm::Value*>(
                    builder_->CreateLoad(vecTy, outAlloca, "split_val"));
            }
            if (methodName == "join") {
                // join(separator) — the receiver is a Vec<string> stored somewhere,
                // but here it's a plain string. This is receiver.join(sep) which
                // doesn't make sense on a plain string. Skip for now.
            }
            if (methodName == "chars" || methodName == "bytes" ||
                methodName == "lines" || methodName == "words") {
                auto* vecTy   = getOrCreateVecStructType();
                auto* voidTy  = llvm::Type::getVoidTy(*context_);
                auto* outAlloca = builder_->CreateAlloca(vecTy, nullptr,
                    methodName + "_out");
                static const std::unordered_map<std::string, std::string> vecFns = {
                    {"chars", "lucis_chars"},
                    {"bytes", "lucis_toBytes"},
                    {"lines", "lucis_lines"},
                    {"words", "lucis_words"},
                };
                auto callee = declareBuiltin(vecFns.at(methodName), voidTy,
                    {ptrTy, ptrTy, usizeTy});
                builder_->CreateCall(callee, {outAlloca, strPtr, strLen});
                return static_cast<llvm::Value*>(
                    builder_->CreateLoad(vecTy, outAlloca, methodName + "_val"));
            }
        }
    }
    // Evaluate receiver lazily; array-method path does not require it and
    // may involve deref forms that should be handled through array metadata.
    llvm::Value* receiverVal = nullptr;
    if (recvArrayDims == 0)
        receiverVal = castValue(visit(ctx->expression()));
    // Collect arguments
    std::vector<llvm::Value*> args;
    if (auto* argList = ctx->argList()) {
        for (auto* argExpr : argList->expression()) {
            args.push_back(castValue(visit(argExpr)));
        }
    }
    // Look up the method descriptor
    const MethodDescriptor* desc = nullptr;
    if (recvArrayDims > 0)
        desc = methodRegistry_.lookupArrayMethod(methodName);
    else if (receiverTI)
        desc = methodRegistry_.lookup(receiverTI->kind, methodName);
    if (!desc) {
        std::cerr << "lucis: unknown method '" << methodName << "'\n";
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }
    // ── Array method codegen for non-variable receivers (field access, etc.) ──
    if (recvArrayDims > 0 && receiverVarName.empty()) {
        auto* receiverValLocal = castValue(visit(ctx->expression()));
        auto* structTy = llvm::dyn_cast<llvm::StructType>(receiverValLocal->getType());
        if (structTy && structTy->getNumElements() == 2 &&
            structTy->getElementType(0)->isPointerTy() &&
            structTy->getElementType(1)->isIntegerTy()) {
            auto* localUsizeTy = module_->getDataLayout().getIntPtrType(*context_);
            auto* ptrVal = builder_->CreateExtractValue(receiverValLocal, 0, "slice.ptr");
            auto* lenVal = builder_->CreateExtractValue(receiverValLocal, 1, "slice.len");
            auto* lenAsUsize = (lenVal->getType() == localUsizeTy)
                ? lenVal : builder_->CreateIntCast(lenVal, localUsizeTy, false, "slice.len.cast");
            auto* elTy = receiverTI ? receiverTI->toLLVMType(*context_, module_->getDataLayout())
                                    : llvm::Type::getInt32Ty(*context_);
            if (methodName == "len")
                return static_cast<llvm::Value*>(lenAsUsize);
            if (methodName == "isEmpty")
                return static_cast<llvm::Value*>(
                    builder_->CreateICmpEQ(lenAsUsize, llvm::ConstantInt::get(localUsizeTy, 0), "slice.empty"));
            if (methodName == "first" || methodName == "last" || methodName == "at") {
                auto* zero = llvm::ConstantInt::get(localUsizeTy, 0);
                llvm::Value* idx = nullptr;
                if (methodName == "last") {
                    idx = builder_->CreateSub(lenAsUsize, llvm::ConstantInt::get(localUsizeTy, 1), "slice.last.idx");
                } else if (methodName == "at" && !args.empty()) {
                    idx = builder_->CreateIntCast(args[0], localUsizeTy, true);
                    emitBoundsCheck(idx, lenAsUsize, "slice_at_expr");
                } else {
                    idx = zero;
                }
                auto* elemPtr = builder_->CreateGEP(elTy, ptrVal, idx, "slice.elem.ptr");
                return static_cast<llvm::Value*>(
                    builder_->CreateLoad(elTy, elemPtr, "slice.elem"));
            }
        }
        std::cerr << "lucis: expected slice type for method '" << methodName << "'\n";
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }
    // ── Array method codegen ─────────────────────────────────────────
    if (recvArrayDims > 0 && !receiverVarName.empty()) {
        auto it = locals_.find(receiverVarName);
        if (it != locals_.end()) {
            auto* arrAlloca = it->second.alloca;
            auto* allocaTy  = arrAlloca->getAllocatedType();
            auto* arrTy     = llvm::dyn_cast<llvm::ArrayType>(allocaTy);
            if (!arrTy && methodName == "len" &&
                it->second.typeInfo && it->second.typeInfo->kind == TypeKind::Pointer &&
                !it->second.fixedArraySizes.empty()) {
                auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
                return static_cast<llvm::Value*>(
                    llvm::ConstantInt::get(usizeTy, it->second.fixedArraySizes[0]));
            }
            // []T parameter/values are lowered as a slice-like struct: {ptr, len}.
            // Support common array-like methods directly on that representation.
            if (!arrTy) {
                auto* sliceTy = llvm::dyn_cast<llvm::StructType>(allocaTy);
                if (sliceTy && sliceTy->getNumElements() == 2) {
                    auto* ptrFieldTy = sliceTy->getElementType(0);
                    auto* lenFieldTy = sliceTy->getElementType(1);
                    if (ptrFieldTy->isPointerTy() && lenFieldTy->isIntegerTy()) {
                        auto* elemTI = it->second.typeInfo;
                        if (!elemTI) {
                            std::cerr << "lucis: unable to resolve slice element type for '"
                                      << receiverVarName << "'\n";
                            return static_cast<llvm::Value*>(
                                llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
                        }
                        auto* i64Ty   = llvm::Type::getInt64Ty(*context_);
                        auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
                        auto* elemTy  = elemTI->toLLVMType(*context_,
                                                           module_->getDataLayout());
                        auto* ptrGep = builder_->CreateStructGEP(sliceTy, arrAlloca, 0, "slice.ptr.gep");
                        auto* lenGep = builder_->CreateStructGEP(sliceTy, arrAlloca, 1, "slice.len.gep");
                        auto* dataPtr = builder_->CreateLoad(ptrFieldTy, ptrGep, "slice.ptr");
                        auto* lenVal  = builder_->CreateLoad(lenFieldTy, lenGep, "slice.len");
                        auto* lenAsUsize = (lenVal->getType() == usizeTy)
                            ? lenVal
                            : builder_->CreateIntCast(lenVal, usizeTy, false, "slice.len.cast");
                        if (methodName == "len") {
                            return static_cast<llvm::Value*>(lenAsUsize);
                        }
                        if (methodName == "isEmpty") {
                            auto* zero = llvm::ConstantInt::get(usizeTy, 0);
                            return static_cast<llvm::Value*>(
                                builder_->CreateICmpEQ(lenAsUsize, zero, "slice.empty"));
                        }
                        llvm::Value* idx = llvm::ConstantInt::get(i64Ty, 0);
                        if (methodName == "last") {
                            auto* lenAsI64 = (lenVal->getType() == i64Ty)
                                ? lenVal
                                : builder_->CreateIntCast(lenVal, i64Ty, false, "slice.len.i64");
                            idx = builder_->CreateSub(lenAsI64,
                                llvm::ConstantInt::get(i64Ty, 1), "slice.last.idx");
                        } else if (methodName == "at") {
                            if (args.empty()) {
                                std::cerr << "lucis: at() requires an index argument\n";
                                return static_cast<llvm::Value*>(llvm::UndefValue::get(elemTy));
                            }
                            idx = args[0];
                            if (idx->getType() != i64Ty)
                                idx = builder_->CreateIntCast(idx, i64Ty, true, "slice.at.idx");
                        }
                        if (methodName == "first" || methodName == "last" || methodName == "at") {
                            if (methodName == "at") {
                                auto* idxU = builder_->CreateIntCast(idx, usizeTy, false, "at.idx");
                                emitBoundsCheck(idxU, lenAsUsize, "slice_at");
                            }
                            auto* elemPtr = builder_->CreateGEP(elemTy, dataPtr, idx, "slice.elem.ptr");
                            return static_cast<llvm::Value*>(
                                builder_->CreateLoad(elemTy, elemPtr, "slice.elem"));
                        }
                        // ── contains on slice ──────────────────────────
                        if (methodName == "contains") {
                            if (args.empty()) {
                                std::cerr << "lucis: contains() requires a value argument\n";
                                return static_cast<llvm::Value*>(
                                    llvm::UndefValue::get(llvm::Type::getInt1Ty(*context_)));
                            }
                            auto* needle = args[0];
                            if (needle->getType() != elemTy)
                                needle = builder_->CreateIntCast(needle, elemTy, true);
                            auto* boolTy = llvm::Type::getInt1Ty(*context_);
                            auto* resA = builder_->CreateAlloca(boolTy, nullptr, "sc.res");
                            builder_->CreateStore(llvm::ConstantInt::get(boolTy, 0), resA);
                            auto* cBB = llvm::BasicBlock::Create(*context_, "sc.cond", currentFunction_);
                            auto* bBB = llvm::BasicBlock::Create(*context_, "sc.body", currentFunction_);
                            auto* fBB = llvm::BasicBlock::Create(*context_, "sc.found", currentFunction_);
                            auto* nBB = llvm::BasicBlock::Create(*context_, "sc.next", currentFunction_);
                            auto* eBB = llvm::BasicBlock::Create(*context_, "sc.end", currentFunction_);
                            auto* iA = builder_->CreateAlloca(usizeTy, nullptr, "sc.idx");
                            builder_->CreateStore(llvm::ConstantInt::get(usizeTy, 0), iA);
                            builder_->CreateBr(cBB);
                            builder_->SetInsertPoint(cBB);
                            auto* iv = builder_->CreateLoad(usizeTy, iA, "sc.i");
                            auto* sc = builder_->CreateICmpULT(iv, lenAsUsize, "sc.cmp");
                            builder_->CreateCondBr(sc, bBB, eBB);
                            builder_->SetInsertPoint(bBB);
                            auto* cv = builder_->CreateLoad(usizeTy, iA, "sc.ci");
                            auto* ep = builder_->CreateGEP(elemTy, dataPtr, cv, "sc.ep");
                            auto* ev = builder_->CreateLoad(elemTy, ep, "sc.ev");
                            auto* eq = elemTy->isIntegerTy() ? builder_->CreateICmpEQ(ev, needle, "sc.eq")
                                : elemTy->isFloatingPointTy() ? builder_->CreateFCmpOEQ(ev, needle, "sc.eq")
                                : builder_->CreateICmpEQ(ev, needle, "sc.eq");
                            builder_->CreateCondBr(eq, fBB, nBB);
                            builder_->SetInsertPoint(fBB);
                            builder_->CreateStore(llvm::ConstantInt::get(boolTy, 1), resA);
                            builder_->CreateBr(eBB);
                            builder_->SetInsertPoint(nBB);
                            auto* nx = builder_->CreateAdd(cv, llvm::ConstantInt::get(usizeTy, 1), "sc.nx");
                            builder_->CreateStore(nx, iA);
                            builder_->CreateBr(cBB);
                            builder_->SetInsertPoint(eBB);
                            return static_cast<llvm::Value*>(
                                builder_->CreateLoad(boolTy, resA, "sc.res"));
                        }
                        // ── sum on slice ────────────────────────────────
                        if (methodName == "sum" && (elemTy->isIntegerTy() || elemTy->isFloatingPointTy())) {
                            auto* accA = builder_->CreateAlloca(elemTy, nullptr, "ss.acc");
                            auto* zero = elemTy->isIntegerTy()
                                ? static_cast<llvm::Value*>(llvm::ConstantInt::get(elemTy, 0))
                                : static_cast<llvm::Value*>(llvm::ConstantFP::get(elemTy, 0.0));
                            builder_->CreateStore(zero, accA);
                            auto* cBB = llvm::BasicBlock::Create(*context_, "ss.cond", currentFunction_);
                            auto* bBB = llvm::BasicBlock::Create(*context_, "ss.body", currentFunction_);
                            auto* eBB = llvm::BasicBlock::Create(*context_, "ss.end", currentFunction_);
                            auto* iA = builder_->CreateAlloca(usizeTy, nullptr, "ss.idx");
                            builder_->CreateStore(llvm::ConstantInt::get(usizeTy, 0), iA);
                            builder_->CreateBr(cBB);
                            builder_->SetInsertPoint(cBB);
                            auto* ci = builder_->CreateLoad(usizeTy, iA, "ss.i");
                            auto* cc = builder_->CreateICmpULT(ci, lenAsUsize, "ss.cmp");
                            builder_->CreateCondBr(cc, bBB, eBB);
                            builder_->SetInsertPoint(bBB);
                            auto* av = builder_->CreateLoad(elemTy, accA, "ss.av");
                            auto* v = builder_->CreateLoad(elemTy,
                                builder_->CreateGEP(elemTy, dataPtr,
                                    builder_->CreateLoad(usizeTy, iA, "ss.ci"), "ss.v"), "ss.v");
                            auto* nv = elemTy->isIntegerTy()
                                ? static_cast<llvm::Value*>(builder_->CreateAdd(av, v, "ss.nv"))
                                : static_cast<llvm::Value*>(builder_->CreateFAdd(av, v, "ss.nv"));
                            builder_->CreateStore(nv, accA);
                            auto* n = builder_->CreateAdd(builder_->CreateLoad(usizeTy, iA, "ss.ni"),
                                llvm::ConstantInt::get(usizeTy, 1), "ss.n");
                            builder_->CreateStore(n, iA);
                            builder_->CreateBr(cBB);
                            builder_->SetInsertPoint(eBB);
                            return static_cast<llvm::Value*>(
                                builder_->CreateLoad(elemTy, accA, "ss.ret"));
                        }
                        // ── indexOf / lastIndexOf on slice ──────────────
                        if (methodName == "indexOf" || methodName == "lastIndexOf") {
                            if (args.empty()) {
                                std::cerr << "lucis: " << methodName << "() requires a value argument\n";
                                return static_cast<llvm::Value*>(
                                    llvm::ConstantInt::get(i64Ty, -1));
                            }
                            auto* needle = args[0];
                            if (needle->getType() != elemTy)
                                needle = builder_->CreateIntCast(needle, elemTy, true);
                            auto* resA = builder_->CreateAlloca(i64Ty, nullptr, "sio.res");
                            builder_->CreateStore(llvm::ConstantInt::get(i64Ty, -1ULL), resA);
                            auto* cBB = llvm::BasicBlock::Create(*context_, "sio.cond", currentFunction_);
                            auto* bBB = llvm::BasicBlock::Create(*context_, "sio.body", currentFunction_);
                            auto* fBB = llvm::BasicBlock::Create(*context_, "sio.found", currentFunction_);
                            auto* nBB = llvm::BasicBlock::Create(*context_, "sio.next", currentFunction_);
                            auto* eBB = llvm::BasicBlock::Create(*context_, "sio.end", currentFunction_);
                            auto* iA = builder_->CreateAlloca(usizeTy, nullptr, "sio.idx");
                            if (methodName == "indexOf") {
                                builder_->CreateStore(llvm::ConstantInt::get(usizeTy, 0), iA);
                            } else {
                                builder_->CreateStore(lenAsUsize, iA);
                            }
                            builder_->CreateBr(cBB);
                            builder_->SetInsertPoint(cBB);
                            auto* ci = builder_->CreateLoad(usizeTy, iA, "sio.i");
                            llvm::Value* cmp = (methodName == "indexOf")
                                ? static_cast<llvm::Value*>(builder_->CreateICmpULT(ci, lenAsUsize, "sio.cmp"))
                                : static_cast<llvm::Value*>(builder_->CreateICmpUGT(ci, llvm::ConstantInt::get(usizeTy, 0), "sio.cmp"));
                            builder_->CreateCondBr(cmp, bBB, eBB);
                            builder_->SetInsertPoint(bBB);
                            auto* curIdx = (methodName == "indexOf")
                                ? builder_->CreateLoad(usizeTy, iA, "sio.ii")
                                : builder_->CreateSub(builder_->CreateLoad(usizeTy, iA, "sio.di"),
                                    llvm::ConstantInt::get(usizeTy, 1), "sio.dec");
                            if (methodName == "lastIndexOf")
                                builder_->CreateStore(curIdx, iA);
                            auto* ev = builder_->CreateLoad(elemTy,
                                builder_->CreateGEP(elemTy, dataPtr, curIdx, "sio.ep"), "sio.ev");
                            auto* eq = elemTy->isIntegerTy() ? builder_->CreateICmpEQ(ev, needle, "sio.eq")
                                : elemTy->isFloatingPointTy() ? builder_->CreateFCmpOEQ(ev, needle, "sio.eq")
                                : builder_->CreateICmpEQ(ev, needle, "sio.eq");
                            builder_->CreateCondBr(eq, fBB, nBB);
                            builder_->SetInsertPoint(fBB);
                            auto* foundIdx = (methodName == "indexOf")
                                ? builder_->CreateLoad(usizeTy, iA, "sio.fi")
                                : curIdx;
                            builder_->CreateStore(builder_->CreateIntCast(foundIdx, i64Ty, false, "sio.fcast"), resA);
                            builder_->CreateBr(eBB);
                            builder_->SetInsertPoint(nBB);
                            auto* nextIdx = (methodName == "indexOf")
                                ? builder_->CreateAdd(builder_->CreateLoad(usizeTy, iA, "sio.ni"),
                                    llvm::ConstantInt::get(usizeTy, 1), "sio.nx")
                                : builder_->CreateLoad(usizeTy, iA, "sio.nd");
                            if (methodName == "indexOf")
                                builder_->CreateStore(nextIdx, iA);
                            builder_->CreateBr(cBB);
                            builder_->SetInsertPoint(eBB);
                            return static_cast<llvm::Value*>(
                                builder_->CreateLoad(i64Ty, resA, "sio.ret"));
                        }
                        // ── count on slice ──────────────────────────────
                        if (methodName == "count") {
                            if (args.empty()) {
                                std::cerr << "lucis: count() requires a value argument\n";
                                return static_cast<llvm::Value*>(
                                    llvm::ConstantInt::get(usizeTy, 0));
                            }
                            auto* needle = args[0];
                            if (needle->getType() != elemTy)
                                needle = builder_->CreateIntCast(needle, elemTy, true);
                            auto* resA = builder_->CreateAlloca(usizeTy, nullptr, "scnt.res");
                            builder_->CreateStore(llvm::ConstantInt::get(usizeTy, 0), resA);
                            auto* cBB = llvm::BasicBlock::Create(*context_, "scnt.cond", currentFunction_);
                            auto* bBB = llvm::BasicBlock::Create(*context_, "scnt.body", currentFunction_);
                            auto* mBB = llvm::BasicBlock::Create(*context_, "scnt.match", currentFunction_);
                            auto* nBB = llvm::BasicBlock::Create(*context_, "scnt.next", currentFunction_);
                            auto* eBB = llvm::BasicBlock::Create(*context_, "scnt.end", currentFunction_);
                            auto* iA = builder_->CreateAlloca(usizeTy, nullptr, "scnt.idx");
                            builder_->CreateStore(llvm::ConstantInt::get(usizeTy, 0), iA);
                            builder_->CreateBr(cBB);
                            builder_->SetInsertPoint(cBB);
                            auto* ci = builder_->CreateLoad(usizeTy, iA, "scnt.i");
                            auto* cmp = builder_->CreateICmpULT(ci, lenAsUsize, "scnt.cmp");
                            builder_->CreateCondBr(cmp, bBB, eBB);
                            builder_->SetInsertPoint(bBB);
                            auto* ev = builder_->CreateLoad(elemTy,
                                builder_->CreateGEP(elemTy, dataPtr,
                                    builder_->CreateLoad(usizeTy, iA, "scnt.ei"), "scnt.ev"), "scnt.ev");
                            auto* eq = elemTy->isIntegerTy() ? builder_->CreateICmpEQ(ev, needle, "scnt.eq")
                                : elemTy->isFloatingPointTy() ? builder_->CreateFCmpOEQ(ev, needle, "scnt.eq")
                                : builder_->CreateICmpEQ(ev, needle, "scnt.eq");
                            builder_->CreateCondBr(eq, mBB, nBB);
                            builder_->SetInsertPoint(mBB);
                            auto* cur = builder_->CreateLoad(usizeTy, resA, "scnt.cur");
                            builder_->CreateStore(
                                builder_->CreateAdd(cur, llvm::ConstantInt::get(usizeTy, 1), "scnt.inc"),
                                resA);
                            builder_->CreateBr(nBB);
                            builder_->SetInsertPoint(nBB);
                            auto* nx = builder_->CreateAdd(builder_->CreateLoad(usizeTy, iA, "scnt.ni"),
                                llvm::ConstantInt::get(usizeTy, 1), "scnt.nx");
                            builder_->CreateStore(nx, iA);
                            builder_->CreateBr(cBB);
                            builder_->SetInsertPoint(eBB);
                            return static_cast<llvm::Value*>(
                                builder_->CreateLoad(usizeTy, resA, "scnt.ret"));
                        }
                        // ── fill on slice ──────────────────────────────
                        if (methodName == "fill") {
                            if (args.empty()) {
                                std::cerr << "lucis: fill() requires a value argument\n";
                                return static_cast<llvm::Value*>(
                                    llvm::UndefValue::get(llvm::Type::getVoidTy(*context_)));
                            }
                            auto* fillVal = args[0];
                            if (fillVal->getType() != elemTy)
                                fillVal = builder_->CreateIntCast(fillVal, elemTy, true);
                            auto* cBB = llvm::BasicBlock::Create(*context_, "sfill.cond", currentFunction_);
                            auto* bBB = llvm::BasicBlock::Create(*context_, "sfill.body", currentFunction_);
                            auto* eBB = llvm::BasicBlock::Create(*context_, "sfill.end", currentFunction_);
                            auto* iA = builder_->CreateAlloca(usizeTy, nullptr, "sfill.idx");
                            builder_->CreateStore(llvm::ConstantInt::get(usizeTy, 0), iA);
                            builder_->CreateBr(cBB);
                            builder_->SetInsertPoint(cBB);
                            auto* ci = builder_->CreateLoad(usizeTy, iA, "sfill.i");
                            auto* cmp = builder_->CreateICmpULT(ci, lenAsUsize, "sfill.cmp");
                            builder_->CreateCondBr(cmp, bBB, eBB);
                            builder_->SetInsertPoint(bBB);
                            builder_->CreateStore(fillVal,
                                builder_->CreateGEP(elemTy, dataPtr,
                                    builder_->CreateLoad(usizeTy, iA, "sfill.ei"), "sfill.ep"));
                            auto* nx = builder_->CreateAdd(builder_->CreateLoad(usizeTy, iA, "sfill.ni"),
                                llvm::ConstantInt::get(usizeTy, 1), "sfill.nx");
                            builder_->CreateStore(nx, iA);
                            builder_->CreateBr(cBB);
                            builder_->SetInsertPoint(eBB);
                            return static_cast<llvm::Value*>(
                                llvm::UndefValue::get(llvm::Type::getVoidTy(*context_)));
                        }
                        // ── swap on slice ──────────────────────────────
                        if (methodName == "swap") {
                            if (args.size() < 2) {
                                std::cerr << "lucis: swap() requires two index arguments\n";
                                return static_cast<llvm::Value*>(
                                    llvm::UndefValue::get(llvm::Type::getVoidTy(*context_)));
                            }
                            auto* idxA = args[0];
                            auto* idxB = args[1];
                            if (idxA->getType() != usizeTy)
                                idxA = builder_->CreateIntCast(idxA, usizeTy, true);
                            if (idxB->getType() != usizeTy)
                                idxB = builder_->CreateIntCast(idxB, usizeTy, true);
                            emitBoundsCheck(idxA, lenAsUsize, "swap_a");
                            emitBoundsCheck(idxB, lenAsUsize, "swap_b");
                            auto* gepA = builder_->CreateGEP(elemTy, dataPtr, idxA, "swap_a");
                            auto* gepB = builder_->CreateGEP(elemTy, dataPtr, idxB, "swap_b");
                            auto* valA = builder_->CreateLoad(elemTy, gepA, "a");
                            auto* valB = builder_->CreateLoad(elemTy, gepB, "b");
                            builder_->CreateStore(valB, gepA);
                            builder_->CreateStore(valA, gepB);
                            return static_cast<llvm::Value*>(
                                llvm::UndefValue::get(llvm::Type::getVoidTy(*context_)));
                        }
                        // ── reverse on slice ───────────────────────────
                        if (methodName == "reverse") {
                            auto* loA = builder_->CreateAlloca(usizeTy, nullptr, "srev.lo");
                            auto* hiA = builder_->CreateAlloca(usizeTy, nullptr, "srev.hi");
                            builder_->CreateStore(llvm::ConstantInt::get(usizeTy, 0), loA);
                            builder_->CreateStore(lenAsUsize, hiA);
                            auto* cBB = llvm::BasicBlock::Create(*context_, "srev.cond", currentFunction_);
                            auto* bBB = llvm::BasicBlock::Create(*context_, "srev.body", currentFunction_);
                            auto* eBB = llvm::BasicBlock::Create(*context_, "srev.end", currentFunction_);
                            builder_->CreateBr(cBB);
                            builder_->SetInsertPoint(cBB);
                            auto* lo = builder_->CreateLoad(usizeTy, loA, "srev.lv");
                            auto* hi = builder_->CreateLoad(usizeTy, hiA, "srev.hv");
                            llvm::Value* hiMinus1 = builder_->CreateSub(hi,
                                llvm::ConstantInt::get(usizeTy, 1), "srev.hm1");
                            auto* rcmp = builder_->CreateICmpULT(lo, hiMinus1, "srev.cmp");
                            builder_->CreateCondBr(rcmp, bBB, eBB);
                            builder_->SetInsertPoint(bBB);
                            auto* loV = builder_->CreateLoad(usizeTy, loA, "srev.l");
                            auto* hiV = builder_->CreateSub(
                                builder_->CreateLoad(usizeTy, hiA, "srev.h"),
                                llvm::ConstantInt::get(usizeTy, 1), "srev.hs");
                            auto* gepL = builder_->CreateGEP(elemTy, dataPtr, loV, "srev.gl");
                            auto* gepH = builder_->CreateGEP(elemTy, dataPtr, hiV, "srev.gh");
                            auto* valL = builder_->CreateLoad(elemTy, gepL, "srev.vl");
                            auto* valH = builder_->CreateLoad(elemTy, gepH, "srev.vh");
                            builder_->CreateStore(valH, gepL);
                            builder_->CreateStore(valL, gepH);
                            auto* nlo = builder_->CreateAdd(loV, llvm::ConstantInt::get(usizeTy, 1), "srev.nl");
                            builder_->CreateStore(nlo, loA);
                            auto* nhi = builder_->CreateSub(hiV, llvm::ConstantInt::get(usizeTy, 0), "srev.nh");
                            builder_->CreateStore(nhi, hiA);
                            builder_->CreateBr(cBB);
                            builder_->SetInsertPoint(eBB);
                            return static_cast<llvm::Value*>(
                                llvm::UndefValue::get(llvm::Type::getVoidTy(*context_)));
                        }
                        // ── equals on slice ────────────────────────────
                        if (methodName == "equals") {
                            auto* boolTy = llvm::Type::getInt1Ty(*context_);
                            if (args.empty()) {
                                std::cerr << "lucis: equals() requires an array argument\n";
                                return static_cast<llvm::Value*>(
                                    llvm::ConstantInt::get(boolTy, 0));
                            }
                            auto* rhsVal = args[0];
                            auto* rhsSliceTy = llvm::dyn_cast<llvm::StructType>(rhsVal->getType());
                            if (rhsSliceTy && rhsSliceTy->getNumElements() == 2 &&
                                rhsSliceTy->getElementType(0)->isPointerTy() &&
                                rhsSliceTy->getElementType(1)->isIntegerTy()) {
                                auto* rhsPtr = builder_->CreateExtractValue(rhsVal, 0, "seq.rp");
                                auto* rhsLen = builder_->CreateExtractValue(rhsVal, 1, "seq.rl");
                                auto* rhsLenU = (rhsLen->getType() == usizeTy)
                                    ? rhsLen : builder_->CreateIntCast(rhsLen, usizeTy, false, "seq.rlc");
                                auto* resA = builder_->CreateAlloca(boolTy, nullptr, "seq.res");
                                builder_->CreateStore(llvm::ConstantInt::get(boolTy, 1), resA);
                                auto* lenEq = builder_->CreateICmpEQ(lenAsUsize, rhsLenU, "seq.le");
                                auto* lenBB = llvm::BasicBlock::Create(*context_, "seq.lenok", currentFunction_);
                                auto* eBB = llvm::BasicBlock::Create(*context_, "seq.end", currentFunction_);
                                builder_->CreateCondBr(lenEq, lenBB, eBB);
                                builder_->SetInsertPoint(lenBB);
                                auto* iA = builder_->CreateAlloca(usizeTy, nullptr, "seq.idx");
                                builder_->CreateStore(llvm::ConstantInt::get(usizeTy, 0), iA);
                                auto* cBB = llvm::BasicBlock::Create(*context_, "seq.cond", currentFunction_);
                                auto* bBB = llvm::BasicBlock::Create(*context_, "seq.body", currentFunction_);
                                auto* nBB = llvm::BasicBlock::Create(*context_, "seq.next", currentFunction_);
                                auto* fBB = llvm::BasicBlock::Create(*context_, "seq.fail", currentFunction_);
                                builder_->CreateBr(cBB);
                                builder_->SetInsertPoint(cBB);
                                auto* ci = builder_->CreateLoad(usizeTy, iA, "seq.i");
                                auto* cc = builder_->CreateICmpULT(ci, lenAsUsize, "seq.cmp");
                                builder_->CreateCondBr(cc, bBB, eBB);
                                builder_->SetInsertPoint(bBB);
                                auto* cv = builder_->CreateLoad(usizeTy, iA, "seq.ci");
                                auto* lv = builder_->CreateLoad(elemTy,
                                    builder_->CreateGEP(elemTy, dataPtr, cv, "seq.lv"), "seq.lv");
                                auto* rv = builder_->CreateLoad(elemTy,
                                    builder_->CreateGEP(elemTy, rhsPtr, cv, "seq.rv"), "seq.rv");
                                auto* eq = elemTy->isIntegerTy() ? builder_->CreateICmpEQ(lv, rv, "seq.eq")
                                    : elemTy->isFloatingPointTy() ? builder_->CreateFCmpOEQ(lv, rv, "seq.eq")
                                    : builder_->CreateICmpEQ(lv, rv, "seq.eq");
                                builder_->CreateCondBr(eq, nBB, fBB);
                                builder_->SetInsertPoint(nBB);
                                auto* nv = builder_->CreateAdd(cv, llvm::ConstantInt::get(usizeTy, 1), "seq.nv");
                                builder_->CreateStore(nv, iA);
                                builder_->CreateBr(cBB);
                                builder_->SetInsertPoint(fBB);
                                builder_->CreateStore(llvm::ConstantInt::get(boolTy, 0), resA);
                                builder_->CreateBr(eBB);
                                builder_->SetInsertPoint(eBB);
                                return static_cast<llvm::Value*>(
                                    builder_->CreateLoad(boolTy, resA, "seq.ret"));
                            }
                            // rhs is a fixed array
                            auto* rhsArrTy = llvm::dyn_cast<llvm::ArrayType>(rhsVal->getType());
                            if (rhsArrTy) {
                                auto* rhsLenU = llvm::ConstantInt::get(usizeTy, rhsArrTy->getNumElements());
                                auto* resA = builder_->CreateAlloca(boolTy, nullptr, "seq.res");
                                builder_->CreateStore(llvm::ConstantInt::get(boolTy, 1), resA);
                                auto* lenEq = builder_->CreateICmpEQ(lenAsUsize, rhsLenU, "seq.le");
                                auto* lenBB = llvm::BasicBlock::Create(*context_, "seq.lenok", currentFunction_);
                                auto* eBB = llvm::BasicBlock::Create(*context_, "seq.end", currentFunction_);
                                builder_->CreateCondBr(lenEq, lenBB, eBB);
                                builder_->SetInsertPoint(lenBB);
                                auto* iA = builder_->CreateAlloca(usizeTy, nullptr, "seq.idx");
                                builder_->CreateStore(llvm::ConstantInt::get(usizeTy, 0), iA);
                                auto* cBB = llvm::BasicBlock::Create(*context_, "seq.cond", currentFunction_);
                                auto* bBB = llvm::BasicBlock::Create(*context_, "seq.body", currentFunction_);
                                auto* nBB = llvm::BasicBlock::Create(*context_, "seq.next", currentFunction_);
                                auto* fBB = llvm::BasicBlock::Create(*context_, "seq.fail", currentFunction_);
                                builder_->CreateBr(cBB);
                                builder_->SetInsertPoint(cBB);
                                auto* ci = builder_->CreateLoad(usizeTy, iA, "seq.i");
                                auto* cc = builder_->CreateICmpULT(ci, lenAsUsize, "seq.cmp");
                                builder_->CreateCondBr(cc, bBB, eBB);
                                builder_->SetInsertPoint(bBB);
                                auto* cv = builder_->CreateLoad(usizeTy, iA, "seq.ci");
                                auto* lv = builder_->CreateLoad(elemTy,
                                    builder_->CreateGEP(elemTy, dataPtr, cv, "seq.lv"), "seq.lv");
                                auto* rv = builder_->CreateLoad(elemTy,
                                    builder_->CreateGEP(arrTy, arrAlloca, {llvm::ConstantInt::get(i64Ty, 0),
                                        builder_->CreateIntCast(cv, i64Ty, false, "seq.rci")}, "seq.rv"), "seq.rv");
                                auto* eq = elemTy->isIntegerTy() ? builder_->CreateICmpEQ(lv, rv, "seq.eq")
                                    : elemTy->isFloatingPointTy() ? builder_->CreateFCmpOEQ(lv, rv, "seq.eq")
                                    : builder_->CreateICmpEQ(lv, rv, "seq.eq");
                                builder_->CreateCondBr(eq, nBB, fBB);
                                builder_->SetInsertPoint(nBB);
                                auto* nv = builder_->CreateAdd(cv, llvm::ConstantInt::get(usizeTy, 1), "seq.nv");
                                builder_->CreateStore(nv, iA);
                                builder_->CreateBr(cBB);
                                builder_->SetInsertPoint(fBB);
                                builder_->CreateStore(llvm::ConstantInt::get(boolTy, 0), resA);
                                builder_->CreateBr(eBB);
                                builder_->SetInsertPoint(eBB);
                                return static_cast<llvm::Value*>(
                                    builder_->CreateLoad(boolTy, resA, "seq.ret"));
                            }
                            std::cerr << "lucis: equals() argument must be an array or slice\n";
                            return static_cast<llvm::Value*>(
                                llvm::ConstantInt::get(boolTy, 0));
                        }
                        // ── product on slice ──────────────────────────────
                        if (methodName == "product" && (elemTy->isIntegerTy() || elemTy->isFloatingPointTy())) {
                            auto* accA = builder_->CreateAlloca(elemTy, nullptr, "sp.acc");
                            auto* init = elemTy->isIntegerTy()
                                ? static_cast<llvm::Value*>(llvm::ConstantInt::get(elemTy, 1))
                                : static_cast<llvm::Value*>(llvm::ConstantFP::get(elemTy, 1.0));
                            builder_->CreateStore(init, accA);
                            auto* cBB = llvm::BasicBlock::Create(*context_, "sp.cond", currentFunction_);
                            auto* bBB = llvm::BasicBlock::Create(*context_, "sp.body", currentFunction_);
                            auto* eBB = llvm::BasicBlock::Create(*context_, "sp.end", currentFunction_);
                            auto* iA = builder_->CreateAlloca(usizeTy, nullptr, "sp.idx");
                            builder_->CreateStore(llvm::ConstantInt::get(usizeTy, 0), iA);
                            builder_->CreateBr(cBB);
                            builder_->SetInsertPoint(cBB);
                            auto* ci = builder_->CreateLoad(usizeTy, iA, "sp.i");
                            auto* cc = builder_->CreateICmpULT(ci, lenAsUsize, "sp.cmp");
                            builder_->CreateCondBr(cc, bBB, eBB);
                            builder_->SetInsertPoint(bBB);
                            auto* av = builder_->CreateLoad(elemTy, accA, "sp.av");
                            auto* v = builder_->CreateLoad(elemTy,
                                builder_->CreateGEP(elemTy, dataPtr,
                                    builder_->CreateLoad(usizeTy, iA, "sp.ci"), "sp.v"), "sp.v");
                            auto* nv = elemTy->isIntegerTy()
                                ? static_cast<llvm::Value*>(builder_->CreateMul(av, v, "sp.nv"))
                                : static_cast<llvm::Value*>(builder_->CreateFMul(av, v, "sp.nv"));
                            builder_->CreateStore(nv, accA);
                            auto* n = builder_->CreateAdd(builder_->CreateLoad(usizeTy, iA, "sp.ni"),
                                llvm::ConstantInt::get(usizeTy, 1), "sp.n");
                            builder_->CreateStore(n, iA);
                            builder_->CreateBr(cBB);
                            builder_->SetInsertPoint(eBB);
                            return static_cast<llvm::Value*>(
                                builder_->CreateLoad(elemTy, accA, "sp.ret"));
                        }
                        // ── min / max on slice ────────────────────────────
                        if (methodName == "min" || methodName == "max") {
                            auto* bestA = builder_->CreateAlloca(elemTy, nullptr, "smm.best");
                            auto* z = llvm::ConstantInt::get(usizeTy, 0);
                            auto* firstPtr = builder_->CreateGEP(elemTy, dataPtr, z, "smm.first");
                            builder_->CreateStore(builder_->CreateLoad(elemTy, firstPtr, "smm.firstv"), bestA);
                            auto* cBB = llvm::BasicBlock::Create(*context_, "smm.cond", currentFunction_);
                            auto* bBB = llvm::BasicBlock::Create(*context_, "smm.body", currentFunction_);
                            auto* eBB = llvm::BasicBlock::Create(*context_, "smm.end", currentFunction_);
                            auto* iA = builder_->CreateAlloca(usizeTy, nullptr, "smm.idx");
                            builder_->CreateStore(llvm::ConstantInt::get(usizeTy, 1), iA);
                            builder_->CreateBr(cBB);
                            builder_->SetInsertPoint(cBB);
                            auto* ci = builder_->CreateLoad(usizeTy, iA, "smm.i");
                            auto* cc = builder_->CreateICmpULT(ci, lenAsUsize, "smm.cmp");
                            builder_->CreateCondBr(cc, bBB, eBB);
                            builder_->SetInsertPoint(bBB);
                            auto* ev = builder_->CreateLoad(elemTy,
                                builder_->CreateGEP(elemTy, dataPtr,
                                    builder_->CreateLoad(usizeTy, iA, "smm.ei"), "smm.ev"), "smm.ev");
                            auto* bv = builder_->CreateLoad(elemTy, bestA, "smm.bv");
                            llvm::Value* cmp;
                            if (methodName == "min") {
                                cmp = elemTy->isIntegerTy()
                                    ? static_cast<llvm::Value*>(builder_->CreateICmpSLT(ev, bv, "smm.lt"))
                                    : static_cast<llvm::Value*>(builder_->CreateFCmpOLT(ev, bv, "smm.lt"));
                            } else {
                                cmp = elemTy->isIntegerTy()
                                    ? static_cast<llvm::Value*>(builder_->CreateICmpSGT(ev, bv, "smm.gt"))
                                    : static_cast<llvm::Value*>(builder_->CreateFCmpOGT(ev, bv, "smm.gt"));
                            }
                            auto* sel = builder_->CreateSelect(cmp, ev, bv, "smm.sel");
                            builder_->CreateStore(sel, bestA);
                            auto* nx = builder_->CreateAdd(builder_->CreateLoad(usizeTy, iA, "smm.ni"),
                                llvm::ConstantInt::get(usizeTy, 1), "smm.nx");
                            builder_->CreateStore(nx, iA);
                            builder_->CreateBr(cBB);
                            builder_->SetInsertPoint(eBB);
                            return static_cast<llvm::Value*>(
                                builder_->CreateLoad(elemTy, bestA, "smm.ret"));
                        }
                        // ── minIndex / maxIndex on slice ──────────────────
                        if (methodName == "minIndex" || methodName == "maxIndex") {
                            auto* bestA = builder_->CreateAlloca(elemTy, nullptr, "smi.best");
                            auto* bestIdxA = builder_->CreateAlloca(usizeTy, nullptr, "smi.bi");
                            auto* z = llvm::ConstantInt::get(usizeTy, 0);
                            auto* firstPtr = builder_->CreateGEP(elemTy, dataPtr, z, "smi.first");
                            builder_->CreateStore(builder_->CreateLoad(elemTy, firstPtr, "smi.firstv"), bestA);
                            builder_->CreateStore(z, bestIdxA);
                            auto* cBB = llvm::BasicBlock::Create(*context_, "smi.cond", currentFunction_);
                            auto* bBB = llvm::BasicBlock::Create(*context_, "smi.body", currentFunction_);
                            auto* eBB = llvm::BasicBlock::Create(*context_, "smi.end", currentFunction_);
                            auto* iA = builder_->CreateAlloca(usizeTy, nullptr, "smi.idx");
                            builder_->CreateStore(llvm::ConstantInt::get(usizeTy, 1), iA);
                            builder_->CreateBr(cBB);
                            builder_->SetInsertPoint(cBB);
                            auto* ci = builder_->CreateLoad(usizeTy, iA, "smi.i");
                            auto* cc = builder_->CreateICmpULT(ci, lenAsUsize, "smi.cmp");
                            builder_->CreateCondBr(cc, bBB, eBB);
                            builder_->SetInsertPoint(bBB);
                            auto* ev = builder_->CreateLoad(elemTy,
                                builder_->CreateGEP(elemTy, dataPtr,
                                    builder_->CreateLoad(usizeTy, iA, "smi.ei"), "smi.ev"), "smi.ev");
                            auto* bv = builder_->CreateLoad(elemTy, bestA, "smi.bv");
                            llvm::Value* cmp;
                            if (methodName == "minIndex") {
                                cmp = elemTy->isIntegerTy()
                                    ? static_cast<llvm::Value*>(builder_->CreateICmpSLT(ev, bv, "smi.lt"))
                                    : static_cast<llvm::Value*>(builder_->CreateFCmpOLT(ev, bv, "smi.lt"));
                            } else {
                                cmp = elemTy->isIntegerTy()
                                    ? static_cast<llvm::Value*>(builder_->CreateICmpSGT(ev, bv, "smi.gt"))
                                    : static_cast<llvm::Value*>(builder_->CreateFCmpOGT(ev, bv, "smi.gt"));
                            }
                            auto* selV = builder_->CreateSelect(cmp, ev, bv, "smi.sv");
                            auto* selI = builder_->CreateSelect(cmp,
                                builder_->CreateLoad(usizeTy, iA, "smi.ci"),
                                builder_->CreateLoad(usizeTy, bestIdxA, "smi.cbi"), "smi.si");
                            builder_->CreateStore(selV, bestA);
                            builder_->CreateStore(selI, bestIdxA);
                            auto* nx = builder_->CreateAdd(builder_->CreateLoad(usizeTy, iA, "smi.ni"),
                                llvm::ConstantInt::get(usizeTy, 1), "smi.nx");
                            builder_->CreateStore(nx, iA);
                            builder_->CreateBr(cBB);
                            builder_->SetInsertPoint(eBB);
                            return static_cast<llvm::Value*>(
                                builder_->CreateLoad(usizeTy, bestIdxA, "smi.ret"));
                        }
                        // ── average on slice ──────────────────────────────
                        if (methodName == "average") {
                            auto* f64Ty = llvm::Type::getDoubleTy(*context_);
                            auto* accA = builder_->CreateAlloca(f64Ty, nullptr, "savg.acc");
                            builder_->CreateStore(llvm::ConstantFP::get(f64Ty, 0.0), accA);
                            auto* cBB = llvm::BasicBlock::Create(*context_, "savg.cond", currentFunction_);
                            auto* bBB = llvm::BasicBlock::Create(*context_, "savg.body", currentFunction_);
                            auto* eBB = llvm::BasicBlock::Create(*context_, "savg.end", currentFunction_);
                            auto* iA = builder_->CreateAlloca(usizeTy, nullptr, "savg.idx");
                            builder_->CreateStore(llvm::ConstantInt::get(usizeTy, 0), iA);
                            builder_->CreateBr(cBB);
                            builder_->SetInsertPoint(cBB);
                            auto* ci = builder_->CreateLoad(usizeTy, iA, "savg.i");
                            auto* cc = builder_->CreateICmpULT(ci, lenAsUsize, "savg.cmp");
                            builder_->CreateCondBr(cc, bBB, eBB);
                            builder_->SetInsertPoint(bBB);
                            auto* ev = builder_->CreateLoad(elemTy,
                                builder_->CreateGEP(elemTy, dataPtr,
                                    builder_->CreateLoad(usizeTy, iA, "savg.ei"), "savg.ev"), "savg.ev");
                            llvm::Value* fv;
                            if (elemTy->isIntegerTy())
                                fv = builder_->CreateSIToFP(ev, f64Ty, "savg.tof");
                            else
                                fv = builder_->CreateFPCast(ev, f64Ty, "savg.tof");
                            auto* av = builder_->CreateLoad(f64Ty, accA, "savg.av");
                            builder_->CreateStore(builder_->CreateFAdd(av, fv, "savg.add"), accA);
                            auto* nx = builder_->CreateAdd(builder_->CreateLoad(usizeTy, iA, "savg.ni"),
                                llvm::ConstantInt::get(usizeTy, 1), "savg.nx");
                            builder_->CreateStore(nx, iA);
                            builder_->CreateBr(cBB);
                            builder_->SetInsertPoint(eBB);
                            auto* sum = builder_->CreateLoad(f64Ty, accA, "savg.sum");
                            auto* lenF = builder_->CreateUIToFP(lenAsUsize, f64Ty, "savg.lenf");
                            return static_cast<llvm::Value*>(
                                builder_->CreateFDiv(sum, lenF, "savg.ret"));
                        }
                        // ── isSorted on slice ─────────────────────────────
                        if (methodName == "isSorted") {
                            auto* boolTy = llvm::Type::getInt1Ty(*context_);
                            auto* resA = builder_->CreateAlloca(boolTy, nullptr, "ssrt.res");
                            builder_->CreateStore(llvm::ConstantInt::get(boolTy, 1), resA);
                            auto* o1 = llvm::ConstantInt::get(usizeTy, 1);
                            auto* limit = builder_->CreateSub(lenAsUsize, o1, "ssrt.lim");
                            auto* cBB = llvm::BasicBlock::Create(*context_, "ssrt.cond", currentFunction_);
                            auto* bBB = llvm::BasicBlock::Create(*context_, "ssrt.body", currentFunction_);
                            auto* eBB = llvm::BasicBlock::Create(*context_, "ssrt.end", currentFunction_);
                            auto* iA = builder_->CreateAlloca(usizeTy, nullptr, "ssrt.idx");
                            builder_->CreateStore(llvm::ConstantInt::get(usizeTy, 0), iA);
                            builder_->CreateBr(cBB);
                            builder_->SetInsertPoint(cBB);
                            auto* ci = builder_->CreateLoad(usizeTy, iA, "ssrt.i");
                            auto* cc = builder_->CreateICmpULT(ci, limit, "ssrt.cmp");
                            builder_->CreateCondBr(cc, bBB, eBB);
                            builder_->SetInsertPoint(bBB);
                            auto* i = builder_->CreateLoad(usizeTy, iA, "ssrt.ci");
                            auto* ip1 = builder_->CreateAdd(i, o1, "ssrt.ip1");
                            auto* a = builder_->CreateLoad(elemTy,
                                builder_->CreateGEP(elemTy, dataPtr, i, "ssrt.a"), "ssrt.a");
                            auto* b = builder_->CreateLoad(elemTy,
                                builder_->CreateGEP(elemTy, dataPtr, ip1, "ssrt.b"), "ssrt.b");
                            llvm::Value* gt;
                            if (elemTy->isIntegerTy())
                                gt = builder_->CreateICmpSGT(a, b, "ssrt.gt");
                            else
                                gt = builder_->CreateFCmpOGT(a, b, "ssrt.gt");
                            auto* cur = builder_->CreateLoad(boolTy, resA, "ssrt.cur");
                            auto* notGt = builder_->CreateNot(gt, "ssrt.ngt");
                            auto* and_ = builder_->CreateAnd(cur, notGt, "ssrt.and");
                            builder_->CreateStore(and_, resA);
                            auto* ni = builder_->CreateAdd(i, o1, "ssrt.ni");
                            builder_->CreateStore(ni, iA);
                            builder_->CreateBr(cBB);
                            builder_->SetInsertPoint(eBB);
                            return static_cast<llvm::Value*>(
                                builder_->CreateLoad(boolTy, resA, "ssrt.ret"));
                        }
                        // ── sort / sortDesc on slice ──────────────────────
                        if (methodName == "sort" || methodName == "sortDesc") {
                            bool desc = (methodName == "sortDesc");
                            auto* o1 = llvm::ConstantInt::get(usizeTy, 1);
                            auto* o0 = llvm::ConstantInt::get(usizeTy, 0);
                            auto* outerA = builder_->CreateAlloca(usizeTy, nullptr, "ss.outer");
                            builder_->CreateStore(o0, outerA);
                            auto* ocBB = llvm::BasicBlock::Create(*context_, "ss.ocond", currentFunction_);
                            auto* obBB = llvm::BasicBlock::Create(*context_, "ss.obody", currentFunction_);
                            auto* oeBB = llvm::BasicBlock::Create(*context_, "ss.oend", currentFunction_);
                            builder_->CreateBr(ocBB);
                            builder_->SetInsertPoint(ocBB);
                            auto* oi = builder_->CreateLoad(usizeTy, outerA, "ss.oi");
                            auto* olim = builder_->CreateSub(lenAsUsize, o1, "ss.olim");
                            auto* oc = builder_->CreateICmpULT(oi, olim, "ss.oc");
                            builder_->CreateCondBr(oc, obBB, oeBB);
                            builder_->SetInsertPoint(obBB);
                            auto* innerA = builder_->CreateAlloca(usizeTy, nullptr, "ss.inner");
                            builder_->CreateStore(o0, innerA);
                            auto* icBB = llvm::BasicBlock::Create(*context_, "ss.icond", currentFunction_);
                            auto* ibBB = llvm::BasicBlock::Create(*context_, "ss.ibody", currentFunction_);
                            auto* swBB = llvm::BasicBlock::Create(*context_, "ss.swap", currentFunction_);
                            auto* inBB = llvm::BasicBlock::Create(*context_, "ss.inext", currentFunction_);
                            auto* ieBB = llvm::BasicBlock::Create(*context_, "ss.iend", currentFunction_);
                            builder_->CreateBr(icBB);
                            builder_->SetInsertPoint(icBB);
                            auto* ii = builder_->CreateLoad(usizeTy, innerA, "ss.ii");
                            auto* oov = builder_->CreateLoad(usizeTy, outerA, "ss.oo");
                            auto* ilim = builder_->CreateSub(
                                builder_->CreateSub(lenAsUsize, o1, "ss.t1"), oov, "ss.ilim");
                            auto* ic = builder_->CreateICmpULT(ii, ilim, "ss.ic");
                            builder_->CreateCondBr(ic, ibBB, ieBB);
                            builder_->SetInsertPoint(ibBB);
                            auto* j = builder_->CreateLoad(usizeTy, innerA, "ss.j");
                            auto* jp1 = builder_->CreateAdd(j, o1, "ss.jp1");
                            auto* vj = builder_->CreateLoad(elemTy,
                                builder_->CreateGEP(elemTy, dataPtr, j, "ss.vj"), "ss.vj");
                            auto* vj1 = builder_->CreateLoad(elemTy,
                                builder_->CreateGEP(elemTy, dataPtr, jp1, "ss.vj1"), "ss.vj1");
                            llvm::Value* shouldSwap;
                            if (!desc) {
                                if (elemTy->isIntegerTy())
                                    shouldSwap = builder_->CreateICmpSGT(vj, vj1, "ss.gt");
                                else
                                    shouldSwap = builder_->CreateFCmpOGT(vj, vj1, "ss.gt");
                            } else {
                                if (elemTy->isIntegerTy())
                                    shouldSwap = builder_->CreateICmpSLT(vj, vj1, "ss.lt");
                                else
                                    shouldSwap = builder_->CreateFCmpOLT(vj, vj1, "ss.lt");
                            }
                            builder_->CreateCondBr(shouldSwap, swBB, inBB);
                            builder_->SetInsertPoint(swBB);
                            auto* sj = builder_->CreateLoad(usizeTy, innerA, "ss.sj");
                            auto* sj1 = builder_->CreateAdd(sj, o1, "ss.sj1");
                            auto* gA = builder_->CreateGEP(elemTy, dataPtr, sj, "ss.ga");
                            auto* gB = builder_->CreateGEP(elemTy, dataPtr, sj1, "ss.gb");
                            auto* tA = builder_->CreateLoad(elemTy, gA, "ss.ta");
                            auto* tB = builder_->CreateLoad(elemTy, gB, "ss.tb");
                            builder_->CreateStore(tB, gA);
                            builder_->CreateStore(tA, gB);
                            builder_->CreateBr(inBB);
                            builder_->SetInsertPoint(inBB);
                            auto* ni = builder_->CreateAdd(
                                builder_->CreateLoad(usizeTy, innerA, "ss.ni"), o1, "ss.nx");
                            builder_->CreateStore(ni, innerA);
                            builder_->CreateBr(icBB);
                            builder_->SetInsertPoint(ieBB);
                            auto* no = builder_->CreateAdd(
                                builder_->CreateLoad(usizeTy, outerA, "ss.no"), o1, "ss.nox");
                            builder_->CreateStore(no, outerA);
                            builder_->CreateBr(ocBB);
                            builder_->SetInsertPoint(oeBB);
                            return static_cast<llvm::Value*>(
                                llvm::UndefValue::get(llvm::Type::getVoidTy(*context_)));
                        }
                        // ── copy on slice ────────────────────────────────
                        if (methodName == "copy") {
                            auto* ptrTy = llvm::PointerType::getUnqual(*context_);
                            auto* elemSz = llvm::ConstantInt::get(usizeTy,
                                module_->getDataLayout().getTypeAllocSize(elemTy));
                            auto* totalSz = builder_->CreateMul(elemSz, lenAsUsize, "scp.size");
                            auto allocFn = declareBuiltin("lucis_alloc", ptrTy, {usizeTy});
                            auto* rawMem = builder_->CreateCall(allocFn, {totalSz}, "scp.mem");
                            auto* typedPtr = builder_->CreateBitCast(rawMem, ptrFieldTy, "scp.typed");
                            auto* dstB = builder_->CreateBitCast(typedPtr, ptrTy, "scp.dstb");
                            auto* srcB = builder_->CreateBitCast(dataPtr, ptrTy, "scp.srcb");
                            auto memcpyFn = declareBuiltin("memcpy", ptrTy, {ptrTy, ptrTy, usizeTy});
                            builder_->CreateCall(memcpyFn, {dstB, srcB, totalSz}, "scp.cpy");
                            llvm::Value* result = llvm::UndefValue::get(sliceTy);
                            result = builder_->CreateInsertValue(result, typedPtr, 0);
                            result = builder_->CreateInsertValue(result, lenAsUsize, 1);
                            return static_cast<llvm::Value*>(result);
                        }
                        // ── slice(start, end) on slice ────────────────────
                        if (methodName == "slice") {
                            if (args.size() < 2) {
                                std::cerr << "lucis: slice() requires start and end arguments\n";
                                return static_cast<llvm::Value*>(
                                    llvm::UndefValue::get(sliceTy));
                            }
                            auto* ptrTy = llvm::PointerType::getUnqual(*context_);
                            auto* startIdx = args[0];
                            auto* endIdx = args[1];
                            if (startIdx->getType() != usizeTy)
                                startIdx = builder_->CreateIntCast(startIdx, usizeTy, true, "ssl.start");
                            if (endIdx->getType() != usizeTy)
                                endIdx = builder_->CreateIntCast(endIdx, usizeTy, true, "ssl.end");
                            emitBoundsCheck(endIdx, lenAsUsize, "ssl_end");
                            emitBoundsCheck(startIdx, endIdx, "ssl_start");
                            auto* newLen = builder_->CreateSub(endIdx, startIdx, "ssl.nlen");
                            auto* elemSz = llvm::ConstantInt::get(usizeTy,
                                module_->getDataLayout().getTypeAllocSize(elemTy));
                            auto* totalSz = builder_->CreateMul(elemSz, newLen, "ssl.size");
                            auto allocFn = declareBuiltin("lucis_alloc", ptrTy, {usizeTy});
                            auto* rawMem = builder_->CreateCall(allocFn, {totalSz}, "ssl.mem");
                            auto* typedPtr = builder_->CreateBitCast(rawMem, ptrFieldTy, "ssl.typed");
                            auto* srcBase = builder_->CreateGEP(elemTy, dataPtr, startIdx, "ssl.src");
                            auto* dstB = builder_->CreateBitCast(typedPtr, ptrTy, "ssl.dstb");
                            auto* srcB = builder_->CreateBitCast(srcBase, ptrTy, "ssl.srcb");
                            auto memcpyFn = declareBuiltin("memcpy", ptrTy, {ptrTy, ptrTy, usizeTy});
                            builder_->CreateCall(memcpyFn, {dstB, srcB, totalSz}, "ssl.cpy");
                            llvm::Value* result = llvm::UndefValue::get(sliceTy);
                            result = builder_->CreateInsertValue(result, typedPtr, 0);
                            result = builder_->CreateInsertValue(result, newLen, 1);
                            return static_cast<llvm::Value*>(result);
                        }
                        // ── toString on slice ─────────────────────────────
                        if (methodName == "toString") {
                            auto* ptrTy = llvm::PointerType::getUnqual(*context_);
                            auto* strTy = llvm::StructType::get(*context_, {ptrTy, usizeTy});
                            auto* i32Ty = llvm::Type::getInt32Ty(*context_);
                            auto* elemStructTy = llvm::dyn_cast<llvm::StructType>(elemTy);
                            bool elemIsStringLike = elemStructTy && elemStructTy->getNumElements() == 2 &&
                                elemStructTy->getElementType(0)->isPointerTy() &&
                                elemStructTy->getElementType(1)->isIntegerTy();
                            if (elemIsStringLike) {
                                auto* vecTy = getOrCreateVecStructType();
                                auto* vecA = builder_->CreateAlloca(vecTy, nullptr, "stos.vec");
                                auto* dg = builder_->CreateStructGEP(vecTy, vecA, 0, "stos.dg");
                                auto* lg = builder_->CreateStructGEP(vecTy, vecA, 1, "stos.lg");
                                auto* cg = builder_->CreateStructGEP(vecTy, vecA, 2, "stos.cg");
                                auto* dataCast = builder_->CreateBitCast(dataPtr, ptrTy, "stos.dc");
                                builder_->CreateStore(dataCast, dg);
                                builder_->CreateStore(lenAsUsize, lg);
                                builder_->CreateStore(lenAsUsize, cg);
                                auto callee = declareBuiltin("lucis_vec_toString_str", strTy, {ptrTy});
                                return static_cast<llvm::Value*>(
                                    builder_->CreateCall(callee, {vecA}, "stos.res"));
                            }
                            auto allocFn = declareBuiltin("lucis_allocString", ptrTy, {usizeTy});
                            auto* perElem = llvm::ConstantInt::get(usizeTy, 24);
                            auto* total = builder_->CreateMul(lenAsUsize, perElem, "stos.t");
                            total = builder_->CreateAdd(total, llvm::ConstantInt::get(usizeTy, 2), "stos.t2");
                            total = builder_->CreateAdd(total,
                                builder_->CreateMul(lenAsUsize, llvm::ConstantInt::get(usizeTy, 2), "stos.ts"), "stos.t3");
                            auto* buf = builder_->CreateCall(allocFn, {total}, "stos.buf");
                            auto snprintfFn = module_->getOrInsertFunction("snprintf",
                                llvm::FunctionType::get(i32Ty, {ptrTy, usizeTy, ptrTy}, true));
                            auto* openBr = builder_->CreateGlobalStringPtr("[", "stos.ob");
                            auto* offA = builder_->CreateAlloca(usizeTy, nullptr, "stos.off");
                            builder_->CreateStore(llvm::ConstantInt::get(usizeTy, 0), offA);
                            auto* w = builder_->CreateCall(snprintfFn, {buf, total, openBr}, "stos.w0");
                            builder_->CreateStore(builder_->CreateIntCast(w, usizeTy, false), offA);
                            std::string fmtElemStr;
                            if (elemTy->isIntegerTy())
                                fmtElemStr = (elemTI && !elemTI->isSigned) ? "%llu" : "%lld";
                            else if (elemTy->isFloatingPointTy())
                                fmtElemStr = "%.6g";
                            else
                                fmtElemStr = "<?>";
                            auto* fmtFirst = builder_->CreateGlobalStringPtr(fmtElemStr, "stos.f1");
                            auto* fmtLater = builder_->CreateGlobalStringPtr(
                                (std::string(", ") + fmtElemStr).c_str(), "stos.fn");
                            auto* o0 = llvm::ConstantInt::get(usizeTy, 0);
                            auto* o1 = llvm::ConstantInt::get(usizeTy, 1);
                            auto* cBB = llvm::BasicBlock::Create(*context_, "stos.cond", currentFunction_);
                            auto* bBB = llvm::BasicBlock::Create(*context_, "stos.body", currentFunction_);
                            auto* eBB = llvm::BasicBlock::Create(*context_, "stos.end", currentFunction_);
                            auto* iA = builder_->CreateAlloca(usizeTy, nullptr, "stos.idx");
                            builder_->CreateStore(o0, iA);
                            builder_->CreateBr(cBB);
                            builder_->SetInsertPoint(cBB);
                            auto* ci = builder_->CreateLoad(usizeTy, iA, "stos.i");
                            auto* cc = builder_->CreateICmpULT(ci, lenAsUsize, "stos.cmp");
                            builder_->CreateCondBr(cc, bBB, eBB);
                            builder_->SetInsertPoint(bBB);
                            auto* idx = builder_->CreateLoad(usizeTy, iA, "stos.ci");
                            auto* isFirst = builder_->CreateICmpEQ(idx, o0, "stos.fst");
                            auto* fmtSel = builder_->CreateSelect(isFirst, fmtFirst, fmtLater, "stos.fmt");
                            auto* elem = builder_->CreateLoad(elemTy,
                                builder_->CreateGEP(elemTy, dataPtr, idx, "stos.ep"), "stos.elem");
                            auto* curOff = builder_->CreateLoad(usizeTy, offA, "stos.co");
                            auto* curPtr = builder_->CreateGEP(
                                llvm::Type::getInt8Ty(*context_), buf, curOff, "stos.cur");
                            auto* rem = builder_->CreateSub(total, curOff, "stos.rem");
                            llvm::Value* printVal = elem;
                            if (elemTy->isIntegerTy())
                                printVal = builder_->CreateIntCast(elem, i64Ty,
                                    elemTI ? elemTI->isSigned : true, "stos.cast");
                            else if (elemTy->isFloatingPointTy())
                                printVal = builder_->CreateFPExt(elem,
                                    llvm::Type::getDoubleTy(*context_), "stos.fext");
                            else
                                printVal = builder_->CreateGlobalStringPtr("<?>", "stos.unk");
                            auto* wr = builder_->CreateCall(snprintfFn,
                                {curPtr, rem, fmtSel, printVal}, "stos.wr");
                            auto* newOff = builder_->CreateAdd(curOff,
                                builder_->CreateIntCast(wr, usizeTy, false), "stos.no");
                            builder_->CreateStore(newOff, offA);
                            auto* nxi = builder_->CreateAdd(idx, o1, "stos.ni");
                            builder_->CreateStore(nxi, iA);
                            builder_->CreateBr(cBB);
                            builder_->SetInsertPoint(eBB);
                            auto* closeBr = builder_->CreateGlobalStringPtr("]", "stos.cb");
                            auto* finalOff = builder_->CreateLoad(usizeTy, offA, "stos.fo");
                            auto* finalPtr = builder_->CreateGEP(
                                llvm::Type::getInt8Ty(*context_), buf, finalOff, "stos.fp");
                            auto* finalRem = builder_->CreateSub(total, finalOff, "stos.fr");
                            auto* wc = builder_->CreateCall(snprintfFn, {finalPtr, finalRem, closeBr}, "stos.wc");
                            auto* totalOff = builder_->CreateAdd(finalOff,
                                builder_->CreateIntCast(wc, usizeTy, false), "stos.to");
                            llvm::Value* result = llvm::UndefValue::get(strTy);
                            result = builder_->CreateInsertValue(result, buf, 0);
                            result = builder_->CreateInsertValue(result, totalOff, 1);
                            return static_cast<llvm::Value*>(result);
                        }
                        // ── join on slice ────────────────────────────────
                        if (methodName == "join") {
                            auto* ptrTy = llvm::PointerType::getUnqual(*context_);
                            auto* strTy = llvm::StructType::get(*context_, {ptrTy, usizeTy});
                            auto* i32Ty = llvm::Type::getInt32Ty(*context_);
                            llvm::Value* sepPtr = nullptr;
                            llvm::Value* sepLen = nullptr;
                            if (!args.empty()) {
                                sepPtr = builder_->CreateExtractValue(args[0], 0, "sjn.sep.ptr");
                                sepLen = builder_->CreateExtractValue(args[0], 1, "sjn.sep.len");
                            } else {
                                sepPtr = builder_->CreateGlobalStringPtr("", "sjn.empty");
                                sepLen = llvm::ConstantInt::get(usizeTy, 0);
                            }
                            if (sepLen->getType() != usizeTy)
                                sepLen = builder_->CreateIntCast(sepLen, usizeTy, false, "sjn.sep.cast");
                            auto* elemStructTy = llvm::dyn_cast<llvm::StructType>(elemTy);
                            bool elemIsStringLike = elemStructTy && elemStructTy->getNumElements() == 2 &&
                                elemStructTy->getElementType(0)->isPointerTy() &&
                                elemStructTy->getElementType(1)->isIntegerTy();
                            if (elemIsStringLike) {
                                auto* vecTy = getOrCreateVecStructType();
                                auto* vecA = builder_->CreateAlloca(vecTy, nullptr, "sjn.vec");
                                auto* dg = builder_->CreateStructGEP(vecTy, vecA, 0, "sjn.dg");
                                auto* lg = builder_->CreateStructGEP(vecTy, vecA, 1, "sjn.lg");
                                auto* cg = builder_->CreateStructGEP(vecTy, vecA, 2, "sjn.cg");
                                auto* dataCast = builder_->CreateBitCast(dataPtr, ptrTy, "sjn.dc");
                                builder_->CreateStore(dataCast, dg);
                                builder_->CreateStore(lenAsUsize, lg);
                                builder_->CreateStore(lenAsUsize, cg);
                                llvm::Value* sep = llvm::UndefValue::get(strTy);
                                sep = builder_->CreateInsertValue(sep, sepPtr, 0);
                                sep = builder_->CreateInsertValue(sep, sepLen, 1);
                                auto callee = declareBuiltin("lucis_vec_join_str", strTy, {ptrTy, strTy});
                                return static_cast<llvm::Value*>(
                                    builder_->CreateCall(callee, {vecA, sep}, "sjn.res"));
                            }
                            auto allocFn = declareBuiltin("lucis_allocString", ptrTy, {usizeTy});
                            auto* perElem = llvm::ConstantInt::get(usizeTy, 24);
                            auto* totalNoSep = builder_->CreateMul(lenAsUsize, perElem, "sjn.tns");
                            auto* o1 = llvm::ConstantInt::get(usizeTy, 1);
                            auto* sepCount = builder_->CreateSub(lenAsUsize, o1, "sjn.sc");
                            auto* sepSpace = builder_->CreateMul(sepCount, sepLen, "sjn.ss");
                            auto* total = builder_->CreateAdd(totalNoSep, sepSpace, "sjn.total");
                            auto* buf = builder_->CreateCall(allocFn, {total}, "sjn.buf");
                            auto snprintfFn = module_->getOrInsertFunction("snprintf",
                                llvm::FunctionType::get(i32Ty, {ptrTy, usizeTy, ptrTy}, true));
                            auto memcpyFn = declareBuiltin("memcpy", ptrTy, {ptrTy, ptrTy, usizeTy});
                            auto* offA = builder_->CreateAlloca(usizeTy, nullptr, "sjn.off");
                            builder_->CreateStore(llvm::ConstantInt::get(usizeTy, 0), offA);
                            std::string fmtElemStr;
                            if (elemTy->isIntegerTy())
                                fmtElemStr = (elemTI && !elemTI->isSigned) ? "%llu" : "%lld";
                            else if (elemTy->isFloatingPointTy())
                                fmtElemStr = "%.6g";
                            else
                                fmtElemStr = "<?>";
                            auto* fmtStr = builder_->CreateGlobalStringPtr(fmtElemStr, "sjn.fmt");
                            auto* o0 = llvm::ConstantInt::get(usizeTy, 0);
                            auto* cBB = llvm::BasicBlock::Create(*context_, "sjn.cond", currentFunction_);
                            auto* bBB = llvm::BasicBlock::Create(*context_, "sjn.body", currentFunction_);
                            auto* eBB = llvm::BasicBlock::Create(*context_, "sjn.end", currentFunction_);
                            auto* iA = builder_->CreateAlloca(usizeTy, nullptr, "sjn.idx");
                            builder_->CreateStore(o0, iA);
                            builder_->CreateBr(cBB);
                            builder_->SetInsertPoint(cBB);
                            auto* ci = builder_->CreateLoad(usizeTy, iA, "sjn.i");
                            auto* cc = builder_->CreateICmpULT(ci, lenAsUsize, "sjn.cmp");
                            builder_->CreateCondBr(cc, bBB, eBB);
                            builder_->SetInsertPoint(bBB);
                            auto* idx = builder_->CreateLoad(usizeTy, iA, "sjn.ci");
                            auto* isFirst = builder_->CreateICmpEQ(idx, o0, "sjn.fst");
                            auto* notFirstBB = llvm::BasicBlock::Create(*context_, "sjn.nfb", currentFunction_);
                            auto* writeBB = llvm::BasicBlock::Create(*context_, "sjn.wb", currentFunction_);
                            builder_->CreateCondBr(isFirst, writeBB, notFirstBB);
                            builder_->SetInsertPoint(notFirstBB);
                            auto* curOff = builder_->CreateLoad(usizeTy, offA, "sjn.co");
                            auto* dst = builder_->CreateGEP(
                                llvm::Type::getInt8Ty(*context_), buf, curOff, "sjn.dst");
                            builder_->CreateCall(memcpyFn, {dst, sepPtr, sepLen});
                            builder_->CreateStore(builder_->CreateAdd(curOff, sepLen, "sjn.no"), offA);
                            builder_->CreateBr(writeBB);
                            builder_->SetInsertPoint(writeBB);
                            auto* writeOff = builder_->CreateLoad(usizeTy, offA, "sjn.wo");
                            auto* writePtr = builder_->CreateGEP(
                                llvm::Type::getInt8Ty(*context_), buf, writeOff, "sjn.wp");
                            auto* rem = builder_->CreateSub(total, writeOff, "sjn.rem");
                            auto* elem = builder_->CreateLoad(elemTy,
                                builder_->CreateGEP(elemTy, dataPtr, idx, "sjn.ep"), "sjn.elem");
                            llvm::Value* printVal = elem;
                            if (elemTy->isIntegerTy())
                                printVal = builder_->CreateIntCast(elem, i64Ty,
                                    elemTI ? elemTI->isSigned : true, "sjn.cast");
                            else if (elemTy->isFloatingPointTy())
                                printVal = builder_->CreateFPExt(elem,
                                    llvm::Type::getDoubleTy(*context_), "sjn.fext");
                            else
                                printVal = builder_->CreateGlobalStringPtr("<?>", "sjn.unk");
                            auto* wr = builder_->CreateCall(snprintfFn,
                                {writePtr, rem, fmtStr, printVal}, "sjn.wr");
                            auto* newOff = builder_->CreateAdd(writeOff,
                                builder_->CreateIntCast(wr, usizeTy, false), "sjn.nwo");
                            builder_->CreateStore(newOff, offA);
                            auto* nxi = builder_->CreateAdd(idx, o1, "sjn.ni");
                            builder_->CreateStore(nxi, iA);
                            builder_->CreateBr(cBB);
                            builder_->SetInsertPoint(eBB);
                            auto* finalLen = builder_->CreateLoad(usizeTy, offA, "sjn.fl");
                            llvm::Value* result = llvm::UndefValue::get(strTy);
                            result = builder_->CreateInsertValue(result, buf, 0);
                            result = builder_->CreateInsertValue(result, finalLen, 1);
                            return static_cast<llvm::Value*>(result);
                        }
                    }
                }
            }
            if (!arrTy) {
                std::cerr << "lucis: expected array type for '" << receiverVarName << "'\n";
                return static_cast<llvm::Value*>(
                    llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
            }
            uint64_t arrLen = arrTy->getNumElements();
            auto* elemTy    = arrTy->getElementType();
            auto* i64Ty     = llvm::Type::getInt64Ty(*context_);
            auto* usizeTy   = module_->getDataLayout().getIntPtrType(*context_);
            // Helper: cast a value to elemTy if types differ
            auto castToElem = [&](llvm::Value* v) -> llvm::Value* {
                if (v->getType() == elemTy) return v;
                if (elemTy->isIntegerTy() && v->getType()->isIntegerTy())
                    return builder_->CreateIntCast(v, elemTy, true);
                if (elemTy->isFloatingPointTy() && v->getType()->isFloatingPointTy())
                    return builder_->CreateFPCast(v, elemTy);
                if (elemTy->isFloatingPointTy() && v->getType()->isIntegerTy())
                    return builder_->CreateSIToFP(v, elemTy);
                if (elemTy->isIntegerTy() && v->getType()->isFloatingPointTy())
                    return builder_->CreateFPToSI(v, elemTy);
                return v;
            };
            auto buildElemEq = [&](llvm::Value* lhs, llvm::Value* rhs,
                                   const std::string& name) -> llvm::Value* {
                if (elemTy->isIntegerTy())
                    return builder_->CreateICmpEQ(lhs, rhs, name);
                if (elemTy->isFloatingPointTy())
                    return builder_->CreateFCmpOEQ(lhs, rhs, name);
                if (elemTy->isPointerTy())
                    return builder_->CreateICmpEQ(lhs, rhs, name);
                auto* st = llvm::dyn_cast<llvm::StructType>(elemTy);
                if (st && st->getNumElements() == 2 &&
                    st->getElementType(0)->isPointerTy() &&
                    st->getElementType(1)->isIntegerTy()) {
                    auto* ptrTyCmp = llvm::PointerType::getUnqual(*context_);
                    auto* usizeTyCmp = module_->getDataLayout().getIntPtrType(*context_);
                    auto* i32TyCmp = llvm::Type::getInt32Ty(*context_);
                    auto* lhsPtr = builder_->CreateExtractValue(lhs, 0, name + ".lptr");
                    auto* lhsLen = builder_->CreateExtractValue(lhs, 1, name + ".llen");
                    auto* rhsPtr = builder_->CreateExtractValue(rhs, 0, name + ".rptr");
                    auto* rhsLen = builder_->CreateExtractValue(rhs, 1, name + ".rlen");
                    if (lhsLen->getType() != usizeTyCmp)
                        lhsLen = builder_->CreateIntCast(lhsLen, usizeTyCmp, false, name + ".llen.cast");
                    if (rhsLen->getType() != usizeTyCmp)
                        rhsLen = builder_->CreateIntCast(rhsLen, usizeTyCmp, false, name + ".rlen.cast");
                    auto calleeCmp = declareBuiltin("lucis_compareTo", i32TyCmp,
                                                    {ptrTyCmp, usizeTyCmp, ptrTyCmp, usizeTyCmp});
                    auto* cmp = builder_->CreateCall(calleeCmp,
                        {lhsPtr, lhsLen, rhsPtr, rhsLen}, name + ".cmp");
                    return builder_->CreateICmpEQ(cmp,
                        llvm::ConstantInt::get(i32TyCmp, 0), name + ".eq");
                }
                std::cerr << "lucis: unsupported array element comparison type\n";
                return llvm::ConstantInt::get(llvm::Type::getInt1Ty(*context_), 0);
            };
            if (methodName == "len") {
                return static_cast<llvm::Value*>(
                    llvm::ConstantInt::get(usizeTy, arrLen));
            }
            if (methodName == "isEmpty") {
                return static_cast<llvm::Value*>(
                    llvm::ConstantInt::get(llvm::Type::getInt1Ty(*context_), arrLen == 0));
            }
            if (methodName == "first") {
                auto* zero = llvm::ConstantInt::get(i64Ty, 0);
                auto* gep  = builder_->CreateInBoundsGEP(arrTy, arrAlloca, {zero, zero}, "first_ptr");
                return static_cast<llvm::Value*>(builder_->CreateLoad(elemTy, gep, "first"));
            }
            if (methodName == "last") {
                auto* zero = llvm::ConstantInt::get(i64Ty, 0);
                auto* last = llvm::ConstantInt::get(i64Ty, arrLen - 1);
                auto* gep  = builder_->CreateInBoundsGEP(arrTy, arrAlloca, {zero, last}, "last_ptr");
                return static_cast<llvm::Value*>(builder_->CreateLoad(elemTy, gep, "last"));
            }
            if (methodName == "at") {
                if (args.empty()) {
                    std::cerr << "lucis: at() requires an index argument\n";
                    return static_cast<llvm::Value*>(llvm::UndefValue::get(elemTy));
                }
                auto* idx  = args[0];
                if (idx->getType() != i64Ty)
                    idx = builder_->CreateIntCast(idx, i64Ty, true);
                auto* arrLenVal = llvm::ConstantInt::get(i64Ty, arrLen);
                emitBoundsCheck(idx, arrLenVal, "arr_at");
                auto* zero = llvm::ConstantInt::get(i64Ty, 0);
                auto* gep  = builder_->CreateInBoundsGEP(arrTy, arrAlloca, {zero, idx}, "at_ptr");
                return static_cast<llvm::Value*>(builder_->CreateLoad(elemTy, gep, "at"));
            }
            if (methodName == "contains") {
                if (args.empty()) {
                    std::cerr << "lucis: contains() requires a value argument\n";
                    return static_cast<llvm::Value*>(
                        llvm::ConstantInt::get(llvm::Type::getInt1Ty(*context_), 0));
                }
                auto* needle = castToElem(args[0]);
                auto* boolTy = llvm::Type::getInt1Ty(*context_);
                auto* resultAlloca = builder_->CreateAlloca(boolTy, nullptr, "contains_res");
                builder_->CreateStore(llvm::ConstantInt::get(boolTy, 0), resultAlloca);
                auto* condBB   = llvm::BasicBlock::Create(*context_, "contains.cond", currentFunction_);
                auto* bodyBB   = llvm::BasicBlock::Create(*context_, "contains.body", currentFunction_);
                auto* foundBB  = llvm::BasicBlock::Create(*context_, "contains.found", currentFunction_);
                auto* nextBB   = llvm::BasicBlock::Create(*context_, "contains.next", currentFunction_);
                auto* endBB    = llvm::BasicBlock::Create(*context_, "contains.end", currentFunction_);
                auto* idxAlloca = builder_->CreateAlloca(i64Ty, nullptr, "contains_idx");
                builder_->CreateStore(llvm::ConstantInt::get(i64Ty, 0), idxAlloca);
                builder_->CreateBr(condBB);
                builder_->SetInsertPoint(condBB);
                auto* idx  = builder_->CreateLoad(i64Ty, idxAlloca, "idx");
                auto* cond = builder_->CreateICmpULT(idx, llvm::ConstantInt::get(i64Ty, arrLen), "cmp");
                builder_->CreateCondBr(cond, bodyBB, endBB);
                builder_->SetInsertPoint(bodyBB);
                auto* curIdx = builder_->CreateLoad(i64Ty, idxAlloca, "idx");
                auto* zero   = llvm::ConstantInt::get(i64Ty, 0);
                auto* gep    = builder_->CreateInBoundsGEP(arrTy, arrAlloca, {zero, curIdx}, "elem_ptr");
                auto* elem   = builder_->CreateLoad(elemTy, gep, "elem");
                auto* eq = buildElemEq(elem, needle, "eq");
                builder_->CreateCondBr(eq, foundBB, nextBB);
                builder_->SetInsertPoint(foundBB);
                builder_->CreateStore(llvm::ConstantInt::get(boolTy, 1), resultAlloca);
                builder_->CreateBr(endBB);
                builder_->SetInsertPoint(nextBB);
                auto* nextIdx = builder_->CreateAdd(
                    builder_->CreateLoad(i64Ty, idxAlloca, "idx"),
                    llvm::ConstantInt::get(i64Ty, 1), "next_idx");
                builder_->CreateStore(nextIdx, idxAlloca);
                builder_->CreateBr(condBB);
                builder_->SetInsertPoint(endBB);
                return static_cast<llvm::Value*>(
                    builder_->CreateLoad(boolTy, resultAlloca, "contains"));
            }
            if (methodName == "indexOf" || methodName == "lastIndexOf") {
                if (args.empty()) {
                    std::cerr << "lucis: " << methodName << "() requires a value argument\n";
                    return static_cast<llvm::Value*>(
                        llvm::ConstantInt::get(i64Ty, -1));
                }
                auto* needle = castToElem(args[0]);
                auto* resultAlloca = builder_->CreateAlloca(i64Ty, nullptr, "indexOf_res");
                builder_->CreateStore(llvm::ConstantInt::get(i64Ty, -1ULL), resultAlloca);
                if (methodName == "indexOf") {
                    // Forward scan: 0..len, break on first match
                    auto* condBB = llvm::BasicBlock::Create(*context_, "indexOf.cond", currentFunction_);
                    auto* bodyBB = llvm::BasicBlock::Create(*context_, "indexOf.body", currentFunction_);
                    auto* foundBB = llvm::BasicBlock::Create(*context_, "indexOf.found", currentFunction_);
                    auto* nextBB = llvm::BasicBlock::Create(*context_, "indexOf.next", currentFunction_);
                    auto* endBB  = llvm::BasicBlock::Create(*context_, "indexOf.end", currentFunction_);
                    auto* idxAlloca = builder_->CreateAlloca(i64Ty, nullptr, "idx");
                    builder_->CreateStore(llvm::ConstantInt::get(i64Ty, 0), idxAlloca);
                    builder_->CreateBr(condBB);
                    builder_->SetInsertPoint(condBB);
                    auto* idx = builder_->CreateLoad(i64Ty, idxAlloca, "idx");
                    auto* cmp = builder_->CreateICmpULT(idx, llvm::ConstantInt::get(i64Ty, arrLen), "cmp");
                    builder_->CreateCondBr(cmp, bodyBB, endBB);
                    builder_->SetInsertPoint(bodyBB);
                    auto* curIdx = builder_->CreateLoad(i64Ty, idxAlloca, "idx");
                    auto* zero = llvm::ConstantInt::get(i64Ty, 0);
                    auto* gep = builder_->CreateInBoundsGEP(arrTy, arrAlloca, {zero, curIdx}, "elem_ptr");
                    auto* elem = builder_->CreateLoad(elemTy, gep, "elem");
                    auto* eq = buildElemEq(elem, needle, "eq");
                    builder_->CreateCondBr(eq, foundBB, nextBB);
                    builder_->SetInsertPoint(foundBB);
                    builder_->CreateStore(builder_->CreateLoad(i64Ty, idxAlloca, "idx"), resultAlloca);
                    builder_->CreateBr(endBB);
                    builder_->SetInsertPoint(nextBB);
                    auto* nextIdx = builder_->CreateAdd(
                        builder_->CreateLoad(i64Ty, idxAlloca, "idx"),
                        llvm::ConstantInt::get(i64Ty, 1), "next");
                    builder_->CreateStore(nextIdx, idxAlloca);
                    builder_->CreateBr(condBB);
                    builder_->SetInsertPoint(endBB);
                } else {
                    // Backward scan: len-1..0, break on first match
                    auto* condBB = llvm::BasicBlock::Create(*context_, "lastIdx.cond", currentFunction_);
                    auto* bodyBB = llvm::BasicBlock::Create(*context_, "lastIdx.body", currentFunction_);
                    auto* foundBB = llvm::BasicBlock::Create(*context_, "lastIdx.found", currentFunction_);
                    auto* nextBB = llvm::BasicBlock::Create(*context_, "lastIdx.next", currentFunction_);
                    auto* endBB  = llvm::BasicBlock::Create(*context_, "lastIdx.end", currentFunction_);
                    auto* idxAlloca = builder_->CreateAlloca(i64Ty, nullptr, "idx");
                    builder_->CreateStore(llvm::ConstantInt::get(i64Ty, arrLen), idxAlloca);
                    builder_->CreateBr(condBB);
                    builder_->SetInsertPoint(condBB);
                    auto* idx = builder_->CreateLoad(i64Ty, idxAlloca, "idx");
                    auto* cmp = builder_->CreateICmpUGT(idx, llvm::ConstantInt::get(i64Ty, 0), "cmp");
                    builder_->CreateCondBr(cmp, bodyBB, endBB);
                    builder_->SetInsertPoint(bodyBB);
                    auto* decIdx = builder_->CreateSub(
                        builder_->CreateLoad(i64Ty, idxAlloca, "idx"),
                        llvm::ConstantInt::get(i64Ty, 1), "dec");
                    builder_->CreateStore(decIdx, idxAlloca);
                    auto* zero = llvm::ConstantInt::get(i64Ty, 0);
                    auto* gep = builder_->CreateInBoundsGEP(arrTy, arrAlloca, {zero, decIdx}, "elem_ptr");
                    auto* elem = builder_->CreateLoad(elemTy, gep, "elem");
                    auto* eq = buildElemEq(elem, needle, "eq");
                    builder_->CreateCondBr(eq, foundBB, nextBB);
                    builder_->SetInsertPoint(foundBB);
                    builder_->CreateStore(builder_->CreateLoad(i64Ty, idxAlloca, "idx"), resultAlloca);
                    builder_->CreateBr(endBB);
                    builder_->SetInsertPoint(nextBB);
                    builder_->CreateBr(condBB);
                    builder_->SetInsertPoint(endBB);
                }
                return static_cast<llvm::Value*>(
                    builder_->CreateLoad(i64Ty, resultAlloca, methodName));
            }
            if (methodName == "count") {
                if (args.empty()) {
                    std::cerr << "lucis: count() requires a value argument\n";
                    return static_cast<llvm::Value*>(llvm::ConstantInt::get(usizeTy, 0));
                }
                auto* needle = castToElem(args[0]);
                auto* countAlloca = builder_->CreateAlloca(usizeTy, nullptr, "count_res");
                builder_->CreateStore(llvm::ConstantInt::get(usizeTy, 0), countAlloca);
                auto* condBB = llvm::BasicBlock::Create(*context_, "count.cond", currentFunction_);
                auto* bodyBB = llvm::BasicBlock::Create(*context_, "count.body", currentFunction_);
                auto* incBB  = llvm::BasicBlock::Create(*context_, "count.inc", currentFunction_);
                auto* nextBB = llvm::BasicBlock::Create(*context_, "count.next", currentFunction_);
                auto* endBB  = llvm::BasicBlock::Create(*context_, "count.end", currentFunction_);
                auto* idxAlloca = builder_->CreateAlloca(i64Ty, nullptr, "idx");
                builder_->CreateStore(llvm::ConstantInt::get(i64Ty, 0), idxAlloca);
                builder_->CreateBr(condBB);
                builder_->SetInsertPoint(condBB);
                auto* idx = builder_->CreateLoad(i64Ty, idxAlloca, "idx");
                auto* cmp = builder_->CreateICmpULT(idx, llvm::ConstantInt::get(i64Ty, arrLen), "cmp");
                builder_->CreateCondBr(cmp, bodyBB, endBB);
                builder_->SetInsertPoint(bodyBB);
                auto* curIdx = builder_->CreateLoad(i64Ty, idxAlloca, "idx");
                auto* zero = llvm::ConstantInt::get(i64Ty, 0);
                auto* gep = builder_->CreateInBoundsGEP(arrTy, arrAlloca, {zero, curIdx}, "elem_ptr");
                auto* elem = builder_->CreateLoad(elemTy, gep, "elem");
                auto* eq = buildElemEq(elem, needle, "eq");
                builder_->CreateCondBr(eq, incBB, nextBB);
                builder_->SetInsertPoint(incBB);
                auto* cur = builder_->CreateLoad(usizeTy, countAlloca, "cur");
                builder_->CreateStore(
                    builder_->CreateAdd(cur, llvm::ConstantInt::get(usizeTy, 1), "inc"),
                    countAlloca);
                builder_->CreateBr(nextBB);
                builder_->SetInsertPoint(nextBB);
                auto* nextIdx = builder_->CreateAdd(
                    builder_->CreateLoad(i64Ty, idxAlloca, "idx"),
                    llvm::ConstantInt::get(i64Ty, 1), "next");
                builder_->CreateStore(nextIdx, idxAlloca);
                builder_->CreateBr(condBB);
                builder_->SetInsertPoint(endBB);
                return static_cast<llvm::Value*>(
                    builder_->CreateLoad(usizeTy, countAlloca, "count"));
            }
            if (methodName == "fill") {
                if (args.empty()) {
                    std::cerr << "lucis: fill() requires a value argument\n";
                    return static_cast<llvm::Value*>(
                        llvm::UndefValue::get(llvm::Type::getVoidTy(*context_)));
                }
                auto* fillVal = castToElem(args[0]);
                for (uint64_t i = 0; i < arrLen; i++) {
                    auto* zero = llvm::ConstantInt::get(i64Ty, 0);
                    auto* idx  = llvm::ConstantInt::get(i64Ty, i);
                    auto* gep  = builder_->CreateInBoundsGEP(arrTy, arrAlloca, {zero, idx}, "fill_ptr");
                    builder_->CreateStore(fillVal, gep);
                }
                return static_cast<llvm::Value*>(
                    llvm::UndefValue::get(llvm::Type::getVoidTy(*context_)));
            }
            if (methodName == "swap") {
                if (args.size() < 2) {
                    std::cerr << "lucis: swap() requires two index arguments\n";
                    return static_cast<llvm::Value*>(
                        llvm::UndefValue::get(llvm::Type::getVoidTy(*context_)));
                }
                auto* idxA = args[0];
                auto* idxB = args[1];
                if (idxA->getType() != i64Ty) idxA = builder_->CreateIntCast(idxA, i64Ty, true);
                if (idxB->getType() != i64Ty) idxB = builder_->CreateIntCast(idxB, i64Ty, true);
                auto* zero = llvm::ConstantInt::get(i64Ty, 0);
                auto* gepA = builder_->CreateInBoundsGEP(arrTy, arrAlloca, {zero, idxA}, "swap_a");
                auto* gepB = builder_->CreateInBoundsGEP(arrTy, arrAlloca, {zero, idxB}, "swap_b");
                auto* valA = builder_->CreateLoad(elemTy, gepA, "a");
                auto* valB = builder_->CreateLoad(elemTy, gepB, "b");
                builder_->CreateStore(valB, gepA);
                builder_->CreateStore(valA, gepB);
                return static_cast<llvm::Value*>(
                    llvm::UndefValue::get(llvm::Type::getVoidTy(*context_)));
            }
            // ── reverse: reverse the array in-place, returns void ────────
            if (methodName == "reverse") {
                auto* loAlloca = builder_->CreateAlloca(i64Ty, nullptr, "rev_lo");
                auto* hiAlloca = builder_->CreateAlloca(i64Ty, nullptr, "rev_hi");
                builder_->CreateStore(llvm::ConstantInt::get(i64Ty, 0), loAlloca);
                builder_->CreateStore(llvm::ConstantInt::get(i64Ty, arrLen - 1), hiAlloca);
                auto* condBB = llvm::BasicBlock::Create(*context_, "rev.cond", currentFunction_);
                auto* bodyBB = llvm::BasicBlock::Create(*context_, "rev.body", currentFunction_);
                auto* endBB  = llvm::BasicBlock::Create(*context_, "rev.end", currentFunction_);
                builder_->CreateBr(condBB);
                builder_->SetInsertPoint(condBB);
                auto* lo = builder_->CreateLoad(i64Ty, loAlloca, "lo");
                auto* hi = builder_->CreateLoad(i64Ty, hiAlloca, "hi");
                builder_->CreateCondBr(builder_->CreateICmpULT(lo, hi), bodyBB, endBB);
                builder_->SetInsertPoint(bodyBB);
                auto* loV = builder_->CreateLoad(i64Ty, loAlloca, "lo");
                auto* hiV = builder_->CreateLoad(i64Ty, hiAlloca, "hi");
                auto* zero = llvm::ConstantInt::get(i64Ty, 0);
                auto* gepA = builder_->CreateInBoundsGEP(arrTy, arrAlloca, {zero, loV}, "rev_a");
                auto* gepB = builder_->CreateInBoundsGEP(arrTy, arrAlloca, {zero, hiV}, "rev_b");
                auto* a = builder_->CreateLoad(elemTy, gepA, "a");
                auto* b = builder_->CreateLoad(elemTy, gepB, "b");
                builder_->CreateStore(b, gepA);
                builder_->CreateStore(a, gepB);
                builder_->CreateStore(
                    builder_->CreateAdd(loV, llvm::ConstantInt::get(i64Ty, 1)), loAlloca);
                builder_->CreateStore(
                    builder_->CreateSub(hiV, llvm::ConstantInt::get(i64Ty, 1)), hiAlloca);
                builder_->CreateBr(condBB);
                builder_->SetInsertPoint(endBB);
                return static_cast<llvm::Value*>(
                    llvm::UndefValue::get(llvm::Type::getVoidTy(*context_)));
            }
            // ── copy: return a new array (heap alloc + memcpy + free) ─
            if (methodName == "copy") {
                auto* ptrTy = llvm::PointerType::getUnqual(*context_);
                auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
                auto* totalSz = llvm::ConstantInt::get(usizeTy,
                    module_->getDataLayout().getTypeAllocSize(arrTy));
                auto allocFn = declareBuiltin("lucis_alloc", ptrTy, {usizeTy});
                auto* rawMem = builder_->CreateCall(allocFn, {totalSz}, "copy.mem");
                auto* voidTy = llvm::Type::getVoidTy(*context_);
                auto memcpyFn = declareBuiltin("memcpy", voidTy, {ptrTy, ptrTy, usizeTy});
                auto* srcPtr = builder_->CreateBitCast(arrAlloca, ptrTy, "copy.src");
                builder_->CreateCall(memcpyFn, {rawMem, srcPtr, totalSz});
                auto* typedPtr = builder_->CreateBitCast(rawMem,
                    llvm::PointerType::getUnqual(arrTy), "copy.typed");
                auto* result = builder_->CreateLoad(arrTy, typedPtr, "copy_val");
                auto freeFn = declareBuiltin("lucis_free", voidTy, {ptrTy});
                builder_->CreateCall(freeFn, {rawMem});
                return static_cast<llvm::Value*>(result);
            }
            // ── slice(start, end): returns a fixed array when bounds are constant ──
            // If bounds are not compile-time constants, fall back to full-size copy.
            if (methodName == "slice") {
                if (args.size() < 2) {
                    std::cerr << "lucis: slice() requires start and end arguments\n";
                    return static_cast<llvm::Value*>(llvm::UndefValue::get(arrTy));
                }
                auto* startIdx = args[0];
                auto* endIdx   = args[1];
                if (startIdx->getType() != i64Ty)
                    startIdx = builder_->CreateIntCast(startIdx, i64Ty, true);
                if (endIdx->getType() != i64Ty)
                    endIdx = builder_->CreateIntCast(endIdx, i64Ty, true);
                auto* startC = llvm::dyn_cast<llvm::ConstantInt>(startIdx);
                auto* endC   = llvm::dyn_cast<llvm::ConstantInt>(endIdx);
                if (startC && endC) {
                    uint64_t s = startC->getZExtValue();
                    uint64_t e = endC->getZExtValue();
                    if (s > arrLen) s = arrLen;
                    if (e > arrLen) e = arrLen;
                    if (e < s) e = s;
                    uint64_t outLen = e - s;
                    auto* outArrTy = llvm::ArrayType::get(elemTy, outLen);
                    auto* ptrTy = llvm::PointerType::getUnqual(*context_);
                    auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
                    auto* totalSz = llvm::ConstantInt::get(usizeTy,
                        module_->getDataLayout().getTypeAllocSize(outArrTy));
                    auto allocFn = declareBuiltin("lucis_alloc", ptrTy, {usizeTy});
                    auto* rawMem = builder_->CreateCall(allocFn, {totalSz}, "slc.mem");
                    auto* voidTy = llvm::Type::getVoidTy(*context_);
                    auto memcpyFn = declareBuiltin("memcpy", voidTy, {ptrTy, ptrTy, usizeTy});
                    auto* srcBase = builder_->CreateGEP(arrTy, arrAlloca,
                        {llvm::ConstantInt::get(i64Ty, 0), startIdx}, "slc.src");
                    auto* dstPtr = builder_->CreateBitCast(rawMem, ptrTy, "slc.dst");
                    auto* srcPtr = builder_->CreateBitCast(srcBase, ptrTy, "slc.srcb");
                    builder_->CreateCall(memcpyFn, {dstPtr, srcPtr, totalSz});
                    auto* typedPtr = builder_->CreateBitCast(rawMem,
                        llvm::PointerType::getUnqual(outArrTy), "slc.typed");
                    auto* constResult = builder_->CreateLoad(outArrTy, typedPtr, "slice_const_val");
                    auto freeFn = declareBuiltin("lucis_free", voidTy, {ptrTy});
                    builder_->CreateCall(freeFn, {rawMem});
                    return static_cast<llvm::Value*>(constResult);
                }
                // Runtime-slice: heap alloc + memcpy from start..end
                auto* ptrTy = llvm::PointerType::getUnqual(*context_);
                auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
                auto* totalSz = llvm::ConstantInt::get(usizeTy,
                    module_->getDataLayout().getTypeAllocSize(arrTy));
                auto allocFn = declareBuiltin("lucis_alloc", ptrTy, {usizeTy});
                auto* rawMem = builder_->CreateCall(allocFn, {totalSz}, "sl.mem");
                auto* voidTy = llvm::Type::getVoidTy(*context_);
                auto memcpyFn = declareBuiltin("memcpy", voidTy, {ptrTy, ptrTy, usizeTy});
                auto* srcBase = builder_->CreateGEP(arrTy, arrAlloca,
                    {llvm::ConstantInt::get(i64Ty, 0), startIdx}, "sl.src");
                auto* dstPtr = builder_->CreateBitCast(rawMem, ptrTy, "sl.dst");
                auto* srcPtr = builder_->CreateBitCast(srcBase, ptrTy, "sl.srcb");
                builder_->CreateCall(memcpyFn, {dstPtr, srcPtr, totalSz});
                auto* typedPtr = builder_->CreateBitCast(rawMem,
                    llvm::PointerType::getUnqual(arrTy), "sl.typed");
                auto* sliceResult = builder_->CreateLoad(arrTy, typedPtr, "slice_val");
                auto freeFn = declareBuiltin("lucis_free", voidTy, {ptrTy});
                builder_->CreateCall(freeFn, {rawMem});
                return static_cast<llvm::Value*>(sliceResult);
            }
            // ── sum: accumulate all elements (numeric only) ──────────────
            if (methodName == "sum") {
                auto* accAlloca = builder_->CreateAlloca(elemTy, nullptr, "sum_acc");
                if (elemTy->isIntegerTy())
                    builder_->CreateStore(llvm::ConstantInt::get(elemTy, 0), accAlloca);
                else
                    builder_->CreateStore(llvm::ConstantFP::get(elemTy, 0.0), accAlloca);
                auto* zero = llvm::ConstantInt::get(i64Ty, 0);
                for (uint64_t i = 0; i < arrLen; i++) {
                    auto* idx = llvm::ConstantInt::get(i64Ty, i);
                    auto* gep = builder_->CreateInBoundsGEP(arrTy, arrAlloca, {zero, idx});
                    auto* elem = builder_->CreateLoad(elemTy, gep, "elem");
                    auto* acc = builder_->CreateLoad(elemTy, accAlloca, "acc");
                    llvm::Value* newAcc;
                    if (elemTy->isIntegerTy())
                        newAcc = builder_->CreateAdd(acc, elem, "sum");
                    else
                        newAcc = builder_->CreateFAdd(acc, elem, "sum");
                    builder_->CreateStore(newAcc, accAlloca);
                }
                return static_cast<llvm::Value*>(
                    builder_->CreateLoad(elemTy, accAlloca, "sum_val"));
            }
            // ── product: multiply all elements (numeric only) ────────────
            if (methodName == "product") {
                auto* accAlloca = builder_->CreateAlloca(elemTy, nullptr, "prod_acc");
                if (elemTy->isIntegerTy())
                    builder_->CreateStore(llvm::ConstantInt::get(elemTy, 1), accAlloca);
                else
                    builder_->CreateStore(llvm::ConstantFP::get(elemTy, 1.0), accAlloca);
                auto* zero = llvm::ConstantInt::get(i64Ty, 0);
                for (uint64_t i = 0; i < arrLen; i++) {
                    auto* idx = llvm::ConstantInt::get(i64Ty, i);
                    auto* gep = builder_->CreateInBoundsGEP(arrTy, arrAlloca, {zero, idx});
                    auto* elem = builder_->CreateLoad(elemTy, gep, "elem");
                    auto* acc = builder_->CreateLoad(elemTy, accAlloca, "acc");
                    llvm::Value* newAcc;
                    if (elemTy->isIntegerTy())
                        newAcc = builder_->CreateMul(acc, elem, "prod");
                    else
                        newAcc = builder_->CreateFMul(acc, elem, "prod");
                    builder_->CreateStore(newAcc, accAlloca);
                }
                return static_cast<llvm::Value*>(
                    builder_->CreateLoad(elemTy, accAlloca, "product_val"));
            }
            // ── min / max: scan for min or max element ───────────────────
            if (methodName == "min" || methodName == "max") {
                auto* bestAlloca = builder_->CreateAlloca(elemTy, nullptr, "best");
                auto* zero = llvm::ConstantInt::get(i64Ty, 0);
                auto* firstGep = builder_->CreateInBoundsGEP(arrTy, arrAlloca,
                    {zero, zero}, "first_ptr");
                builder_->CreateStore(builder_->CreateLoad(elemTy, firstGep, "first"), bestAlloca);
                for (uint64_t i = 1; i < arrLen; i++) {
                    auto* idx = llvm::ConstantInt::get(i64Ty, i);
                    auto* gep = builder_->CreateInBoundsGEP(arrTy, arrAlloca, {zero, idx});
                    auto* elem = builder_->CreateLoad(elemTy, gep, "elem");
                    auto* best = builder_->CreateLoad(elemTy, bestAlloca, "best");
                    llvm::Value* cmp;
                    if (methodName == "min") {
                        if (elemTy->isIntegerTy())
                            cmp = builder_->CreateICmpSLT(elem, best, "lt");
                        else
                            cmp = builder_->CreateFCmpOLT(elem, best, "lt");
                    } else {
                        if (elemTy->isIntegerTy())
                            cmp = builder_->CreateICmpSGT(elem, best, "gt");
                        else
                            cmp = builder_->CreateFCmpOGT(elem, best, "gt");
                    }
                    auto* sel = builder_->CreateSelect(cmp, elem, best, "sel");
                    builder_->CreateStore(sel, bestAlloca);
                }
                return static_cast<llvm::Value*>(
                    builder_->CreateLoad(elemTy, bestAlloca, methodName + "_val"));
            }
            // ── minIndex / maxIndex: return index of min/max element ─────
            if (methodName == "minIndex" || methodName == "maxIndex") {
                auto* bestAlloca = builder_->CreateAlloca(elemTy, nullptr, "best");
                auto* bestIdxAlloca = builder_->CreateAlloca(i64Ty, nullptr, "bestIdx");
                auto* zero = llvm::ConstantInt::get(i64Ty, 0);
                auto* firstGep = builder_->CreateInBoundsGEP(arrTy, arrAlloca,
                    {zero, zero}, "first_ptr");
                builder_->CreateStore(builder_->CreateLoad(elemTy, firstGep, "first"), bestAlloca);
                builder_->CreateStore(llvm::ConstantInt::get(i64Ty, 0), bestIdxAlloca);
                for (uint64_t i = 1; i < arrLen; i++) {
                    auto* idx = llvm::ConstantInt::get(i64Ty, i);
                    auto* gep = builder_->CreateInBoundsGEP(arrTy, arrAlloca, {zero, idx});
                    auto* elem = builder_->CreateLoad(elemTy, gep, "elem");
                    auto* best = builder_->CreateLoad(elemTy, bestAlloca, "best");
                    llvm::Value* cmp;
                    if (methodName == "minIndex") {
                        if (elemTy->isIntegerTy())
                            cmp = builder_->CreateICmpSLT(elem, best, "lt");
                        else
                            cmp = builder_->CreateFCmpOLT(elem, best, "lt");
                    } else {
                        if (elemTy->isIntegerTy())
                            cmp = builder_->CreateICmpSGT(elem, best, "gt");
                        else
                            cmp = builder_->CreateFCmpOGT(elem, best, "gt");
                    }
                    auto* sel = builder_->CreateSelect(cmp, elem, best, "selV");
                    auto* selI = builder_->CreateSelect(cmp, idx,
                        builder_->CreateLoad(i64Ty, bestIdxAlloca, "bi"), "selI");
                    builder_->CreateStore(sel, bestAlloca);
                    builder_->CreateStore(selI, bestIdxAlloca);
                }
                return static_cast<llvm::Value*>(
                    builder_->CreateLoad(i64Ty, bestIdxAlloca, methodName + "_val"));
            }
            // ── average: sum / len as float64 ────────────────────────────
            if (methodName == "average") {
                auto* f64Ty = llvm::Type::getDoubleTy(*context_);
                auto* accAlloca = builder_->CreateAlloca(f64Ty, nullptr, "avg_acc");
                builder_->CreateStore(llvm::ConstantFP::get(f64Ty, 0.0), accAlloca);
                auto* zero = llvm::ConstantInt::get(i64Ty, 0);
                for (uint64_t i = 0; i < arrLen; i++) {
                    auto* idx = llvm::ConstantInt::get(i64Ty, i);
                    auto* gep = builder_->CreateInBoundsGEP(arrTy, arrAlloca, {zero, idx});
                    auto* elem = builder_->CreateLoad(elemTy, gep, "elem");
                    llvm::Value* fElem;
                    if (elemTy->isIntegerTy())
                        fElem = builder_->CreateSIToFP(elem, f64Ty, "toF");
                    else
                        fElem = builder_->CreateFPCast(elem, f64Ty, "toF");
                    auto* acc = builder_->CreateLoad(f64Ty, accAlloca, "acc");
                    builder_->CreateStore(builder_->CreateFAdd(acc, fElem, "sum"), accAlloca);
                }
                auto* sum = builder_->CreateLoad(f64Ty, accAlloca, "sum");
                auto* lenF = llvm::ConstantFP::get(f64Ty, (double)arrLen);
                return static_cast<llvm::Value*>(
                    builder_->CreateFDiv(sum, lenF, "average"));
            }
            // ── isSorted: check if elements are in ascending order ───────
            if (methodName == "isSorted") {
                auto* boolTy = llvm::Type::getInt1Ty(*context_);
                if (arrLen <= 1) {
                    return static_cast<llvm::Value*>(
                        llvm::ConstantInt::get(boolTy, 1));
                }
                auto* resultAlloca = builder_->CreateAlloca(boolTy, nullptr, "sorted");
                builder_->CreateStore(llvm::ConstantInt::get(boolTy, 1), resultAlloca);
                auto* zero = llvm::ConstantInt::get(i64Ty, 0);
                for (uint64_t i = 0; i + 1 < arrLen; i++) {
                    auto* idxA = llvm::ConstantInt::get(i64Ty, i);
                    auto* idxB = llvm::ConstantInt::get(i64Ty, i + 1);
                    auto* gepA = builder_->CreateInBoundsGEP(arrTy, arrAlloca, {zero, idxA});
                    auto* gepB = builder_->CreateInBoundsGEP(arrTy, arrAlloca, {zero, idxB});
                    auto* a = builder_->CreateLoad(elemTy, gepA, "a");
                    auto* b = builder_->CreateLoad(elemTy, gepB, "b");
                    llvm::Value* gt;
                    if (elemTy->isIntegerTy())
                        gt = builder_->CreateICmpSGT(a, b, "gt");
                    else
                        gt = builder_->CreateFCmpOGT(a, b, "gt");
                    // If a > b, set result to false
                    auto* cur = builder_->CreateLoad(boolTy, resultAlloca, "cur");
                    auto* notGt = builder_->CreateNot(gt, "notGt");
                    builder_->CreateStore(builder_->CreateAnd(cur, notGt, "and"), resultAlloca);
                }
                return static_cast<llvm::Value*>(
                    builder_->CreateLoad(boolTy, resultAlloca, "isSorted"));
            }
            // ── sort / sortDesc: bubble sort in-place ────────────────────
            if (methodName == "sort" || methodName == "sortDesc") {
                bool desc = (methodName == "sortDesc");
                auto* zero = llvm::ConstantInt::get(i64Ty, 0);
                // Simple bubble sort for fixed-size arrays
                auto* outerAlloca = builder_->CreateAlloca(i64Ty, nullptr, "outer");
                builder_->CreateStore(llvm::ConstantInt::get(i64Ty, 0), outerAlloca);
                auto* outerCond = llvm::BasicBlock::Create(*context_, "sort.ocond", currentFunction_);
                auto* outerBody = llvm::BasicBlock::Create(*context_, "sort.obody", currentFunction_);
                auto* outerEnd  = llvm::BasicBlock::Create(*context_, "sort.oend", currentFunction_);
                builder_->CreateBr(outerCond);
                builder_->SetInsertPoint(outerCond);
                auto* outerI = builder_->CreateLoad(i64Ty, outerAlloca, "oi");
                builder_->CreateCondBr(
                    builder_->CreateICmpULT(outerI,
                        llvm::ConstantInt::get(i64Ty, arrLen > 0 ? arrLen - 1 : 0)),
                    outerBody, outerEnd);
                builder_->SetInsertPoint(outerBody);
                auto* innerAlloca = builder_->CreateAlloca(i64Ty, nullptr, "inner");
                builder_->CreateStore(llvm::ConstantInt::get(i64Ty, 0), innerAlloca);
                auto* innerCond = llvm::BasicBlock::Create(*context_, "sort.icond", currentFunction_);
                auto* innerBody = llvm::BasicBlock::Create(*context_, "sort.ibody", currentFunction_);
                auto* swapBB    = llvm::BasicBlock::Create(*context_, "sort.swap", currentFunction_);
                auto* innerNext = llvm::BasicBlock::Create(*context_, "sort.inext", currentFunction_);
                auto* innerEnd  = llvm::BasicBlock::Create(*context_, "sort.iend", currentFunction_);
                builder_->CreateBr(innerCond);
                builder_->SetInsertPoint(innerCond);
                auto* innerI = builder_->CreateLoad(i64Ty, innerAlloca, "ii");
                auto* outer  = builder_->CreateLoad(i64Ty, outerAlloca, "oi");
                auto* limit  = builder_->CreateSub(
                    llvm::ConstantInt::get(i64Ty, arrLen > 0 ? arrLen - 1 : 0), outer, "lim");
                builder_->CreateCondBr(
                    builder_->CreateICmpULT(innerI, limit), innerBody, innerEnd);
                builder_->SetInsertPoint(innerBody);
                auto* j   = builder_->CreateLoad(i64Ty, innerAlloca, "j");
                auto* j1  = builder_->CreateAdd(j, llvm::ConstantInt::get(i64Ty, 1), "j1");
                auto* gepJ  = builder_->CreateInBoundsGEP(arrTy, arrAlloca, {zero, j}, "gj");
                auto* gepJ1 = builder_->CreateInBoundsGEP(arrTy, arrAlloca, {zero, j1}, "gj1");
                auto* vJ  = builder_->CreateLoad(elemTy, gepJ, "vj");
                auto* vJ1 = builder_->CreateLoad(elemTy, gepJ1, "vj1");
                llvm::Value* shouldSwap;
                if (!desc) {
                    if (elemTy->isIntegerTy())
                        shouldSwap = builder_->CreateICmpSGT(vJ, vJ1, "gt");
                    else
                        shouldSwap = builder_->CreateFCmpOGT(vJ, vJ1, "gt");
                } else {
                    if (elemTy->isIntegerTy())
                        shouldSwap = builder_->CreateICmpSLT(vJ, vJ1, "lt");
                    else
                        shouldSwap = builder_->CreateFCmpOLT(vJ, vJ1, "lt");
                }
                builder_->CreateCondBr(shouldSwap, swapBB, innerNext);
                builder_->SetInsertPoint(swapBB);
                auto* sj  = builder_->CreateLoad(i64Ty, innerAlloca, "sj");
                auto* sj1 = builder_->CreateAdd(sj, llvm::ConstantInt::get(i64Ty, 1), "sj1");
                auto* gA  = builder_->CreateInBoundsGEP(arrTy, arrAlloca, {zero, sj}, "gA");
                auto* gB  = builder_->CreateInBoundsGEP(arrTy, arrAlloca, {zero, sj1}, "gB");
                auto* tmpA = builder_->CreateLoad(elemTy, gA, "tmpA");
                auto* tmpB = builder_->CreateLoad(elemTy, gB, "tmpB");
                builder_->CreateStore(tmpB, gA);
                builder_->CreateStore(tmpA, gB);
                builder_->CreateBr(innerNext);
                builder_->SetInsertPoint(innerNext);
                auto* nextInner = builder_->CreateAdd(
                    builder_->CreateLoad(i64Ty, innerAlloca, "ii"),
                    llvm::ConstantInt::get(i64Ty, 1), "nextI");
                builder_->CreateStore(nextInner, innerAlloca);
                builder_->CreateBr(innerCond);
                builder_->SetInsertPoint(innerEnd);
                auto* nextOuter = builder_->CreateAdd(
                    builder_->CreateLoad(i64Ty, outerAlloca, "oi"),
                    llvm::ConstantInt::get(i64Ty, 1), "nextO");
                builder_->CreateStore(nextOuter, outerAlloca);
                builder_->CreateBr(outerCond);
                builder_->SetInsertPoint(outerEnd);
                return static_cast<llvm::Value*>(
                    llvm::UndefValue::get(llvm::Type::getVoidTy(*context_)));
            }
            // ── equals: compare two arrays element-by-element ────────────
            if (methodName == "equals") {
                auto* boolTy = llvm::Type::getInt1Ty(*context_);
                if (args.empty()) {
                    return static_cast<llvm::Value*>(
                        llvm::ConstantInt::get(boolTy, 0));
                }
                if (ctx->argList() && !ctx->argList()->expression().empty()) {
                    if (auto* arrLit = dynamic_cast<LucisParser::ArrayLitExprContext*>(
                            ctx->argList()->expression()[0])) {
                        auto litElems = arrLit->expression();
                        if (litElems.size() != arrLen) {
                            return static_cast<llvm::Value*>(
                                llvm::ConstantInt::get(boolTy, 0));
                        }
                        auto* resultAlloca = builder_->CreateAlloca(boolTy, nullptr, "eq_lit_res");
                        builder_->CreateStore(llvm::ConstantInt::get(boolTy, 1), resultAlloca);
                        auto* zero = llvm::ConstantInt::get(i64Ty, 0);
                        for (uint64_t i = 0; i < arrLen; i++) {
                            auto* idx = llvm::ConstantInt::get(i64Ty, i);
                            auto* gepA = builder_->CreateInBoundsGEP(arrTy, arrAlloca, {zero, idx});
                            auto* a = builder_->CreateLoad(elemTy, gepA, "a");
                            auto* rhs = castValue(visit(litElems[i]));
                            rhs = castToElem(rhs);
                            auto* eq = buildElemEq(a, rhs, "eq");
                            auto* cur = builder_->CreateLoad(boolTy, resultAlloca, "cur");
                            builder_->CreateStore(builder_->CreateAnd(cur, eq, "and"), resultAlloca);
                        }
                        return static_cast<llvm::Value*>(
                            builder_->CreateLoad(boolTy, resultAlloca, "equals_lit"));
                    }
                }
                // The argument is another array value (loaded)
                auto* other = args[0];
                auto* otherAlloca = builder_->CreateAlloca(arrTy, nullptr, "eq_other");
                builder_->CreateStore(other, otherAlloca);
                auto* resultAlloca = builder_->CreateAlloca(boolTy, nullptr, "eq_res");
                builder_->CreateStore(llvm::ConstantInt::get(boolTy, 1), resultAlloca);
                auto* zero = llvm::ConstantInt::get(i64Ty, 0);
                for (uint64_t i = 0; i < arrLen; i++) {
                    auto* idx = llvm::ConstantInt::get(i64Ty, i);
                    auto* gepA = builder_->CreateInBoundsGEP(arrTy, arrAlloca, {zero, idx});
                    auto* gepB = builder_->CreateInBoundsGEP(arrTy, otherAlloca, {zero, idx});
                    auto* a = builder_->CreateLoad(elemTy, gepA, "a");
                    auto* b = builder_->CreateLoad(elemTy, gepB, "b");
                    auto* eq = buildElemEq(a, b, "eq");
                    auto* cur = builder_->CreateLoad(boolTy, resultAlloca, "cur");
                    builder_->CreateStore(builder_->CreateAnd(cur, eq, "and"), resultAlloca);
                }
                return static_cast<llvm::Value*>(
                    builder_->CreateLoad(boolTy, resultAlloca, "equals"));
            }
            // ── toString: format as "[a, b, c, ...]" ─────────────────────
            if (methodName == "toString") {
                auto* ptrTy  = llvm::PointerType::getUnqual(*context_);
                auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
                auto* strTy  = llvm::StructType::get(*context_, {ptrTy, usizeTy});
                auto* i32T   = llvm::Type::getInt32Ty(*context_);
                auto buildVecHeaderForArray = [&](const std::string& name) -> llvm::Value* {
                    auto* vecTy = getOrCreateVecStructType();
                    auto* vecAlloca = builder_->CreateAlloca(vecTy, nullptr, name);
                    llvm::Value* dataPtr = nullptr;
                    if (arrLen > 0) {
                        auto* zero = llvm::ConstantInt::get(i64Ty, 0);
                        dataPtr = builder_->CreateInBoundsGEP(arrTy, arrAlloca, {zero, zero},
                            name + ".data.ptr");
                    } else {
                        dataPtr = llvm::ConstantPointerNull::get(
                            llvm::cast<llvm::PointerType>(ptrTy));
                    }
                    if (dataPtr->getType() != ptrTy)
                        dataPtr = builder_->CreateBitCast(dataPtr, ptrTy, name + ".data.cast");
                    auto* lenVal = llvm::ConstantInt::get(usizeTy, arrLen);
                    auto* capVal = llvm::ConstantInt::get(usizeTy, arrLen);
                    auto* dataGep = builder_->CreateStructGEP(vecTy, vecAlloca, 0, name + ".data.gep");
                    auto* lenGep = builder_->CreateStructGEP(vecTy, vecAlloca, 1, name + ".len.gep");
                    auto* capGep = builder_->CreateStructGEP(vecTy, vecAlloca, 2, name + ".cap.gep");
                    builder_->CreateStore(dataPtr, dataGep);
                    builder_->CreateStore(lenVal, lenGep);
                    builder_->CreateStore(capVal, capGep);
                    return vecAlloca;
                };
                auto* elemStructTy = llvm::dyn_cast<llvm::StructType>(elemTy);
                bool elemIsStringLike = elemStructTy && elemStructTy->getNumElements() == 2 &&
                                        elemStructTy->getElementType(0)->isPointerTy() &&
                                        elemStructTy->getElementType(1)->isIntegerTy();
                if (elemIsStringLike) {
                    auto* vecArg = buildVecHeaderForArray("arr_tostr_vec");
                    auto callee = declareBuiltin("lucis_vec_toString_str", strTy, {ptrTy});
                    return static_cast<llvm::Value*>(
                        builder_->CreateCall(callee, {vecArg}, "arr_tostr_str"));
                }
                auto mallocCallee = declareBuiltin("lucis_allocString", ptrTy, {usizeTy});
                auto* bufSize = llvm::ConstantInt::get(usizeTy, arrLen * 24 + 4);
                std::vector<llvm::Value*> mallocArgs = {bufSize};
                auto* buf = builder_->CreateCall(mallocCallee, mallocArgs, "buf");
                auto snprintfCallee = module_->getOrInsertFunction("snprintf",
                    llvm::FunctionType::get(i32T, {ptrTy, usizeTy, ptrTy}, true));
                auto* openBracket = builder_->CreateGlobalStringPtr("[", "open");
                auto* offsetAlloca = builder_->CreateAlloca(usizeTy, nullptr, "off");
                builder_->CreateStore(llvm::ConstantInt::get(usizeTy, 0), offsetAlloca);
                std::vector<llvm::Value*> snArgs = {buf, bufSize, openBracket};
                auto* wrote = builder_->CreateCall(snprintfCallee, snArgs, "wrote");
                builder_->CreateStore(
                    builder_->CreateIntCast(wrote, usizeTy, false), offsetAlloca);
                auto* zero = llvm::ConstantInt::get(i64Ty, 0);
                std::string fmtElem;
                if (elemTy->isIntegerTy())
                    fmtElem = (receiverTI && !receiverTI->isSigned) ? "%llu" : "%lld";
                else
                    fmtElem = "%.6g";
                for (uint64_t i = 0; i < arrLen; i++) {
                    std::string fmt = (i > 0) ? (", " + fmtElem) : fmtElem;
                    auto* fmtStr = builder_->CreateGlobalStringPtr(fmt, "fmt");
                    auto* off = builder_->CreateLoad(usizeTy, offsetAlloca, "off");
                    auto* ptr = builder_->CreateInBoundsGEP(
                        llvm::Type::getInt8Ty(*context_), buf, off, "cur");
                    auto* rem = builder_->CreateSub(bufSize, off, "rem");
                    auto* idx = llvm::ConstantInt::get(i64Ty, i);
                    auto* gep = builder_->CreateInBoundsGEP(arrTy, arrAlloca, {zero, idx});
                    auto* elem = builder_->CreateLoad(elemTy, gep, "elem");
                    llvm::Value* printVal = elem;
                    if (elemTy->isIntegerTy())
                        printVal = builder_->CreateIntCast(elem, i64Ty,
                            receiverTI ? receiverTI->isSigned : true, "toI64");
                    else if (elemTy->isFloatTy())
                        printVal = builder_->CreateFPExt(elem,
                            llvm::Type::getDoubleTy(*context_), "toD");
                    else
                        printVal = builder_->CreateGlobalStringPtr("<?>", "unk");
                    std::vector<llvm::Value*> fmtArgs = {ptr, rem, fmtStr, printVal};
                    auto* w = builder_->CreateCall(snprintfCallee, fmtArgs, "w");
                    auto* newOff = builder_->CreateAdd(off,
                        builder_->CreateIntCast(w, usizeTy, false), "newOff");
                    builder_->CreateStore(newOff, offsetAlloca);
                }
                auto* closeFmt = builder_->CreateGlobalStringPtr("]", "close");
                auto* off = builder_->CreateLoad(usizeTy, offsetAlloca, "off");
                auto* ptr = builder_->CreateInBoundsGEP(
                    llvm::Type::getInt8Ty(*context_), buf, off, "cur");
                auto* rem = builder_->CreateSub(bufSize, off, "rem");
                std::vector<llvm::Value*> closeArgs = {ptr, rem, closeFmt};
                auto* w = builder_->CreateCall(snprintfCallee, closeArgs, "w");
                auto* finalOff = builder_->CreateAdd(off,
                    builder_->CreateIntCast(w, usizeTy, false), "finalOff");
                llvm::Value* s = llvm::UndefValue::get(strTy);
                s = builder_->CreateInsertValue(s, buf, 0);
                s = builder_->CreateInsertValue(s, finalOff, 1);
                return static_cast<llvm::Value*>(s);
            }
            // ── join(separator): join elements as string ─────────────────
            if (methodName == "join") {
                auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
                auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
                auto* strTy   = llvm::StructType::get(*context_, {ptrTy, usizeTy});
                auto* i32T    = llvm::Type::getInt32Ty(*context_);
                auto buildVecHeaderForArray = [&](const std::string& name) -> llvm::Value* {
                    auto* vecTy = getOrCreateVecStructType();
                    auto* vecAlloca = builder_->CreateAlloca(vecTy, nullptr, name);
                    llvm::Value* dataPtr = nullptr;
                    if (arrLen > 0) {
                        auto* zero = llvm::ConstantInt::get(i64Ty, 0);
                        dataPtr = builder_->CreateInBoundsGEP(arrTy, arrAlloca, {zero, zero},
                            name + ".data.ptr");
                    } else {
                        dataPtr = llvm::ConstantPointerNull::get(
                            llvm::cast<llvm::PointerType>(ptrTy));
                    }
                    if (dataPtr->getType() != ptrTy)
                        dataPtr = builder_->CreateBitCast(dataPtr, ptrTy, name + ".data.cast");
                    auto* lenVal = llvm::ConstantInt::get(usizeTy, arrLen);
                    auto* capVal = llvm::ConstantInt::get(usizeTy, arrLen);
                    auto* dataGep = builder_->CreateStructGEP(vecTy, vecAlloca, 0, name + ".data.gep");
                    auto* lenGep = builder_->CreateStructGEP(vecTy, vecAlloca, 1, name + ".len.gep");
                    auto* capGep = builder_->CreateStructGEP(vecTy, vecAlloca, 2, name + ".cap.gep");
                    builder_->CreateStore(dataPtr, dataGep);
                    builder_->CreateStore(lenVal, lenGep);
                    builder_->CreateStore(capVal, capGep);
                    return vecAlloca;
                };
                llvm::Value* sepPtr = nullptr;
                llvm::Value* sepLen = nullptr;
                if (!args.empty()) {
                    sepPtr = builder_->CreateExtractValue(args[0], 0, "sep_ptr");
                    sepLen = builder_->CreateExtractValue(args[0], 1, "sep_len");
                } else {
                    sepPtr = builder_->CreateGlobalStringPtr("", "empty");
                    sepLen = llvm::ConstantInt::get(usizeTy, 0);
                }
                if (sepLen->getType() != usizeTy)
                    sepLen = builder_->CreateIntCast(sepLen, usizeTy, false);
                auto* elemStructTy = llvm::dyn_cast<llvm::StructType>(elemTy);
                bool elemIsStringLike = elemStructTy && elemStructTy->getNumElements() == 2 &&
                                        elemStructTy->getElementType(0)->isPointerTy() &&
                                        elemStructTy->getElementType(1)->isIntegerTy();
                if (elemIsStringLike) {
                    llvm::Value* sep = llvm::UndefValue::get(strTy);
                    sep = builder_->CreateInsertValue(sep, sepPtr, 0);
                    sep = builder_->CreateInsertValue(sep, sepLen, 1);
                    auto* vecArg = buildVecHeaderForArray("arr_join_vec");
                    auto callee = declareBuiltin("lucis_vec_join_str", strTy, {ptrTy, strTy});
                    return static_cast<llvm::Value*>(
                        builder_->CreateCall(callee, {vecArg, sep}, "arr_join_str"));
                }
                auto mallocCallee = declareBuiltin("lucis_allocString", ptrTy, {usizeTy});
                auto* bufSize = llvm::ConstantInt::get(usizeTy, arrLen * 24 + arrLen * 16 + 4);
                std::vector<llvm::Value*> mallocArgs = {bufSize};
                auto* buf = builder_->CreateCall(mallocCallee, mallocArgs, "joinbuf");
                auto snprintfCallee = module_->getOrInsertFunction("snprintf",
                    llvm::FunctionType::get(i32T, {ptrTy, usizeTy, ptrTy}, true));
                auto memcpyCallee = declareBuiltin("memcpy", ptrTy,
                    {ptrTy, ptrTy, usizeTy});
                auto* offsetAlloca = builder_->CreateAlloca(usizeTy, nullptr, "joff");
                builder_->CreateStore(llvm::ConstantInt::get(usizeTy, 0), offsetAlloca);
                auto* zero = llvm::ConstantInt::get(i64Ty, 0);
                std::string fmtElem;
                if (elemTy->isIntegerTy())
                    fmtElem = (receiverTI && !receiverTI->isSigned) ? "%llu" : "%lld";
                else if (elemTy->isFloatingPointTy())
                    fmtElem = "%.6g";
                else
                    fmtElem = "%s";
                for (uint64_t i = 0; i < arrLen; i++) {
                    if (i > 0) {
                        auto* curOff = builder_->CreateLoad(usizeTy, offsetAlloca, "co");
                        auto* dst = builder_->CreateInBoundsGEP(
                            llvm::Type::getInt8Ty(*context_), buf, curOff, "dst");
                        std::vector<llvm::Value*> cpyArgs = {dst, sepPtr, sepLen};
                        builder_->CreateCall(memcpyCallee, cpyArgs);
                        builder_->CreateStore(
                            builder_->CreateAdd(curOff, sepLen, "no"), offsetAlloca);
                    }
                    auto* fmtStr = builder_->CreateGlobalStringPtr(fmtElem, "jfmt");
                    auto* off = builder_->CreateLoad(usizeTy, offsetAlloca, "off");
                    auto* idx = llvm::ConstantInt::get(i64Ty, i);
                    auto* gep = builder_->CreateInBoundsGEP(arrTy, arrAlloca, {zero, idx});
                    auto* elem = builder_->CreateLoad(elemTy, gep, "elem");
                    auto* cur = builder_->CreateInBoundsGEP(llvm::Type::getInt8Ty(*context_), buf, off, "cur");
                    auto* rem = builder_->CreateSub(bufSize, off, "rem");
                    llvm::Value* printVal = elem;
                    if (elemTy->isIntegerTy())
                        printVal = builder_->CreateIntCast(elem, i64Ty,
                            receiverTI ? receiverTI->isSigned : true, "toI64");
                    else if (elemTy->isFloatTy())
                        printVal = builder_->CreateFPExt(elem,
                            llvm::Type::getDoubleTy(*context_), "toD");
                    else
                        printVal = builder_->CreateGlobalStringPtr("<?>", "unk");
                    std::vector<llvm::Value*> fmtArgs = {cur, rem, fmtStr, printVal};
                    auto* w = builder_->CreateCall(snprintfCallee, fmtArgs, "w");
                    auto* newOff = builder_->CreateAdd(off,
                        builder_->CreateIntCast(w, usizeTy, false), "newOff");
                    builder_->CreateStore(newOff, offsetAlloca);
                }
                auto* finalLen = builder_->CreateLoad(usizeTy, offsetAlloca, "finalLen");
                llvm::Value* s = llvm::UndefValue::get(strTy);
                s = builder_->CreateInsertValue(s, buf, 0);
                s = builder_->CreateInsertValue(s, finalLen, 1);
                return static_cast<llvm::Value*>(s);
            }
            // ── rotate(n): positive rotates right, negative rotates left ───
            if (methodName == "rotate") {
                if (args.empty() || arrLen <= 1) {
                    return static_cast<llvm::Value*>(
                        llvm::UndefValue::get(llvm::Type::getVoidTy(*context_)));
                }
                auto* n = args[0];
                if (n->getType() != i64Ty)
                    n = builder_->CreateIntCast(n, i64Ty, true);
                auto* lenConst = llvm::ConstantInt::get(i64Ty, arrLen);
                auto* nMod = builder_->CreateSRem(n, lenConst, "normN");
                auto* neg = builder_->CreateICmpSLT(nMod, llvm::ConstantInt::get(i64Ty, 0));
                n = builder_->CreateSelect(neg,
                                           builder_->CreateAdd(nMod, lenConst),
                                           nMod,
                                           "normPos");
                // Use temp buffer: copy all to temp, then write back rotated
                auto* tmpAlloca = builder_->CreateAlloca(arrTy, nullptr, "rot_tmp");
                auto* zero = llvm::ConstantInt::get(i64Ty, 0);
                // Copy original to temp
                for (uint64_t i = 0; i < arrLen; i++) {
                    auto* idx = llvm::ConstantInt::get(i64Ty, i);
                    auto* srcGep = builder_->CreateInBoundsGEP(arrTy, arrAlloca, {zero, idx});
                    auto* dstGep = builder_->CreateInBoundsGEP(arrTy, tmpAlloca, {zero, idx});
                    builder_->CreateStore(builder_->CreateLoad(elemTy, srcGep), dstGep);
                }
                // Write back rotated right: arr[i] = tmp[(i + len - n) % len]
                for (uint64_t i = 0; i < arrLen; i++) {
                    auto* idx = llvm::ConstantInt::get(i64Ty, i);
                    auto* srcIdx = builder_->CreateURem(
                        builder_->CreateAdd(
                            builder_->CreateSub(idx, n), lenConst),
                        lenConst,
                        "ri");
                    auto* srcGep = builder_->CreateInBoundsGEP(arrTy, tmpAlloca, {zero, srcIdx});
                    auto* dstGep = builder_->CreateInBoundsGEP(arrTy, arrAlloca, {zero, idx});
                    builder_->CreateStore(builder_->CreateLoad(elemTy, srcGep), dstGep);
                }
                return static_cast<llvm::Value*>(
                    llvm::UndefValue::get(llvm::Type::getVoidTy(*context_)));
            }
        }
    }
    if (!receiverVal)
        receiverVal = castValue(visit(ctx->expression()));
    // ── Primitive type method codegen ─────────────────────────────────
    {
        auto tag = desc->emitTag;
        auto* recvTy = receiverVal->getType();
        auto* i1Ty   = llvm::Type::getInt1Ty(*context_);
        auto* i8Ty   = llvm::Type::getInt8Ty(*context_);
        auto* i32Ty  = llvm::Type::getInt32Ty(*context_);
        auto* i64Ty  = llvm::Type::getInt64Ty(*context_);
        auto* f32Ty  = llvm::Type::getFloatTy(*context_);
        auto* f64Ty  = llvm::Type::getDoubleTy(*context_);
        auto* ptrTy  = llvm::PointerType::getUnqual(*context_);
        auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
        auto* strTy  = llvm::StructType::get(*context_, {ptrTy, usizeTy});
        bool  isSigned = receiverTI ? receiverTI->isSigned : false;
        // Helper: cast arg to receiver type
        auto castArg = [&](llvm::Value* v) -> llvm::Value* {
            if (v->getType() == recvTy) return v;
            if (recvTy->isIntegerTy() && v->getType()->isIntegerTy())
                return builder_->CreateIntCast(v, recvTy, isSigned);
            if (recvTy->isFloatingPointTy() && v->getType()->isFloatingPointTy())
                return builder_->CreateFPCast(v, recvTy);
            return v;
        };
        // Helper: get LLVM intrinsic function
        auto getIntrinsic = [&](llvm::Intrinsic::ID id,
                                llvm::ArrayRef<llvm::Type*> tys = {}) -> llvm::Function* {
            auto* fnTy = llvm::Intrinsic::getType(*context_, id, tys);
            auto name = llvm::Intrinsic::getName(id, tys, module_, fnTy);
            auto callee = module_->getOrInsertFunction(name, fnTy);
            return llvm::cast<llvm::Function>(callee.getCallee());
        };
        auto callUnaryMath = [&](const char* f32Name,
                                 const char* f64Name,
                                 llvm::Value* x,
                                 const std::string& name) -> llvm::Value* {
            const char* fn = x->getType()->isFloatTy() ? f32Name : f64Name;
            auto callee = declareBuiltin(fn, x->getType(), {x->getType()});
            return builder_->CreateCall(callee, {x}, name);
        };
        auto callBinaryMath = [&](const char* f32Name,
                                  const char* f64Name,
                                  llvm::Value* a,
                                  llvm::Value* b,
                                  const std::string& name) -> llvm::Value* {
            const char* fn = a->getType()->isFloatTy() ? f32Name : f64Name;
            auto callee = declareBuiltin(fn, a->getType(), {a->getType(), a->getType()});
            return builder_->CreateCall(callee, {a, b}, name);
        };
        // ── Integer methods ──────────────────────────────────────────────
        if (tag == "int.abs") {
            auto* fn = getIntrinsic(llvm::Intrinsic::abs, {recvTy});
            return static_cast<llvm::Value*>(
                builder_->CreateCall(fn, {receiverVal, builder_->getFalse()}, "abs"));
        }
        if (tag == "int.max") {
            auto id = isSigned ? llvm::Intrinsic::smax : llvm::Intrinsic::umax;
            auto* fn = getIntrinsic(id, {recvTy});
            return static_cast<llvm::Value*>(
                builder_->CreateCall(fn, {receiverVal, castArg(args[0])}, "max"));
        }
        if (tag == "int.min") {
            auto id = isSigned ? llvm::Intrinsic::smin : llvm::Intrinsic::umin;
            auto* fn = getIntrinsic(id, {recvTy});
            return static_cast<llvm::Value*>(
                builder_->CreateCall(fn, {receiverVal, castArg(args[0])}, "min"));
        }
        if (tag == "int.clamp") {
            auto minId = isSigned ? llvm::Intrinsic::smin : llvm::Intrinsic::umin;
            auto maxId = isSigned ? llvm::Intrinsic::smax : llvm::Intrinsic::umax;
            auto* fnMax = getIntrinsic(maxId, {recvTy});
            auto* fnMin = getIntrinsic(minId, {recvTy});
            auto* lo = castArg(args[0]);
            auto* hi = castArg(args[1]);
            auto* clamped = builder_->CreateCall(fnMax, {receiverVal, lo}, "clamp.lo");
            return static_cast<llvm::Value*>(
                builder_->CreateCall(fnMin, {clamped, hi}, "clamp"));
        }
        if (tag == "int.pow") {
            // Integer power via loop: result = 1; while(exp > 0) { if(exp&1) result *= base; base *= base; exp >>= 1; }
            auto* exp = args[0];
            if (exp->getType() != i32Ty)
                exp = builder_->CreateIntCast(exp, i32Ty, false);
            auto* entryBB = builder_->GetInsertBlock();
            auto* condBB  = llvm::BasicBlock::Create(*context_, "pow.cond", currentFunction_);
            auto* bodyBB  = llvm::BasicBlock::Create(*context_, "pow.body", currentFunction_);
            auto* mulBB   = llvm::BasicBlock::Create(*context_, "pow.mul", currentFunction_);
            auto* nextBB  = llvm::BasicBlock::Create(*context_, "pow.next", currentFunction_);
            auto* endBB   = llvm::BasicBlock::Create(*context_, "pow.end", currentFunction_);
            auto* resAlloca  = builder_->CreateAlloca(recvTy, nullptr, "pow.res");
            auto* baseAlloca = builder_->CreateAlloca(recvTy, nullptr, "pow.base");
            auto* expAlloca  = builder_->CreateAlloca(i32Ty, nullptr, "pow.exp");
            builder_->CreateStore(llvm::ConstantInt::get(recvTy, 1), resAlloca);
            builder_->CreateStore(receiverVal, baseAlloca);
            builder_->CreateStore(exp, expAlloca);
            builder_->CreateBr(condBB);
            builder_->SetInsertPoint(condBB);
            auto* curExp = builder_->CreateLoad(i32Ty, expAlloca, "exp");
            auto* cond = builder_->CreateICmpUGT(curExp, llvm::ConstantInt::get(i32Ty, 0), "pow.cmp");
            builder_->CreateCondBr(cond, bodyBB, endBB);
            builder_->SetInsertPoint(bodyBB);
            auto* e = builder_->CreateLoad(i32Ty, expAlloca, "exp");
            auto* odd = builder_->CreateAnd(e, llvm::ConstantInt::get(i32Ty, 1), "odd");
            auto* isOdd = builder_->CreateICmpNE(odd, llvm::ConstantInt::get(i32Ty, 0), "isodd");
            builder_->CreateCondBr(isOdd, mulBB, nextBB);
            builder_->SetInsertPoint(mulBB);
            auto* r = builder_->CreateLoad(recvTy, resAlloca, "r");
            auto* b = builder_->CreateLoad(recvTy, baseAlloca, "b");
            builder_->CreateStore(builder_->CreateMul(r, b, "pow.mul"), resAlloca);
            builder_->CreateBr(nextBB);
            builder_->SetInsertPoint(nextBB);
            auto* base2 = builder_->CreateLoad(recvTy, baseAlloca, "b");
            builder_->CreateStore(builder_->CreateMul(base2, base2, "pow.sq"), baseAlloca);
            auto* exp2 = builder_->CreateLoad(i32Ty, expAlloca, "e");
            builder_->CreateStore(builder_->CreateLShr(exp2, 1, "pow.shr"), expAlloca);
            builder_->CreateBr(condBB);
            builder_->SetInsertPoint(endBB);
            return static_cast<llvm::Value*>(
                builder_->CreateLoad(recvTy, resAlloca, "pow"));
        }
        if (tag == "int.wrappingAdd") {
            return static_cast<llvm::Value*>(
                builder_->CreateAdd(receiverVal, castArg(args[0]), "wadd"));
        }
        if (tag == "int.wrappingSub") {
            return static_cast<llvm::Value*>(
                builder_->CreateSub(receiverVal, castArg(args[0]), "wsub"));
        }
        if (tag == "int.wrappingMul") {
            return static_cast<llvm::Value*>(
                builder_->CreateMul(receiverVal, castArg(args[0]), "wmul"));
        }
        if (tag == "int.saturatingAdd") {
            auto id = isSigned ? llvm::Intrinsic::sadd_sat : llvm::Intrinsic::uadd_sat;
            auto* fn = getIntrinsic(id, {recvTy});
            return static_cast<llvm::Value*>(
                builder_->CreateCall(fn, {receiverVal, castArg(args[0])}, "satadd"));
        }
        if (tag == "int.saturatingSub") {
            auto id = isSigned ? llvm::Intrinsic::ssub_sat : llvm::Intrinsic::usub_sat;
            auto* fn = getIntrinsic(id, {recvTy});
            return static_cast<llvm::Value*>(
                builder_->CreateCall(fn, {receiverVal, castArg(args[0])}, "satsub"));
        }
        if (tag == "int.leadingZeros") {
            auto* fn = getIntrinsic(llvm::Intrinsic::ctlz, {recvTy});
            auto* lz = builder_->CreateCall(fn, {receiverVal, builder_->getFalse()}, "ctlz");
            return static_cast<llvm::Value*>(
                builder_->CreateIntCast(lz, i32Ty, false, "lz"));
        }
        if (tag == "int.trailingZeros") {
            auto* fn = getIntrinsic(llvm::Intrinsic::cttz, {recvTy});
            auto* tz = builder_->CreateCall(fn, {receiverVal, builder_->getFalse()}, "cttz");
            return static_cast<llvm::Value*>(
                builder_->CreateIntCast(tz, i32Ty, false, "tz"));
        }
        if (tag == "int.countOnes") {
            auto* fn = getIntrinsic(llvm::Intrinsic::ctpop, {recvTy});
            auto* pop = builder_->CreateCall(fn, {receiverVal}, "ctpop");
            return static_cast<llvm::Value*>(
                builder_->CreateIntCast(pop, i32Ty, false, "ones"));
        }
        if (tag == "int.rotateLeft") {
            auto* fn = getIntrinsic(llvm::Intrinsic::fshl, {recvTy});
            auto* amt = args[0];
            if (amt->getType() != recvTy)
                amt = builder_->CreateIntCast(amt, recvTy, false);
            return static_cast<llvm::Value*>(
                builder_->CreateCall(fn, {receiverVal, receiverVal, amt}, "rotl"));
        }
        if (tag == "int.rotateRight") {
            auto* fn = getIntrinsic(llvm::Intrinsic::fshr, {recvTy});
            auto* amt = args[0];
            if (amt->getType() != recvTy)
                amt = builder_->CreateIntCast(amt, recvTy, false);
            return static_cast<llvm::Value*>(
                builder_->CreateCall(fn, {receiverVal, receiverVal, amt}, "rotr"));
        }
        if (tag == "int.byteSwap") {
            unsigned bw = recvTy->getIntegerBitWidth();
            if (bw >= 16) {
                auto* fn = getIntrinsic(llvm::Intrinsic::bswap, {recvTy});
                return static_cast<llvm::Value*>(
                    builder_->CreateCall(fn, {receiverVal}, "bswap"));
            }
            return static_cast<llvm::Value*>(receiverVal);
        }
        if (tag == "int.toBigEndian") {
            // On little-endian, byte swap; on big-endian, noop
            unsigned bw = recvTy->getIntegerBitWidth();
            if (bw >= 16 && module_->getDataLayout().isLittleEndian()) {
                auto* fn = getIntrinsic(llvm::Intrinsic::bswap, {recvTy});
                return static_cast<llvm::Value*>(
                    builder_->CreateCall(fn, {receiverVal}, "to_be"));
            }
            return static_cast<llvm::Value*>(receiverVal);
        }
        if (tag == "int.toLittleEndian") {
            unsigned bw = recvTy->getIntegerBitWidth();
            if (bw >= 16 && !module_->getDataLayout().isLittleEndian()) {
                auto* fn = getIntrinsic(llvm::Intrinsic::bswap, {recvTy});
                return static_cast<llvm::Value*>(
                    builder_->CreateCall(fn, {receiverVal}, "to_le"));
            }
            return static_cast<llvm::Value*>(receiverVal);
        }
        if (tag == "int.isPowerOfTwo") {
            // (x != 0) && ((x & (x - 1)) == 0)
            auto* zero = llvm::ConstantInt::get(recvTy, 0);
            auto* one  = llvm::ConstantInt::get(recvTy, 1);
            auto* notZero = builder_->CreateICmpNE(receiverVal, zero, "nz");
            auto* xm1 = builder_->CreateSub(receiverVal, one, "xm1");
            auto* andV = builder_->CreateAnd(receiverVal, xm1, "and");
            auto* isZero = builder_->CreateICmpEQ(andV, zero, "iz");
            return static_cast<llvm::Value*>(
                builder_->CreateAnd(notZero, isZero, "ispow2"));
        }
        if (tag == "int.nextPowerOfTwo") {
            // Compute next power of two: if already pow2, return self
            // Algorithm: v--; v |= v>>1; v |= v>>2; v |= v>>4; ... v++;
            unsigned bw = recvTy->getIntegerBitWidth();
            auto* one  = llvm::ConstantInt::get(recvTy, 1);
            auto* v = builder_->CreateSub(receiverVal, one, "np2.dec");
            for (unsigned s = 1; s < bw; s <<= 1) {
                auto* shifted = builder_->CreateLShr(v, llvm::ConstantInt::get(recvTy, s));
                v = builder_->CreateOr(v, shifted);
            }
            return static_cast<llvm::Value*>(
                builder_->CreateAdd(v, one, "np2"));
        }
        if (tag == "int.log2") {
            // floor(log2(x)) = bitwidth - 1 - ctlz(x)
            unsigned bw = recvTy->getIntegerBitWidth();
            auto* fn = getIntrinsic(llvm::Intrinsic::ctlz, {recvTy});
            auto* lz = builder_->CreateCall(fn, {receiverVal, builder_->getTrue()}, "ctlz");
            auto* bwV = llvm::ConstantInt::get(recvTy, bw - 1);
            auto* log = builder_->CreateSub(bwV, lz, "log2");
            return static_cast<llvm::Value*>(
                builder_->CreateIntCast(log, i32Ty, false, "log2.i32"));
        }
        if (tag == "int.sign") {
            // -1 if x < 0, 0 if x == 0, 1 if x > 0
            auto* zero = llvm::ConstantInt::get(recvTy, 0);
            auto* isNeg = builder_->CreateICmpSLT(receiverVal, zero, "neg");
            auto* isPos = builder_->CreateICmpSGT(receiverVal, zero, "pos");
            auto* posV  = builder_->CreateSelect(isPos, llvm::ConstantInt::get(i32Ty, 1),
                                                 llvm::ConstantInt::get(i32Ty, 0), "pos.sel");
            return static_cast<llvm::Value*>(
                builder_->CreateSelect(isNeg, llvm::ConstantInt::getSigned(i32Ty, -1),
                                       posV, "sign"));
        }
        if (tag == "int.toFloat") {
            if (isSigned)
                return static_cast<llvm::Value*>(
                    builder_->CreateSIToFP(receiverVal, f64Ty, "tofloat"));
            else
                return static_cast<llvm::Value*>(
                    builder_->CreateUIToFP(receiverVal, f64Ty, "tofloat"));
        }
        if (tag == "int.toChar") {
            return static_cast<llvm::Value*>(
                builder_->CreateIntCast(receiverVal, i8Ty, false, "tochar"));
        }
        if (tag == "int.toString") {
            // Call lucis_itoa / lucis_utoa depending on signedness.
            // Use CreateIntCast (handles both extension AND truncation)
            // so types wider than i64 (int128, uint128) work correctly.
            llvm::Value* val = receiverVal;
            std::string fname;
            llvm::Type* paramTy = i64Ty;
            if (isSigned) {
                fname = "lucis_itoa";
            } else {
                fname = "lucis_utoa";
            }
            val = builder_->CreateIntCast(val, paramTy, isSigned, "icast");
            auto callee = declareBuiltin(fname, strTy, {paramTy});
            return static_cast<llvm::Value*>(
                builder_->CreateCall(callee, {val}, "tostr"));
        }
        if (tag == "int.toStringRadix") {
            llvm::Value* val = receiverVal;
            if (val->getType() != i64Ty)
                val = builder_->CreateIntCast(val, i64Ty, isSigned, "icast");
            auto* radix = args[0];
            if (radix->getType() != i32Ty)
                radix = builder_->CreateIntCast(radix, i32Ty, false);
            auto callee = declareBuiltin("lucis_itoaRadix", strTy, {i64Ty, i32Ty});
            return static_cast<llvm::Value*>(
                builder_->CreateCall(callee, {val, radix}, "tostr_radix"));
        }
        // ── Float methods ────────────────────────────────────────────────
        if (tag == "float.abs") {
            return static_cast<llvm::Value*>(
                builder_->CreateUnaryIntrinsic(llvm::Intrinsic::fabs, receiverVal, nullptr, "fabs"));
        }
        if (tag == "float.ceil") {
            return static_cast<llvm::Value*>(
                builder_->CreateUnaryIntrinsic(llvm::Intrinsic::ceil, receiverVal, nullptr, "ceil"));
        }
        if (tag == "float.floor") {
            return static_cast<llvm::Value*>(
                builder_->CreateUnaryIntrinsic(llvm::Intrinsic::floor, receiverVal, nullptr, "floor"));
        }
        if (tag == "float.round") {
            return static_cast<llvm::Value*>(
                builder_->CreateUnaryIntrinsic(llvm::Intrinsic::round, receiverVal, nullptr, "round"));
        }
        if (tag == "float.trunc") {
            return static_cast<llvm::Value*>(
                builder_->CreateUnaryIntrinsic(llvm::Intrinsic::trunc, receiverVal, nullptr, "trunc"));
        }
        if (tag == "float.fract") {
            // x - floor(x)
            auto* fl = builder_->CreateUnaryIntrinsic(llvm::Intrinsic::floor, receiverVal, nullptr, "fl");
            return static_cast<llvm::Value*>(
                builder_->CreateFSub(receiverVal, fl, "fract"));
        }
        if (tag == "float.sqrt") {
            return static_cast<llvm::Value*>(
                builder_->CreateUnaryIntrinsic(llvm::Intrinsic::sqrt, receiverVal, nullptr, "sqrt"));
        }
        if (tag == "float.cbrt") {
            // cbrt(x) = pow(x, 1/3) — use C library for precision
            std::string fname = recvTy->isFloatTy() ? "cbrtf" : "cbrt";
            auto callee = declareBuiltin(fname, recvTy, {recvTy});
            return static_cast<llvm::Value*>(
                builder_->CreateCall(callee, {receiverVal}, "cbrt"));
        }
        if (tag == "float.pow") {
            auto* fn = getIntrinsic(llvm::Intrinsic::pow, {recvTy});
            return static_cast<llvm::Value*>(
                builder_->CreateCall(fn, {receiverVal, castArg(args[0])}, "pow"));
        }
        if (tag == "float.exp") {
            return static_cast<llvm::Value*>(
                builder_->CreateUnaryIntrinsic(llvm::Intrinsic::exp, receiverVal, nullptr, "exp"));
        }
        if (tag == "float.exp2") {
            return static_cast<llvm::Value*>(
                builder_->CreateUnaryIntrinsic(llvm::Intrinsic::exp2, receiverVal, nullptr, "exp2"));
        }
        if (tag == "float.ln") {
            return static_cast<llvm::Value*>(
                builder_->CreateUnaryIntrinsic(llvm::Intrinsic::log, receiverVal, nullptr, "ln"));
        }
        if (tag == "float.log2") {
            return static_cast<llvm::Value*>(
                builder_->CreateUnaryIntrinsic(llvm::Intrinsic::log2, receiverVal, nullptr, "log2"));
        }
        if (tag == "float.log10") {
            return static_cast<llvm::Value*>(
                builder_->CreateUnaryIntrinsic(llvm::Intrinsic::log10, receiverVal, nullptr, "log10"));
        }
        if (tag == "float.sin") {
            return static_cast<llvm::Value*>(
                builder_->CreateUnaryIntrinsic(llvm::Intrinsic::sin, receiverVal, nullptr, "sin"));
        }
        if (tag == "float.cos") {
            return static_cast<llvm::Value*>(
                builder_->CreateUnaryIntrinsic(llvm::Intrinsic::cos, receiverVal, nullptr, "cos"));
        }
        if (tag == "float.tan") {
            return static_cast<llvm::Value*>(
                callUnaryMath("tanf", "tan", receiverVal, "tan"));
        }
        if (tag == "float.asin") {
            return static_cast<llvm::Value*>(
                callUnaryMath("asinf", "asin", receiverVal, "asin"));
        }
        if (tag == "float.acos") {
            return static_cast<llvm::Value*>(
                callUnaryMath("acosf", "acos", receiverVal, "acos"));
        }
        if (tag == "float.atan") {
            return static_cast<llvm::Value*>(
                callUnaryMath("atanf", "atan", receiverVal, "atan"));
        }
        if (tag == "float.atan2") {
            return static_cast<llvm::Value*>(
                callBinaryMath("atan2f", "atan2", receiverVal, castArg(args[0]), "atan2"));
        }
        if (tag == "float.sinh") {
            return static_cast<llvm::Value*>(
                callUnaryMath("sinhf", "sinh", receiverVal, "sinh"));
        }
        if (tag == "float.cosh") {
            return static_cast<llvm::Value*>(
                callUnaryMath("coshf", "cosh", receiverVal, "cosh"));
        }
        if (tag == "float.tanh") {
            return static_cast<llvm::Value*>(
                callUnaryMath("tanhf", "tanh", receiverVal, "tanh"));
        }
        if (tag == "float.hypot") {
            // hypot not an LLVM intrinsic — call C library
            std::string fname = recvTy->isFloatTy() ? "hypotf" : "hypot";
            auto callee = declareBuiltin(fname, recvTy, {recvTy, recvTy});
            return static_cast<llvm::Value*>(
                builder_->CreateCall(callee, {receiverVal, castArg(args[0])}, "hypot"));
        }
        if (tag == "float.min") {
            return static_cast<llvm::Value*>(
                builder_->CreateBinaryIntrinsic(llvm::Intrinsic::minnum,
                    receiverVal, castArg(args[0]), nullptr, "fmin"));
        }
        if (tag == "float.max") {
            return static_cast<llvm::Value*>(
                builder_->CreateBinaryIntrinsic(llvm::Intrinsic::maxnum,
                    receiverVal, castArg(args[0]), nullptr, "fmax"));
        }
        if (tag == "float.clamp") {
            auto* lo = castArg(args[0]);
            auto* hi = castArg(args[1]);
            auto* clamped = builder_->CreateBinaryIntrinsic(
                llvm::Intrinsic::maxnum, receiverVal, lo, nullptr, "clamp.lo");
            return static_cast<llvm::Value*>(
                builder_->CreateBinaryIntrinsic(
                    llvm::Intrinsic::minnum, clamped, hi, nullptr, "clamp"));
        }
        if (tag == "float.lerp") {
            // lerp(a, b, t) = a + t * (b - a)   where receiver=a, args[0]=b, args[1]=t
            auto* b = castArg(args[0]);
            auto* t = castArg(args[1]);
            auto* diff = builder_->CreateFSub(b, receiverVal, "lerp.diff");
            auto* prod = builder_->CreateFMul(t, diff, "lerp.mul");
            return static_cast<llvm::Value*>(
                builder_->CreateFAdd(receiverVal, prod, "lerp"));
        }
        if (tag == "float.sign") {
            auto* zero = llvm::ConstantFP::get(recvTy, 0.0);
            auto* pos  = llvm::ConstantFP::get(recvTy, 1.0);
            auto* neg  = llvm::ConstantFP::get(recvTy, -1.0);
            auto* isNeg = builder_->CreateFCmpOLT(receiverVal, zero, "neg");
            auto* isPos = builder_->CreateFCmpOGT(receiverVal, zero, "pos");
            auto* sel1 = builder_->CreateSelect(isPos, pos, zero, "sign.pos");
            return static_cast<llvm::Value*>(
                builder_->CreateSelect(isNeg, neg, sel1, "sign"));
        }
        if (tag == "float.copySign") {
            auto* fn = getIntrinsic(llvm::Intrinsic::copysign, {recvTy});
            return static_cast<llvm::Value*>(
                builder_->CreateCall(fn, {receiverVal, castArg(args[0])}, "copysign"));
        }
        if (tag == "float.isNaN") {
            return static_cast<llvm::Value*>(
                builder_->CreateFCmpUNO(receiverVal, receiverVal, "isnan"));
        }
        if (tag == "float.isInf") {
            auto* fn = getIntrinsic(llvm::Intrinsic::fabs, {recvTy});
            auto* absV = builder_->CreateCall(fn, {receiverVal}, "abs");
            auto* inf = llvm::ConstantFP::getInfinity(recvTy);
            return static_cast<llvm::Value*>(
                builder_->CreateFCmpOEQ(absV, inf, "isinf"));
        }
        if (tag == "float.isFinite") {
            // !isNaN && !isInf
            auto* fn = getIntrinsic(llvm::Intrinsic::fabs, {recvTy});
            auto* absV = builder_->CreateCall(fn, {receiverVal}, "abs");
            auto* inf = llvm::ConstantFP::getInfinity(recvTy);
            auto* notInf = builder_->CreateFCmpONE(absV, inf, "notinf");
            auto* notNaN = builder_->CreateFCmpORD(receiverVal, receiverVal, "notnan");
            return static_cast<llvm::Value*>(
                builder_->CreateAnd(notInf, notNaN, "isfinite"));
        }
        if (tag == "float.isNormal") {
            // isFinite && abs(x) >= FLT_MIN/DBL_MIN
            auto* fn = getIntrinsic(llvm::Intrinsic::fabs, {recvTy});
            auto* absV = builder_->CreateCall(fn, {receiverVal}, "abs");
            double minNormal = recvTy->isFloatTy() ? 1.17549435e-38 : 2.2250738585072014e-308;
            auto* minV = llvm::ConstantFP::get(recvTy, minNormal);
            auto* inf = llvm::ConstantFP::getInfinity(recvTy);
            auto* geMin = builder_->CreateFCmpOGE(absV, minV, "ge_min");
            auto* ltInf = builder_->CreateFCmpOLT(absV, inf, "lt_inf");
            return static_cast<llvm::Value*>(
                builder_->CreateAnd(geMin, ltInf, "isnormal"));
        }
        if (tag == "float.isNegative") {
            auto* zero = llvm::ConstantFP::get(recvTy, 0.0);
            return static_cast<llvm::Value*>(
                builder_->CreateFCmpOLT(receiverVal, zero, "isneg"));
        }
        if (tag == "float.toRadians") {
            auto* factor = llvm::ConstantFP::get(recvTy, 3.14159265358979323846 / 180.0);
            return static_cast<llvm::Value*>(
                builder_->CreateFMul(receiverVal, factor, "torad"));
        }
        if (tag == "float.toDegrees") {
            auto* factor = llvm::ConstantFP::get(recvTy, 180.0 / 3.14159265358979323846);
            return static_cast<llvm::Value*>(
                builder_->CreateFMul(receiverVal, factor, "todeg"));
        }
        if (tag == "float.toInt") {
            return static_cast<llvm::Value*>(
                builder_->CreateFPToSI(receiverVal, i64Ty, "toint"));
        }
        if (tag == "float.toBits") {
            auto* destTy = recvTy->isFloatTy() ? i32Ty : i64Ty;
            return static_cast<llvm::Value*>(
                builder_->CreateBitCast(receiverVal, destTy, "tobits"));
        }
        if (tag == "float.toString") {
            llvm::Value* val = receiverVal;
            if (val->getType()->isFloatTy())
                val = builder_->CreateFPExt(val, f64Ty, "ext");
            auto callee = declareBuiltin("lucis_ftoa", strTy, {f64Ty});
            return static_cast<llvm::Value*>(
                builder_->CreateCall(callee, {val}, "ftostr"));
        }
        if (tag == "float.toStringPrecision") {
            llvm::Value* val = receiverVal;
            if (val->getType()->isFloatTy())
                val = builder_->CreateFPExt(val, f64Ty, "ext");
            auto* prec = args[0];
            if (prec->getType() != i32Ty)
                prec = builder_->CreateIntCast(prec, i32Ty, false);
            auto callee = declareBuiltin("lucis_ftoaPrecision", strTy, {f64Ty, i32Ty});
            return static_cast<llvm::Value*>(
                builder_->CreateCall(callee, {val, prec}, "ftostr_p"));
        }
        // ── Char methods ─────────────────────────────────────────────────
        if (tag == "char.isAlpha") {
            // (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
            auto* geA = builder_->CreateICmpUGE(receiverVal, llvm::ConstantInt::get(i8Ty, 'A'));
            auto* leZ = builder_->CreateICmpULE(receiverVal, llvm::ConstantInt::get(i8Ty, 'Z'));
            auto* upper = builder_->CreateAnd(geA, leZ, "upper");
            auto* gea = builder_->CreateICmpUGE(receiverVal, llvm::ConstantInt::get(i8Ty, 'a'));
            auto* lez = builder_->CreateICmpULE(receiverVal, llvm::ConstantInt::get(i8Ty, 'z'));
            auto* lower = builder_->CreateAnd(gea, lez, "lower");
            return static_cast<llvm::Value*>(
                builder_->CreateOr(upper, lower, "isalpha"));
        }
        if (tag == "char.isDigit") {
            auto* ge0 = builder_->CreateICmpUGE(receiverVal, llvm::ConstantInt::get(i8Ty, '0'));
            auto* le9 = builder_->CreateICmpULE(receiverVal, llvm::ConstantInt::get(i8Ty, '9'));
            return static_cast<llvm::Value*>(
                builder_->CreateAnd(ge0, le9, "isdigit"));
        }
        if (tag == "char.isHexDigit") {
            auto* ge0 = builder_->CreateICmpUGE(receiverVal, llvm::ConstantInt::get(i8Ty, '0'));
            auto* le9 = builder_->CreateICmpULE(receiverVal, llvm::ConstantInt::get(i8Ty, '9'));
            auto* dig = builder_->CreateAnd(ge0, le9);
            auto* geA = builder_->CreateICmpUGE(receiverVal, llvm::ConstantInt::get(i8Ty, 'A'));
            auto* leF = builder_->CreateICmpULE(receiverVal, llvm::ConstantInt::get(i8Ty, 'F'));
            auto* hexU = builder_->CreateAnd(geA, leF);
            auto* gea = builder_->CreateICmpUGE(receiverVal, llvm::ConstantInt::get(i8Ty, 'a'));
            auto* lef = builder_->CreateICmpULE(receiverVal, llvm::ConstantInt::get(i8Ty, 'f'));
            auto* hexL = builder_->CreateAnd(gea, lef);
            auto* hex = builder_->CreateOr(hexU, hexL);
            return static_cast<llvm::Value*>(
                builder_->CreateOr(dig, hex, "ishex"));
        }
        if (tag == "char.isAlphaNum") {
            auto* geA = builder_->CreateICmpUGE(receiverVal, llvm::ConstantInt::get(i8Ty, 'A'));
            auto* leZ = builder_->CreateICmpULE(receiverVal, llvm::ConstantInt::get(i8Ty, 'Z'));
            auto* upper = builder_->CreateAnd(geA, leZ);
            auto* gea = builder_->CreateICmpUGE(receiverVal, llvm::ConstantInt::get(i8Ty, 'a'));
            auto* lez = builder_->CreateICmpULE(receiverVal, llvm::ConstantInt::get(i8Ty, 'z'));
            auto* lower = builder_->CreateAnd(gea, lez);
            auto* ge0 = builder_->CreateICmpUGE(receiverVal, llvm::ConstantInt::get(i8Ty, '0'));
            auto* le9 = builder_->CreateICmpULE(receiverVal, llvm::ConstantInt::get(i8Ty, '9'));
            auto* digit = builder_->CreateAnd(ge0, le9);
            auto* alpha = builder_->CreateOr(upper, lower);
            return static_cast<llvm::Value*>(
                builder_->CreateOr(alpha, digit, "isalnum"));
        }
        if (tag == "char.isUpper") {
            auto* ge = builder_->CreateICmpUGE(receiverVal, llvm::ConstantInt::get(i8Ty, 'A'));
            auto* le = builder_->CreateICmpULE(receiverVal, llvm::ConstantInt::get(i8Ty, 'Z'));
            return static_cast<llvm::Value*>(
                builder_->CreateAnd(ge, le, "isupper"));
        }
        if (tag == "char.isLower") {
            auto* ge = builder_->CreateICmpUGE(receiverVal, llvm::ConstantInt::get(i8Ty, 'a'));
            auto* le = builder_->CreateICmpULE(receiverVal, llvm::ConstantInt::get(i8Ty, 'z'));
            return static_cast<llvm::Value*>(
                builder_->CreateAnd(ge, le, "islower"));
        }
        if (tag == "char.isSpace") {
            // space, tab, newline, carriage return, form feed, vertical tab
            auto* isSP = builder_->CreateICmpEQ(receiverVal, llvm::ConstantInt::get(i8Ty, ' '));
            auto* isTB = builder_->CreateICmpEQ(receiverVal, llvm::ConstantInt::get(i8Ty, '\t'));
            auto* isNL = builder_->CreateICmpEQ(receiverVal, llvm::ConstantInt::get(i8Ty, '\n'));
            auto* isCR = builder_->CreateICmpEQ(receiverVal, llvm::ConstantInt::get(i8Ty, '\r'));
            auto* isFF = builder_->CreateICmpEQ(receiverVal, llvm::ConstantInt::get(i8Ty, '\f'));
            auto* isVT = builder_->CreateICmpEQ(receiverVal, llvm::ConstantInt::get(i8Ty, '\v'));
            auto* r1 = builder_->CreateOr(isSP, isTB);
            auto* r2 = builder_->CreateOr(isNL, isCR);
            auto* r3 = builder_->CreateOr(isFF, isVT);
            auto* r4 = builder_->CreateOr(r1, r2);
            return static_cast<llvm::Value*>(
                builder_->CreateOr(r4, r3, "isspace"));
        }
        if (tag == "char.isPrintable") {
            // 0x20 <= c <= 0x7E
            auto* ge = builder_->CreateICmpUGE(receiverVal, llvm::ConstantInt::get(i8Ty, 0x20));
            auto* le = builder_->CreateICmpULE(receiverVal, llvm::ConstantInt::get(i8Ty, 0x7E));
            return static_cast<llvm::Value*>(
                builder_->CreateAnd(ge, le, "isprint"));
        }
        if (tag == "char.isControl") {
            // c < 0x20 || c == 0x7F
            auto* lt = builder_->CreateICmpULT(receiverVal, llvm::ConstantInt::get(i8Ty, 0x20));
            auto* isDel = builder_->CreateICmpEQ(receiverVal, llvm::ConstantInt::get(i8Ty, 0x7F));
            return static_cast<llvm::Value*>(
                builder_->CreateOr(lt, isDel, "isctrl"));
        }
        if (tag == "char.isPunct") {
            // isPrintable && !isAlphaNum && !isSpace (c == ' ')
            auto* ge20 = builder_->CreateICmpUGE(receiverVal, llvm::ConstantInt::get(i8Ty, 0x20));
            auto* le7E = builder_->CreateICmpULE(receiverVal, llvm::ConstantInt::get(i8Ty, 0x7E));
            auto* printable = builder_->CreateAnd(ge20, le7E);
            auto* geA = builder_->CreateICmpUGE(receiverVal, llvm::ConstantInt::get(i8Ty, 'A'));
            auto* leZ = builder_->CreateICmpULE(receiverVal, llvm::ConstantInt::get(i8Ty, 'Z'));
            auto* upper = builder_->CreateAnd(geA, leZ);
            auto* gea = builder_->CreateICmpUGE(receiverVal, llvm::ConstantInt::get(i8Ty, 'a'));
            auto* lez = builder_->CreateICmpULE(receiverVal, llvm::ConstantInt::get(i8Ty, 'z'));
            auto* lower = builder_->CreateAnd(gea, lez);
            auto* ge0 = builder_->CreateICmpUGE(receiverVal, llvm::ConstantInt::get(i8Ty, '0'));
            auto* le9 = builder_->CreateICmpULE(receiverVal, llvm::ConstantInt::get(i8Ty, '9'));
            auto* digit = builder_->CreateAnd(ge0, le9);
            auto* alpha = builder_->CreateOr(upper, lower);
            auto* alnum = builder_->CreateOr(alpha, digit);
            auto* isSP = builder_->CreateICmpEQ(receiverVal, llvm::ConstantInt::get(i8Ty, ' '));
            auto* notAlnum = builder_->CreateNot(alnum);
            auto* notSpace = builder_->CreateNot(isSP);
            auto* r = builder_->CreateAnd(printable, notAlnum);
            return static_cast<llvm::Value*>(
                builder_->CreateAnd(r, notSpace, "ispunct"));
        }
        if (tag == "char.isAscii") {
            return static_cast<llvm::Value*>(
                builder_->CreateICmpULT(receiverVal, llvm::ConstantInt::get(i8Ty, 128), "isascii"));
        }
        if (tag == "char.toUpper") {
            auto* ge = builder_->CreateICmpUGE(receiverVal, llvm::ConstantInt::get(i8Ty, 'a'));
            auto* le = builder_->CreateICmpULE(receiverVal, llvm::ConstantInt::get(i8Ty, 'z'));
            auto* isLow = builder_->CreateAnd(ge, le, "islower");
            auto* upper = builder_->CreateSub(receiverVal, llvm::ConstantInt::get(i8Ty, 32), "upper");
            return static_cast<llvm::Value*>(
                builder_->CreateSelect(isLow, upper, receiverVal, "toupper"));
        }
        if (tag == "char.toLower") {
            auto* ge = builder_->CreateICmpUGE(receiverVal, llvm::ConstantInt::get(i8Ty, 'A'));
            auto* le = builder_->CreateICmpULE(receiverVal, llvm::ConstantInt::get(i8Ty, 'Z'));
            auto* isUp = builder_->CreateAnd(ge, le, "isupper");
            auto* lower = builder_->CreateAdd(receiverVal, llvm::ConstantInt::get(i8Ty, 32), "lower");
            return static_cast<llvm::Value*>(
                builder_->CreateSelect(isUp, lower, receiverVal, "tolower"));
        }
        if (tag == "char.toInt") {
            return static_cast<llvm::Value*>(
                builder_->CreateZExt(receiverVal, i32Ty, "toint"));
        }
        if (tag == "char.digitToInt") {
            return static_cast<llvm::Value*>(
                builder_->CreateSub(
                    builder_->CreateZExt(receiverVal, i32Ty, "ext"),
                    llvm::ConstantInt::get(i32Ty, '0'), "digval"));
        }
        if (tag == "char.toString") {
            // Allocate 1-byte string on heap, return {ptr, 1}
            auto* one = llvm::ConstantInt::get(usizeTy, 1);
            auto callee = declareBuiltin("lucis_allocString", ptrTy, {usizeTy});
            auto* buf = builder_->CreateCall(callee, {one}, "char_buf");
            builder_->CreateStore(receiverVal, buf);
            llvm::Value* s = llvm::UndefValue::get(strTy);
            s = builder_->CreateInsertValue(s, buf, 0);
            s = builder_->CreateInsertValue(s, one, 1);
            return static_cast<llvm::Value*>(s);
        }
        if (tag == "char.repeat") {
            // Allocate n bytes, memset with char, return {ptr, n}
            auto* n = args[0];
            if (n->getType() != usizeTy)
                n = builder_->CreateIntCast(n, usizeTy, false);
            auto mallocCallee = declareBuiltin("lucis_allocString", ptrTy, {usizeTy});
            auto* buf = builder_->CreateCall(mallocCallee, {n}, "rep_buf");
            auto memsetCallee = declareBuiltin("memset", ptrTy, {ptrTy, i32Ty, usizeTy});
            auto* charI32 = builder_->CreateZExt(receiverVal, i32Ty, "ch32");
            builder_->CreateCall(memsetCallee, {buf, charI32, n});
            llvm::Value* s = llvm::UndefValue::get(strTy);
            s = builder_->CreateInsertValue(s, buf, 0);
            s = builder_->CreateInsertValue(s, n, 1);
            return static_cast<llvm::Value*>(s);
        }
        // ── Bool methods ─────────────────────────────────────────────────
        if (tag == "bool.toggle") {
            return static_cast<llvm::Value*>(
                builder_->CreateNot(receiverVal, "toggle"));
        }
        if (tag == "bool.toInt") {
            return static_cast<llvm::Value*>(
                builder_->CreateZExt(receiverVal, i32Ty, "toint"));
        }
        if (tag == "bool.toString") {
            auto* trueStr  = builder_->CreateGlobalString("true", ".str.true", 0, module_);
            auto* falseStr = builder_->CreateGlobalString("false", ".str.false", 0, module_);
            auto* trueLen  = llvm::ConstantInt::get(usizeTy, 4);
            auto* falseLen = llvm::ConstantInt::get(usizeTy, 5);
            auto* ptr = builder_->CreateSelect(receiverVal, trueStr, falseStr, "bstr_ptr");
            auto* len = builder_->CreateSelect(receiverVal, trueLen, falseLen, "bstr_len");
            llvm::Value* s = llvm::UndefValue::get(strTy);
            s = builder_->CreateInsertValue(s, ptr, 0);
            s = builder_->CreateInsertValue(s, len, 1);
            return static_cast<llvm::Value*>(s);
        }
        if (tag == "ptr.toString") {
            // Format pointer as hex with 0x prefix (matching printf output).
            auto* intVal = builder_->CreatePtrToInt(receiverVal, i64Ty, "ptr2int");
            auto radix16 = llvm::ConstantInt::get(i32Ty, 16);
            auto hexCallee = declareBuiltin("lucis_itoaRadix", strTy, {i64Ty, i32Ty});
            auto* hexStr = builder_->CreateCall(hexCallee, {intVal, radix16}, "ptrhex");
            // Create "0x" prefix
            auto* prefixStr = builder_->CreateGlobalString("0x", ".str.0x", 0, module_);
            auto prefixLen = llvm::ConstantInt::get(usizeTy, 2);
            llvm::Value* prefix = llvm::UndefValue::get(strTy);
            prefix = builder_->CreateInsertValue(prefix, prefixStr, 0);
            prefix = builder_->CreateInsertValue(prefix, prefixLen, 1);
            // Concat "0x" + hex
            auto* lhsPtr = builder_->CreateExtractValue(prefix, {0}, "pfx.ptr");
            auto* lhsLen = builder_->CreateExtractValue(prefix, {1}, "pfx.len");
            auto* rhsPtr = builder_->CreateExtractValue(hexStr, {0}, "hex.ptr");
            auto* rhsLen = builder_->CreateExtractValue(hexStr, {1}, "hex.len");
            auto concatCallee = declareBuiltin("lucis_concat", strTy,
                                               {ptrTy, usizeTy, ptrTy, usizeTy});
            return static_cast<llvm::Value*>(
                builder_->CreateCall(concatCallee, {lhsPtr, lhsLen, rhsPtr, rhsLen}, "ptrstr"));
        }
    }
    // Fallback: return undef of the expected return type (should not reach here
    // if all methods are implemented, but kept for safety)
    const TypeInfo* retTI = nullptr;
    if (desc->returnType == "_self")
        retTI = receiverTI;
    else if (desc->returnType == "_elem")
        retTI = receiverTI;
    else
        retTI = typeRegistry_.lookup(desc->returnType);
    llvm::Type* retTy = retTI
        ? retTI->toLLVMType(*context_, module_->getDataLayout())
        : llvm::Type::getVoidTy(*context_);
    // ── BitStruct methods ────────────────────────────────────
    if (desc->emitTag == "bitstruct.raw") {
        return static_cast<llvm::Value*>(receiverVal);
    }
    if (desc->emitTag == "bitstruct.clearAll" || desc->emitTag == "bitstruct.setAll") {
        llvm::Value* targetPtr = nullptr;
        if (!receiverVarName.empty()) {
            auto it = locals_.find(receiverVarName);
            if (it != locals_.end())
                targetPtr = it->second.alloca;
        } else {
            targetPtr = resolveMethodReceiverAddress(ctx->expression());
        }
        if (targetPtr) {
            auto* backTy = receiverTI->backingType->toLLVMType(*context_, module_->getDataLayout());
            if (desc->emitTag == "bitstruct.clearAll") {
                builder_->CreateStore(llvm::Constant::getNullValue(backTy), targetPtr);
            } else {
                builder_->CreateStore(llvm::Constant::getAllOnesValue(backTy), targetPtr);
            }
        }
        return static_cast<llvm::Value*>(llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }
    std::cerr << "lucis: unimplemented method codegen for '" << desc->emitTag << "'\n";
    if (retTy->isVoidTy())
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    return static_cast<llvm::Value*>(llvm::UndefValue::get(retTy));
}
// ── Arrow method call: expr->method(args) ───────────────────────────
// Resolves as pointer dereference + method call.
// self->doubled() is equivalent to (*self).doubled()
std::any
IRGen::visitArrowMethodCallExpr(LucisParser::ArrowMethodCallExprContext* ctx) {
    auto methodName = ctx->IDENTIFIER()->getText(); // X
    auto* baseExpr = ctx->expression();
    // Resolve receiver type info — the base must be a pointer to struct
    const TypeInfo* receiverTI = nullptr;
    std::string receiverVarName;
    if (auto* identBase = dynamic_cast<LucisParser::IdentExprContext*>(baseExpr)) {
        receiverVarName = identBase->IDENTIFIER()->getText();
        auto it = locals_.find(receiverVarName);
        if (it != locals_.end())
            receiverTI = it->second.typeInfo;
    }
    if (!receiverTI)
        receiverTI = resolveExprTypeInfo(baseExpr);
    // Must be a pointer — dereference to get pointee type
    if (!receiverTI || receiverTI->kind != TypeKind::Pointer || !receiverTI->pointeeType) {
        std::cerr << "lucis: '->' requires a pointer type for method call\n";
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }
    auto* structTI = receiverTI->pointeeType;
    if (structTI->kind != TypeKind::Struct) {
        std::cerr << "lucis: '->' method call requires pointer to struct\n";
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }
    // Look up struct method
    auto smIt = structMethods_.find(structTI->name);
    if (smIt == structMethods_.end() || smIt->second.find(methodName) == smIt->second.end()) {
        std::cerr << "lucis: struct '" << structTI->name
                  << "' has no method '" << methodName << "'\n";
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }
    auto* fn = smIt->second.at(methodName).fn;
    // Build call args: first arg is the pointer (self)
    std::vector<llvm::Value*> callArgs;
    auto it = locals_.find(receiverVarName);
    if (it != locals_.end()) {
        // receiver is a pointer var — load the pointer value
        auto* ptrTy = llvm::PointerType::getUnqual(*context_);
        callArgs.push_back(
            builder_->CreateLoad(ptrTy, it->second.alloca, "self_load"));
    } else {
        callArgs.push_back(castValue(visit(baseExpr)));
    }
    // Remaining args
    if (auto* argList = ctx->argList()) {
        auto* fnType = fn->getFunctionType();
        size_t paramIdx = 1;
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
        builder_->CreateCall(fn, callArgs, "arrow_method"));
}
// ── spawn expression ────────────────────────────────────────────────────
// spawn funcName(args) → Task<T> (returns ptr to lucis_Task)
//
// Implementation:
//   1. Pack arguments into a heap-allocated struct
//   2. Create a trampoline function: void* __spawn_N(void* packed) that
//      unpacks args, calls the target, and returns heap-allocated result
//   3. Call lucis_taskCreate(trampoline, packed) → Task*
