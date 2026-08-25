// Test mode IR synthesis: builds the test harness `main` for `lucis test`.
//
// The synthesized main drives the C harness runtime (src/builtins/test/
// test_runner.c) through lucis_test_begin / lucis_test_run / lucis_test_end.
// Every test case gets a void(void) wrapper that calls the test function and
// _exit(0): the C runtime forks per test, so a failing assertion, a crash or
// a timeout in one test can never take down the runner.

#include "IRBuilder/IRGen.h"

#include <iostream>

namespace {

// Declare (or reuse) an external function in the current module.
llvm::Function* declareExtern(llvm::Module* mod, llvm::LLVMContext* ctx,
                              const char* name, llvm::Type* retTy,
                              std::vector<llvm::Type*> argTys) {
    if (auto* existing = mod->getFunction(name))
        return existing;
    return llvm::Function::Create(
        llvm::FunctionType::get(retTy, argTys, false),
        llvm::Function::ExternalLinkage, name, mod);
}

}  // namespace

void IRGen::synthesizeTestMain() {
    auto* ctx    = context_;
    auto* mod    = module_;
    auto* i32Ty  = llvm::Type::getInt32Ty(*ctx);
    auto* i64Ty  = llvm::Type::getInt64Ty(*ctx);
    auto* voidTy = llvm::Type::getVoidTy(*ctx);
    auto* ptrTy  = llvm::PointerType::getUnqual(*ctx);

    auto* beginFn = declareExtern(mod, ctx, "lucis_test_begin",
                                  i32Ty, {i64Ty, i32Ty, i32Ty});
    auto* runFn   = declareExtern(mod, ctx, "lucis_test_run",
                                  i32Ty, {i32Ty, ptrTy, i32Ty, ptrTy,
                                          i32Ty, i64Ty, ptrTy});
    auto* endFn   = declareExtern(mod, ctx, "lucis_test_end", i32Ty, {});
    auto* exitFn  = declareExtern(mod, ctx, "_exit", voidTy, {i32Ty});

    // ── Per-test wrapper: void(void) — calls the test and _exit(0). ────
    std::vector<llvm::Function*> wrappers;
    wrappers.reserve(testCases_.size());
    for (size_t i = 0; i < testCases_.size(); ++i) {
        const auto& tc = testCases_[i];

        auto* wrapper = llvm::Function::Create(
            llvm::FunctionType::get(voidTy, false),
            llvm::Function::InternalLinkage,
            "lucis_test_case_" + std::to_string(i), mod);
        auto* entry = llvm::BasicBlock::Create(*ctx, "entry", wrapper);
        builder_->SetInsertPoint(entry);

        auto* testFn = mod->getFunction(tc.symbolName);
        if (!testFn) {
            // Defined in another unit — declare extern; resolved at link.
            auto* retTy = tc.returnsVoid ? voidTy : i32Ty;
            testFn = declareExtern(mod, ctx, tc.symbolName.c_str(), retTy, {});
        }
        if (testFn)
            builder_->CreateCall(testFn, {});
        builder_->CreateCall(exitFn, {builder_->getInt32(0)});
        builder_->CreateRetVoid();  // unreachable; keeps the IR well-formed
        wrappers.push_back(wrapper);
    }

    // ── main() → i32 ────────────────────────────────────────────────────
    auto* mainFn = llvm::Function::Create(
        llvm::FunctionType::get(i32Ty, false),
        llvm::Function::ExternalLinkage, "main", mod);
    auto* entry = llvm::BasicBlock::Create(*ctx, "entry", mainFn);
    builder_->SetInsertPoint(entry);

    builder_->CreateCall(beginFn, {
        builder_->getInt64(static_cast<int64_t>(testCases_.size())),
        builder_->getInt32(testJobs_),
        builder_->getInt32(testVerbose_ ? 1 : 0)});

    for (size_t i = 0; i < testCases_.size(); ++i) {
        const auto& tc = testCases_[i];

        const std::string& display = tc.spec.displayName.empty()
            ? tc.funcName : tc.spec.displayName;
        auto* name = builder_->CreateGlobalStringPtr(display);

        std::string reason;
        if (tc.spec.skip)
            reason = tc.spec.skipReason;
        auto* reasonPtr = builder_->CreateGlobalStringPtr(reason);

        int64_t timeoutMs = tc.spec.timeoutMs > 0 ? tc.spec.timeoutMs
                                                  : testTimeoutMs_;
        builder_->CreateCall(runFn, {
            builder_->getInt32(static_cast<int>(i)),
            name,
            builder_->getInt32(tc.spec.skip ? 1 : 0),
            reasonPtr,
            builder_->getInt32(tc.spec.expectedFail ? 1 : 0),
            builder_->getInt64(timeoutMs),
            llvm::ConstantExpr::getBitCast(wrappers[i], ptrTy)});
    }

    auto* ret = builder_->CreateCall(endFn, {});
    builder_->CreateRet(ret);
}