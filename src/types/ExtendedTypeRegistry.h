#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

#include "types/TypeInfo.h"
#include "types/MethodRegistry.h"

// Describes the LLVM struct layout for an extended type.
// Each field is: { fieldName, typeDescriptor }
// typeDescriptor: "ptr" = opaque pointer, "usize" = pointer-sized int
struct ExtendedFieldLayout {
    std::string name;
    std::string type; // "ptr", "usize", "i32", etc.
};

// Describes an extended type (e.g. Vec<T>, Map<K,V>).
// Contains everything the compiler needs to emit code.
struct ExtendedTypeDescriptor {
    std::string                       baseName;       // "Vec", "Map", etc.
    unsigned                          genericArity;   // 1 for Vec<T>, 2 for Map<K,V>
    std::vector<ExtendedFieldLayout>  layout;         // LLVM struct fields
    std::string                       cPrefix;        // C function prefix: "lucis_vec"
    std::vector<MethodDescriptor>     methods;        // methods for this extended type
};

// Central registry of all extended types available in the language.
// vec<T>, map<K,V>, set<T> are native keywords — no import required.
// The compiler queries this to get the type descriptor.
class ExtendedTypeRegistry {
public:
    ExtendedTypeRegistry();

    // Lookup an extended type by its base name (e.g. "Vec")
    const ExtendedTypeDescriptor* lookup(const std::string& baseName) const;

    // Get all registered extended type names
    std::vector<std::string> allTypes() const;

private:
    std::unordered_map<std::string, ExtendedTypeDescriptor> types_;

    void registerType(ExtendedTypeDescriptor desc);
    void registerBuiltins();
};
