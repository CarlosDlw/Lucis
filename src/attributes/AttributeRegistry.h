#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

#include "attributes/Attribute.h"

// Forward declarations
namespace llvm {
    class GlobalVariable;
    class Function;
    class Value;
}
struct TypeInfo;
class LucisParser;

struct AttributeHandler {
    // Validation callback: called by Checker to validate attr usage.
    // Return false to emit an error (error message is appended to outErrors).
    std::function<bool(
        const Attribute& attr,
        const TypeInfo* targetType,
        std::vector<std::string>& outErrors
    )> validate;

    // IRGen callback: called by IRGen to apply attribute effects.
    // Return true if the attribute was handled.
    std::function<bool(
        const Attribute& attr,
        void* decl,           // LucisParser::*DeclContext*
        llvm::Function* fn,   // non-null for function attributes
        llvm::GlobalVariable* gv // non-null for global variable attributes
    )> emit;
};

class AttributeRegistry {
public:
    void registerAttribute(const std::string& name, AttributeHandler handler);

    const AttributeHandler* lookup(const std::string& name) const;

    bool isKnown(const std::string& name) const;

    std::vector<std::string> allNames() const;

private:
    std::unordered_map<std::string, AttributeHandler> handlers_;
};

// Registration function for built-in attributes
void registerBuiltinAttributes(AttributeRegistry& reg);
