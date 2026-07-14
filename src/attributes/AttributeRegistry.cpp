#include "attributes/AttributeRegistry.h"

void AttributeRegistry::registerAttribute(const std::string& name, AttributeHandler handler) {
    handlers_[name] = std::move(handler);
}

const AttributeHandler* AttributeRegistry::lookup(const std::string& name) const {
    auto it = handlers_.find(name);
    return it != handlers_.end() ? &it->second : nullptr;
}

bool AttributeRegistry::isKnown(const std::string& name) const {
    return handlers_.count(name) > 0;
}

std::vector<std::string> AttributeRegistry::allNames() const {
    std::vector<std::string> names;
    names.reserve(handlers_.size());
    for (auto& [name, _] : handlers_)
        names.push_back(name);
    return names;
}

void registerBuiltinAttributes(AttributeRegistry& reg) {
    // Each built-in attribute will be wired here as we implement them.
    // For now, register the ones that existed before:
    // - "error" on enum variants (migrated from #[error])
    reg.registerAttribute("error", AttributeHandler{
        .validate = [](const Attribute& attr, const TypeInfo*, std::vector<std::string>&) {
            // #[error] takes no arguments
            if (!attr.args.empty())
                return false;
            return true;
        },
        .emit = [](const Attribute&, void*, llvm::Function*, llvm::GlobalVariable*) {
            return true;
        }
    });
}
