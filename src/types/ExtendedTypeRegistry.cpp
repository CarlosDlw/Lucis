#include "types/ExtendedTypeRegistry.h"

ExtendedTypeRegistry::ExtendedTypeRegistry() {
    registerBuiltins();
}

const ExtendedTypeDescriptor* ExtendedTypeRegistry::lookup(
    const std::string& baseName) const {
    auto it = types_.find(baseName);
    return it != types_.end() ? &it->second : nullptr;
}

std::vector<std::string> ExtendedTypeRegistry::allTypes() const {
    std::vector<std::string> result;
    for (auto& [name, _] : types_)
        result.push_back(name);
    return result;
}

void ExtendedTypeRegistry::registerType(ExtendedTypeDescriptor desc) {
    types_[desc.baseName] = std::move(desc);
}

void ExtendedTypeRegistry::registerBuiltins() {

    // ═══════════════════════════════════════════════════════════════════════
    // Vec<T> — Dynamic growable array
    //
    // LLVM layout: { T* ptr, usize len, usize cap }
    // C prefix: lucis_vec
    // Native keyword: vec<T>
    // ═══════════════════════════════════════════════════════════════════════

    ExtendedTypeDescriptor vec;
    vec.baseName     = "Vec";
    vec.genericArity = 1;
    vec.layout       = {
        { "ptr", "ptr"   },
        { "len", "usize" },
        { "cap", "usize" },
    };
    vec.cPrefix = "lucis_vec";

    // ── Capacity / size ──────────────────────────────────────────────────
    vec.methods.push_back({ "len",      {TypeKind::Extended}, {}, "usize",
                            .emitTag = "vec.len" });
    vec.methods.push_back({ "capacity", {TypeKind::Extended}, {}, "usize",
                            .emitTag = "vec.capacity" });
    vec.methods.push_back({ "isEmpty",  {TypeKind::Extended}, {}, "bool",
                            .emitTag = "vec.isEmpty" });

    // ── Element access ───────────────────────────────────────────────────
    vec.methods.push_back({ "at",    {TypeKind::Extended}, {"usize"}, "_elem",
                            .emitTag = "vec.at" });
    vec.methods.push_back({ "first", {TypeKind::Extended}, {}, "_elem",
                            .emitTag = "vec.first" });
    vec.methods.push_back({ "last",  {TypeKind::Extended}, {}, "_elem",
                            .emitTag = "vec.last" });

    // ── Mutation ─────────────────────────────────────────────────────────
    vec.methods.push_back({ "push",     {TypeKind::Extended}, {"_elem"}, "void",
                            .emitTag = "vec.push" });
    vec.methods.push_back({ "pop",      {TypeKind::Extended}, {}, "_elem",
                            .emitTag = "vec.pop" });
    vec.methods.push_back({ "insert",   {TypeKind::Extended}, {"usize", "_elem"}, "void",
                            .emitTag = "vec.insert" });
    vec.methods.push_back({ "removeAt", {TypeKind::Extended}, {"usize"}, "_elem",
                            .emitTag = "vec.removeAt" });
    vec.methods.push_back({ "removeSwap", {TypeKind::Extended}, {"usize"}, "_elem",
                            .emitTag = "vec.removeSwap" });
    vec.methods.push_back({ "clear",    {TypeKind::Extended}, {}, "void",
                            .emitTag = "vec.clear" });
    vec.methods.push_back({ "fill",     {TypeKind::Extended}, {"_elem"}, "void",
                            .emitTag = "vec.fill" });
    vec.methods.push_back({ "swap",     {TypeKind::Extended}, {"usize", "usize"}, "void",
                            .emitTag = "vec.swap" });

    // ── Memory management ────────────────────────────────────────────────
    vec.methods.push_back({ "reserve",  {TypeKind::Extended}, {"usize"}, "void",
                            .emitTag = "vec.reserve" });
    vec.methods.push_back({ "shrink",   {TypeKind::Extended}, {}, "void",
                            .emitTag = "vec.shrink" });
    vec.methods.push_back({ "resize",   {TypeKind::Extended}, {"usize", "_elem"}, "void",
                            .emitTag = "vec.resize" });
    vec.methods.push_back({ "truncate", {TypeKind::Extended}, {"usize"}, "void",
                            .emitTag = "vec.truncate" });

    // ── Search ───────────────────────────────────────────────────────────
    vec.methods.push_back({ "contains",   {TypeKind::Extended}, {"_elem"}, "bool",
                            .emitTag = "vec.contains" });
    vec.methods.push_back({ "indexOf",    {TypeKind::Extended}, {"_elem"}, "int64",
                            .emitTag = "vec.indexOf" });
    vec.methods.push_back({ "lastIndexOf",{TypeKind::Extended}, {"_elem"}, "int64",
                            .emitTag = "vec.lastIndexOf" });
    vec.methods.push_back({ "count",      {TypeKind::Extended}, {"_elem"}, "usize",
                            .emitTag = "vec.count" });

    // ── Reordering ───────────────────────────────────────────────────────
    vec.methods.push_back({ "reverse", {TypeKind::Extended}, {}, "void",
                            .emitTag = "vec.reverse" });
    vec.methods.push_back({ "sort",    {TypeKind::Extended}, {}, "void",
                            .requireNumeric = true, .emitTag = "vec.sort" });
    vec.methods.push_back({ "sortDesc",{TypeKind::Extended}, {}, "void",
                            .requireNumeric = true, .emitTag = "vec.sortDesc" });
    vec.methods.push_back({ "rotate",  {TypeKind::Extended}, {"int32"}, "void",
                            .emitTag = "vec.rotate" });

    // ── Aggregation (numeric only) ───────────────────────────────────────
    vec.methods.push_back({ "sum",     {TypeKind::Extended}, {}, "_elem",
                            .requireNumeric = true, .emitTag = "vec.sum" });
    vec.methods.push_back({ "product", {TypeKind::Extended}, {}, "_elem",
                            .requireNumeric = true, .emitTag = "vec.product" });
    vec.methods.push_back({ "min",     {TypeKind::Extended}, {}, "_elem",
                            .requireNumeric = true, .emitTag = "vec.min" });
    vec.methods.push_back({ "max",     {TypeKind::Extended}, {}, "_elem",
                            .requireNumeric = true, .emitTag = "vec.max" });
    vec.methods.push_back({ "average", {TypeKind::Extended}, {}, "float64",
                            .requireNumeric = true, .emitTag = "vec.average" });

    // ── Comparison / query ───────────────────────────────────────────────
    vec.methods.push_back({ "equals",   {TypeKind::Extended}, {"_self"}, "bool",
                            .emitTag = "vec.equals" });
    vec.methods.push_back({ "isSorted", {TypeKind::Extended}, {}, "bool",
                            .requireNumeric = true, .emitTag = "vec.isSorted" });

    // ── Conversion ───────────────────────────────────────────────────────
    vec.methods.push_back({ "toString", {TypeKind::Extended}, {}, "string",
                            .emitTag = "vec.toString" });
    vec.methods.push_back({ "join",     {TypeKind::Extended}, {"string"}, "string",
                            .emitTag = "vec.join" });
    vec.methods.push_back({ "clone",    {TypeKind::Extended}, {}, "_self",
                            .emitTag = "vec.clone" });

    // ── Cleanup ──────────────────────────────────────────────────────────
    vec.methods.push_back({ "free", {TypeKind::Extended}, {}, "void",
                            .emitTag = "vec.free" });

    registerType(std::move(vec));

    // ═══════════════════════════════════════════════════════════════════════
    // Map<K,V> — Open-addressing hash map
    //
    // LLVM layout: { ptr states, ptr keys, ptr values, ptr hashes,
    //                usize len, usize cap, usize key_size, usize val_size }
    // C prefix: lucis_map
    // Native keyword: map<K,V>
    // ═══════════════════════════════════════════════════════════════════════

    ExtendedTypeDescriptor map;
    map.baseName     = "Map";
    map.genericArity = 2;
    map.layout       = {
        { "states",   "ptr"   },
        { "keys",     "ptr"   },
        { "values",   "ptr"   },
        { "hashes",   "ptr"   },
        { "len",      "usize" },
        { "cap",      "usize" },
        { "key_size", "usize" },
        { "val_size", "usize" },
    };
    map.cPrefix = "lucis_map";

    // ── Size / query ─────────────────────────────────────────────────────
    map.methods.push_back({ "len",     {TypeKind::Extended}, {}, "usize",
                            .emitTag = "map.len" });
    map.methods.push_back({ "isEmpty", {TypeKind::Extended}, {}, "bool",
                            .emitTag = "map.isEmpty" });

    // ── Lookup ───────────────────────────────────────────────────────────
    map.methods.push_back({ "get",          {TypeKind::Extended}, {"_key"}, "_val",
                            .emitTag = "map.get" });
    map.methods.push_back({ "getOrDefault", {TypeKind::Extended}, {"_key", "_val"}, "_val",
                            .emitTag = "map.getOrDefault" });
    map.methods.push_back({ "has",          {TypeKind::Extended}, {"_key"}, "bool",
                            .emitTag = "map.has" });

    // ── Mutation ─────────────────────────────────────────────────────────
    map.methods.push_back({ "insert", {TypeKind::Extended}, {"_key", "_val"}, "void",
                            .emitTag = "map.insert" });
    map.methods.push_back({ "remove", {TypeKind::Extended}, {"_key"}, "bool",
                            .emitTag = "map.remove" });
    map.methods.push_back({ "clear",  {TypeKind::Extended}, {}, "void",
                            .emitTag = "map.clear" });

    // ── Cleanup ──────────────────────────────────────────────────────────
    map.methods.push_back({ "free", {TypeKind::Extended}, {}, "void",
                            .emitTag = "map.free" });

    // ── Iteration ────────────────────────────────────────────────────────
    map.methods.push_back({ "keys",   {TypeKind::Extended}, {}, "_vec_key",
                            .emitTag = "map.keys" });
    map.methods.push_back({ "values", {TypeKind::Extended}, {}, "_vec_val",
                            .emitTag = "map.values" });

    registerType(std::move(map));

    // ═══════════════════════════════════════════════════════════════════════
    // Set<T> — Open-addressing hash set
    //
    // LLVM layout: { ptr states, ptr keys, ptr hashes,
    //                usize len, usize cap, usize key_size }
    // C prefix: lucis_set
    // Native keyword: set<T>
    // ═══════════════════════════════════════════════════════════════════════

    ExtendedTypeDescriptor set;
    set.baseName     = "Set";
    set.genericArity = 1;
    set.layout       = {
        { "states",   "ptr"   },
        { "keys",     "ptr"   },
        { "hashes",   "ptr"   },
        { "len",      "usize" },
        { "cap",      "usize" },
        { "key_size", "usize" },
    };
    set.cPrefix = "lucis_set";

    // ── Size / query ─────────────────────────────────────────────────────
    set.methods.push_back({ "len",     {TypeKind::Extended}, {}, "usize",
                            .emitTag = "set.len" });
    set.methods.push_back({ "isEmpty", {TypeKind::Extended}, {}, "bool",
                            .emitTag = "set.isEmpty" });

    // ── Mutation ─────────────────────────────────────────────────────────
    set.methods.push_back({ "add",    {TypeKind::Extended}, {"_elem"}, "bool",
                            .emitTag = "set.add" });
    set.methods.push_back({ "has",    {TypeKind::Extended}, {"_elem"}, "bool",
                            .emitTag = "set.has" });
    set.methods.push_back({ "remove", {TypeKind::Extended}, {"_elem"}, "bool",
                            .emitTag = "set.remove" });
    set.methods.push_back({ "clear",  {TypeKind::Extended}, {}, "void",
                            .emitTag = "set.clear" });

    // ── Cleanup ──────────────────────────────────────────────────────────
    set.methods.push_back({ "free", {TypeKind::Extended}, {}, "void",
                            .emitTag = "set.free" });

    // ── Iteration ────────────────────────────────────────────────────────
    set.methods.push_back({ "values", {TypeKind::Extended}, {}, "_self",
                            .emitTag = "set.values" });

    registerType(std::move(set));

    // ═══════════════════════════════════════════════════════════════════════
    // Task<T> — Async task handle (opaque pointer wrapping pthread)
    //
    // LLVM layout: { ptr handle } (opaque — managed by C runtime)
    // C prefix: lucis_task
    // Module: std::thread
    // ═══════════════════════════════════════════════════════════════════════

    ExtendedTypeDescriptor task;
    task.baseName     = "Task";
    task.genericArity = 1;
    task.layout       = {
        { "handle", "ptr" },
    };
    task.cPrefix = "lucis_task";

    registerType(std::move(task));
}
