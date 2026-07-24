#pragma once

#include "comptime/ComptimeValue.h"
#include "comptime/ComptimeBuiltinRegistry.h"
#include "comptime/ComptimeRegistry.h"
#include "types/TypeRegistry.h"
#include <optional>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

class ComptimeInterpreter {
public:
    using ConstEvalFn = std::function<std::optional<ComptimeValue>(void*)>;

    ComptimeInterpreter(const TypeRegistry& types, ComptimeRegistry& registry,
                        const ComptimeBuiltinRegistry* builtins,
                        ConstEvalFn constEval);

    void setConstEval(ConstEvalFn fn) { constEval_ = fn; }

    ComptimeValue evaluate(void* funcDecl,
                           const std::vector<ComptimeValue>& args);

private:
    struct Frame {
        void* func = nullptr;
        std::unordered_map<std::string, ComptimeValue> locals;
    };

    const TypeRegistry& types_;
    ComptimeRegistry& registry_;
    const ComptimeBuiltinRegistry* builtins_ = nullptr;
    ConstEvalFn constEval_;
    std::vector<Frame> frames_;

    ComptimeValue evalBlock(void* blockCtx);
    ComptimeValue evalStmt(void* stmtCtx);
    ComptimeValue evalExpr(void* exprCtx);

    ComptimeValue evalReturn(void* retCtx);
    ComptimeValue evalIf(void* ifCtx);
    ComptimeValue evalVarDecl(void* declCtx);
    ComptimeValue evalAssign(void* assignCtx);
    ComptimeValue evalCompoundAssign(void* compoundCtx);
    ComptimeValue evalForIn(void* forCtx);
    ComptimeValue evalWhile(void* whileCtx);
    ComptimeValue evalFnCall(void* callCtx);
    ComptimeValue evalMatch(void* matchCtx);
};
