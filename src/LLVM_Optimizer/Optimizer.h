#pragma once

#include "LLVM_IR/IRModule.h"

enum class OptimizationLevel {
    O0 = 0,
    O1 = 1,
    O2 = 2,
    O3 = 3,
    Os,
    Oz,
    Ofast
};

class Optimizer {
public:
    static void optimize(IRModule& irModule, OptimizationLevel level);
};
