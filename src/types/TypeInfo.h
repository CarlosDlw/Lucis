#pragma once

#include <string>
#include <vector>

namespace llvm {
class Type;
class LLVMContext;
class DataLayout;
}

namespace antlr4 {
class ParserRuleContext;
}

enum class TypeKind {
    Integer,
    Float,
    Bool,
    Char,
    Void,
    String,
    Struct, // user-defined struct types
    Union,  // user-defined union types (fields share memory)
    Enum,   // user-defined enum types
    Pointer, // pointer to another type
    Function, // function type: fn(params) -> ret
    Extended, // extended types from stdlib (Vec<T>, Map<K,V>, etc.)
    Tuple,    // tuple type: tuple<T1, T2, ...>
    VAList,   // native variadic argument list state
};

struct TypeInfo;

struct FieldInfo {
    std::string     name;
    const TypeInfo* typeInfo;
    unsigned        arrayDims = 0;           // [] depth for this field/payload (0 = scalar)
    std::vector<unsigned> arraySizes;        // fixed sizes for [N]...[M]T (empty for non-fixed dims)
    bool            autoFill = false;  // compiler fills this field automatically
};

enum class EnumPayloadKind {
    Unit,
    Tuple,
    Named,
};

struct EnumVariantInfo {
    std::string name;
    unsigned discriminant = 0;
    EnumPayloadKind payloadKind = EnumPayloadKind::Unit;
    std::vector<FieldInfo> payloadFields;
};

struct TypeInfo {
    std::string name;           // language name: "int32", "uint64", "float", etc.
    TypeKind    kind;
    unsigned    bitWidth;       // for Integer: 1,8,16,32,64,128,256. 0 = pointer-sized (isize/usize)
    bool        isSigned;       // meaningful for Integer only
    std::string builtinSuffix;  // suffix for C builtins: "i32", "u64", "f32", "str", "bool"
    std::vector<FieldInfo> fields;          // non-empty only for Struct
    std::vector<std::string> enumVariants;  // non-empty only for Enum
    std::vector<EnumVariantInfo> enumVariantInfos; // detailed variant metadata for Enum
    const TypeInfo* pointeeType = nullptr;   // non-null only for Pointer
    const TypeInfo* returnType = nullptr;                // non-null only for Function
    std::vector<const TypeInfo*> paramTypes;              // non-empty only for Function
    bool isVariadic = false;                             // true if last param is variadic (...)
    const TypeInfo* elementType = nullptr;   // non-null for Extended generics (Vec<T> → T)
    const TypeInfo* keyType = nullptr;       // non-null for Map<K,V> → K
    const TypeInfo* valueType = nullptr;     // non-null for Map<K,V> → V
    std::string extendedKind;                // extended type base name: "Vec", "Map", etc.
    unsigned arraySize = 0;                  // >0 for fixed-size arrays: [N]T
    std::vector<unsigned> arraySizes;        // multi-dim array sizes for type aliases: [3][3]T → {3,3}
    std::vector<const TypeInfo*> tupleElements; // non-empty only for Tuple
    bool dropTracked = false;                   // ownership/autodrop relevant type
    bool moveOnly = false;                      // value should move instead of implicit copy

    // User-defined generic instance metadata (set by Checker during monomorphization)
    bool isGenericInstance = false;                     // true when Foo<int32> was instantiated
    std::string genericBaseName;                       // "Node" for Node<int32>
    std::vector<std::string> typeParamNames;           // ["T"] — from the template definition
    std::vector<const TypeInfo*> typeArgs;             // [int32] — resolved concrete types
    // Opaque pointer to the original struct AST node — cast to StructDeclContext* at use sites
    antlr4::ParserRuleContext* genericStructDecl = nullptr;
    antlr4::ParserRuleContext* genericEnumDecl = nullptr;

    llvm::Type* toLLVMType(llvm::LLVMContext& ctx,
                           const llvm::DataLayout& dl) const;
};
