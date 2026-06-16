# SemanticDB — Unified Declaration Store

## Overview

`SemanticDB` is the **single source of truth** for all declarations (types, functions, generics, methods) known to the Lucis compiler. It replaces the previous architecture where `TypeRegistry`, `MethodRegistry`, `ExtendedTypeRegistry`, `BuiltinRegistry`, `IntrinsicRegistry`, template maps, and method maps were duplicated between the Checker and IRGen phases.

## Architecture

```
ANTLR4 Parse Tree
       │
       ▼
┌──────────────┐     ┌─────────────────────┐
│  Checker     │────▶│    SemanticDB        │
│  (producer)  │     │  (single source)     │
└──────────────┘     │                      │
                     │  types_              │
         ┌───────────│  functions_          │───────────┐
         │           │  generics_           │           │
         ▼           │  builtins_           │           ▼
   ┌──────────┐      │  intrinsics_         │     ┌──────────┐
   │  IRGen   │      └─────────────────────┘     │   LSP    │
   │(consumer)│                                   │(consumer)│
   └──────────┘                                   └──────────┘
```

### Key Properties

- **Populated once** during semantic analysis (Checker phase)
- **Read-only** for all subsequent phases (IRGen, LSP, Optimizer)
- **Auto-validated** — `verifySemanticDBConsistency()` runs at the end of each check unit
- **Serializable** — cached to `.lucis/build/cache/semantic.db` for incremental builds

## Decl Hierarchy

The old `TypeInfo` struct (a flat union of fields for different type kinds) is replaced by a polymorphic hierarchy:

```
Decl (base)
├── PrimitiveDecl    — int32, float64, bool, char, void, string, va_list
├── StructDecl       — struct { fields, methods from extend blocks }
├── UnionDecl        — union { fields }
├── EnumDecl         — enum { variants with Unit/Tuple/Named payloads }
├── PointerDecl      — *T
├── FunctionDecl     — fn(params) -> ret
├── ExtendedDecl     — Vec<T>, Map<K,V>, Set<T>, Task<T>, Mutex<T>
├── TupleDecl        — (T1, T2, ...)
├── TypeAliasDecl    — type MyInt = int32
├── GenericTemplateDecl — struct Node<T> { ... }
└── IntrinsicDecl    — compiler intrinsics (trap, bitcast, etc.)
```

### Supporting Types

| Type | Fields |
|------|--------|
| `FieldInfo` | `name`, `type*`, `arrayDims`, `arraySizes`, `autoFill` |
| `MethodInfo` | `name`, `returnType*`, `params[]`, `isStatic` |
| `VariantInfo` | `name`, `discriminant`, `payloadKind`, `payloadFields[]` |
| `ParamInfo` | `name`, `type*` |
| `SourceLocation` | `file`, `line`, `column` |

All type references are non-owning `const Decl*` pointers into the same `SemanticDB`.

## API

### Registration (Checker)

```cpp
// Forward-declare a type for recursive/cross-file references
db.forwardDeclare("Node", DeclKind::Struct, "lib/data", loc);

// Register fully resolved types
db.registerType(std::make_unique<StructDecl>(...));
db.registerFunction(std::make_unique<FunctionDecl>(...));
db.registerGeneric(std::make_unique<GenericTemplateDecl>(...));

// Merge extend-block methods into a struct
db.mergeExtendMethods("Point", {method1, method2});

// Register builtins (primitives, Vec, Map, Set)
db.registerBuiltin(std::make_unique<PrimitiveDecl>(...));

// Instantiate a generic type
auto* node_i32 = db.instantiateGeneric("Node", {int32Decl}, loc);
```

### Query (IRGen, LSP)

```cpp
// Typed lookup
const auto* sd = db.lookup<StructDecl>("Point");
for (const auto& f : sd->fields) { ... }

// Untyped lookup
const Decl* d = db.lookupAny("main");
if (auto* fd = d->as<FunctionDecl>()) { ... }

// Cross-file
const Decl* sym = db.findInModule("lib/math", "add");
```

### Generic Instantiation

```cpp
// Generic template stored with resolved pattern
GenericTemplateDecl {
    typeParams: ["T"]
    pattern: StructDecl {
        name: "Node"
        fields: [{name: "value", type: T}, {name: "next", type: *Node}]
    }
}

// Instantiation: clone + substitute
instantiateGeneric("Node", {int32Decl})
  → StructDecl {
        name: "Node__int32"
        fields: [{name: "value", type: int32}, {name: "next", type: *Node__int32}]
    }
```

## Data Flow

### Phase 1: Checker populates SemanticDB

During semantic analysis, each `check*Decl()` method registers declarations in both the legacy `TypeRegistry` (for backward compatibility) and the new `SemanticDB`. Methods:

- `syncToSemanticDB_Struct()` — structs with fields
- `syncToSemanticDB_Union()` — unions
- `syncToSemanticDB_Enum()` — enums with variants
- `syncToSemanticDB_TypeAlias()` — type aliases
- `syncToSemanticDB_Function()` — function signatures
- `syncToSemanticDB_Extend()` — extend-block methods
- `syncToSemanticDB_GenericStruct/Union/Enum/Func/Extend()` — generic templates

### Phase 2: IRGen reads from SemanticDB

IRGen receives `const SemanticDB*` and uses it as the authoritative source for type data:

- `syncTypeFromSemanticDB(name)` — converts `Decl` → `TypeInfo` and registers in local TypeRegistry, avoiding AST re-walking
- Cross-file type resolution uses SemanticDB instead of re-parsing external files

### Phase 3: Consistency verification

`verifySemanticDBConsistency()` runs at the end of each `Checker::check()` call, comparing TypeRegistry and SemanticDB contents to detect data loss.

### Phase 4: LSP independence from AST

`ExportedSymbol` in `ModuleRegistry` now stores `line`/`column` directly, allowing LSP go-to-definition to navigate cross-file symbols without accessing the parse tree.

### Phase 5: Serialization

```cpp
// Save after successful check
db.save(".lucis/build/cache/semantic.db");

// Load for incremental builds
auto db = SemanticDB::load(".lucis/build/cache/semantic.db");
```

Cache format (line-based text):
```
VER 1
B int32 Integer 32 1 "i32"
X Vec Vec lucis_vec void - -
T Point Struct
  F x float64 0
  F y float64 0
  M distance float64 instance
E
C add int32 (int32 int32)
G Node Struct T
```

## Migration Status

| Phase | Description | Status |
|-------|-------------|--------|
| 0 | Decl hierarchy + SemanticDB | Done |
| 1 | Checker populates SemanticDB | Done |
| 2 | IRGen receives SemanticDB | Done |
| 3 | Consistency verification | Done |
| 4 | LSP location independence | Done |
| 5 | Serialization / cache | Done |
| — | Remove legacy TypeRegistry (future) | Pending |
| — | IRGen uses SemanticDB exclusively (future) | Pending |

## Files

| File | Role |
|------|------|
| `src/semantic/Decl.h` | Decl hierarchy (11 subtypes) |
| `src/semantic/Decl.cpp` | clone(), substituteTypes(), toLLVMType() |
| `src/semantic/SemanticDB.h` | Central database API |
| `src/semantic/SemanticDB.cpp` | Implementation + serialization |
| `src/checkers/Checker.h` | SemanticDB* member + conversion helpers |
| `src/checkers/Checker.cpp` | 12 syncToSemanticDB_* methods + initSemanticDB() |
| `src/IRBuilder/IRGen.h` | semanticDB_ member |
| `src/IRBuilder/IRGen.cpp` | syncTypeFromSemanticDB() |
| `src/namespace/ModuleRegistry.h` | line/column in ExportedSymbol |
| `src/cli/LucisPipeline.cpp` | Creates SemanticDB, passes to Checker/IRGen, saves cache |
