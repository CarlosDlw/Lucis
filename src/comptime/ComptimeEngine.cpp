#include "comptime/ComptimeEngine.h"

ComptimeEngine::ComptimeEngine() {}

void ComptimeEngine::setTypeRegistry(const TypeRegistry& types) {
    typeRegistry_ = &types;
}

void ComptimeEngine::setConstEvalFn(ComptimeInterpreter::ConstEvalFn fn) {
    constEvalFn_ = std::move(fn);
}

ComptimeValue
ComptimeEngine::evaluate(void* funcDecl,
                         const std::vector<ComptimeValue>& args) {
    if (!interpreter_ && typeRegistry_) {
        interpreter_ = std::make_unique<ComptimeInterpreter>(
            *typeRegistry_, registry_, &builtinRegistry_, constEvalFn_);
    }
    if (!interpreter_)
        return ComptimeValue::voidVal();

    return interpreter_->evaluate(funcDecl, args);
}
