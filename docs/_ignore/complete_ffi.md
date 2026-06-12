# Complete FFI — Native C Interop for Lucis

## 1. Executive Summary

This document defines the complete plan for adding **native C FFI** to the Lucis compiler.
The goal is **total, seamless, zero-cost interoperability** between TM and C — they must be
siblings. A TM program should be able to `#include` a C header, call any C function, pass
and receive any C type, and do so with **no runtime overhead** compared to calling from C itself.

The system is **compiler-resolved**: there is no runtime FFI dispatch (no libffi at call sites).
The compiler reads C headers via a built-in C header resolver, generates LLVM declarations at
compile time, and links the resulting object files directly. The result is identical to what a
C compiler would produce.

---

## 2. Current State — What TM Already Has

### 2.1 Type Inventory

| TM Type | LLVM Representation | C Equivalent | Direct Mapping? |
|---------|---------------------|--------------|-----------------|
| `int8` | `i8` | `int8_t` / `signed char` | **Yes** |
| `int16` | `i16` | `int16_t` / `short` | **Yes** |
| `int32` | `i32` | `int32_t` / `int` | **Yes** |
| `int64` | `i64` | `int64_t` / `long long` | **Yes** |
| `int128` | `i128` | `__int128` | **Yes** |
| `uint8` | `i8` | `uint8_t` / `unsigned char` | **Yes** |
| `uint16` | `i16` | `uint16_t` / `unsigned short` | **Yes** |
| `uint32` | `i32` | `uint32_t` / `unsigned int` | **Yes** |
| `uint64` | `i64` | `uint64_t` / `unsigned long long` | **Yes** |
| `uint128` | `i128` | `unsigned __int128` | **Yes** |
| `isize` | `i64` (x86-64) | `ssize_t` / `intptr_t` | **Yes** |
| `usize` | `i64` (x86-64) | `size_t` / `uintptr_t` | **Yes** |
| `float32` | `float` | `float` | **Yes** |
| `float64` | `double` | `double` | **Yes** |
| `bool` | `i1` | — | **Adapt** (C uses `int` or `_Bool`) |
| `char` | `i8` | `char` | **Yes** |
| `void` | `void` | `void` | **Yes** |
| `*T` | `ptr` (opaque) | `T*` | **Yes** |
| `**T` | `ptr` | `T**` | **Yes** |
| `string` | `{ ptr, usize }` | — | **No** (C uses `char*`) |
| `[]T` | `[N x T]` | `T[N]` | **Partial** (TM hides size) |
| `struct S` | `%S = type { ... }` | `struct S { ... }` | **Yes** (same layout) |
| `enum E` | `i32` | `enum E` | **Yes** |
| `fn(A)->R` | `ptr` | `R(*)(A)` | **Yes** (opaque pointer) |
| `vec<T>` | `{ ptr, usize, usize }` | — | **No** (Lucis-only) |
| `map<K,V>` | 8-field struct | — | **No** (Lucis-only) |
| `set<T>` | 6-field struct | — | **No** (Lucis-only) |
| `int1` | `i1` | — | **No** (C has no 1-bit int) |
| `uint1` | `i1` | — | **No** (C has no 1-bit uint) |
| `intinf` | `i256` | — | **No** (TM-only) |

### 2.2 What Already Works for C Interop

1. **Calling convention**: TM uses C calling convention (cdecl) by default — identical ABI.
2. **Pointer model**: TM has full pointer support (`*T`, `&x`, `*ptr`, `ptr->field`).
3. **Struct layout**: TM structs follow LLVM default layout — same rules as C structs.
4. **Integer types**: `int8` through `int64`, and unsigned variants, map 1:1 to C `<stdint.h>` types.
5. **Float types**: `float32` = `float`, `float64` = `double`.
6. **Enums**: TM enums are `i32` — same as C enums.
7. **Object files**: TM emits standard ELF `.o` files — linkable with any C code.
8. **Linking**: Already uses `clang`/`gcc` for linking — can link against any C library.
9. **Builtins**: All Lucis builtins are C functions with `lucis_` prefix — the pattern is proven.
10. **Cast**: `as` operator supports `int↔ptr`, `int↔int`, `float↔float`, `int↔float`.

### 2.3 What's Missing

| Gap | Description | Priority |
|-----|-------------|----------|
| **`#include` directive** | Parse C headers and extract declarations | Critical |
| **C header resolver** | Read `.h` files, extract types/functions/macros | Critical |
| **`extern` declarations** | Declare external C symbols in TM | Critical |
| **`char*` / null-terminated strings** | C strings are `char*`, not `{ptr, len}` | Critical |
| **C struct import** | Map C struct definitions to TM types | Critical |
| **C enum import** | Map C enum definitions to TM enums | High |
| **C typedef import** | Map C typedefs to TM type aliases | High |
| **C macro constants** | `#define FOO 42` → TM constant | High |
| **C union support** | TM has no `union` type | High |
| **`long double` (80/128-bit)** | TM has no fp80/fp128 type | Medium |
| **`va_list` / C variadic** | TM variadic is different from C `...` | Medium |
| **Bitfields** | C `struct { int x : 3; }` | Low |
| **Volatile / restrict** | C qualifiers not in TM | Low |
| **`_Complex`** | C complex number types | Low |
| **`_Atomic`** | C atomic types | Low |

---

## 3. Architecture — How It Will Work

### 3.1 High-Level Flow

```
  User writes:
    #include <stdio.h>
    #include "mylib.h"

  Compiler pipeline:
    1. Lexer sees #include → CHeaderResolver receives the path
    2. CHeaderResolver invokes libclang to parse the C header
    3. Extracts: functions, structs, enums, typedefs, #define constants
    4. Generates CBindings: { name → CSymbol } registry
    5. Checker validates TM calls against CBindings signatures
    6. IRGen emits `declare` for external C functions (no body)
    7. Linker links TM .o against user-specified C library (-l flags)
```

### 3.2 Key Design Decisions

1. **libclang for header parsing** — not manual parsing. C headers are too complex
   (macros, conditional compilation, platform-specific types). libclang gives us the
   full preprocessor and type system for free.

2. **Zero runtime cost** — all C bindings are resolved at compile time. Every C function
   call compiles to a direct `call` instruction in LLVM IR, identical to what clang
   would emit.

3. **Automatic type mapping** — the resolver maps C types to TM types automatically.
   `int` → `int32`, `char*` → `*char`, `struct tm` → TM struct, etc.

4. **Explicit conversion for strings** — TM `string` and C `char*` are incompatible.
   The compiler provides `cstr()` and `fromCStr()` conversion functions, and warns
   when passing `string` where `*char` is expected.

5. **Library linking via CLI** — the user specifies linker flags:
   ```
   lucis main.lc ./main -lSDL2 -lm -lpng
   ```

---

## 4. New Types to Add

### 4.1 `long double` / `float80` / `float128`

C has `long double` which varies by platform:
- x86-64 Linux/macOS: 80-bit extended precision (`x86_fp80` in LLVM)
- x86-64 Windows: same as `double` (64-bit)
- AArch64: 128-bit IEEE quad precision (`fp128` in LLVM)

**Addition:**

| TM Type | LLVM Type | C Equivalent |
|---------|-----------|--------------|
| `float80` | `x86_fp80` | `long double` (x86) |
| `float128` | `fp128` | `_Float128` / `__float128` |

### 4.2 `union` Type

C unions share memory between fields — the struct occupies the size of its largest field.

```
union Value {
    int32 i;
    float32 f;
    *void ptr;
}
```

**LLVM representation**: A union is an alloca of the largest field size, with bitcast
access to different field types. With LLVM opaque pointers, this becomes a single alloca
with typed loads/stores at offset 0.

**Grammar addition:**
```antlr
unionDecl: UNION IDENTIFIER LBRACE unionField* RBRACE;
unionField: typeSpec IDENTIFIER SEMI;
```

### 4.3 `cstring` Type alias

A convenience type alias for null-terminated C strings:

```
// Built-in alias:  cstring = *char
```

This is purely syntactic sugar. Under the hood, `cstring` is just `*char`.
But it makes C interop intent explicit and enables the compiler to provide
better error messages.

### 4.4 C-compatible `bool` (`cbool`)

C's `_Bool` is stored as `i8` (not `i1` like TM's `bool`). When calling C
functions that take/return `_Bool`, the compiler must zero-extend `i1` → `i8`
on call and truncate `i8` → `i1` on return.

**Decision**: Handle this automatically in the FFI layer — no new type needed.
The compiler detects `_Bool` in C signatures and inserts the zext/trunc
transparently. TM users always see `bool`.

### 4.5 Fixed-size Array Syntax

C functions often take fixed-size arrays: `void foo(int arr[10])`.
TM currently uses `[]T` without explicit size. For FFI, we need:

```
// New syntax: [N]T for fixed-size arrays
[10]int32 buffer;
```

**Grammar addition:**
```antlr
typeSpec: LBRACKET INT_LIT RBRACKELucis typeSpec    // [10]int32
        | LBRACKET RBRACKELucis typeSpec             // []int32 (existing)
        | ...
        ;
```

This maps directly to `[10 x i32]` in LLVM — identical to C's `int buffer[10]`.

---

## 5. New Features to Add

### 5.1 `#include` Directive

The lexer/parser must recognize `#include` at the top level:

```
#include <stdio.h>          // System header (search /usr/include, etc.)
#include "mylib.h"          // Relative to source file
#include <SDL2/SDL.h>       // Library header
```

**Grammar addition:**
```antlr
program
    : namespaceDecl? includeDecl* useDecl* topLevelDecl* EOF
    ;

includeDecl
    : HASH_INCLUDE LT_PATH GT        // #include <stdio.h>
    | HASH_INCLUDE STRING_LIT        // #include "mylib.h"
    ;
```

**Lexer addition:**
```antlr
HASH_INCLUDE: '#include';
```

The actual header content is NOT fed to the TM parser. Instead, the
`CHeaderResolver` takes the path and uses libclang to extract declarations.

### 5.2 `extern` Keyword

For manual C function declarations (without `#include`):

```
extern int32 printf(cstring fmt, ...);
extern int32 open(cstring path, int32 flags);
extern void free(*void ptr);
```

**Grammar addition:**
```antlr
topLevelDecl
    : ...
    | externDecl
    ;

externDecl
    : EXTERN typeSpec IDENTIFIER LPAREN externParamList? (COMMA ELLIPSIS)? RPAREN SEMI
    ;

externParamList
    : externParam (COMMA externParam)*
    ;

externParam
    : typeSpec IDENTIFIER?   // name is optional in extern declarations
    ;
```

**Key distinction from LucisM functions**: `extern` functions have no body — they
generate `declare` in LLVM IR and are resolved at link time.

### 5.3 C Variadic Functions (`...`)

C's `...` variadic (e.g., `printf`) is fundamentally different from LucisM's
spread (`int32 ...args`). C variadic uses `va_list` internally and has no
type safety.

**Implementation**:
- `extern` functions with `...` emit LLVM `declare ... (...)` (true LLVM variadic)
- At call sites, the compiler applies default C argument promotions:
  - `float32` → `float64` (C's float → double promotion)
  - `int8`/`int16` → `int32` (C's integer promotion)
  - `bool` → `int32`
- TM's `string` is **NOT** automatically convertible — user must call `cstr()`

```
extern int32 printf(cstring fmt, ...);

fn main() int32 {
    printf(c"Hello %s, you are %d years old\n", c"World", 42);
    ret 0;
}
```

### 5.4 C String Literals (`c"..."`)

A new string literal prefix for null-terminated C string constants:

```
cstring msg = c"Hello, World!\n";   // *char pointing to null-terminated global
printf(c"Value: %d\n", x);
```

**Implementation**: `c"..."` compiles to `@.str = private unnamed_addr constant`
(a global null-terminated array), same as how clang emits string literals.
The result type is `*char` (aka `cstring`).

**Lexer addition:**
```antlr
C_STRING_LIT: 'c"' (~["\\\r\n] | EscapeSeq)* '"';
```

### 5.5 String Conversion Functions

Built-in conversion between TM `string` and C `*char`:

| Function | Signature | Description |
|----------|-----------|-------------|
| `cstr(s)` | `string → *char` | Allocates null-terminated copy (caller must `free`) |
| `fromCStr(p)` | `*char → string` | Creates TM string from null-terminated `char*` (computes `strlen`) |
| `fromCStrLen(p, n)` | `(*char, usize) → string` | Creates TM string from `char*` with known length |
| `s.raw()` | `string → *char` | Returns raw pointer WITHOUT null terminator (unsafe, no copy) |
| `s.len` | `string → usize` | Already exists — returns byte length |

### 5.6 `CHeaderResolver` — The Core Engine

This is the heart of the FFI system. It uses libclang to parse C headers and
extract declarations into a registry the compiler can query.

**File**: `src/ffi/CHeaderResolver.h`, `src/ffi/CHeaderResolver.cpp`

```cpp
struct CFunction {
    std::string name;
    std::string returnType;      // Mapped to TM type name
    std::vector<CParam> params;
    bool isVariadic;             // true if ends with ...
};

struct CStruct {
    std::string name;
    std::vector<CField> fields;
    bool isPacked;               // __attribute__((packed))
    size_t alignment;            // Custom alignment if specified
};

struct CEnum {
    std::string name;
    std::vector<std::pair<std::string, int64_t>> values;
};

struct CTypedef {
    std::string alias;
    std::string target;          // Resolved TM type name
};

struct CConstant {
    std::string name;
    std::string type;            // Inferred from value
    int64_t intValue;
    double floatValue;
    std::string strValue;
};

class CHeaderResolver {
public:
    bool resolveHeader(const std::string& headerPath, bool isSystem);
    
    const CFunction* findFunction(const std::string& name) const;
    const CStruct* findStruct(const std::string& name) const;
    const CEnum* findEnum(const std::string& name) const;
    const CTypedef* findTypedef(const std::string& name) const;
    const CConstant* findConstant(const std::string& name) const;
    
    // Map a C type (from libclang) to TM type name
    std::string mapCTypeToTM(CXType clangType) const;
    
private:
    std::unordered_map<std::string, CFunction> functions_;
    std::unordered_map<std::string, CStruct> structs_;
    std::unordered_map<std::string, CEnum> enums_;
    std::unordered_map<std::string, CTypedef> typedefs_;
    std::unordered_map<std::string, CConstant> constants_;
};
```

### 5.7 C Type → TM Type Mapping Table

This is the complete mapping the CHeaderResolver uses:

| C Type | libclang Kind | TM Type | Notes |
|--------|---------------|---------|-------|
| `void` | `CXType_Void` | `void` | Direct |
| `char` | `CXType_Char_S` | `char` | Direct |
| `signed char` | `CXType_SChar` | `int8` | Direct |
| `unsigned char` | `CXType_UChar` | `uint8` | Direct |
| `short` | `CXType_Short` | `int16` | Direct |
| `unsigned short` | `CXType_UShort` | `uint16` | Direct |
| `int` | `CXType_Int` | `int32` | Direct |
| `unsigned int` | `CXType_UInt` | `uint32` | Direct |
| `long` | `CXType_Long` | `int64` | x86-64 Linux (8 bytes) |
| `unsigned long` | `CXType_ULong` | `uint64` | x86-64 Linux |
| `long long` | `CXType_LongLong` | `int64` | Direct |
| `unsigned long long` | `CXType_ULongLong` | `uint64` | Direct |
| `float` | `CXType_Float` | `float32` | Direct |
| `double` | `CXType_Double` | `float64` | Direct |
| `long double` | `CXType_LongDouble` | `float80` | **New type** |
| `_Bool` | `CXType_Bool` | `bool` | Auto zext/trunc |
| `T*` | `CXType_Pointer` | `*T` | Recursive mapping |
| `T[N]` | `CXType_ConstantArray` | `[N]T` | **New syntax** |
| `T[]` | `CXType_IncompleteArray` | `*T` | Degrades to pointer |
| `struct S` | `CXType_Record` | `struct S` | Auto-import fields |
| `union U` | `CXType_Record` | `union U` | **New type** |
| `enum E` | `CXType_Enum` | `enum E` | Auto-import variants |
| `R(*)(A...)` | `CXType_FunctionProto` | `fn(A...) -> R` | Direct |
| `typedef T name` | `CXType_Typedef` | `type name = T;` | Recursive resolve |
| `size_t` | typedef → `unsigned long` | `usize` | Platform-aware |
| `ssize_t` | typedef → `long` | `isize` | Platform-aware |
| `int8_t` etc. | typedef → base | `int8` etc. | Platform-aware |
| `intptr_t` | typedef | `isize` | Platform-aware |
| `uintptr_t` | typedef | `usize` | Platform-aware |
| `ptrdiff_t` | typedef → `long` | `isize` | Platform-aware |
| `FILE*` | opaque struct ptr | `*void` | Opaque |
| `va_list` | platform-specific | — | Not exposed to TM |

### 5.8 Linker Flags via CLI

The user specifies external C libraries via the command line:

```bash
lucis main.lc ./main -lSDL2 -lm -lpng -I/usr/include/SDL2
```

**New CLI flags:**

| Flag | Description |
|------|-------------|
| `-l<lib>` | Link against library (passed to linker) |
| `-L<path>` | Add library search path |
| `-I<path>` | Add header include search path |
| `-framework <name>` | macOS framework (passed to linker) |

**Implementation**: Collect these flags in `CLIOptions` and pass them through
to `CodeGen::linkObjectFiles()` which appends them to the linker command.

---

## 6. Detailed Implementation Plan

### Phase 1: Foundation Types (No libclang yet)

**Goal**: Add the missing types and language features that C interop requires.
These are useful on their own even without FFI.

#### 1.1 `union` type
- Add `UNION` token to lexer
- Add `unionDecl` grammar rule
- Add `TypeKind::Union` to TypeInfo
- Implement in Checker: validate field types, compute max field size
- Implement in IRGen: alloca of max size, typed loads/stores at offset 0
- Add to NamespaceRegistry export

#### 1.2 Fixed-size array syntax `[N]T`
- Add grammar rule: `LBRACKET INT_LIT RBRACKELucis typeSpec`
- Implement in IRGen: emit `[N x T]` exactly
- Update countArrayDims to handle sized arrays
- Can coerce `[N]T` to `*T` for function calls (decay, like C)

#### 1.3 C string literal `c"..."`
- Add `C_STRING_LIT` token to lexer
- Add `cStrLitExpr` grammar alternative
- Implement in IRGen: emit global constant with null terminator, return `*char`

#### 1.4 `cstring` type alias
- Register `cstring` as built-in type alias for `*char` in TypeRegistry

#### 1.5 `float80` / `float128` types
- Add tokens to lexer
- Register in TypeRegistry with LLVM `x86_fp80` / `fp128`
- Add cast support between floating point widths

#### 1.6 String conversion builtins
- `cstr(string) -> *char` — call `lucis_cstr` (allocate + null-terminate)
- `fromCStr(*char) -> string` — call `lucis_from_cstr` (strlen + wrap)
- `fromCStrLen(*char, usize) -> string` — wrap pointer directly
- Implement C builtins in `src/builtins/string/string.c`

### Phase 2: `extern` Declarations

**Goal**: Allow manual declaration of C functions without header parsing.

#### 2.1 `extern` keyword
- Add `EXTERN` token to lexer
- Add `externDecl` to parser
- Add `ELLIPSIS` token for C-style `...` variadic
- Implement in Checker: register extern function signatures
- Implement in IRGen: emit `declare` (no body)

#### 2.2 C variadic support
- Detect `...` at end of extern param list
- Emit LLVM variadic function type: `declare i32 @printf(ptr, ...)`
- Apply C promotion rules at call sites
- Prevent using `...` in non-extern TM functions

### Phase 3: `#include` + C Header Resolver

**Goal**: Automatically parse C headers and make all declarations available.

#### 3.1 libclang integration
- Add libclang dependency to CMakeLists.txt
- Create `src/ffi/CHeaderResolver.h/.cpp`
- Implement `resolveHeader()` using `clang_parseTranslationUnit()`
- Walk AST with `clang_visitChildren()` and extract:
  - Function declarations → `CFunction`
  - Struct definitions → `CStruct`
  - Enum definitions → `CEnum`
  - Typedef declarations → `CTypedef`
  - `#define` integer/string constants → `CConstant`

#### 3.2 CType → TM type mapping
- Implement `mapCTypeToTM()` with the full mapping table (section 5.7)
- Handle recursive types (pointers to structs, etc.)
- Handle opaque structs (→ `*void`)
- Handle `typedef` chains (resolve to base type)

#### 3.3 Integration into compiler pipeline
- CLI collects `-I<path>` flags
- Lexer/parser recognizes `#include` directives
- After parsing, before checking: run CHeaderResolver on each included header
- CHeaderResolver results are merged into a `CBindings` registry
- Checker validates calls against CBindings (type-safe)
- IRGen emits `declare` for each C function actually used
- Linker receives `-l<lib>` and `-L<path>` flags

#### 3.4 Auto-import C structs
- When a C function returns `struct tm*`, the struct fields are imported
- Fields mapped recursively using CType → TM mapping
- Imported C structs available in TM by their C name
- Field access uses `ptr->field` or `val.field` — same as TM structs

#### 3.5 Auto-import C enums
- When a C header defines `enum color { RED = 0, GREEN = 1 }`, import as TM enum
- Variants accessible as `color::RED`, `color::GREEN`
- Underlying type is `int32` (matching C)

#### 3.6 C macro constants
- Extract `#define` constants that expand to integer or string literals
- Register as compile-time constants in TM: `C::SEEK_SET`, `C::O_RDONLY`
- Complex macros (function-like, computed) are silently skipped

### Phase 4: CLI Linker Flags

#### 4.1 Library flags
- Parse `-l<lib>`, `-L<path>`, `-I<path>` in CLI
- Store in CLIOptions
- Pass through to CodeGen linker command
- Pass `-I<path>` to CHeaderResolver include paths

#### 4.2 System header detection
- `#include <...>` searches:
  1. Paths from `-I` flags
  2. `/usr/include`
  3. `/usr/local/include`
  4. Platform-specific paths (`/usr/include/x86_64-linux-gnu`)
- `#include "..."` searches:
  1. Directory of the including file
  2. Paths from `-I` flags
  3. Same system paths as above

---

## 7. Type Adaptation Details

### 7.1 `bool` ↔ C `_Bool` / `int`

TM `bool` is `i1` in LLVM. C's `_Bool` is `i8`, and many C functions use `int` for booleans.

**Rules:**
- Calling C function that expects `_Bool`: auto `zext i1 → i8`
- Calling C function that expects `int` for boolean: auto `zext i1 → i32`
- Receiving `_Bool` from C: auto `trunc i8 → i1`
- Receiving `int` from C as bool: auto `icmp ne i32 %val, 0`

### 7.2 `string` ↔ C `char*`

**TM string**: `{ char*, usize }` — NOT null-terminated, length-prefixed.
**C string**: `char*` — null-terminated, no length.

These are fundamentally incompatible. The compiler MUST:
- **Error** if user passes `string` where `*char` is expected (with helpful message)
- **Error** if user passes `*char` where `string` is expected
- Provide explicit conversion: `cstr()`, `fromCStr()`, `fromCStrLen()`

```
#include <stdio.h>
use std::log::println;

fn main() int32 {
    string name = "World";
    
    // This would be a compile ERROR:
    // printf(c"Hello %s\n", name);  // string is not *char!
    
    // Correct:
    *char cname = cstr(name);        // allocate null-terminated copy
    printf(c"Hello %s\n", cname);
    free(cname as *void);            // caller must free
    
    // Or for read-only use (no null terminator guarantee):
    // Use fromCStr for the reverse direction
    *char raw = c"Hello from C";
    string s = fromCStr(raw);
    println(s);
    
    ret 0;
}
```

### 7.3 Array ↔ C Array

**C**: Arrays decay to pointers when passed to functions. `int arr[10]` in a
function parameter is actually `int*`.

**TM**: `[]int32` is a fixed-size stack array. `[10]int32` (new) is also fixed-size.

**Rules:**
- `[N]T` in TM = `T[N]` in C — identical LLVM representation
- When calling C function with `T*` param, TM auto-decays `[N]T` → `*T`
  (passes pointer to first element via GEP)
- When calling C function with `T[N]` param (treated as `T*`), same decay

### 7.4 `struct` ↔ C `struct`

TM structs and C structs have **identical** LLVM layout (ordered fields, platform
alignment). No conversion needed.

**Caveats:**
- C `__attribute__((packed))` structs need packed LLVM struct representation
- C structs with bitfields cannot be directly mapped — emit as opaque
- C anonymous structs/unions need synthetic names

### 7.5 Function Pointers ↔ C Function Pointers

Both are opaque pointers in LLVM's opaque pointer mode. Direct interop, no conversion.

**Example:**
```
#include <stdlib.h>

// qsort expects: void qsort(void* base, size_t n, size_t size, int (*compar)(const void*, const void*))
extern void qsort(*void base, usize n, usize size, fn(*void, *void) -> int32 compar);

fn compare(*void a, *void b) int32 {
    int32 va = *(a as *int32);
    int32 vb = *(b as *int32);
    ret va - vb;
}

fn main() int32 {
    [5]int32 arr = [5, 3, 1, 4, 2];
    qsort(&arr as *void, 5, 4, compare);
    // arr is now [1, 2, 3, 4, 5]
    ret 0;
}
```

---

## 8. C → TM Direction (Calling TM from C)

TM functions with namespace mangling produce C-compatible symbols:

| TM Declaration | Emitted Symbol | Callable from C? |
|----------------|----------------|-------------------|
| `fn main() int32` | `main` | Yes (entry point) |
| `fn add(int32 a, int32 b) int32` in namespace `Math` | `Math__add` | Yes (via `Math__add`) |

For C code to call TM functions, the user creates a C header manually:

```c
// math_tm.h — hand-written C header for TM Math namespace
extern int32_t Math__add(int32_t a, int32_t b);
extern int32_t Math__multiply(int32_t a, int32_t b);
```

**Future improvement**: The compiler could auto-generate C header files from LucisM
namespaces (`lucis --emit-header Math > math_tm.h`).

---

## 9. Performance Guarantees

The FFI system must maintain **zero overhead** compared to native C:

1. **No thunks**: C function calls are direct `call @funcname(...)` — no wrapper functions.
2. **No boxing**: Primitive types pass in registers — no heap allocation.
3. **No runtime dispatch**: All function signatures resolved at compile time.
4. **No copies** (for compatible types): Structs, pointers, integers pass by value or
   reference exactly as C would.
5. **Only copies for strings**: `cstr()` must allocate because it adds null terminator.
   `fromCStrLen()` is zero-copy (wraps existing pointer).

**Benchmark target**: A TM program calling `printf` 1 million times must be within 1%
of the same C program compiled with `clang -O2`.

---

## 10. File Structure

```
src/
├── ffi/
│   ├── CHeaderResolver.h        — libclang-based C header parser
│   ├── CHeaderResolver.cpp      — Implementation
│   ├── CBindings.h              — Registry of imported C declarations
│   ├── CBindings.cpp            — Lookup functions, type mapping
│   ├── CTypeMapper.h            — C type → TM type conversion
│   └── CTypeMapper.cpp          — Full mapping table implementation
├── builtins/
│   └── string/
│       └── string.c             — Add cstr(), fromCStr(), fromCStrLen()
├── types/
│   └── TypeRegistry.cpp         — Add float80, float128, cstring alias
├── grammar/
│   ├── LucisLexer.g4           — Add EXTERN, UNION, C_STRING_LIT, ELLIPSIS, HASH_INCLUDE
│   └── LucisParser.g4          — Add externDecl, unionDecl, includeDecl, [N]T syntax
└── ...
```

---

## 11. Example: Full FFI Usage

```
namespace Main;

#include <stdio.h>
#include <math.h>
#include <string.h>

use std::log::println;

struct Point {
    float64 x;
    float64 y;
}

fn distance(Point a, Point b) float64 {
    float64 dx = a.x - b.x;
    float64 dy = a.y - b.y;
    ret sqrt(dx * dx + dy * dy);  // sqrt from <math.h>
}

fn main() int32 {
    Point a = Point { x: 0.0, y: 0.0 };
    Point b = Point { x: 3.0, y: 4.0 };

    float64 d = distance(a, b);

    // Using C printf directly
    printf(c"Distance: %.2f\n", d);

    // Using TM println
    println(d);

    // C string manipulation
    *char buf = malloc(100) as *char;
    sprintf(buf, c"Point (%.1f, %.1f)", b.x, b.y);

    string result = fromCStr(buf);
    println(result);

    free(buf as *void);

    ret 0;
}
```

**Compiled with:**
```bash
lucis main.lc ./main -lm
```

---

## 12. Summary of All Changes

### Grammar Changes

| File | Change |
|------|--------|
| `LucisLexer.g4` | Add tokens: `EXTERN`, `UNION`, `HASH_INCLUDE`, `C_STRING_LIT`, `ELLIPSIS`, `FLOAT80`, `FLOAT128` |
| `LucisParser.g4` | Add rules: `includeDecl`, `externDecl`, `unionDecl`, `[N]T` in typeSpec, `c"..."` literal, `...` in extern params |

### New Files

| File | Purpose |
|------|---------|
| `src/ffi/CHeaderResolver.h/.cpp` | libclang-based C header parser |
| `src/ffi/CBindings.h/.cpp` | Registry of imported C symbols |
| `src/ffi/CTypeMapper.h/.cpp` | C → TM type mapping engine |

### Modified Files

| File | Changes |
|------|---------|
| `src/types/TypeRegistry.cpp` | Add `float80`, `float128`, `cstring` alias |
| `src/types/TypeInfo.h/.cpp` | Add `TypeKind::Union`, union LLVM type generation |
| `src/checkers/Checker.cpp` | Validate extern decls, C calls type checking, string/cstring mismatch errors |
| `src/IRBuilder/IRGen.cpp` | Emit extern declares, union alloca, C string literals, bool promotion |
| `src/machine_code/CodeGen.cpp` | Accept `-l`, `-L` flags for linking |
| `src/cli/CLI.h/.cpp` | Parse `-l`, `-L`, `-I` flags, pass to pipeline |
| `src/builtins/string/string.c` | Add `lucis_cstr`, `lucis_from_cstr`, `lucis_from_cstr_len` |
| `CMakeLists.txt` | Add libclang dependency, new source files |

### New Types Summary

| Type | LLVM | Purpose |
|------|------|---------|
| `float80` | `x86_fp80` | C `long double` interop |
| `float128` | `fp128` | C `_Float128` interop |
| `cstring` | `ptr` (= `*char`) | Semantic alias for null-terminated strings |
| `union U` | `alloca(max_field_size)` | C union interop |
| `[N]T` | `[N x T]` | Fixed-size array with explicit length |

### New Features Summary

| Feature | Syntax | Purpose |
|---------|--------|---------|
| `#include` | `#include <stdio.h>` | Import C header declarations |
| `extern` | `extern int32 puts(cstring s);` | Manual C function declaration |
| C string literal | `c"Hello\n"` | Null-terminated string constant |
| C variadic | `extern int32 printf(cstring, ...);` | Call C variadic functions |
| `-l` flag | `lucis main.lc ./out -lSDL2` | Link against C libraries |
| `-I` flag | `lucis main.lc ./out -I/opt/include` | Header search path |
| `-L` flag | `lucis main.lc ./out -L/opt/lib` | Library search path |
| `cstr()` | `*char c = cstr(myString);` | TM string → C string conversion |
| `fromCStr()` | `string s = fromCStr(cptr);` | C string → TM string conversion |
