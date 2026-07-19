#include "types/TypeRegistry.h"

TypeRegistry::TypeRegistry() {
    registerBuiltins();
}

const TypeInfo* TypeRegistry::lookup(const std::string& name) const {
    auto it = types_.find(name);
    return it != types_.end() ? &it->second : nullptr;
}

TypeInfo* TypeRegistry::lookupMutable(const std::string& name) {
    auto it = types_.find(name);
    return it != types_.end() ? &it->second : nullptr;
}

void TypeRegistry::registerType(TypeInfo info) {
    types_[info.name] = std::move(info);
}

std::vector<std::string> TypeRegistry::allTypes() const {
    std::vector<std::string> result;
    result.reserve(types_.size());
    for (auto& [name, _] : types_)
        result.push_back(name);
    return result;
}

bool TypeRegistry::isIntegerType(const std::string& name) const {
    auto* ti = lookup(name);
    return ti && ti->kind == TypeKind::Integer;
}

bool TypeRegistry::isUnsignedType(const std::string& name) const {
    auto* ti = lookup(name);
    return ti && ti->kind == TypeKind::Integer && !ti->isSigned;
}

void TypeRegistry::registerBuiltins() {
    // ── Signed integers ─────────────────────────────────────────────────
    registerType({ "int1",   TypeKind::Integer, 1,   false, "i1"   });
    registerType({ "int8",   TypeKind::Integer, 8,   true, "i8"   });
    registerType({ "int16",  TypeKind::Integer, 16,  true, "i16"  });
    registerType({ "int32",  TypeKind::Integer, 32,  true, "i32"  });
    registerType({ "int64",  TypeKind::Integer, 64,  true, "i64"  });
    registerType({ "int128", TypeKind::Integer, 128, true, "i128" });
    registerType({ "intinf", TypeKind::Integer, 256, true, "iinf" });
    registerType({ "isize",  TypeKind::Integer, 0,   true, "i64"  }); // platform-dependent

    // ── Unsigned integers ───────────────────────────────────────────────
    registerType({ "uint1",   TypeKind::Integer, 1,   false, "u1"   });
    registerType({ "uint8",   TypeKind::Integer, 8,   false, "u8"   });
    registerType({ "uint16",  TypeKind::Integer, 16,  false, "u16"  });
    registerType({ "uint32",  TypeKind::Integer, 32,  false, "u32"  });
    registerType({ "uint64",  TypeKind::Integer, 64,  false, "u64"  });
    registerType({ "uint128", TypeKind::Integer, 128, false, "u128" });
    registerType({ "usize",   TypeKind::Integer, 0,   false, "u64"  }); // platform-dependent

    // ── Other primitives ────────────────────────────────────────────────
    registerType({ "float32",  TypeKind::Float,  32,  true, "f32"  });
    registerType({ "float64",  TypeKind::Float,  64,  true, "f64"  });
    registerType({ "float80",  TypeKind::Float,  80,  true, "f80"  });
    registerType({ "float128", TypeKind::Float,  128, true, "f128" });
    registerType({ "double",   TypeKind::Float,  64,  true, "f64"  });
    registerType({ "bool",    TypeKind::Bool,   1,  true, "bool" });
    registerType({ "char",    TypeKind::Char,   8,  true, "char" });
    registerType({ "void",    TypeKind::Void,   0,  true, ""     });
    registerType({ "*void",   TypeKind::Pointer, 0,  true, "ptr"  });
    registerType({ "string",  TypeKind::String, 0,  true, "str"  });

    // ── Concurrency types ───────────────────────────────────────────────
    TypeInfo mutexTI;
    mutexTI.name = "Mutex";
    mutexTI.kind = TypeKind::Extended;
    mutexTI.bitWidth = 0;
    mutexTI.isSigned = false;
    mutexTI.builtinSuffix = "mutex";
    mutexTI.extendedKind = "Mutex";
    registerType(std::move(mutexTI));

    // ── Error type (built-in struct) ────────────────────────────────────
    // Fields are resolved after all primitives are registered.
    TypeInfo errorTI;
    errorTI.name = "Error";
    errorTI.kind = TypeKind::Struct;
    errorTI.bitWidth = 0;
    errorTI.isSigned = false;

    const TypeInfo* strTI  = lookup("string");
    const TypeInfo* i32TI  = lookup("int32");

    errorTI.fields.push_back({ "message", strTI, 0, {}, false });
    errorTI.fields.push_back({ "file",    strTI, 0, {}, true  });
    errorTI.fields.push_back({ "line",    i32TI, 0, {}, true  });
    errorTI.fields.push_back({ "column",  i32TI, 0, {}, true  });
    registerType(std::move(errorTI));
}
