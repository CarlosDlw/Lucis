#include "intrinsics/IntrinsicRegistry.h"

#include <algorithm>

IntrinsicRegistry::IntrinsicRegistry() {
    registerCoreNamespace(*this);
    registerDebugNamespace(*this);
    registerUnsafeNamespace(*this);
}

void IntrinsicRegistry::registerNamespace(IntrinsicNamespace ns) {
    auto& stored = namespaces_[ns.name] = std::move(ns);
    for (auto& func : stored.functions) {
        flatLookup_[stored.name + "::" + func.name] = &func;
    }
}

const IntrinsicFunction* IntrinsicRegistry::lookup(
        const std::string& ns,
        const std::string& funcName) const {
    auto it = flatLookup_.find(ns + "::" + funcName);
    if (it != flatLookup_.end())
        return it->second;
    return nullptr;
}

bool IntrinsicRegistry::hasNamespace(const std::string& ns) const {
    return namespaces_.count(ns) > 0;
}

std::string IntrinsicRegistry::namespaceDescription(const std::string& ns) const {
    auto it = namespaces_.find(ns);
    if (it != namespaces_.end())
        return it->second.description;
    return "";
}

std::vector<const IntrinsicFunction*>
IntrinsicRegistry::functionsInNamespace(const std::string& ns) const {
    std::vector<const IntrinsicFunction*> result;
    auto it = namespaces_.find(ns);
    if (it != namespaces_.end()) {
        result.reserve(it->second.functions.size());
        for (auto& func : it->second.functions)
            result.push_back(&func);
    }
    return result;
}

std::vector<std::string> IntrinsicRegistry::allNamespaces() const {
    std::vector<std::string> result;
    result.reserve(namespaces_.size());
    for (auto& [ns, _] : namespaces_)
        result.push_back(ns);
    std::sort(result.begin(), result.end());
    return result;
}

bool IntrinsicRegistry::isIntrinsicPrefix(const std::string& firstSegment) {
    return firstSegment == "lux";
}

bool IntrinsicRegistry::parseIntrinsicPath(
        const std::vector<std::string>& ids,
        std::string& outNs,
        std::string& outFunc) {
    if (ids.size() != 3)
        return false;
    if (ids[0] != "lux")
        return false;
    outNs = ids[1];
    outFunc = ids[2];
    return true;
}
