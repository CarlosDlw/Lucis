#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace llvm {
class Type;
class LLVMContext;
class DataLayout;
}

namespace semantic {

// ── SourceLocation ────────────────────────────────────────────────────────────

struct SourceLocation {
    std::string file;
    unsigned    line   = 0;
    unsigned    column = 0;
};

// ── DeclKind ──────────────────────────────────────────────────────────────────

enum class DeclKind {
    // Primitives
    Integer,
    Float,
    Bool,
    Char,
    Void,
    String,
    VAList,
    // Composites
    Struct,
    Union,
    Enum,
    Pointer,
    Function,
    Extended,
    Tuple,
    // Aliases / templates
    TypeAlias,
    GenericTemplate,
    // Intrinsics
    Intrinsic,
};

// ── Forward declarations ──────────────────────────────────────────────────────

struct Decl;
struct PrimitiveDecl;
struct StructDecl;
struct UnionDecl;
struct EnumDecl;
struct PointerDecl;
struct FunctionDecl;
struct ExtendedDecl;
struct TupleDecl;
struct TypeAliasDecl;
struct GenericTemplateDecl;

// ── Supporting types ──────────────────────────────────────────────────────────

struct FieldInfo {
    std::string name;
    Decl*       type = nullptr;       // non-owning pointer into SemanticDB
    unsigned    arrayDims = 0;
    std::vector<unsigned> arraySizes;
    bool        autoFill = false;
};

enum class VariantPayloadKind { Unit, Tuple, Named };

struct VariantInfo {
    std::string          name;
    unsigned             discriminant = 0;
    VariantPayloadKind   payloadKind = VariantPayloadKind::Unit;
    std::vector<FieldInfo> payloadFields;
};

struct ParamInfo {
    std::string name;
    Decl*       type = nullptr;       // non-owning pointer into SemanticDB
};

struct MethodInfo {
    std::string           name;
    Decl*                 returnType = nullptr;
    std::vector<ParamInfo> params;
    bool                  isStatic = false;
};

// ── Decl base ─────────────────────────────────────────────────────────────────

struct Decl {
    std::string    name;
    std::string    modulePath;        // empty = builtin
    SourceLocation loc;
    DeclKind       kind;
    bool           dropTracked = false;
    bool           moveOnly    = false;

    explicit Decl(DeclKind k) : kind(k) {}
    virtual ~Decl() = default;

    // Deep copy (returns a self-contained clone with the same type references).
    // Type references (FieldInfo::type, etc.) are kept as-is — they point back
    // into the SemanticDB that owns this Decl and its clone.
    virtual std::unique_ptr<Decl> clone() const = 0;

    // Replace type-parameter names with concrete Decl pointers.
    // Called on a cloned pattern during generic instantiation.
    virtual void substituteTypes(
        const std::unordered_map<std::string, Decl*>& subst) {}

    // Convert to an LLVM type (requires LLVMContext + DataLayout).
    virtual llvm::Type* toLLVMType(llvm::LLVMContext& ctx,
                                   const llvm::DataLayout& dl) const;

    // ── Type-checking helpers (const, non-virtual) ──────────────────────

    template <typename T> const T* as() const {
        static_assert(std::is_base_of_v<Decl, T>);
        return (kind == T::staticKind) ? static_cast<const T*>(this) : nullptr;
    }

    bool isPrimitive() const;
};

// ── PrimitiveDecl ─────────────────────────────────────────────────────────────

struct PrimitiveDecl : Decl {
    static constexpr DeclKind staticKind = DeclKind::Integer; // overridden per instance

    unsigned    bitWidth = 0;          // 0 = pointer-sized (isize/usize)
    bool        isSigned = false;
    std::string builtinSuffix;         // "i32", "u64", "f32", "str", "bool", "char"

    explicit PrimitiveDecl(DeclKind k = DeclKind::Integer) : Decl(k) {}

    std::unique_ptr<Decl> clone() const override;
};

// ── StructDecl ────────────────────────────────────────────────────────────────

struct StructDecl : Decl {
    static constexpr DeclKind staticKind = DeclKind::Struct;

    std::vector<std::string> genericParams; // empty = non-generic
    std::vector<FieldInfo>   fields;
    std::vector<MethodInfo>  methods;       // merged from extend blocks
    std::string parentName;                 // empty = no parent struct

    StructDecl() : Decl(DeclKind::Struct) {}

    std::unique_ptr<Decl> clone() const override;
    void substituteTypes(
        const std::unordered_map<std::string, Decl*>& subst) override;
};

// ── UnionDecl ─────────────────────────────────────────────────────────────────

struct UnionDecl : Decl {
    static constexpr DeclKind staticKind = DeclKind::Union;

    std::vector<std::string> genericParams;
    std::vector<FieldInfo>   fields;

    UnionDecl() : Decl(DeclKind::Union) {}

    std::unique_ptr<Decl> clone() const override;
    void substituteTypes(
        const std::unordered_map<std::string, Decl*>& subst) override;
};

// ── EnumDecl ──────────────────────────────────────────────────────────────────

struct EnumDecl : Decl {
    static constexpr DeclKind staticKind = DeclKind::Enum;

    std::vector<std::string> genericParams;
    std::vector<VariantInfo>  variants;
    unsigned                  bitWidth = 32;   // tag discriminant width

    EnumDecl() : Decl(DeclKind::Enum) {}

    bool hasPayload() const;

    std::unique_ptr<Decl> clone() const override;
    void substituteTypes(
        const std::unordered_map<std::string, Decl*>& subst) override;
};

// ── PointerDecl ───────────────────────────────────────────────────────────────

struct PointerDecl : Decl {
    static constexpr DeclKind staticKind = DeclKind::Pointer;

    Decl* pointeeType = nullptr;

    PointerDecl() : Decl(DeclKind::Pointer) {}

    std::unique_ptr<Decl> clone() const override;
    void substituteTypes(
        const std::unordered_map<std::string, Decl*>& subst) override;
};

// ── FunctionDecl ──────────────────────────────────────────────────────────────

struct FunctionDecl : Decl {
    static constexpr DeclKind staticKind = DeclKind::Function;

    Decl*                   returnType = nullptr;
    std::vector<ParamInfo>   params;
    bool                    isVariadic = false;
    bool                    isExtern   = false;
    std::string             externABI;          // "C" or empty
    std::vector<std::string> genericParams;     // empty = non-generic

    FunctionDecl() : Decl(DeclKind::Function) {}

    std::unique_ptr<Decl> clone() const override;
    void substituteTypes(
        const std::unordered_map<std::string, Decl*>& subst) override;
};

// ── ExtendedDecl (Vec<T>, Map<K,V>, Set<T>, Task<T>, Mutex<T>) ────────────────

struct ExtendedDecl : Decl {
    static constexpr DeclKind staticKind = DeclKind::Extended;

    std::string extendedKind;           // "Vec", "Map", "Set", "Task", "Mutex"
    Decl*       elementType = nullptr;  // Vec<T>, Set<T>
    Decl*       keyType     = nullptr;  // Map<K,V>
    Decl*       valueType   = nullptr;  // Map<K,V>
    std::string cPrefix;                // C runtime function prefix

    ExtendedDecl() : Decl(DeclKind::Extended) {}

    std::unique_ptr<Decl> clone() const override;
    void substituteTypes(
        const std::unordered_map<std::string, Decl*>& subst) override;
};

// ── TupleDecl ─────────────────────────────────────────────────────────────────

struct TupleDecl : Decl {
    static constexpr DeclKind staticKind = DeclKind::Tuple;

    std::vector<Decl*> elements;

    TupleDecl() : Decl(DeclKind::Tuple) {}

    std::unique_ptr<Decl> clone() const override;
    void substituteTypes(
        const std::unordered_map<std::string, Decl*>& subst) override;
};

// ── TypeAliasDecl ─────────────────────────────────────────────────────────────

struct TypeAliasDecl : Decl {
    static constexpr DeclKind staticKind = DeclKind::TypeAlias;

    Decl*                   target = nullptr;
    unsigned                arraySize = 0;
    std::vector<unsigned>   arraySizes;

    TypeAliasDecl() : Decl(DeclKind::TypeAlias) {}

    std::unique_ptr<Decl> clone() const override;
    void substituteTypes(
        const std::unordered_map<std::string, Decl*>& subst) override;
};

// ── GenericTemplateDecl ───────────────────────────────────────────────────────

struct GenericTemplateDecl : Decl {
    static constexpr DeclKind staticKind = DeclKind::GenericTemplate;

    std::vector<std::string>  typeParams;       // ["T"] or ["K", "V"]
    std::unique_ptr<Decl>     pattern;          // fully resolved StructDecl/EnumDecl/etc

    GenericTemplateDecl() : Decl(DeclKind::GenericTemplate) {}

    std::unique_ptr<Decl> clone() const override;
    void substituteTypes(
        const std::unordered_map<std::string, Decl*>& subst) override;
};

// ── IntrinsicDecl (compiler intrinsics) ───────────────────────────────────────

enum class IntrinsicLowering { LLVMIntrinsic, BuiltinCall, InlineIR };

struct IntrinsicDecl : Decl {
    static constexpr DeclKind staticKind = DeclKind::Intrinsic;

    Decl*               returnType = nullptr;
    std::vector<ParamInfo> params;
    IntrinsicLowering   lowering = IntrinsicLowering::LLVMIntrinsic;
    std::string         llvmIntrinsic;     // e.g. "llvm.trap"
    std::string         builtinName;       // e.g. "memcpy"

    IntrinsicDecl() : Decl(DeclKind::Intrinsic) {}

    std::unique_ptr<Decl> clone() const override;
};

} // namespace semantic
