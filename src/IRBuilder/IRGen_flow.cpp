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



std::any IRGen::visitSpawnExpr(LucisParser::SpawnExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto* innerExpr = ctx->expression();

    // The inner expression must be a function call
    auto* fnCall = dynamic_cast<LucisParser::FnCallExprContext*>(innerExpr);
    if (!fnCall) {
        std::cerr << "lucis: spawn requires a function call expression\n";
        return static_cast<llvm::Value*>(
            llvm::ConstantPointerNull::get(
                llvm::PointerType::getUnqual(*context_)));
    }

    // The fnCallExpr has expression()(argList?) — the callee is the inner expression
    auto* calleeExpr = fnCall->expression();
    auto* identExpr = dynamic_cast<LucisParser::IdentExprContext*>(calleeExpr);
    if (!identExpr) {
        std::cerr << "lucis: spawn requires a direct function call (e.g. spawn func())\n";
        return static_cast<llvm::Value*>(
            llvm::ConstantPointerNull::get(
                llvm::PointerType::getUnqual(*context_)));
    }

    std::string targetName = identExpr->IDENTIFIER()->getText();

    // Evaluate arguments
    std::vector<llvm::Value*> args;
    if (auto* argList = fnCall->argList()) {
        for (auto* e : argList->expression())
            args.push_back(castValue(visit(e)));
    }

    // Look up the target function (apply name mangling)
    auto resolvedName = resolveCallTarget(targetName);
    auto* targetFn = module_->getFunction(resolvedName);
    if (!targetFn) {
        std::cerr << "lucis: spawn target function '" << targetName << "' not found\n";
        return static_cast<llvm::Value*>(
            llvm::ConstantPointerNull::get(
                llvm::PointerType::getUnqual(*context_)));
    }

    auto* ptrTy = llvm::PointerType::getUnqual(*context_);
    auto* i8Ty  = llvm::Type::getInt8Ty(*context_);

    // ── Step 1: Pack arguments into a heap struct ──────────────────────
    llvm::Value* packedPtr = llvm::ConstantPointerNull::get(ptrTy);
    llvm::StructType* packedTy = nullptr;

    if (!args.empty()) {
        // Cast arguments to match target function parameter types
        auto* fnType = targetFn->getFunctionType();
        for (size_t i = 0; i < args.size() && i < fnType->getNumParams(); i++) {
            auto* paramTy = fnType->getParamType(i);
            if (args[i]->getType() != paramTy) {
                if (args[i]->getType()->isIntegerTy() && paramTy->isIntegerTy()) {
                    args[i] = builder_->CreateIntCast(args[i], paramTy, true);
                } else if (args[i]->getType()->isFloatingPointTy() && paramTy->isFloatingPointTy()) {
                    args[i] = builder_->CreateFPCast(args[i], paramTy);
                } else if (args[i]->getType()->isIntegerTy() && paramTy->isFloatingPointTy()) {
                    args[i] = builder_->CreateSIToFP(args[i], paramTy);
                } else if (args[i]->getType()->isFloatingPointTy() && paramTy->isIntegerTy()) {
                    args[i] = builder_->CreateFPToSI(args[i], paramTy);
                }
            }
        }

        std::vector<llvm::Type*> fieldTypes;
        for (auto* a : args)
            fieldTypes.push_back(a->getType());

        packedTy = llvm::StructType::create(*context_, fieldTypes,
            "__spawn_args_" + std::to_string(spawnCounter_));

        // malloc(sizeof(packedTy))
        auto* dlayout = &module_->getDataLayout();
        auto* allocSize = llvm::ConstantInt::get(
            llvm::Type::getInt64Ty(*context_),
            dlayout->getTypeAllocSize(packedTy));
        auto mallocFn = declareBuiltin("malloc", ptrTy,
            {llvm::Type::getInt64Ty(*context_)});
        packedPtr = builder_->CreateCall(mallocFn, {allocSize}, "spawn_args");

        // Store each arg
        for (size_t i = 0; i < args.size(); i++) {
            auto* gep = builder_->CreateStructGEP(packedTy, packedPtr, i);
            builder_->CreateStore(args[i], gep);
        }
    }

    // ── Step 2: Create trampoline function ─────────────────────────────
    auto trampolineName = "__spawn_trampoline_" + std::to_string(spawnCounter_++);
    auto* trampolineType = llvm::FunctionType::get(ptrTy, {ptrTy}, false);
    auto* trampolineFn = llvm::Function::Create(
        trampolineType, llvm::Function::InternalLinkage,
        trampolineName, module_);

    // Save current state
    auto* savedBB   = builder_->GetInsertBlock();
    auto* savedFunc = currentFunction_;
    auto  savedLocals = locals_;

    // Build trampoline body
    auto* trampEntry = llvm::BasicBlock::Create(*context_, "entry", trampolineFn);
    builder_->SetInsertPoint(trampEntry);
    currentFunction_ = trampolineFn;

    auto* argPtr = trampolineFn->getArg(0);
    argPtr->setName("packed");

    // Unpack arguments
    std::vector<llvm::Value*> unpackedArgs;
    if (packedTy) {
        for (size_t i = 0; i < args.size(); i++) {
            auto* gep = builder_->CreateStructGEP(packedTy, argPtr, i);
            auto* val = builder_->CreateLoad(args[i]->getType(), gep);
            unpackedArgs.push_back(val);
        }
        // Free the packed args struct
        auto freeFn = declareBuiltin("free",
            llvm::Type::getVoidTy(*context_), {ptrTy});
        builder_->CreateCall(freeFn, {argPtr});
    }

    // Call the target function
    auto* retTy = targetFn->getReturnType();
    llvm::Value* result;
    if (retTy->isVoidTy()) {
        builder_->CreateCall(targetFn, unpackedArgs);
        result = llvm::ConstantPointerNull::get(ptrTy);
    } else {
        auto* callResult = builder_->CreateCall(targetFn, unpackedArgs, "spawn_result");

        // Heap-allocate the result
        auto* resultSize = llvm::ConstantInt::get(
            llvm::Type::getInt64Ty(*context_),
            module_->getDataLayout().getTypeAllocSize(retTy));
        auto mallocFn = declareBuiltin("malloc", ptrTy,
            {llvm::Type::getInt64Ty(*context_)});
        auto* heapResult = builder_->CreateCall(mallocFn, {resultSize}, "heap_result");
        builder_->CreateStore(callResult, heapResult);
        result = heapResult;
    }

    builder_->CreateRet(result);

    // ── Restore caller state ───────────────────────────────────────────
    currentFunction_ = savedFunc;
    locals_ = savedLocals;
    builder_->SetInsertPoint(savedBB);

    // ── Step 3: Call lucis_taskCreate(trampoline, packed) ──────────────
    auto taskCreateFn = declareBuiltin("lucis_taskCreate", ptrTy,
        {ptrTy, ptrTy});
    auto* task = builder_->CreateCall(taskCreateFn,
        {trampolineFn, packedPtr}, "task");

    return static_cast<llvm::Value*>(task);
}

// ── await expression ────────────────────────────────────────────────────
// await task → T (extracts result from Task<T>)


std::any IRGen::visitAwaitExpr(LucisParser::AwaitExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto* taskVal = castValue(visit(ctx->expression()));

    auto* ptrTy = llvm::PointerType::getUnqual(*context_);

    // Load the task handle if it's stored in an alloca
    if (auto* alloca = llvm::dyn_cast<llvm::AllocaInst>(taskVal)) {
        taskVal = builder_->CreateLoad(ptrTy, alloca, "task_ptr");
    }

    // Call lucis_taskAwait(task) → void* (result pointer)
    auto awaitFn = declareBuiltin("lucis_taskAwait", ptrTy, {ptrTy});
    auto* resultPtr = builder_->CreateCall(awaitFn, {taskVal}, "await_result");

    // Resolve the Task<T> element type to know what to load from result
    const TypeInfo* taskTI = nullptr;
    if (auto* identExpr = dynamic_cast<LucisParser::IdentExprContext*>(
            ctx->expression())) {
        auto it = locals_.find(identExpr->IDENTIFIER()->getText());
        if (it != locals_.end())
            taskTI = it->second.typeInfo;
    }

    // If Task<T> with T != void, load the result
    if (taskTI && taskTI->elementType &&
        taskTI->elementType->kind != TypeKind::Void) {
        auto* resultTy = taskTI->elementType->toLLVMType(
            *context_, module_->getDataLayout());
        auto* result = builder_->CreateLoad(resultTy, resultPtr, "task_value");

        // Free the heap-allocated result and the task handle
        auto freeFn = declareBuiltin("free",
            llvm::Type::getVoidTy(*context_), {ptrTy});
        builder_->CreateCall(freeFn, {resultPtr});

        auto taskFreeFn = declareBuiltin("lucis_taskFree",
            llvm::Type::getVoidTy(*context_), {ptrTy});
        builder_->CreateCall(taskFreeFn, {taskVal});

        return static_cast<llvm::Value*>(result);
    }

    // Void task — just await and free
    auto taskFreeFn = declareBuiltin("lucis_taskFree",
        llvm::Type::getVoidTy(*context_), {ptrTy});
    builder_->CreateCall(taskFreeFn, {taskVal});

    return static_cast<llvm::Value*>(
        llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
}

// ── lock statement ──────────────────────────────────────────────────────
// lock(mtx) { ... } → lock, execute block, unlock (even on early return)


std::any IRGen::visitLockStmt(LucisParser::LockStmtContext* ctx) {
        SET_DBG_LOC(ctx);
    auto* mtxVal = castValue(visit(ctx->expression()));

    auto* ptrTy = llvm::PointerType::getUnqual(*context_);

    // Load mutex handle from alloca
    if (auto* alloca = llvm::dyn_cast<llvm::AllocaInst>(mtxVal)) {
        mtxVal = builder_->CreateLoad(ptrTy, alloca, "mtx_ptr");
    }

    // Lock
    auto lockFn = declareBuiltin("lucis_mutexLock",
        llvm::Type::getVoidTy(*context_), {ptrTy});
    builder_->CreateCall(lockFn, {mtxVal});

    // Execute block body
    for (auto* stmt : ctx->block()->statement()) {
        visit(stmt);
    }

    // Unlock
    auto unlockFn = declareBuiltin("lucis_mutexUnlock",
        llvm::Type::getVoidTy(*context_), {ptrTy});
    builder_->CreateCall(unlockFn, {mtxVal});

    return {};
}

// ── try/catch/finally ────────────────────────────────────────────────────────



std::any IRGen::visitTryCatchStmt(LucisParser::TryCatchStmtContext* ctx) {
        SET_DBG_LOC(ctx);
    auto* fn     = currentFunction_;
    auto* ptrTy  = llvm::PointerType::getUnqual(*context_);
    auto* i32Ty  = llvm::Type::getInt32Ty(*context_);
    auto* voidTy = llvm::Type::getVoidTy(*context_);
    auto* errorTy = llvm::StructType::getTypeByName(*context_, "Error");

    // Allocate eh_frame on the heap (opaque, platform-independent size)
    auto allocFn = declareBuiltin("lucis_eh_alloc", ptrTy, {});
    auto* framePtr = builder_->CreateCall(allocFn, {}, "eh_frame");

    // Push frame onto exception handler stack
    auto pushFn = declareBuiltin("lucis_eh_push", voidTy, {ptrTy});
    builder_->CreateCall(pushFn, {framePtr});

    // Get jmp_buf pointer from frame and call setjmp
    auto getJmpBufFn = declareBuiltin("lucis_eh_get_jmpbuf", ptrTy, {ptrTy});
    auto* jmpBufPtr  = builder_->CreateCall(getJmpBufFn, {framePtr}, "jmpbuf_ptr");
    auto setjmpFn    = declareBuiltin("setjmp", i32Ty, {ptrTy});
    auto* sjResult   = builder_->CreateCall(setjmpFn, {jmpBufPtr});

    auto* isCaught = builder_->CreateICmpNE(sjResult,
        llvm::ConstantInt::get(i32Ty, 0), "is_caught");

    // Basic blocks
    auto* tryBB     = llvm::BasicBlock::Create(*context_, "try",     fn);
    auto* catchBB   = llvm::BasicBlock::Create(*context_, "catch",   fn);
    auto* finallyBB = llvm::BasicBlock::Create(*context_, "finally", fn);
    auto* mergeBB   = llvm::BasicBlock::Create(*context_, "try_end", fn);

    builder_->CreateCondBr(isCaught, catchBB, tryBB);

    // ── try block ───────────────────────────────────────────────────────
    builder_->SetInsertPoint(tryBB);
    for (auto* stmt : ctx->block()->statement())
        visit(stmt);

    // Pop handler after successful try
    auto popFn = declareBuiltin("lucis_eh_pop", voidTy, {});
    if (!builder_->GetInsertBlock()->getTerminator()) {
        builder_->CreateCall(popFn, {});
        builder_->CreateBr(finallyBB);
    }

    // ── catch block(s) with type dispatch ───────────────────────────────
    builder_->SetInsertPoint(catchBB);

    // Pop handler before running catch body
    builder_->CreateCall(popFn, {});

    // Load error from frame via helper
    auto getErrFn = declareBuiltin("lucis_eh_get_error", ptrTy, {ptrTy});
    auto* errorPtr = builder_->CreateCall(getErrFn, {framePtr}, "error_ptr");
    auto* errorVal = builder_->CreateLoad(errorTy, errorPtr, "caught_error");

    // Read type discriminator fields from the caught error
    auto* i64Ty = llvm::Type::getInt64Ty(*context_);
    auto* thrownTypeId = builder_->CreateExtractValue(errorVal, {4}, "thrown_type_id");
    auto* thrownParentId = builder_->CreateExtractValue(errorVal, {5}, "thrown_parent_id");

    // FNV-1a hash for comparing type names
    auto fnvHash = [](const std::string& s) -> uint64_t {
        uint64_t h = 14695981039346656037ULL;
        for (char c : s) { h ^= static_cast<uint8_t>(c); h *= 1099511628211ULL; }
        return h;
    };

    // Build a chain of catch clauses with type checking.
    // Clause 0 check is at catchBB; subsequent checks are at noMatch chain.
    // After matching and executing a body, branch to finallyBB.

    llvm::BasicBlock* prevNoMatch = nullptr;

    auto emitCatchCheck = [&](llvm::BasicBlock* insertAt,
                               size_t ci,
                               llvm::BasicBlock*& outNoMatch) {
        auto* catchCtx = ctx->catchClause()[ci];
        auto varName = catchCtx->IDENTIFIER()->getText();
        auto* catchTS = catchCtx->typeSpec();
        auto* catchTI = catchTS ? resolveTypeInfo(catchTS)
                                : typeRegistry_.lookup("Error");
        uint64_t catchHash = fnvHash(catchTI ? catchTI->name : "Error");
        uint64_t catchParentHash = 0;
        if (catchTI && catchTI->parentType)
            catchParentHash = fnvHash(catchTI->parentType->name);

        builder_->SetInsertPoint(insertAt);
        auto* matchExact = builder_->CreateICmpEQ(thrownTypeId,
            llvm::ConstantInt::get(i64Ty, catchHash), "catch.match_exact");
        auto* matchAsChild = builder_->CreateICmpEQ(thrownParentId,
            llvm::ConstantInt::get(i64Ty, catchHash), "catch.match_child");
        auto* matches = builder_->CreateOr(matchExact, matchAsChild, "catch.matches");

        auto* bodyBB = llvm::BasicBlock::Create(*context_,
            "catch.body." + std::to_string(ci), fn);
        outNoMatch = llvm::BasicBlock::Create(*context_,
            "catch.nomatch." + std::to_string(ci), fn);
        builder_->CreateCondBr(matches, bodyBB, outNoMatch);

        // Catch body
        builder_->SetInsertPoint(bodyBB);
        auto* errAlloca = builder_->CreateAlloca(errorTy, nullptr, varName);
        builder_->CreateStore(errorVal, errAlloca);
        auto* errTI = typeRegistry_.lookup("Error");
        auto saved = locals_[varName];
        locals_[varName] = { errAlloca, errTI, 0 };
        for (auto* stmt : catchCtx->block()->statement())
            visit(stmt);
        if (saved.alloca)
            locals_[varName] = saved;
        else
            locals_.erase(varName);
        if (!builder_->GetInsertBlock()->getTerminator())
            builder_->CreateBr(finallyBB);
    };

    for (size_t ci = 0; ci < ctx->catchClause().size(); ci++) {
        llvm::BasicBlock* noMatchBB = nullptr;
        if (ci == 0) {
            emitCatchCheck(catchBB, ci, noMatchBB);
        } else if (ci + 1 < ctx->catchClause().size()) {
            // Middle clause: check falls through from previous noMatch
            emitCatchCheck(prevNoMatch, ci, noMatchBB);
            prevNoMatch = noMatchBB;
        } else {
            // Last clause: noMatch goes to rethrow
            emitCatchCheck(prevNoMatch, ci, noMatchBB);
            // No match for last clause: re-throw
            builder_->SetInsertPoint(noMatchBB);
            builder_->CreateCall(pushFn, {framePtr});
            auto* rethrowPtr = builder_->CreateCall(getErrFn, {framePtr}, "rethrow_ptr");
            builder_->CreateStore(errorVal, rethrowPtr);
            auto throwFn2 = declareBuiltin("lucis_eh_throw", voidTy, {ptrTy});
            builder_->CreateCall(throwFn2, {rethrowPtr});
            builder_->CreateUnreachable();
        }
        if (ci == 0) prevNoMatch = noMatchBB;
    }

    // Single clause: noMatch -> rethrow
    if (ctx->catchClause().size() == 1) {
        builder_->SetInsertPoint(prevNoMatch);
        builder_->CreateCall(pushFn, {framePtr});
        auto* rethrowPtr = builder_->CreateCall(getErrFn, {framePtr}, "rethrow_ptr");
        builder_->CreateStore(errorVal, rethrowPtr);
        auto throwFn2 = declareBuiltin("lucis_eh_throw", voidTy, {ptrTy});
        builder_->CreateCall(throwFn2, {rethrowPtr});
        builder_->CreateUnreachable();
    }

    // ── finally block ───────────────────────────────────────────────────
    builder_->SetInsertPoint(finallyBB);

    if (ctx->finallyClause()) {
        for (auto* stmt : ctx->finallyClause()->block()->statement()) {
            visit(stmt);
        }
    }

    // Free the heap-allocated frame
    auto freeFn = declareBuiltin("lucis_eh_free", voidTy, {ptrTy});
    if (!builder_->GetInsertBlock()->getTerminator()) {
        builder_->CreateCall(freeFn, {framePtr});
        builder_->CreateBr(mergeBB);
    }

    builder_->SetInsertPoint(mergeBB);
    return {};
}



std::any IRGen::visitThrowStmt(LucisParser::ThrowStmtContext* ctx) {
        SET_DBG_LOC(ctx);
    auto* ptrTy  = llvm::PointerType::getUnqual(*context_);
    auto* voidTy = llvm::Type::getVoidTy(*context_);
    auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
    auto* i32Ty  = llvm::Type::getInt32Ty(*context_);
    auto* errorTy = llvm::StructType::getTypeByName(*context_, "Error");

    // Get source location from the throw statement token
    auto* throwToken = ctx->THROW()->getSymbol();
    unsigned srcLine = throwToken->getLine();
    unsigned srcCol  = throwToken->getCharPositionInLine();

    // Get the source file name
    std::string fileName = module_->getSourceFileName();

    // Evaluate the expression (expected to be Error struct literal)
    auto* errorVal = castValue(visit(ctx->expression()));

    // Error struct layout: { {ptr,usize} message, {ptr,usize} file, i32 line, i32 column,
    //                        i64 type_id, i64 parent_type_id }
    //                        field 0               field 1           field 2    field 3
    //                        field 4        field 5

    // Compute FNV-1a hash for type identification
    auto fnvHash = [](const std::string& s) -> uint64_t {
        uint64_t h = 14695981039346656037ULL;
        for (char c : s) { h ^= static_cast<uint8_t>(c); h *= 1099511628211ULL; }
        return h;
    };

    // Set type_id and parent_type_id from the thrown value's LLVM struct type name
    {
        auto* i64Ty = llvm::Type::getInt64Ty(*context_);
        std::string thrownTypeName = "Error";
        uint64_t parentHash = 0;
        if (auto* stTy = llvm::dyn_cast<llvm::StructType>(errorVal->getType())) {
            thrownTypeName = stTy->getName().str();
            // Look up parent type if this type inherits from another
            auto* thrownTI = typeRegistry_.lookup(thrownTypeName);
            if (thrownTI && thrownTI->parentType)
                parentHash = fnvHash(thrownTI->parentType->name);
        }
        errorVal = builder_->CreateInsertValue(errorVal,
            llvm::ConstantInt::get(i64Ty, fnvHash(thrownTypeName)), {4});
        errorVal = builder_->CreateInsertValue(errorVal,
            llvm::ConstantInt::get(i64Ty, parentHash), {5});
    }

    // Auto-fill file if empty (len == 0)
    auto* fileLen = builder_->CreateExtractValue(errorVal, {1, 1}, "file_len");
    auto* fileIsEmpty = builder_->CreateICmpEQ(fileLen,
        llvm::ConstantInt::get(usizeTy, 0), "file_empty");

    auto* fileGlobal = builder_->CreateGlobalString(fileName, ".throw_file", 0, module_);
    auto* fileLenVal = llvm::ConstantInt::get(usizeTy, fileName.size());

    auto* curFilePtr = builder_->CreateExtractValue(errorVal, {1, 0});
    auto* filePtr    = builder_->CreateSelect(fileIsEmpty, fileGlobal, curFilePtr);
    auto* fileLenFin = builder_->CreateSelect(fileIsEmpty, fileLenVal, fileLen);
    errorVal = builder_->CreateInsertValue(errorVal, filePtr,    {1, 0});
    errorVal = builder_->CreateInsertValue(errorVal, fileLenFin, {1, 1});

    // Auto-fill line if zero
    auto* lineVal    = builder_->CreateExtractValue(errorVal, {2}, "line_val");
    auto* lineIsZero = builder_->CreateICmpEQ(lineVal, llvm::ConstantInt::get(i32Ty, 0));
    auto* lineFinal  = builder_->CreateSelect(lineIsZero,
        llvm::ConstantInt::get(i32Ty, srcLine), lineVal);
    errorVal = builder_->CreateInsertValue(errorVal, lineFinal, {2});

    // Auto-fill column if zero
    auto* colVal    = builder_->CreateExtractValue(errorVal, {3}, "col_val");
    auto* colIsZero = builder_->CreateICmpEQ(colVal, llvm::ConstantInt::get(i32Ty, 0));
    auto* colFinal  = builder_->CreateSelect(colIsZero,
        llvm::ConstantInt::get(i32Ty, srcCol), colVal);
    errorVal = builder_->CreateInsertValue(errorVal, colFinal, {3});

    // Store to temp alloca and pass by pointer to lucis_eh_throw
    auto* errAlloca = builder_->CreateAlloca(errorTy, nullptr, "throw_err");
    builder_->CreateStore(errorVal, errAlloca);

    // Phase 1: emit deferred cleanups and auto-drops before throw.
    // The throw uses longjmp so normal scope exit does not run.
    emitAllCleanups();
    deferStack_.clear();  // prevent double-emission on scope exit after catch

    auto throwFn = declareBuiltin("lucis_eh_throw", voidTy, {ptrTy});
    builder_->CreateCall(throwFn, {errAlloca});

    builder_->CreateUnreachable();

    // Dead block for any subsequent unreachable statements
    auto* deadBB = llvm::BasicBlock::Create(*context_, "throw.dead", currentFunction_);
    builder_->SetInsertPoint(deadBB);

    return {};
}



std::any IRGen::visitBailStmt(LucisParser::BailStmtContext* ctx) {
        SET_DBG_LOC(ctx);
    if (!currentFnReturnType_ || currentFnReturnType_->kind != TypeKind::Fallible) {
        std::cerr << "lucis: 'bail' requires enclosing function with !T return type\n";
        return {};
    }

    auto* errVal = castValue(visit(ctx->expression()));
    auto* fnTy = currentFnReturnType_;
    auto* fnLLTy = fnTy->toLLVMType(*context_, module_->getDataLayout());
    auto* errTy = fnTy->errorType->toLLVMType(*context_, module_->getDataLayout());

    // "string → Error" shorthand: wrap string into Error{message: str}
    if (errTy && errTy->isStructTy() && errVal->getType()->isStructTy() &&
        llvm::cast<llvm::StructType>(errVal->getType())->getNumElements() == 2) {
        auto* strLLTy = errTy->getStructElementType(0);
        auto* i32Ty = llvm::Type::getInt32Ty(*context_);
        auto* i64Ty = llvm::Type::getInt64Ty(*context_);
        auto* msgPtr = builder_->CreateExtractValue(errVal, {0}, "bail.msg.ptr");
        auto* msgLen = builder_->CreateExtractValue(errVal, {1}, "bail.msg.len");
        llvm::Value* msgStrVal = llvm::Constant::getNullValue(strLLTy);
        msgStrVal = builder_->CreateInsertValue(msgStrVal, msgPtr, {0});
        msgStrVal = builder_->CreateInsertValue(msgStrVal, msgLen, {1});
        errVal = llvm::Constant::getNullValue(errTy);
        errVal = builder_->CreateInsertValue(errVal, msgStrVal, {0});
        errVal = builder_->CreateInsertValue(errVal,
            llvm::Constant::getNullValue(strLLTy), {1});
        errVal = builder_->CreateInsertValue(errVal,
            llvm::ConstantInt::get(i32Ty, ctx->getStart()->getLine()), {2});
        errVal = builder_->CreateInsertValue(errVal,
            llvm::ConstantInt::get(i32Ty, ctx->getStart()->getCharPositionInLine() + 1), {3});
        errVal = builder_->CreateInsertValue(errVal,
            llvm::ConstantInt::get(i64Ty, 0), {4});
        errVal = builder_->CreateInsertValue(errVal,
            llvm::ConstantInt::get(i64Ty, 0), {5});
    } else if (errVal->getType() != errTy) {
        std::cerr << "lucis: 'bail' type mismatch\n";
        return {};
    }

    auto* tag = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), 1);
    auto* alloca = builder_->CreateAlloca(fnLLTy, nullptr, "bail.ret");
    builder_->CreateStore(tag,
        builder_->CreateStructGEP(fnLLTy, alloca, 0));
    builder_->CreateStore(errVal,
        builder_->CreateStructGEP(fnLLTy, alloca, 2));
    auto* retVal = builder_->CreateLoad(fnLLTy, alloca);

    emitAllCleanups();
    builder_->CreateRet(retVal);
    return {};
}



std::any IRGen::visitTryExpr(LucisParser::TryExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto* fn    = currentFunction_;
    auto* i32Ty = llvm::Type::getInt32Ty(*context_);
    auto* ptrTy = llvm::PointerType::getUnqual(*context_);
    auto* voidTy = llvm::Type::getVoidTy(*context_);

    // Resolve type info before branching (type info is compile-time, not runtime)
    auto* sourceTI  = resolveExprTypeInfo(ctx->expression(0));
    UnwrapCatchPatternInfo pattern;
    std::string reason;
    bool isUnwrapEnum = sourceTI && classifyUnwrapCatchEnum(sourceTI, pattern, reason);

    // Determine result type
    llvm::Type* resultTy = nullptr;
    if (isUnwrapEnum) {
        auto* okPayloadTI = singlePayloadType(*pattern.okVariant);
        resultTy = okPayloadTI
            ? okPayloadTI->toLLVMType(*context_, module_->getDataLayout())
            : (sourceTI ? sourceTI->toLLVMType(*context_, module_->getDataLayout())
                        : llvm::Type::getInt32Ty(*context_));
    } else if (sourceTI) {
        resultTy = sourceTI->toLLVMType(*context_, module_->getDataLayout());
    } else {
        // Visit the expression first to get a concrete LLVM type (no setjmp needed)
        auto* val = castValue(visit(ctx->expression(0)));
        resultTy = val->getType();
        if (ctx->OR() && ctx->expression().size() >= 2) {
            auto* fbVal = castValue(visit(ctx->expression(1)));
            return static_cast<llvm::Value*>(
                fbVal ? coerceValueToType(fbVal, resultTy)
                      : llvm::Constant::getNullValue(resultTy));
        }
        return static_cast<llvm::Value*>(val);
    }

    // Set up setjmp/longjmp for exception handling (also used for enum path
    // to maintain consistent exception handler state)
    auto allocFn = declareBuiltin("lucis_eh_alloc", ptrTy, {});
    auto* framePtr = builder_->CreateCall(allocFn, {}, "try_frame");
    auto pushFn = declareBuiltin("lucis_eh_push", voidTy, {ptrTy});
    builder_->CreateCall(pushFn, {framePtr});
    auto getJmpBufFn = declareBuiltin("lucis_eh_get_jmpbuf", ptrTy, {ptrTy});
    auto* jmpBufPtr = builder_->CreateCall(getJmpBufFn, {framePtr}, "try_jmpbuf");
    auto setjmpFn = declareBuiltin("setjmp", i32Ty, {ptrTy});
    auto* sjResult = builder_->CreateCall(setjmpFn, {jmpBufPtr});
    auto* isCaught = builder_->CreateICmpNE(sjResult,
        llvm::ConstantInt::get(i32Ty, 0), "try_caught");

    auto* exprBB   = llvm::BasicBlock::Create(*context_, "try.expr",   fn);
    auto* caughtBB = llvm::BasicBlock::Create(*context_, "try.caught", fn);
    auto* mergeBB  = llvm::BasicBlock::Create(*context_, "try.merge",  fn);
    builder_->CreateCondBr(isCaught, caughtBB, exprBB);

    auto popFreeHere = [&]() {
        auto popFn  = declareBuiltin("lucis_eh_pop",  voidTy, {});
        auto freeFn = declareBuiltin("lucis_eh_free", voidTy, {ptrTy});
        builder_->CreateCall(popFn, {});
        builder_->CreateCall(freeFn, {framePtr});
    };

    // ── Exception caught path (shared by enum and non-enum) ─────────────
    builder_->SetInsertPoint(caughtBB);
    popFreeHere();
    llvm::Value* caughtFallback = nullptr;
    if (ctx->OR() && ctx->expression().size() >= 2) {
        auto* fbVal = castValue(visit(ctx->expression(1)));
        caughtFallback = fbVal ? coerceValueToType(fbVal, resultTy)
                               : llvm::Constant::getNullValue(resultTy);
    } else {
        caughtFallback = llvm::Constant::getNullValue(resultTy);
    }
    auto* caughtEndBB = builder_->GetInsertBlock();
    builder_->CreateBr(mergeBB);

    // ── Normal evaluation path ──────────────────────────────────────────
    builder_->SetInsertPoint(exprBB);

    auto* sourceVal = castValue(visit(ctx->expression(0)));

    if (isUnwrapEnum) {
        // Enum unwrapping: check tag, extract Ok payload or use fallback
        auto* enumLLTy = sourceTI->toLLVMType(*context_, module_->getDataLayout());

        auto* tempAlloca = builder_->CreateAlloca(enumLLTy, nullptr, "try.tmp");
        builder_->CreateStore(sourceVal, tempAlloca);
        auto* tagPtr = builder_->CreateStructGEP(enumLLTy, tempAlloca, 0, "try.tag.ptr");
        auto* tagVal = builder_->CreateLoad(i32Ty, tagPtr, "try.tag");

        auto* isErr = builder_->CreateICmpEQ(
            tagVal,
            llvm::ConstantInt::get(i32Ty, pattern.errVariant->discriminant),
            "try.is_err");

        auto* okBB  = llvm::BasicBlock::Create(*context_, "try.ok",  fn);
        auto* errBB = llvm::BasicBlock::Create(*context_, "try.err", fn);
        builder_->CreateCondBr(isErr, errBB, okBB);

        // Ok: extract payload
        builder_->SetInsertPoint(okBB);
        popFreeHere();
        auto* okPayloadTI = singlePayloadType(*pattern.okVariant);
        llvm::Value* okResult = nullptr;
        if (okPayloadTI) {
            auto* rawPayloadPtr = builder_->CreateStructGEP(enumLLTy, tempAlloca, 1, "try.ok.payload.ptr");
            auto* payloadPtr = builder_->CreateBitCast(
                rawPayloadPtr,
                llvm::PointerType::getUnqual(resultTy),
                "try.ok.payload.cast");
            okResult = builder_->CreateLoad(resultTy, payloadPtr, "try.ok.payload");
        } else {
            okResult = llvm::Constant::getNullValue(resultTy);
        }
        auto* okEndBB = builder_->GetInsertBlock();
        builder_->CreateBr(mergeBB);

        // Err: use fallback
        builder_->SetInsertPoint(errBB);
        popFreeHere();
        llvm::Value* errResult = nullptr;
        if (ctx->OR() && ctx->expression().size() >= 2) {
            auto* fbVal = castValue(visit(ctx->expression(1)));
            errResult = fbVal ? coerceValueToType(fbVal, resultTy)
                              : llvm::Constant::getNullValue(resultTy);
        } else {
            errResult = llvm::Constant::getNullValue(resultTy);
        }
        auto* errEndBB = builder_->GetInsertBlock();
        builder_->CreateBr(mergeBB);

        // Merge (3 predecessors: okBB, errBB, caughtBB)
        builder_->SetInsertPoint(mergeBB);
        auto* phi = builder_->CreatePHI(resultTy, 3, "try_result");
        phi->addIncoming(okResult, okEndBB);
        phi->addIncoming(errResult, errEndBB);
        phi->addIncoming(caughtFallback, caughtEndBB);
        return static_cast<llvm::Value*>(phi);
    }

    // ── Non-enum path ───────────────────────────────────────────────────
    popFreeHere();
    auto* exprEndBB = builder_->GetInsertBlock();
    builder_->CreateBr(mergeBB);

    // ── Merge (expr + caught) ───────────────────────────────────────────
    builder_->SetInsertPoint(mergeBB);
    auto* phi = builder_->CreatePHI(resultTy, 2, "try_result");
    phi->addIncoming(sourceVal, exprEndBB);
    phi->addIncoming(caughtFallback, caughtEndBB);
    return static_cast<llvm::Value*>(phi);
}



std::any IRGen::visitCatchUnwrapExpr(LucisParser::CatchUnwrapExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto* fn = currentFunction_;
    auto* sourceVal = castValue(visit(ctx->expression()));
    auto* sourceTI = resolveExprTypeInfo(ctx->expression());

    // Fallible source: !T — handle natively
    if (sourceTI && sourceTI->kind == TypeKind::Fallible) {
        auto* fallibleLLTy = sourceTI->toLLVMType(*context_, module_->getDataLayout());
        auto* okPayloadTy = sourceTI->successType->toLLVMType(*context_, module_->getDataLayout());
        auto* errPayloadTy = sourceTI->errorType->toLLVMType(*context_, module_->getDataLayout());

        auto* tempAlloca = builder_->CreateAlloca(fallibleLLTy, nullptr, "unwrap.tmp");
        builder_->CreateStore(sourceVal, tempAlloca);

        auto* tagPtr = builder_->CreateStructGEP(fallibleLLTy, tempAlloca, 0, "unwrap.tag.ptr");
        auto* tagVal = builder_->CreateLoad(llvm::Type::getInt32Ty(*context_), tagPtr, "unwrap.tag");
        auto* isErr = builder_->CreateICmpEQ(tagVal,
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), 1), "unwrap.is_err");

        auto* okBB = llvm::BasicBlock::Create(*context_, "unwrap.ok", fn);
        auto* errBB = llvm::BasicBlock::Create(*context_, "unwrap.err", fn);
        auto* mergeBB = llvm::BasicBlock::Create(*context_, "unwrap.merge", fn);
        builder_->CreateCondBr(isErr, errBB, okBB);

        // Success path: extract field 1
        builder_->SetInsertPoint(okBB);
        auto* okPayload = builder_->CreateExtractValue(sourceVal, {1}, "unwrap.ok.payload");
        builder_->CreateBr(mergeBB);
        auto* okEndBB = builder_->GetInsertBlock();

        // Error path: extract error from field 2, bind `it`
        builder_->SetInsertPoint(errBB);
        auto* errPayload = builder_->CreateExtractValue(sourceVal, {2}, "unwrap.err.payload");

        auto itSaved = locals_.find("it");
        bool hadIt = itSaved != locals_.end();
        VarInfo savedInfo;
        if (hadIt) savedInfo = itSaved->second;

        auto* itAlloca = builder_->CreateAlloca(errPayloadTy, nullptr, "it");
        builder_->CreateStore(errPayload, itAlloca);
        locals_["it"] = {itAlloca, sourceTI->errorType, 0};

        for (auto* stmt : ctx->block()->statement()) visit(stmt);

        if (hadIt) locals_["it"] = savedInfo;
        else locals_.erase("it");

        bool errFallsThrough = !builder_->GetInsertBlock()->getTerminator();
        llvm::Value* errFallback = nullptr;
        llvm::BasicBlock* errEndBB = nullptr;
        if (errFallsThrough) {
            errFallback = llvm::Constant::getNullValue(okPayloadTy);
            builder_->CreateBr(mergeBB);
            errEndBB = builder_->GetInsertBlock();
        }

        builder_->SetInsertPoint(mergeBB);
        if (errFallsThrough) {
            auto* phi = builder_->CreatePHI(okPayloadTy, 2, "unwrap.value");
            phi->addIncoming(okPayload, okEndBB);
            phi->addIncoming(errFallback, errEndBB);
            return static_cast<llvm::Value*>(phi);
        }
        return static_cast<llvm::Value*>(okPayload);
    }

    UnwrapCatchPatternInfo pattern;
    std::string reason;
    if (!classifyUnwrapCatchEnum(sourceTI, pattern, reason)) {
        std::cerr << "lucis: " << reason << "\n";
        return static_cast<llvm::Value*>(llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }

    auto* enumLLTy = sourceTI->toLLVMType(*context_, module_->getDataLayout());
    auto* okPayloadTy = getEnumVariantPayloadType(*pattern.okVariant);
    auto* errPayloadTy = getEnumVariantPayloadType(*pattern.errVariant);
    if (!okPayloadTy) {
        std::cerr << "lucis: invalid unwrap-catch enum payload layout\n";
        return static_cast<llvm::Value*>(llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }

    auto* tempAlloca = builder_->CreateAlloca(enumLLTy, nullptr, "unwrap.tmp");
    builder_->CreateStore(sourceVal, tempAlloca);

    auto* tagPtr = builder_->CreateStructGEP(enumLLTy, tempAlloca, 0, "unwrap.tag.ptr");
    auto* tagVal = builder_->CreateLoad(llvm::Type::getInt32Ty(*context_), tagPtr, "unwrap.tag");
    auto* isErr = builder_->CreateICmpEQ(
        tagVal,
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), pattern.errVariant->discriminant),
        "unwrap.is_err");

    auto* okBB = llvm::BasicBlock::Create(*context_, "unwrap.ok", fn);
    auto* errBB = llvm::BasicBlock::Create(*context_, "unwrap.err", fn);
    auto* mergeBB = llvm::BasicBlock::Create(*context_, "unwrap.merge", fn);
    builder_->CreateCondBr(isErr, errBB, okBB);

    // Success path: extract payload and continue.
    builder_->SetInsertPoint(okBB);
    auto* rawOkPayloadPtr = builder_->CreateStructGEP(enumLLTy, tempAlloca, 1, "unwrap.ok.payload.ptr");
    auto* okPayloadPtr = builder_->CreateBitCast(
        rawOkPayloadPtr,
        llvm::PointerType::getUnqual(okPayloadTy),
        "unwrap.ok.payload.cast");
    auto* okPayloadVal = builder_->CreateLoad(okPayloadTy, okPayloadPtr, "unwrap.ok.payload");
    builder_->CreateBr(mergeBB);
    auto* okEndBB = builder_->GetInsertBlock();

    // Error path: bind implicit `it` (if err has payload) and run catch block.
    builder_->SetInsertPoint(errBB);
    if (errPayloadTy) {
        auto* rawErrPayloadPtr = builder_->CreateStructGEP(enumLLTy, tempAlloca, 1, "unwrap.err.payload.ptr");
        auto* errPayloadPtr = builder_->CreateBitCast(
            rawErrPayloadPtr,
            llvm::PointerType::getUnqual(errPayloadTy),
            "unwrap.err.payload.cast");
        auto* errPayloadVal = builder_->CreateLoad(errPayloadTy, errPayloadPtr, "unwrap.err.payload");

        auto itSaved = locals_.find("it");
        bool hadIt = itSaved != locals_.end();
        VarInfo savedInfo;
        if (hadIt) savedInfo = itSaved->second;

        auto* itAlloca = builder_->CreateAlloca(errPayloadTy, nullptr, "it");
        builder_->CreateStore(errPayloadVal, itAlloca);
        unsigned itDims = 0;
        if (!pattern.errVariant->payloadFields.empty())
            itDims = pattern.errVariant->payloadFields[0].arrayDims;
        locals_["it"] = {itAlloca, singlePayloadType(*pattern.errVariant), itDims};

        for (auto* stmt : ctx->block()->statement()) {
            visit(stmt);
        }

        if (hadIt) locals_["it"] = savedInfo;
        else locals_.erase("it");
    } else {
        // No payload in error variant (e.g. Option<T>::Nothing) — no `it` binding
        for (auto* stmt : ctx->block()->statement()) {
            visit(stmt);
        }
    }

    bool errFallsThrough = !builder_->GetInsertBlock()->getTerminator();
    llvm::Value* errFallback = nullptr;
    llvm::BasicBlock* errEndBB = nullptr;
    if (errFallsThrough) {
        errFallback = llvm::Constant::getNullValue(okPayloadTy);
        builder_->CreateBr(mergeBB);
        errEndBB = builder_->GetInsertBlock();
    }

    builder_->SetInsertPoint(mergeBB);

    if (errFallsThrough) {
        auto* phi = builder_->CreatePHI(okPayloadTy, 2, "unwrap.value");
        phi->addIncoming(okPayloadVal, okEndBB);
        phi->addIncoming(errFallback, errEndBB);
        return static_cast<llvm::Value*>(phi);
    }

    return static_cast<llvm::Value*>(okPayloadVal);
}



std::any IRGen::visitPropagateExpr(LucisParser::PropagateExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto* fn = currentFunction_;
    auto* sourceVal = castValue(visit(ctx->expression()));
    auto* sourceTI = resolveExprTypeInfo(ctx->expression());

    // Fallible source: !T — propagate natively
    if (sourceTI && sourceTI->kind == TypeKind::Fallible) {
        auto* fnRetTy = currentFnReturnType_;
        if (!fnRetTy || fnRetTy->kind != TypeKind::Fallible) {
            std::cerr << "lucis: '?' requires enclosing !T return type, got '"
                      << (fnRetTy ? fnRetTy->name : "void") << "'\n";
            return static_cast<llvm::Value*>(
                llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
        }

        auto* fallibleLLTy = sourceTI->toLLVMType(*context_, module_->getDataLayout());
        auto* tempAlloca = builder_->CreateAlloca(fallibleLLTy, nullptr, "prop.tmp");
        builder_->CreateStore(sourceVal, tempAlloca);

        auto* tagPtr = builder_->CreateStructGEP(fallibleLLTy, tempAlloca, 0, "prop.tag.ptr");
        auto* tagVal = builder_->CreateLoad(llvm::Type::getInt32Ty(*context_), tagPtr, "prop.tag");
        auto* isErr = builder_->CreateICmpEQ(tagVal,
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), 1), "prop.is_err");

        auto* okBB  = llvm::BasicBlock::Create(*context_, "prop.ok",  fn);
        auto* errBB = llvm::BasicBlock::Create(*context_, "prop.err", fn);
        auto* mergeBB = llvm::BasicBlock::Create(*context_, "prop.merge", fn);
        builder_->CreateCondBr(isErr, errBB, okBB);

        // Error path: build return !T with error payload
        builder_->SetInsertPoint(errBB);
        if (auto* ctxBlock = ctx->block()) {
            for (auto* stmt : ctxBlock->statement()) visit(stmt);
        }
        auto* errPayload = builder_->CreateExtractValue(sourceVal, {2}, "prop.err.payload");
        auto* retFallibleTy = fnRetTy->toLLVMType(*context_, module_->getDataLayout());
        auto* retAlloca = builder_->CreateAlloca(retFallibleTy, nullptr, "prop.ret");
        auto* retTag = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), 1);
        builder_->CreateStore(retTag,
            builder_->CreateStructGEP(retFallibleTy, retAlloca, 0));
        builder_->CreateStore(errPayload,
            builder_->CreateStructGEP(retFallibleTy, retAlloca, 2));
        auto* retVal = builder_->CreateLoad(retFallibleTy, retAlloca);
        emitAllCleanups();
        builder_->CreateRet(retVal);

        // Success path: extract value and continue
        builder_->SetInsertPoint(okBB);
        auto* okPayload = builder_->CreateExtractValue(sourceVal, {1}, "prop.ok.payload");
        builder_->CreateBr(mergeBB);
        auto* okEndBB = builder_->GetInsertBlock();

        builder_->SetInsertPoint(mergeBB);
        auto* succTy = sourceTI->successType->toLLVMType(*context_, module_->getDataLayout());
        auto* phi = builder_->CreatePHI(succTy, 1, "prop.value");
        phi->addIncoming(okPayload, okEndBB);
        return static_cast<llvm::Value*>(phi);
    }

    // Phase 3: void functions can have unit success variant
    bool isVoidReturn = currentFnReturnType_ &&
                        currentFnReturnType_->kind == TypeKind::Void;

    UnwrapCatchPatternInfo pattern;
    std::string reason;
    if (isVoidReturn) {
        if (!sourceTI || sourceTI->kind != TypeKind::Enum ||
            sourceTI->enumVariantInfos.empty()) {
            std::cerr << "lucis: ? requires an enum expression\n";
            return {};
        }
        for (const auto& v : sourceTI->enumVariantInfos) {
            // Error variant: 0 payload fields, #[error], or payload is Error
            bool isErr = v.payloadFields.empty()
                || v.isError
                || (v.payloadFields.size() == 1
                    && v.payloadFields[0].typeInfo
                    && isOrInheritsFrom(v.payloadFields[0].typeInfo, "Error"));
            if (isErr) { pattern.errVariant = &v; break; }
        }
        if (!pattern.errVariant) {
            std::cerr << "lucis: ? in void function requires enum with error variant\n";
            return {};
        }
    } else {
        if (!classifyUnwrapCatchEnum(sourceTI, pattern, reason)) {
            std::cerr << "lucis: " << reason << "\n";
            return static_cast<llvm::Value*>(llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
        }
    }

    // Phase 2: when source enum differs from function return enum, use the
    // return enum's variant info when constructing the error-path ret value.
    const TypeInfo* retEnumTI = sourceTI;
    const UnwrapCatchPatternInfo* retPattern = &pattern;
    UnwrapCatchPatternInfo retPatternStorage;
    if (currentFnReturnType_ && currentFnReturnType_ != sourceTI &&
        currentFnReturnType_->kind == TypeKind::Enum) {
        std::string retReason;
        if (classifyUnwrapCatchEnum(currentFnReturnType_, retPatternStorage, retReason)) {
            retEnumTI = currentFnReturnType_;
            retPattern = &retPatternStorage;
        }
    }

    auto* enumLLTy = sourceTI->toLLVMType(*context_, module_->getDataLayout());
    auto* okPayloadTy = isVoidReturn ? nullptr
                        : getEnumVariantPayloadType(*pattern.okVariant);
    auto* errPayloadTy = getEnumVariantPayloadType(*pattern.errVariant);
    if (!isVoidReturn && !okPayloadTy) {
        std::cerr << "lucis: invalid propagate enum payload layout\n";
        return static_cast<llvm::Value*>(llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }

    auto* tempAlloca = builder_->CreateAlloca(enumLLTy, nullptr, "prop.tmp");
    builder_->CreateStore(sourceVal, tempAlloca);

    auto* tagPtr = builder_->CreateStructGEP(enumLLTy, tempAlloca, 0, "prop.tag.ptr");
    auto* tagVal = builder_->CreateLoad(llvm::Type::getInt32Ty(*context_), tagPtr, "prop.tag");
    auto* isErr = builder_->CreateICmpEQ(
        tagVal,
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), pattern.errVariant->discriminant),
        "prop.is_err");

    // Phase 3: void functions — discard success, return void on error
    auto* okBB  = llvm::BasicBlock::Create(*context_, "prop.ok",  fn);
    auto* errBB = llvm::BasicBlock::Create(*context_, "prop.err", fn);
    auto* mergeBB = isVoidReturn ? nullptr
                    : llvm::BasicBlock::Create(*context_, "prop.merge", fn);
    builder_->CreateCondBr(isErr, errBB, okBB);

    // Phase 5: optional context block executes on error path before propagation
    auto* ctxBlock = ctx->block();

    // Error path
    builder_->SetInsertPoint(errBB);
    if (ctxBlock) {
        for (auto* stmt : ctxBlock->statement())
            visit(stmt);
    }
    if (isVoidReturn) {
        emitAllCleanups();
        builder_->CreateRetVoid();
    } else {
        llvm::Value* errEnumVal;
        if (errPayloadTy) {
            auto* rawErrPayloadPtr = builder_->CreateStructGEP(enumLLTy, tempAlloca, 1, "prop.err.payload.ptr");
            auto* errPayloadPtr = builder_->CreateBitCast(
                rawErrPayloadPtr,
                llvm::PointerType::getUnqual(*context_),
                "prop.err.payload.cast");
            auto* errPayloadVal = builder_->CreateLoad(errPayloadTy, errPayloadPtr, "prop.err.payload");
            std::vector<llvm::Value*> payloads = {errPayloadVal};
            errEnumVal = buildEnumVariantValue(retEnumTI, *retPattern->errVariant, payloads);
        } else {
            // No payload in error variant (e.g. Option<T>::Nothing)
            errEnumVal = buildEnumVariantValue(retEnumTI, *retPattern->errVariant, {});
        }
        emitAllCleanups();
        builder_->CreateRet(errEnumVal);
    }

    // Success path
    builder_->SetInsertPoint(okBB);
    if (isVoidReturn) {
        // void function: discard success, just continue
        return {};
    }
    auto* rawOkPayloadPtr = builder_->CreateStructGEP(enumLLTy, tempAlloca, 1, "prop.ok.payload.ptr");
    auto* okPayloadPtr = builder_->CreateBitCast(
        rawOkPayloadPtr,
        llvm::PointerType::getUnqual(*context_),
        "prop.ok.payload.cast");
    auto* okPayloadVal = builder_->CreateLoad(okPayloadTy, okPayloadPtr, "prop.ok.payload");
    builder_->CreateBr(mergeBB);
    auto* okEndBB = builder_->GetInsertBlock();

    builder_->SetInsertPoint(mergeBB);
    auto* phi = builder_->CreatePHI(okPayloadTy, 1, "prop.value");
    phi->addIncoming(okPayloadVal, okEndBB);
    return static_cast<llvm::Value*>(phi);
}



std::any IRGen::visitMatchExpr(LucisParser::MatchExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto* fn = currentFunction_;
    auto* sourceVal = castValue(visit(ctx->expression()));
    auto* sourceTI = resolveExprTypeInfo(ctx->expression());

    if (!sourceTI) {
        std::cerr << "lucis: match requires a typed expression\n";
        return static_cast<llvm::Value*>(llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }

    // Check if this is a literal match (has literal patterns)
    bool hasLiteralPattern = false;
    for (auto* arm : ctx->matchArm()) {
        for (auto* pattern : arm->pattern()) {
            if (pattern->literalPattern()) {
                hasLiteralPattern = true;
                break;
            }
        }
        if (hasLiteralPattern) break;
    }

    // Validate type based on pattern type
    if (hasLiteralPattern) {
        bool validType = sourceTI->kind == TypeKind::Integer ||
                        sourceTI->kind == TypeKind::Float ||
                        sourceTI->kind == TypeKind::String ||
                        sourceTI->kind == TypeKind::Bool ||
                        sourceTI->kind == TypeKind::Char;
        if (!validType) {
            std::cerr << "lucis: match with literal patterns requires int, float, string, bool, or char\n";
            return static_cast<llvm::Value*>(llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
        }
    } else if (sourceTI->kind != TypeKind::Enum) {
        std::cerr << "lucis: match requires an enum expression\n";
        return static_cast<llvm::Value*>(llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }

    auto* sourceLLTy = sourceTI->toLLVMType(*context_, module_->getDataLayout());
    auto* i32Ty = llvm::Type::getInt32Ty(*context_);
    bool isUnitEnum = (sourceTI->kind == TypeKind::Enum && 
                       (sourceLLTy == i32Ty || sourceLLTy->isIntegerTy()));

    auto* tempAlloca = builder_->CreateAlloca(sourceLLTy, nullptr, "match.tmp");
    builder_->CreateStore(sourceVal, tempAlloca);

    auto* mergeBB = llvm::BasicBlock::Create(*context_, "match.merge", fn);
    llvm::Type* resultTy = nullptr;
    std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>> phiIncoming;

    auto arms = ctx->matchArm();
    for (size_t i = 0; i < arms.size(); i++) {
        auto* arm = arms[i];
        bool isLast = (i + 1 == arms.size());

        // Or-patterns: arm matches if ANY pattern matches
        bool hasWildcard = false;
        llvm::Value* armMatches = nullptr;
        for (size_t pi = 0; pi < arm->pattern().size(); pi++) {
            auto* pattern = arm->pattern(pi);
            if (pattern->WILDCARD()) { hasWildcard = true; break; }

            // Handle literal patterns
            if (auto* lit = pattern->literalPattern()) {
                llvm::Value* litVal = nullptr;
                llvm::Value* cmpVal = nullptr;

                if (lit->INT_LIT() || lit->HEX_LIT() || lit->OCT_LIT() || lit->BIN_LIT()) {
                    std::string litText = lit->getText();
                    int64_t intVal = 0;
                    if (litText.size() > 2 && litText[0] == '0' && (litText[1] == 'x' || litText[1] == 'X')) {
                        intVal = std::stoll(litText, nullptr, 16);
                    } else if (litText.size() > 2 && litText[0] == '0' && (litText[1] == 'o' || litText[1] == 'O')) {
                        intVal = std::stoll(litText.substr(2), nullptr, 8);
                    } else if (litText.size() > 2 && litText[0] == '0' && (litText[1] == 'b' || litText[1] == 'B')) {
                        intVal = std::stoll(litText.substr(2), nullptr, 2);
                    } else {
                        intVal = std::stoll(litText);
                    }
                    auto* sourceLLTy = sourceTI->toLLVMType(*context_, module_->getDataLayout());
                    litVal = llvm::ConstantInt::get(sourceLLTy, intVal);
                    cmpVal = builder_->CreateLoad(sourceLLTy, tempAlloca, "match.val");
                    auto* patMatch = builder_->CreateICmpEQ(cmpVal, litVal);
                    if (armMatches)
                        armMatches = builder_->CreateOr(armMatches, patMatch);
                    else
                        armMatches = patMatch;
                } else if (lit->FLOAT_LIT()) {
                    double floatVal = std::stod(lit->getText());
                    auto* sourceLLTy = sourceTI->toLLVMType(*context_, module_->getDataLayout());
                    litVal = llvm::ConstantFP::get(sourceLLTy, floatVal);
                    cmpVal = builder_->CreateLoad(sourceLLTy, tempAlloca, "match.val");
                    auto* patMatch = builder_->CreateFCmpOEQ(cmpVal, litVal);
                    if (armMatches)
                        armMatches = builder_->CreateOr(armMatches, patMatch);
                    else
                        armMatches = patMatch;
                } else if (lit->BOOL_LIT()) {
                    bool boolVal = (lit->getText() == "true");
                    litVal = llvm::ConstantInt::get(llvm::Type::getInt1Ty(*context_), boolVal);
                    cmpVal = builder_->CreateLoad(llvm::Type::getInt1Ty(*context_), tempAlloca, "match.val");
                    auto* patMatch = builder_->CreateICmpEQ(cmpVal, litVal);
                    if (armMatches)
                        armMatches = builder_->CreateOr(armMatches, patMatch);
                    else
                        armMatches = patMatch;
                } else if (lit->CHAR_LIT()) {
                    std::string charText = lit->getText();
                    char charVal = charText.size() >= 3 ? charText[1] : '\0';
                    litVal = llvm::ConstantInt::get(llvm::Type::getInt8Ty(*context_), charVal);
                    cmpVal = builder_->CreateLoad(llvm::Type::getInt8Ty(*context_), tempAlloca, "match.val");
                    auto* patMatch = builder_->CreateICmpEQ(cmpVal, litVal);
                    if (armMatches)
                        armMatches = builder_->CreateOr(armMatches, patMatch);
                    else
                        armMatches = patMatch;
                } else if (lit->STR_LIT() || lit->C_STR_LIT()) {
                    // String comparison using strcmp
                    std::string strText = lit->getText();
                    if (strText.size() >= 2) {
                        strText = strText.substr(1, strText.size() - 2); // Remove quotes
                    }
                    auto* strConst = builder_->CreateGlobalStringPtr(strText, "match.str");
                    auto* sourceLLTy = sourceTI->toLLVMType(*context_, module_->getDataLayout());
                    auto* matchVal = builder_->CreateLoad(sourceLLTy, tempAlloca, "match.val");
                    // Extract pointer from {ptr, i64} string struct
                    llvm::Value* strPtr = matchVal;
                    if (sourceLLTy->isStructTy()) {
                        strPtr = builder_->CreateExtractValue(matchVal, 0, "match.str.ptr");
                    }
                    auto* ptrTy = llvm::PointerType::getUnqual(*context_);
                    auto strcmpCallee = module_->getOrInsertFunction("strcmp",
                        llvm::FunctionType::get(i32Ty, {ptrTy, ptrTy}, false));
                    auto* cmpResult = builder_->CreateCall(strcmpCallee, {strPtr, strConst});
                    auto* patMatch = builder_->CreateICmpEQ(cmpResult, llvm::ConstantInt::get(i32Ty, 0));
                    if (armMatches)
                        armMatches = builder_->CreateOr(armMatches, patMatch);
                    else
                        armMatches = patMatch;
                } else if (lit->NULL_LIT()) {
                    auto* ptrTy = sourceTI->toLLVMType(*context_, module_->getDataLayout());
                    litVal = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptrTy));
                    cmpVal = builder_->CreateLoad(ptrTy, tempAlloca, "match.val");
                    auto* patMatch = builder_->CreateICmpEQ(cmpVal, litVal);
                    if (armMatches)
                        armMatches = builder_->CreateOr(armMatches, patMatch);
                    else
                        armMatches = patMatch;
                }
                continue;
            }

            std::string variantName;
            if (pattern->SCOPE() && pattern->IDENTIFIER().size() >= 2)
                variantName = pattern->IDENTIFIER(1)->getText();
            else if (!pattern->IDENTIFIER().empty())
                variantName = pattern->IDENTIFIER(0)->getText();

            unsigned disc = 0;
            for (auto& v : sourceTI->enumVariantInfos)
                if (v.name == variantName) { disc = v.discriminant; break; }

            llvm::Value* tagVal;
            if (isUnitEnum)
                tagVal = builder_->CreateLoad(i32Ty, tempAlloca, "match.tag");
            else {
                auto* tagPtr = builder_->CreateStructGEP(sourceLLTy, tempAlloca, 0, "match.tag.ptr");
                tagVal = builder_->CreateLoad(i32Ty, tagPtr, "match.tag");
            }
            auto* patMatch = builder_->CreateICmpEQ(tagVal,
                llvm::ConstantInt::get(i32Ty, disc));
            if (armMatches)
                armMatches = builder_->CreateOr(armMatches, patMatch);
            else
                armMatches = patMatch;
        }

        auto* armBB  = llvm::BasicBlock::Create(*context_, "match.arm" + std::to_string(i), fn);
        auto* nextBB = isLast ? mergeBB
            : llvm::BasicBlock::Create(*context_, "match.next" + std::to_string(i), fn);

        // Guard: if present, create intermediate block for condition check
        auto* guardBB = arm->IF()
            ? llvm::BasicBlock::Create(*context_, "match.guard" + std::to_string(i), fn)
            : armBB;

        auto* cmpBB = builder_->GetInsertBlock();

        if (hasWildcard || !armMatches) {
            builder_->CreateBr(guardBB);
        } else {
            builder_->CreateCondBr(armMatches, guardBB, nextBB);
        }

        // Set up payload binding BEFORE guard check (so guard can reference it)
        LucisParser::PatternContext* bindPattern = nullptr;
        for (size_t pi = 0; pi < arm->pattern().size(); pi++) {
            auto* p = arm->pattern(pi);
            if (!p->WILDCARD() && p->LPAREN()) { bindPattern = p; break; }
        }

        // Guard check (or arm body if no guard)
        if (arm->IF()) {
            builder_->SetInsertPoint(guardBB);
            // Set up binding so guard expression can use it
            if (!hasWildcard && !isUnitEnum && bindPattern && !bindPattern->IDENTIFIER().empty()) {
                std::string bindName = bindPattern->IDENTIFIER().back()->getText();
                if (bindName != "_") {
                    std::string variantName;
                    if (bindPattern->SCOPE() && bindPattern->IDENTIFIER().size() >= 2)
                        variantName = bindPattern->IDENTIFIER(1)->getText();
                    else
                        variantName = bindPattern->IDENTIFIER(0)->getText();
                    const EnumVariantInfo* vi = nullptr;
                    for (auto& v : sourceTI->enumVariantInfos)
                        if (v.name == variantName) { vi = &v; break; }
                    if (vi && !vi->payloadFields.empty() && vi->payloadFields[0].typeInfo) {
                        auto* payloadTy = vi->payloadFields[0].typeInfo->toLLVMType(*context_, module_->getDataLayout());
                        auto* rawPtr = builder_->CreateStructGEP(sourceLLTy, tempAlloca, 1, "match.payload");
                        auto* castPtr = builder_->CreateBitCast(rawPtr, llvm::PointerType::getUnqual(*context_));
                        auto* payloadVal = builder_->CreateLoad(payloadTy, castPtr, "match.bind");
                        auto* alloca = builder_->CreateAlloca(payloadTy, nullptr, bindName);
                        builder_->CreateStore(payloadVal, alloca);
                        VarInfo bindVi;
                        bindVi.alloca = alloca;
                        bindVi.typeInfo = vi->payloadFields[0].typeInfo;
                        bindVi.arrayDims = 0;
                        bindVi.isParam = true;
                        bindVi.isBorrowed = false;
                        locals_[bindName] = bindVi;
                    }
                }
            }
            auto* guardVal = castValue(visit(arm->expression(0)));
            auto* guardOk = builder_->CreateICmpNE(guardVal,
                llvm::ConstantInt::get(llvm::Type::getInt1Ty(*context_), 0));
            builder_->CreateCondBr(guardOk, armBB, nextBB);
        }

        // Arm body
        builder_->SetInsertPoint(armBB);

        // Handle payload binding (only for non-unit enums with LPAREN, use first non-wildcard pattern)
        // (bindPattern already resolved above)
        if (!hasWildcard && !isUnitEnum && bindPattern && !bindPattern->IDENTIFIER().empty()) {
            std::string bindName = bindPattern->IDENTIFIER().back()->getText();
            if (bindName != "_") {
                std::string variantName;
                if (bindPattern->SCOPE() && bindPattern->IDENTIFIER().size() >= 2)
                    variantName = bindPattern->IDENTIFIER(1)->getText();
                else
                    variantName = bindPattern->IDENTIFIER(0)->getText();

                const EnumVariantInfo* vi = nullptr;
                for (auto& v : sourceTI->enumVariantInfos)
                    if (v.name == variantName) { vi = &v; break; }

                if (vi && !vi->payloadFields.empty() && vi->payloadFields[0].typeInfo) {
                    auto* payloadTy = vi->payloadFields[0].typeInfo->toLLVMType(*context_, module_->getDataLayout());
                    auto* rawPtr = builder_->CreateStructGEP(sourceLLTy, tempAlloca, 1, "match.payload");
                    auto* castPtr = builder_->CreateBitCast(rawPtr, llvm::PointerType::getUnqual(*context_));
                    auto* payloadVal = builder_->CreateLoad(payloadTy, castPtr, "match.bind");
                    auto* alloca = builder_->CreateAlloca(payloadTy, nullptr, bindName);
                    builder_->CreateStore(payloadVal, alloca);
                    VarInfo bindVi;
                    bindVi.alloca = alloca;
                    bindVi.typeInfo = vi->payloadFields[0].typeInfo;
                    bindVi.arrayDims = 0;
                    bindVi.isParam = true;
                    bindVi.isBorrowed = false;
                    locals_[bindName] = bindVi;
                }
            }
        }

        // Emit body — prefer expression, fall back to block
        llvm::Value* bodyVal = nullptr;
        size_t bodyExprIdx = arm->IF() ? 1 : 0;
        if (bodyExprIdx < arm->expression().size()) {
            bodyVal = castValue(visit(arm->expression(bodyExprIdx)));
        } else if (arm->block()) {
            auto stmts = arm->block()->statement();
            for (size_t si = 0; si < stmts.size(); si++) {
                auto* stmt = stmts[si];
                // For the last statement, try to capture its value if it's an expression
                if (si + 1 == stmts.size()) {
                    if (auto* exprS = stmt->exprStmt()) {
                        bodyVal = castValue(visit(exprS->expression()));
                    } else if (auto* ret = stmt->returnStmt()) {
                        if (ret->expression()) {
                            bodyVal = castValue(visit(ret->expression()));
                        }
                        visit(stmt); // Still visit to emit the return
                    } else {
                        visit(stmt);
                    }
                } else {
                    visit(stmt);
                }
            }
        }
        if (!resultTy && bodyVal) resultTy = bodyVal->getType();
        auto* armEndBB = builder_->GetInsertBlock();
        if (!armEndBB->getTerminator()) {
            if (!bodyVal) bodyVal = llvm::UndefValue::get(i32Ty);
            builder_->CreateBr(mergeBB);
        }
        if (bodyVal) {
            phiIncoming.push_back({bodyVal, armEndBB});
        }

        // Clean up binding
        if (!hasWildcard && !isUnitEnum && bindPattern && bindPattern->LPAREN() && !bindPattern->IDENTIFIER().empty()) {
            std::string bindName = bindPattern->IDENTIFIER().back()->getText();
            if (bindName != "_") locals_.erase(bindName);
        }

        // For last non-wildcard arm, failure path goes directly to merge → needs PHI entry
        if (isLast && !hasWildcard) {
            if (!resultTy) resultTy = i32Ty;
            phiIncoming.push_back({llvm::UndefValue::get(resultTy), cmpBB});
        } else if (!isLast || hasWildcard) {
            builder_->SetInsertPoint(nextBB);
        }
    }

    // PHI merge
    builder_->SetInsertPoint(mergeBB);
    if (phiIncoming.empty())
        return static_cast<llvm::Value*>(llvm::UndefValue::get(i32Ty));

    if (!resultTy) resultTy = phiIncoming[0].first->getType();
    auto* phi = builder_->CreatePHI(resultTy, phiIncoming.size(), "match.result");
    for (auto& [val, bb] : phiIncoming) {
        auto* coerced = coerceValueToType(val, resultTy);
        phi->addIncoming(coerced, bb);
    }
    return static_cast<llvm::Value*>(phi);
}

std::pair<llvm::Value*, llvm::Value*>
IRGen::promoteArithmetic(llvm::Value* lhs, llvm::Value* rhs) {
    auto* lhsTy = lhs->getType();
    auto* rhsTy = rhs->getType();
    if (lhsTy == rhsTy) return {lhs, rhs};

    // Both integer: if the wider operand is a constant that fits in the
    // narrower type, truncate it down instead of promoting narrower up
    // (which would break x86 codegen for wide literal types).
    if (lhsTy->isIntegerTy() && rhsTy->isIntegerTy()) {
        unsigned lhsBits = lhsTy->getIntegerBitWidth();
        unsigned rhsBits = rhsTy->getIntegerBitWidth();
        auto canTruncate = [](llvm::Value* v, unsigned bits) {
            auto* ci = llvm::dyn_cast<llvm::ConstantInt>(v);
            if (!ci) return false;
            return ci->getValue().isIntN(bits);
        };
        if (lhsBits > rhsBits) {
            if (canTruncate(lhs, rhsBits))
                lhs = builder_->CreateIntCast(lhs, rhsTy, true, "trunc_lit");
            else
                rhs = builder_->CreateSExt(rhs, lhsTy, "prom");
        } else {
            if (canTruncate(rhs, lhsBits))
                rhs = builder_->CreateIntCast(rhs, lhsTy, true, "trunc_lit");
            else
                lhs = builder_->CreateSExt(lhs, rhsTy, "prom");
        }
        return {lhs, rhs};
    }

    // Both float: extend narrower to wider
    if (lhsTy->isFloatingPointTy() && rhsTy->isFloatingPointTy()) {
        if (lhsTy->getPrimitiveSizeInBits() < rhsTy->getPrimitiveSizeInBits())
            lhs = builder_->CreateFPExt(lhs, rhsTy, "prom");
        else
            rhs = builder_->CreateFPExt(rhs, lhsTy, "prom");
        return {lhs, rhs};
    }

    return {lhs, rhs};
}


