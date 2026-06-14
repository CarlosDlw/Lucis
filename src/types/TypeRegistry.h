#pragma once

#include "types/TypeInfo.h"

#include <string>
#include <map>

class TypeRegistry {
public:
    TypeRegistry();

    // Lookup by language name ("int32", "uint8", "float", etc.)
    const TypeInfo* lookup(const std::string& name) const;

    // Register a new type (for future struct/alias support)
    void registerType(TypeInfo info);

    // Enumerate all registered type names
    std::vector<std::string> allTypes() const;

    // Check if a name corresponds to any integer type (signed or unsigned)
    bool isIntegerType(const std::string& name) const;

    // Check if a name corresponds to any unsigned integer type
    bool isUnsignedType(const std::string& name) const;

private:
    std::map<std::string, TypeInfo> types_;

    void registerBuiltins();
};
