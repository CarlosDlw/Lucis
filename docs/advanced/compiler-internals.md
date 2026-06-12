# Compiler Internals

This page describes how the Lucis compiler transforms source code into a native executable. The pipeline has 7 stages, from scanning project files to linking the final binary.

---

## Pipeline Overview

```
Source Files (.lc)
       │
       ▼
┌──────────────┐
│ 1. Scan      │  Find all .lc files in project
└──────┬───────┘
       ▼
┌──────────────┐
│ 2. Parse     │  ANTLR4 lexer + parser → AST
└──────┬───────┘
       ▼
┌──────────────┐
│ 3. Namespace │  Register all symbols across files
└──────┬───────┘
       ▼
┌──────────────┐
│ 4. FFI       │  Resolve C headers (system + local)
└──────┬───────┘
       ▼
┌──────────────┐
│ 5. Check     │  5-pass type checking
└──────┬───────┘
       ▼
┌──────────────┐
│ 6. IR + Opt  │  Generate LLVM IR, optimize, emit .o
└──────┬───────┘
       ▼
┌──────────────┐
│ 7. Link      │  Combine .o files → executable
└──────────────┘
```

---

## Stage 1: Project Scanning

The compiler starts by finding the project root and scanning for all `.lc` source files.

**Entry point:** `CLI::run()` in `src/cli/CLI.cpp`

The function `getProjectRoot()` walks up from the current directory looking for a `.lucis/` directory (the project marker). Then `ProjectScanner::scan()` collects all `.lc` files recursively.

For a single-file compilation like `lucis build main.lc -o ./main`, the scanner only processes the file specified on the command line.

---

## Stage 2: Parsing

Each source file is parsed using ANTLR4, which generates a concrete syntax tree (CST).

### Lexer → Parser → Tree

```
Source text
    │
    ▼
┌─────────────────┐
│ ANTLRInputStream │  Raw bytes → character stream
└────────┬────────┘
         ▼
┌─────────────────┐
│ LucisLexer     │  Characters → token stream
└────────┬────────┘
         ▼
┌─────────────────┐
│ CommonTokenStream│  Buffered token sequence
└────────┬────────┘
         ▼
┌─────────────────┐
│ LucisParser    │  Tokens → parse tree (ProgramContext)
└─────────────────┘
```

The grammar is defined in two files:
- `grammar/LucisLexer.g4` — keyword tokens, operators, literals
- `grammar/LucisParser.g4` — syntax rules (statements, expressions, declarations)

### Parse Tree Structure

The parser produces a tree of context objects. The top-level `ProgramContext` contains a list of `TopLevelDeclContext` nodes:

```
ProgramContext
├── TopLevelDeclContext
│   └── FunctionDeclContext (int32 main() { ... })
├── TopLevelDeclContext
│   └── StructDeclContext (struct Point { ... })
├── TopLevelDeclContext
│   └── EnumDeclContext (enum Color { ... })
├── TopLevelDeclContext
│   └── ExternDeclContext (extern fn printf(...))
└── ...
```

Each context node has typed accessors for its children (e.g., `ctx->functionDecl()`, `ctx->structDecl()`), making it easy to walk the tree in later stages.

---

## Stage 3: Namespace Registration

After parsing all files, the compiler builds a global symbol registry. This allows cross-file references — a function defined in one file can be called from another.

### NamespaceRegistry

The `NamespaceRegistry` class tracks every exported symbol:

```
NamespaceRegistry
├── registerFile(namespace, symbols)
│   Records all functions, structs, enums from a file
│
├── findSymbol(namespace, name)
│   Looks up a symbol by namespace + name
│
└── getExternalSymbols(namespace)
    Returns all symbols visible to a namespace via `use` imports
```

Each symbol is stored as an `ExportedSymbol` with:
- Name (e.g., `"add"`)
- Kind (function, struct, enum, type alias)
- Source file path
- Namespace path (e.g., `"math::utils"`)

### How `use` Works

When you write `use math::utils::add;`, the checker resolves this through the namespace registry:

1. Look up namespace `math::utils`
2. Find symbol `add` in that namespace
3. Make it available in the current file's scope

---

## Stage 4: C FFI Resolution

Before type checking, the compiler resolves all C header includes to extract type information for FFI.

### System Headers

For `#include <stdio.h>`, the compiler uses `libclang` to parse the system header and extract:
- Function signatures (e.g., `printf(const char*, ...) -> int`)
- Type definitions (e.g., `typedef unsigned long size_t`)
- Macros and constants

### Local Headers

For `#include "mymath.h"`, the compiler:
1. Resolves the path relative to the source file
2. Parses the header with `libclang`
3. Extracts function signatures, struct definitions, and enum values
4. If a matching `.c` file exists (e.g., `mymath.c`), automatically compiles it to an object file using the system C compiler

All extracted bindings are stored in a `CBindings` object that the checker and IR generator use to validate and emit calls to C functions.

### Auto-Compilation of Local C Files

When you `#include "mymath.h"` and `mymath.c` exists alongside it, the compiler:
1. Detects the `.c` file
2. Compiles it with `cc -c mymath.c -o mymath.o`
3. Includes the resulting `.o` file in the final link step

This means you don't need a separate build step for local C code.

---

## Stage 5: Type Checking

The checker performs **5 passes** over the parse tree. Multiple passes are needed because symbols can reference each other in any order (e.g., function A calls function B, which is defined later in the file).

### Pass Overview

| Pass | Purpose |
|------|---------|
| **0** | Register built-in types and functions |
| **0.5** | Register C FFI bindings from resolved headers |
| **1** | Process `use` imports, resolve cross-file references |
| **1.5** | Cross-file symbol resolution |
| **2** | Register type aliases |
| **3** | Register struct fields, enum variants |
| **3.5** | Process `extend` method declarations |
| **4** | Register function signatures (parameters, return types) |
| **5** | Check function bodies (expressions, statements, control flow) |

### Type Registries

The checker maintains several registries:

- **TypeRegistry** — all known types (builtins, structs, enums, aliases)
- **MethodRegistry** — methods on types (e.g., `Vec.push()`, `string.len()`)
- **ExtendedTypeRegistry** — generic collection types (`vec<T>`, `map<K,V>`, `set<T>`)
- **BuiltinRegistry** — stdlib functions (`println`, `sqrt`, `abs`, etc.)

### Error Reporting

When the checker finds an error, it records the file, line number, and a descriptive message:

```
error: main.lc:15 — cannot assign string to int32
error: main.lc:23 — unknown function 'foobar'
error: main.lc:31 — struct 'Point' has no field 'z'
```

If any errors are found during checking, compilation stops — no IR is generated.

---

## Stage 6: IR Generation

The IR generator walks the parse tree using the visitor pattern, emitting LLVM IR for each node. It receives the type information from the checker, so it doesn't need to re-validate anything.

### Visitor Pattern

The `IRGen` class inherits from `LucisParserBaseVisitor` and overrides 100+ visitor methods:

```
IRGen::generate(parseTree, filePath)
    ├── Creates LLVMContext, Module, IRBuilder
    ├── Visits each top-level declaration
    │   ├── visitFunctionDecl() → emits function IR
    │   ├── visitStructDecl() → registers struct type
    │   ├── visitEnumDecl() → registers enum constants
    │   └── visitExternDecl() → declares external function
    └── Returns IRModule { context, module }
```

### Function Code Generation

For each function, the IR generator:

1. Creates an LLVM function with the correct signature
2. Creates an entry basic block
3. Allocates stack space for parameters and locals
4. Visits each statement in the function body:
   - Variable declarations → `alloca` + `store`
   - Assignments → `load` + `store`
   - Function calls → `call`
   - Control flow → `br`, `condbr`, `phi`
   - Returns → cleanup + `ret`

### Monomorphization

Generic types like `vec<T>` are compiled into type-specific implementations. The compiler generates a unique function name for each concrete type:

```
vec<int32>.push()  → calls lucis_vec_push_i32()
vec<string>.push() → calls lucis_vec_push_str()
vec<float64>.pop() → calls lucis_vec_pop_f64()
```

The suffix (`i32`, `str`, `f64`, etc.) is determined by the element type's `builtinSuffix` property. Each suffixed function is implemented in C in the builtins library.

### Cleanup Generation

At every return point in a function, the IR generator calls `emitAllCleanups()`, which:

1. Emits all deferred statements in reverse order
2. Scans locals for collection types and emits free calls
3. Skips the return variable (if the function returns a collection)

This ensures resources are always cleaned up, regardless of which return path is taken.

---

## Stage 6b: Optimization

After generating IR for a source file, the compiler optionally runs LLVM optimization passes. See the [Optimization](optimization.md) page for details.

If no optimization flag is specified (default), this step is skipped entirely.

---

## Stage 6c: Object Emission

The optimized (or unoptimized) LLVM module is lowered to native machine code and written to a `.o` object file.

```
LLVM Module
    │
    ▼
┌─────────────────────────┐
│ Initialize LLVM Backend │  All targets, MCs, asm printers
└────────────┬────────────┘
             ▼
┌─────────────────────────┐
│ Create TargetMachine    │  Native triple, host CPU, PIC relocation
└────────────┬────────────┘
             ▼
┌─────────────────────────┐
│ Emit Object File        │  IR → machine code → .o file
└─────────────────────────┘
```

The compiler always targets the **native platform** — it detects the host CPU and target triple automatically using `llvm::sys::getDefaultTargetTriple()` and `llvm::sys::getHostCPUName()`.

Object files are written to the `.lucis/build/` directory.

---

## Stage 7: Linking

All object files are combined into a final executable using the system linker.

### What Gets Linked

```
┌─────────────────────────┐
│ Your .o files            │  From stage 6c (one per source file)
├─────────────────────────┤
│ liblucis.a             │  Builtins static library
│   lucis_vec_*          │    Vec operations
│   lucis_map_*          │    Map operations
│   lucis_set_*          │    Set operations
│   lucis_string_*       │    String operations
│   lucis_*              │    Math, IO, FS, thread, etc.
├─────────────────────────┤
│ System libraries        │
│   -lm                   │    Math (libm)
│   -lz                   │    Compression (zlib)
│   -lpthread             │    Threading (libpthread)
├─────────────────────────┤
│ User libraries          │  Via -l and -L flags
│   -lfoo                 │
│   -L/path/to/lib        │
└─────────────────────────┘
         │
         ▼
    Final executable
```

The linker is invoked by forking a child process and calling `execvp("ld", ...)`. The compiler waits for the linker to finish and reports success or failure.

### CLI Examples

```bash
# Simple compilation (no extra libraries)
lucis main.lc ./main

# With optimization
lucis main.lc ./main -o2

# With external library
lucis main.lc ./main -lssl -lcrypto

# With custom library path
lucis main.lc ./main -L/usr/local/lib -lmylib

# With include path for C headers
lucis main.lc ./main -I/usr/local/include
```

---

## Data Flow Summary

```
Source files (.lc)
    │
    ├── Scanned: collect all .lc files
    │
    ├── Parsed: ANTLR4 → parse trees (ProgramContext per file)
    │
    ├── Namespaces: symbols registered in NamespaceRegistry
    │
    ├── FFI: C headers parsed → CBindings
    │
    ├── Checked: 5-pass type validation
    │   Output: types, function signatures, error list
    │
    ├── IR Generated: visitor pattern → LLVM Module
    │   Output: IRModule { LLVMContext, llvm::Module* }
    │
    ├── Optimized: PassBuilder runs passes (if -o1/o2/o3)
    │
    ├── Emitted: LLVM → machine code → .o file
    │
    └── Linked: ld combines .o + builtins + system libs
        Output: native executable
```
