#pragma once

#include <string>
#include <memory>
#include "comptime/ComptimeValue.h"

namespace llvm {
    class Module;
    class LLVMContext;
    class Function;
    class ExecutionEngine;
}

// JIT compiler for comptime functions.
// Compiles a single Lucis function to LLVM IR, executes it,
// and returns the result as a ComptimeValue.
class ComptimeJIT {
public:
    ComptimeJIT();
    ~ComptimeJIT();

    // Compile and execute a comptime function with the given arguments.
    // Returns the computed value.
    ComptimeValue evaluate(void* funcDecl,
                           const std::vector<ComptimeValue>& args);

    // Check if the JIT engine is ready.
    bool isReady() const { return ready_; }

private:
    // Generate LLVM IR from a comptime function declaration.
    std::unique_ptr<llvm::Module> compileToIR(void* funcDecl);

    // Execute the compiled function with the given LLVM values.
    ComptimeValue runJIT(llvm::Module* module,
                         const std::string& funcName,
                         const std::vector<ComptimeValue>& args);

    bool ready_ = false;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
