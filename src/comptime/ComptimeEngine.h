#pragma once

#include "comptime/ComptimeValue.h"
#include "comptime/ComptimeJIT.h"
#include "comptime/ComptimeRegistry.h"
#include <memory>

// Orchestrates compile-time evaluation of comptime functions.
// Called by the build pipeline between checker and IRGen.
class ComptimeEngine {
public:
    ComptimeEngine();

    // Evaluate a comptime function call.
    // funcDecl: the FunctionDeclContext* from the parsed AST
    // args: already-evaluated comptime values (from compile-time constant folding)
    ComptimeValue evaluate(void* funcDecl,
                           const std::vector<ComptimeValue>& args);

    // Whether the JIT engine is ready.
    bool isReady() const { return jit_ && jit_->isReady(); }

    // Access the comptime function registry.
    ComptimeRegistry& registry() { return registry_; }
    const ComptimeRegistry& registry() const { return registry_; }

private:
    std::unique_ptr<ComptimeJIT> jit_;
    ComptimeRegistry registry_;
};
