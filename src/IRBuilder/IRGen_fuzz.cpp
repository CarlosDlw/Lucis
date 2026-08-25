// Fuzz mode IR synthesis for `lucis fuzz`.
//
// Two responsibilities:
//   1. injectFuzzCoverage() — inserts a `lucis_fuzz_trace(id)` call at the
//      top of every basic block of user (non-stdlib) functions. The C runner
//      records each id in a shared bitmap; the parent process uses new bits
//      as coverage feedback to steer mutation (AFL model).
//   2. synthesizeFuzzMain() — generates `lucis_fuzz_dispatch(data, len)`
//      (bridges the runner into the user's `fn([]u8) void` target by value)
//      and a `main(argc, argv)` that hands control to `lucis_fuzz_main`.

#include "IRBuilder/IRGen.h"

namespace {

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

void IRGen::injectFuzzCoverage() {
    // Cover only user code: stdlib units would pollute the coverage map and
    // steer the mutator away from the target's own logic.
    if (isStdlib_ || !fuzzMode_) return;

    auto* traceFn = module_->getFunction("lucis_fuzz_trace");
    if (!traceFn) {
        auto* i32Ty = llvm::Type::getInt32Ty(*context_);
        traceFn = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getVoidTy(*context_), {i32Ty}, false),
            llvm::Function::ExternalLinkage, "lucis_fuzz_trace", module_);
    }

    int64_t id = 0;
    for (auto& fn : *module_) {
        if (fn.isDeclaration()) continue;
        auto name = fn.getName().str();
        if (name.rfind("lucis_", 0) == 0) continue;  // generated helpers
        for (auto& bb : fn) {
            if (auto* first = bb.getFirstNonPHI()) {
                llvm::CallInst::Create(
                    traceFn, {builder_->getInt32(static_cast<int32_t>(id & 0xFFFF))},
                    "", first);
            }
            ++id;
        }
    }
}

void IRGen::synthesizeFuzzMain() {
    if (!fuzzTarget_) return;

    auto* ctx    = context_;
    auto* mod    = module_;
    auto* i32Ty  = llvm::Type::getInt32Ty(*ctx);
    auto* voidTy = llvm::Type::getVoidTy(*ctx);
    auto* ptrTy  = llvm::PointerType::getUnqual(*ctx);
    auto* usizeTy = mod->getDataLayout().getIntPtrType(*ctx);
    auto* sliceTy = llvm::StructType::get(*ctx, {ptrTy, usizeTy});

    // ── lucis_fuzz_dispatch(const uint8* data, size_t len) → void ───────
    auto* dispatch = mod->getFunction("lucis_fuzz_dispatch");
    if (!dispatch) {
        dispatch = llvm::Function::Create(
            llvm::FunctionType::get(voidTy, {ptrTy, usizeTy}, false),
            llvm::Function::ExternalLinkage, "lucis_fuzz_dispatch", mod);
        auto* entry = llvm::BasicBlock::Create(*ctx, "entry", dispatch);
        builder_->SetInsertPoint(entry);

        auto* data = dispatch->getArg(0);
        auto* len  = dispatch->getArg(1);

        // Build the []u8 slice {ptr, len} and call the user target.
        auto* sliceAlloca = builder_->CreateAlloca(sliceTy, nullptr, "fuzz_slice");
        builder_->CreateStore(data, builder_->CreateStructGEP(sliceTy, sliceAlloca, 0));
        builder_->CreateStore(len,  builder_->CreateStructGEP(sliceTy, sliceAlloca, 1));
        auto* sliceVal = builder_->CreateLoad(sliceTy, sliceAlloca, "fuzz_slice");

        auto* targetFn = mod->getFunction(fuzzTarget_->symbolName);
        if (!targetFn) {
            // Defined in another unit — declare extern; resolved at link.
            targetFn = declareExtern(mod, ctx, fuzzTarget_->symbolName.c_str(),
                                     voidTy, {sliceTy});
        }
        if (targetFn)
            builder_->CreateCall(targetFn, {sliceVal});
        builder_->CreateRetVoid();
    }

    // ── main(i32 argc, ptr argv) → i32 — delegates to the C runner ──────
    auto* runnerFn = declareExtern(mod, ctx, "lucis_fuzz_main", i32Ty, {i32Ty, ptrTy});
    auto* mainFn = mod->getFunction("main");
    if (!mainFn) {
        mainFn = llvm::Function::Create(
            llvm::FunctionType::get(i32Ty, {i32Ty, ptrTy}, false),
            llvm::Function::ExternalLinkage, "main", mod);
        auto* entry = llvm::BasicBlock::Create(*ctx, "entry", mainFn);
        builder_->SetInsertPoint(entry);
        auto* argc = mainFn->getArg(0);
        auto* argv = mainFn->getArg(1);
        auto* ret = builder_->CreateCall(runnerFn, {argc, argv});
        builder_->CreateRet(ret);
    }
}