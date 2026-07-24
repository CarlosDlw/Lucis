#pragma once

#include "comptime/ComptimeValue.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

struct ComptimeBuiltin {
    std::string name;
    std::vector<ComptimeValue::Kind> paramKinds;
    std::function<ComptimeValue(const std::vector<ComptimeValue>&)> fn;
};

class ComptimeBuiltinRegistry {
public:
    ComptimeBuiltinRegistry();

    const ComptimeBuiltin* lookup(const std::string& name) const;

private:
    void add(std::string name,
             std::vector<ComptimeValue::Kind> paramKinds,
             std::function<ComptimeValue(const std::vector<ComptimeValue>&)> fn);

    std::unordered_map<std::string, ComptimeBuiltin> builtins_;
};
