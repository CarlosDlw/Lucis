#include "LLVM_Optimizer/Optimizer.h"

#include <llvm/Passes/PassBuilder.h>
#include <llvm/Analysis/LoopAnalysisManager.h>
#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/IR/PassManager.h>

void Optimizer::optimize(IRModule& irModule, OptimizationLevel level) {
    if (level == OptimizationLevel::O0) return;

    llvm::PassBuilder            passBuilder;
    llvm::LoopAnalysisManager    lam;
    llvm::FunctionAnalysisManager fam;
    llvm::CGSCCAnalysisManager   cgam;
    llvm::ModuleAnalysisManager  mam;

    passBuilder.registerModuleAnalyses(mam);
    passBuilder.registerCGSCCAnalyses(cgam);
    passBuilder.registerFunctionAnalyses(fam);
    passBuilder.registerLoopAnalyses(lam);
    passBuilder.crossRegisterProxies(lam, fam, cgam, mam);

    llvm::OptimizationLevel llvmLevel;
    switch (level) {
        case OptimizationLevel::O1:    llvmLevel = llvm::OptimizationLevel::O1; break;
        case OptimizationLevel::O2:    llvmLevel = llvm::OptimizationLevel::O2; break;
        case OptimizationLevel::O3:    llvmLevel = llvm::OptimizationLevel::O3; break;
        case OptimizationLevel::Ofast: llvmLevel = llvm::OptimizationLevel::O3; break; // Ofast is O3 + fast-math in many contexts
        case OptimizationLevel::Os:    llvmLevel = llvm::OptimizationLevel::Os; break;
        case OptimizationLevel::Oz:    llvmLevel = llvm::OptimizationLevel::Oz; break;
        default: return;
    }

    auto mpm = passBuilder.buildPerModuleDefaultPipeline(llvmLevel);
    mpm.run(*irModule.module(), mam);
}
