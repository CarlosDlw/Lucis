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

    // @fields(T) → string: field names (as list internally, string at runtime)
    add("fields", {ComptimeValue::Kind::Type},
        [](const std::vector<ComptimeValue>& args) -> ComptimeValue {
            auto* ti = static_cast<const TypeInfo*>(args[0].asType());
            std::string s;
            if (ti && (ti->kind == TypeKind::Struct || ti->kind == TypeKind::Union))
                for (size_t i = 0; i < ti->fields.size(); i++) {
                    if (i > 0) s += ", ";
                    s += ti->fields[i].name;
                }
            return ComptimeValue::stringVal(s);
        });

    // @variants(T) → string: variant/enum names
    add("variants", {ComptimeValue::Kind::Type},
        [](const std::vector<ComptimeValue>& args) -> ComptimeValue {
            auto* ti = static_cast<const TypeInfo*>(args[0].asType());
            std::string s;
            if (ti && ti->kind == TypeKind::Enum)
                for (size_t i = 0; i < ti->enumVariants.size(); i++) {
                    if (i > 0) s += ", ";
                    s += ti->enumVariants[i];
                }
            return ComptimeValue::stringVal(s);
        });

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

    // @isStruct(T) → bool
    add("isStruct", {ComptimeValue::Kind::Type},
        [](const std::vector<ComptimeValue>& args) -> ComptimeValue {
            auto* ti = static_cast<const TypeInfo*>(args[0].asType());
            return ComptimeValue::boolVal(ti && ti->kind == TypeKind::Struct);
        });

    // @isEnum(T) → bool
    add("isEnum", {ComptimeValue::Kind::Type},
        [](const std::vector<ComptimeValue>& args) -> ComptimeValue {
            auto* ti = static_cast<const TypeInfo*>(args[0].asType());
            return ComptimeValue::boolVal(ti && ti->kind == TypeKind::Enum);
        });

    // @len(list) → int32: returns list length (needs new grammar, disabled for now)
    // @list(...) → List: creates a comptime list (needs new grammar, disabled for now)

    // @isUnion(T) → bool
    add("isUnion", {ComptimeValue::Kind::Type},
        [](const std::vector<ComptimeValue>& args) -> ComptimeValue {
            auto* ti = static_cast<const TypeInfo*>(args[0].asType());
            return ComptimeValue::boolVal(ti && ti->kind == TypeKind::Union);
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
