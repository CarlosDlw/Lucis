#include "comptime/ComptimeBuiltinRegistry.h"
#include "types/TypeInfo.h"
#include <cstdint>
#include <cstring>


ComptimeBuiltinRegistry::ComptimeBuiltinRegistry() {
    // @typeName(T) → string: returns the name of a type
    add("typeName", {ComptimeValue::Kind::Type},
        [](const std::vector<ComptimeValue>& args) -> ComptimeValue {
            auto* ti = static_cast<const TypeInfo*>(args[0].asType());
            return ComptimeValue::stringVal(ti ? ti->name : "unknown");
        });

    // @fields(T) → string: returns field names of a struct/union
    add("fields", {ComptimeValue::Kind::Type},
        [](const std::vector<ComptimeValue>& args) -> ComptimeValue {
            auto* ti = static_cast<const TypeInfo*>(args[0].asType());
            std::string result;
            if (ti && (ti->kind == TypeKind::Struct || ti->kind == TypeKind::Union)) {
                for (size_t i = 0; i < ti->fields.size(); i++) {
                    if (i > 0) result += ", ";
                    result += ti->fields[i].name;
                }
            }
            return ComptimeValue::stringVal(result);
        });

    // @variants(T) → string: returns variant names of an enum
    add("variants", {ComptimeValue::Kind::Type},
        [](const std::vector<ComptimeValue>& args) -> ComptimeValue {
            auto* ti = static_cast<const TypeInfo*>(args[0].asType());
            std::string result;
            if (ti && ti->kind == TypeKind::Enum) {
                for (size_t i = 0; i < ti->enumVariants.size(); i++) {
                    if (i > 0) result += ", ";
                    result += ti->enumVariants[i];
                }
            }
            return ComptimeValue::stringVal(result);
        });

    // @typeId(T) → int32: returns FNV-1a hash of type name (for switch dispatch)
    add("typeId", {ComptimeValue::Kind::Type},
        [](const std::vector<ComptimeValue>& args) -> ComptimeValue {
            auto* ti = static_cast<const TypeInfo*>(args[0].asType());
            std::string name = ti ? ti->name : "";
            uint64_t hash = 0xcbf29ce484222325ULL;
            for (char c : name) {
                hash ^= static_cast<uint8_t>(c);
                hash *= 0x100000001b3ULL;
            }
            return ComptimeValue::intVal(static_cast<int64_t>(hash));
        });
}

const ComptimeBuiltin* ComptimeBuiltinRegistry::lookup(
    const std::string& name) const {
    auto it = builtins_.find(name);
    return it != builtins_.end() ? &it->second : nullptr;
}

void ComptimeBuiltinRegistry::add(
    std::string name,
    std::vector<ComptimeValue::Kind> paramKinds,
    std::function<ComptimeValue(const std::vector<ComptimeValue>&)> fn) {
    ComptimeBuiltin b;
    b.name = name;
    b.paramKinds = std::move(paramKinds);
    b.fn = std::move(fn);
    builtins_[name] = std::move(b);
}
