#pragma once

#include <clang-c/Index.h>

#include "types/TypeInfo.h"
#include "types/TypeRegistry.h"
#include "ffi/CBindings.h"

// Maps libclang CXType values to Lucis TypeInfo pointers.
// Uses TypeRegistry for primitive types and CBindings for ownership
// of dynamically created pointer/function types.
class CTypeMapper {
public:
    CTypeMapper(TypeRegistry& registry, CBindings& bindings);

    // Map a libclang type to a Lucis TypeInfo.
    // Returns nullptr for unsupported types.
    const TypeInfo* map(CXType cxType);

    // Map a struct cursor: extract fields and register as CStruct + TypeInfo.
    // Returns the TypeInfo* for the struct, or nullptr if it contains unmappable fields.
    const TypeInfo* mapStruct(CXCursor structCursor);

    // Build a TypeInfo for a fixed-size C array (e.g. uint32_t[6]).
    const TypeInfo* makeArrayOf(const TypeInfo* element, unsigned size);

private:
    TypeRegistry& registry_;
    CBindings&    bindings_;

    const TypeInfo* mapPrimitive(CXType cxType);
    const TypeInfo* mapPointer(CXType cxType);
    const TypeInfo* mapFunctionProto(CXType cxType);
    const TypeInfo* makePointerTo(const TypeInfo* pointee);
};
