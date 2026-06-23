#include "comptime/ComptimeEngine.h"

ComptimeEngine::ComptimeEngine()
    : jit_(std::make_unique<ComptimeJIT>()) {}

ComptimeValue
ComptimeEngine::evaluate(void* funcDecl,
                          const std::vector<ComptimeValue>& args) {
    if (!jit_ || !jit_->isReady())
        return ComptimeValue::voidVal();

    return jit_->evaluate(funcDecl, args);
}
