#pragma once

#include "comptime/ComptimeValue.h"
#include "comptime/ComptimeBuiltinRegistry.h"
#include "comptime/ComptimeInterpreter.h"
#include "comptime/ComptimeRegistry.h"
#include "types/TypeRegistry.h"
#include <memory>

class ComptimeEngine {
public:
    ComptimeEngine();

    void setTypeRegistry(const TypeRegistry& types);

    void setConstEvalFn(ComptimeInterpreter::ConstEvalFn fn);

    ComptimeValue evaluate(void* funcDecl,
                           const std::vector<ComptimeValue>& args);

    bool isReady() const { return true; }

    void setTargetTriple(const std::string&) {}

    ComptimeRegistry& registry() { return registry_; }
    const ComptimeRegistry& registry() const { return registry_; }
    ComptimeBuiltinRegistry& builtinRegistry() { return builtinRegistry_; }
    const ComptimeBuiltinRegistry& builtinRegistry() const { return builtinRegistry_; }

private:
    std::unique_ptr<ComptimeInterpreter> interpreter_;
    const TypeRegistry* typeRegistry_ = nullptr;
    ComptimeInterpreter::ConstEvalFn constEvalFn_;
    ComptimeRegistry registry_;
    ComptimeBuiltinRegistry builtinRegistry_;
};
