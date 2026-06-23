#include "comptime/ComptimeRegistry.h"

void ComptimeRegistry::registerFunction(const std::string& name,
                                         void* decl,
                                         bool isGeneric) {
    entries_[name] = {name, decl, isGeneric};
}

bool ComptimeRegistry::isComptime(const std::string& name) const {
    return entries_.count(name) > 0;
}

void* ComptimeRegistry::lookup(const std::string& name) const {
    auto it = entries_.find(name);
    return (it != entries_.end()) ? it->second.decl : nullptr;
}

std::vector<std::string> ComptimeRegistry::allNames() const {
    std::vector<std::string> names;
    for (auto& [name, _] : entries_)
        names.push_back(name);
    return names;
}

void ComptimeRegistry::clear() {
    entries_.clear();
}
