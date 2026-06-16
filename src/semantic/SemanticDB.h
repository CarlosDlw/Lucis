#pragma once

#include "semantic/Decl.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace semantic {

// ── SemanticDB ────────────────────────────────────────────────────────────────
//
// Single source of truth for all declarations known to the compiler.
//
// Lifecycle:
//   Phase 1 — Checker populates the DB during semantic analysis:
//     forwardDeclare()  → record that a name exists (recursive/cross-file refs)
//     registerType()    → store a fully resolved type
//     registerFunction()→ store a function signature
//     registerGeneric() → store a generic template
//     mergeExtendMethods() → attach methods to a struct
//     instantiateGeneric() → produce a concrete type from a template
//
//   Phase 2 — IRGen / LSP / Optimizer query the DB (read-only):
//     lookup<T>()       → typed lookup by name
//     lookupAny()       → untyped lookup by name
//     lookupFunction()  → function-specific lookup
//     lookupGeneric()   → template-specific lookup
//     findInModule()    → cross-file symbol resolution
//
// Ownership:
//   The SemanticDB owns all Decls via unique_ptr.
//   All external references are const Decl* (read-only, non-owning).
//   Cross-references within the DB (FieldInfo::type, etc.) use raw Decl*
//   pointers that point into the same SemanticDB.

class SemanticDB {
public:
    SemanticDB() = default;
    ~SemanticDB();

    // Not copyable (owns Decls)
    SemanticDB(const SemanticDB&) = delete;
    SemanticDB& operator=(const SemanticDB&) = delete;

    // Movable
    SemanticDB(SemanticDB&&) noexcept = default;
    SemanticDB& operator=(SemanticDB&&) noexcept = default;

    // ── Forward declarations ──────────────────────────────────────────────

    // Record that a type name exists before its full definition is known.
    // Used for recursive types (struct with *Self field) and cross-file
    // references where only the name is known initially.
    // Returns false if the name is already fully registered as a type.
    bool forwardDeclare(const std::string& name,
                        DeclKind kind,
                        const std::string& modulePath,
                        SourceLocation loc);

    // Returns true if a forward decl (but not yet a full type) exists.
    bool hasForwardDecl(const std::string& name) const;

    // Returns the forward-declared kind, or throws if not found.
    DeclKind forwardDeclKind(const std::string& name) const;

    // ── Type registration ─────────────────────────────────────────────────

    // Register a fully resolved type.  If a forward declaration exists for
    // the same name and kind, it is consumed (upgraded).  If a full type
    // already exists, this is a hard error (returns false).
    //
    // Pending extend methods (from mergeExtendMethods called before the type
    // was fully registered) are automatically applied.
    bool registerType(std::unique_ptr<Decl> decl);

    // ── Function registration ─────────────────────────────────────────────

    bool registerFunction(std::unique_ptr<FunctionDecl> decl);

    // ── Generic template registration ─────────────────────────────────────

    bool registerGeneric(std::unique_ptr<GenericTemplateDecl> tmpl);

    // ── Intrinsic registration ────────────────────────────────────────────

    bool registerIntrinsic(std::unique_ptr<IntrinsicDecl> decl);

    // ── Builtin registration (primitives, extended types like Vec/Map) ───

    bool registerBuiltin(std::unique_ptr<Decl> decl);

    // ── Extend methods ────────────────────────────────────────────────────

    // Merge extend-block methods into the target struct.
    // If the struct is already fully registered, methods are appended directly.
    // If only a forward declaration exists, methods are queued and applied
    // when registerType() is called for the struct.
    bool mergeExtendMethods(const std::string& structName,
                            std::vector<MethodInfo> methods);

    // ── Lookup (read-only) ────────────────────────────────────────────────

    // Typed lookup. Returns nullptr if not found or wrong kind.
    template <typename T>
    const T* lookup(const std::string& name) const {
        const Decl* d = lookupAny(name);
        return d ? d->as<T>() : nullptr;
    }

    // Untyped lookup across all categories (types, functions, builtins).
    // Priority: types_ > functions_ > builtins_ > intrinsics_ > generics_
    const Decl* lookupAny(const std::string& name) const;

    const FunctionDecl* lookupFunction(const std::string& name) const;
    const GenericTemplateDecl* lookupGeneric(const std::string& name) const;
    const IntrinsicDecl* lookupIntrinsic(const std::string& name) const;

    // ── Cross-file lookup ─────────────────────────────────────────────────

    // Find a declaration by module path + name.
    // (Module index is built automatically during registerType / registerFunction.)
    const Decl* findInModule(const std::string& modulePath,
                             const std::string& name) const;

    // ── Existence checks ──────────────────────────────────────────────────

    bool hasType(const std::string& name) const;
    bool hasFunction(const std::string& name) const;
    bool hasGeneric(const std::string& name) const;
    bool hasIntrinsic(const std::string& name) const;
    bool hasBuiltin(const std::string& name) const;

    // ── Generic instantiation ─────────────────────────────────────────────

    // Instantiate a generic template with concrete type arguments.
    // Clones the pattern Decl, substitutes type params → args, registers the
    // result under the mangled name.
    //
    // Returns the concrete type on success, nullptr on failure
    // (template not found, argument count mismatch, cycle detected).
    const Decl* instantiateGeneric(const std::string& baseName,
                                   const std::vector<Decl*>& typeArgs,
                                   SourceLocation usageLoc);

    // Mangle a generic name for storage: "Node" + [int32] → "Node__int32"
    static std::string mangleGenericName(
        const std::string& baseName,
        const std::vector<Decl*>& typeArgs);

    // ── Iteration ─────────────────────────────────────────────────────────

    std::vector<const Decl*> allTypes() const;
    std::vector<const FunctionDecl*> allFunctions() const;
    std::vector<const GenericTemplateDecl*> allGenerics() const;

    // ── Serialization (Phase 5) ───────────────────────────────────────────

    void save(const std::string& path) const;
    static std::unique_ptr<SemanticDB> load(const std::string& path);

private:
    // Forward declarations (before full definition)
    struct ForwardInfo {
        DeclKind     kind;
        std::string  modulePath;
        SourceLocation loc;
    };
    std::unordered_map<std::string, ForwardInfo> forwardDecls_;

    // Completed types, indexed by name
    std::unordered_map<std::string, std::unique_ptr<Decl>> types_;

    // Functions
    std::unordered_map<std::string, std::unique_ptr<FunctionDecl>> functions_;

    // Generic templates
    std::unordered_map<std::string, std::unique_ptr<GenericTemplateDecl>> generics_;

    // Intrinsics
    std::unordered_map<std::string, std::unique_ptr<IntrinsicDecl>> intrinsics_;

    // Builtins (primitives, extended — not in types_ to avoid name clashes)
    std::unordered_map<std::string, std::unique_ptr<Decl>> builtins_;

    // Module index: modulePath → {symbolName → Decl*}
    // Populated during registerType / registerFunction / registerBuiltin.
    void indexInModule(const std::string& modulePath,
                       const std::string& name, Decl* ptr);
    std::unordered_map<std::string,
        std::unordered_map<std::string, Decl*>> modules_;

    // Pending extend methods for structs that are only forward-declared.
    // Keyed by struct name.  Flushed when the struct is fully registered.
    std::unordered_map<std::string, std::vector<MethodInfo>> pendingExtends_;

    // Apply queued extend methods to a struct (called from registerType).
    void flushPendingExtends(const std::string& structName, StructDecl* sd);

    // Cycle detection during generic instantiation.
    std::unordered_set<std::string> instantiating_;
};

} // namespace semantic
