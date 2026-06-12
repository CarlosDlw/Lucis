# FFI Implementation Plan — Step-by-Step

This is the ordered execution plan for implementing native C FFI in Lucis.
Each step is a self-contained unit: grammar → checker → IRGen → test.
Steps are ordered by dependency — each one builds on the previous.

---

## Step 1: `extern` function declarations

**What**: Allow declaring C functions without a body. This is the minimal
primitive that makes all C interop possible — everything else builds on it.

**Syntax**:
```
extern int32 puts(*char s);
extern float64 sqrt(float64 x);
extern *void malloc(usize size);
extern void free(*void ptr);
```

**Changes**:

| File | Change |
|------|--------|
| `LucisLexer.g4` | Add `EXTERN : 'extern';` token |
| `LucisParser.g4` | Add `externDecl` rule to `topLevelDecl`: `EXTERN typeSpec IDENTIFIER LPAREN externParamList? RPAREN SEMI` |
| `LucisParser.g4` | Add `externParamList` and `externParam` rules (type + optional name) |
| `Checker.cpp` | Add `checkExternDecl()` — register function signature in known functions, validate types |
| `IRGen.cpp` | Add `visitExternDecl()` — emit `declare` with exact LLVM types, no function body |
| `NamespaceRegistry.cpp` | Register extern declarations as exported symbols (so cross-file resolution works) |

**IRGen output** for `extern int32 puts(*char s);`:
```llvm
declare i32 @puts(ptr)
```

**Call resolution**: extern functions are looked up by their original name
(no namespace mangling — they are external C symbols). The call path is:
`module_->getFunction(name)` finds the `declare` and emits a direct `call`.

**Performance**: Zero overhead — identical to what clang emits for `puts()`.

**Test**: Compile and run:
```
namespace Main;
extern int32 puts(*char s);
fn main() int32 { ret 0; }
```
Should compile and link without error (puts is in libc, always linked).

---

## Step 2: C string literals `c"..."`

**What**: Null-terminated string constants that produce `*char` (not TM `string`).
Required for calling any C function that expects `const char*`.

**Syntax**:
```
*char msg = c"Hello, World!\n";
puts(c"Hello");
printf(c"Value: %d\n", 42);
```

**Changes**:

| File | Change |
|------|--------|
| `LucisLexer.g4` | Add `C_STRING_LIT : 'c"' (~["\\\r\n] \| '\\' .)* '"';` token |
| `LucisParser.g4` | Add `C_STRING_LIT` as `# cStrLitExpr` alternative in `expression` |
| `Checker.cpp` | Add `resolveExprType` case for `cStrLitExpr` → returns `*char` (pointer to char) |
| `IRGen.cpp` | Add `visitCStrLitExpr()` — emit `@.cstr.N = private unnamed_addr constant [K x i8] c"...\00"` and return GEP to index 0 |

**IRGen output** for `c"Hello"`:
```llvm
@.cstr.0 = private unnamed_addr constant [6 x i8] c"Hello\00"
; At use site: ptr to @.cstr.0 (GEP with indices [0, 0])
```

**Test**:
```
namespace Main;
extern int32 puts(*char s);
fn main() int32 {
    puts(c"Hello from LucisM via C FFI!");
    ret 0;
}
```
Expected output: `Hello from LucisM via C FFI!`

---

## Step 3: C variadic functions (`...`)

**What**: Support C-style variadic in `extern` declarations. Required for
`printf`, `sprintf`, `scanf`, `open`, etc.

**Syntax**:
```
extern int32 printf(*char fmt, ...);
printf(c"Name: %s, Age: %d\n", c"Carlos", 30);
```

**Changes**:

| File | Change |
|------|--------|
| `LucisLexer.g4` | Add `ELLIPSIS : '...';` — wait, `SPREAD` already is `'...'`. Reuse `SPREAD` for this. |
| `LucisParser.g4` | Update `externDecl`: `EXTERN typeSpec IDENTIFIER LPAREN externParamList? (COMMA SPREAD)? RPAREN SEMI` |
| `Checker.cpp` | Track `isExternVariadic` flag. Skip param count validation for variadic extras. |
| `IRGen.cpp` | In `visitExternDecl()`, if variadic: emit `FunctionType::get(retTy, paramTys, /*isVarArg=*/true)` |
| `IRGen.cpp` | At call sites of variadic extern functions, apply C promotion rules: `float32→float64`, `int8/int16→int32`, `bool→int32` |

**IRGen output** for `extern int32 printf(*char fmt, ...);`:
```llvm
declare i32 @printf(ptr, ...)
```

**Key detail — C promotion at call site**: When calling `printf(c"%d", myInt8)`,
the `i8` value must be promoted to `i32` before the call. The IRGen inserts
`sext`/`zext` or `fpext` automatically for arguments past the fixed params.

**Test**:
```
namespace Main;
extern int32 printf(*char fmt, ...);
fn main() int32 {
    printf(c"Sum: %d + %d = %d\n", 3, 4, 7);
    printf(c"Pi: %.6f\n", 3.141592);
    ret 0;
}
```
Expected output:
```
Sum: 3 + 4 = 7
Pi: 3.141592
```

---

## Step 4: String conversion builtins (`cstr`, `fromCStr`)

**What**: Built-in functions to convert between TM `string` and C `*char`.
Without these, users can't bridge the two string worlds.

**Functions**:
| Function | TM Signature | C Implementation |
|----------|-------------|------------------|
| `cstr(s)` | `string → *char` | `malloc(len+1)`, `memcpy`, add `\0` |
| `fromCStr(p)` | `*char → string` | `strlen(p)`, return `{p, len}` |
| `fromCStrLen(p, n)` | `(*char, usize) → string` | return `{p, n}` (zero-copy) |

**Changes**:

| File | Change |
|------|--------|
| `src/builtins/string/string.h` | Add `lucis_cstr`, `lucis_from_cstr`, `lucis_from_cstr_len` declarations |
| `src/builtins/string/string.c` | Implement the three functions |
| `BuiltinRegistry.cpp` | Register `cstr`, `fromCStr`, `fromCStrLen` signatures |
| `ImportResolver.cpp` | Add to `std::str` or make them global builtins (no import needed) |
| `Checker.cpp` | Add type-checking for these functions |
| `IRGen.cpp` | Add call emission — `cstr` returns `ptr`, `fromCStr` returns `{ptr, usize}` |

**Decision — global or import?**: Make `cstr` and `fromCStr` **global builtins**
(like `exit`, `panic`, `assert`) since they're fundamental and constantly needed.
No `use` import required.

**Test**:
```
namespace Main;
extern int32 printf(*char fmt, ...);
use std::log::println;

fn main() int32 {
    string name = "World";
    *char cname = cstr(name);
    printf(c"Hello, %s!\n", cname);
    free(cname as *void);

    *char raw = c"From C to TM";
    string s = fromCStr(raw);
    println(s);

    ret 0;
}
```

---

## Step 5: `cstring` type alias

**What**: A built-in type alias `cstring = *char` for readability.
Purely syntactic sugar, zero implementation cost.

**Changes**:

| File | Change |
|------|--------|
| `LucisLexer.g4` | Add `CSTRING : 'cstring';` token |
| `LucisParser.g4` | Add `CSTRING` to `primitiveType` alternatives |
| `TypeRegistry.cpp` | Register `cstring` as `TypeKind::Pointer` with `pointeeType = char` |
| `TypeInfo.cpp` | Map `cstring` to `llvm::PointerType::getUnqual(ctx)` |

**After this**:
```
extern int32 printf(cstring fmt, ...);
cstring msg = c"Hello";
```

---

## Step 6: Fixed-size arrays `[N]T`

**What**: Declare arrays with explicit compile-time size. Required for C array
interop and safer than implicit-size `[]T`.

**Syntax**:
```
[10]int32 buffer;
[256]char name;
[3][3]float64 matrix;
```

**Changes**:

| File | Change |
|------|--------|
| `LucisParser.g4` | Add `LBRACKET INT_LIT RBRACKELucis typeSpec` to `typeSpec` rule |
| `TypeInfo.h` | Add `arraySize` field to TypeInfo (0 = unsized `[]T`, >0 = `[N]T`) |
| `Checker.cpp` | Validate array size is positive integer. Track sized arrays in type resolution. |
| `IRGen.cpp` | `resolveTypeInfo` for `[N]T` → `llvm::ArrayType::get(elemTy, N)` |
| `IRGen.cpp` | Array decay: when passing `[N]T` to a function expecting `*T`, auto-GEP to first element |

**IRGen**: `[10]int32` → `[10 x i32]`, identical to C's `int buffer[10]`.

**Array decay**: When calling `extern void fill(*int32 buf, int32 n);`
with a `[10]int32`, the compiler auto-generates a GEP to decay the array
to a pointer — same behavior as C.

**Test**:
```
namespace Main;
extern int32 puts(*char s);

fn main() int32 {
    [6]char msg = ['H', 'e', 'l', 'l', 'o', '\0'];
    puts(&msg as *char);
    ret 0;
}
```

---

## Step 7: `union` type

**What**: C-compatible union where all fields share the same memory.
Needed for interfacing with C libraries that use unions.

**Syntax**:
```
union Value {
    int32 i;
    float32 f;
    *void ptr;
}
```

**Changes**:

| File | Change |
|------|--------|
| `LucisLexer.g4` | Add `UNION : 'union';` token |
| `LucisParser.g4` | Add `unionDecl` rule (same shape as `structDecl`) |
| `LucisParser.g4` | Add `unionDecl` to `topLevelDecl` |
| `TypeInfo.h` | Add `TypeKind::Union` |
| `TypeInfo.cpp` | `toLLVMType` for Union: compute max field size, return array type of that size |
| `TypeRegistry.cpp` | Handle union registration |
| `Checker.cpp` | Add `checkUnionDecl` — validate field types, compute max size |
| `IRGen.cpp` | Add `visitUnionDecl` — create LLVM struct with single `[max_size x i8]` body |
| `IRGen.cpp` | Union field access: bitcast pointer to field type, load/store at offset 0 |
| `NamespaceRegistry.cpp` | Add Union to exported symbols |

**LLVM representation**: `%Value = type { [8 x i8] }` (8 = max of i32:4, float:4, ptr:8).
Field access: GEP to offset 0, bitcast to target type, load/store.

**Test**:
```
namespace Main;
extern int32 printf(*char fmt, ...);

union IntOrFloat {
    int32 i;
    float32 f;
}

fn main() int32 {
    IntOrFloat v = IntOrFloat { i: 42 };
    printf(c"i = %d\n", v.i);
    v.f = 3.14;
    printf(c"f = %f\n", v.f as float64);
    ret 0;
}
```

---

## Step 8: `float80` and `float128` types

**What**: Support C's `long double` (80-bit on x86) and `_Float128`.
Needed for full C type compatibility.

**Changes**:

| File | Change |
|------|--------|
| `LucisLexer.g4` | Add `FLOAT80 : 'float80';` and `FLOAT128 : 'float128';` tokens |
| `LucisParser.g4` | Add `FLOAT80` and `FLOAT128` to `primitiveType` |
| `TypeRegistry.cpp` | Register `float80` (bitWidth=80) and `float128` (bitWidth=128) as `TypeKind::Float` |
| `TypeInfo.cpp` | In `toLLVMType`: bitWidth==80 → `x86_fp80`, bitWidth==128 → `fp128` |
| `IRGen.cpp` | Add cast support: `fpext`/`fptrunc` between float32/64/80/128 |

**Test**:
```
namespace Main;
extern int32 printf(*char fmt, ...);

fn main() int32 {
    float80 x = 3.14159265358979323846;
    printf(c"x = %Lf\n", x);
    ret 0;
}
```

---

## Step 9: `-l`, `-L`, `-I` CLI flags

**What**: Pass linker and include flags through the CLI so users can link
against any C library.

**Syntax**:
```bash
lucis main.lc ./main -lm -lSDL2 -L/usr/local/lib -I/usr/local/include
```

**Changes**:

| File | Change |
|------|--------|
| `CLI.h` | Add `linkerFlags`, `libPaths`, `includePaths` to `CLIOptions` |
| `CLI.cpp` | Parse `-l*`, `-L*`, `-I*` flags in `parse()` |
| `CodeGen.h` | Update `linkObjectFiles` to accept extra linker flags |
| `CodeGen.cpp` | Append `-l`, `-L` flags to the clang/gcc link command |

**Implementation in CodeGen**: The `tryLinkMulti()` argv builder appends each
`-l` and `-L` flag from the CLI options directly into the linker command.
No processing needed — clang/gcc will handle them.

---

## Step 10: `#include` + CHeaderResolver (libclang)

**What**: Automatically parse C headers and make all declarations available
in TM without manual `extern` declarations.

**Syntax**:
```
#include <stdio.h>
#include <math.h>
#include "mylib.h"
```

**Changes**:

| File | Change |
|------|--------|
| `LucisLexer.g4` | Add `HASH_INCLUDE` token and header path fragments |
| `LucisParser.g4` | Add `includeDecl` to `program` rule |
| `CMakeLists.txt` | Add `find_package(Clang)` / link `libclang` |
| `src/ffi/CHeaderResolver.h` | New — parse C headers via `clang_parseTranslationUnit()` |
| `src/ffi/CHeaderResolver.cpp` | New — walk libclang AST, extract functions/structs/enums/typedefs/macros |
| `src/ffi/CTypeMapper.h` | New — map libclang `CXType` → TM `TypeInfo` |
| `src/ffi/CTypeMapper.cpp` | New — complete mapping table (section 5.7 of complete_ffi.md) |
| `src/ffi/CBindings.h` | New — registry of imported C symbols |
| `src/ffi/CBindings.cpp` | New — lookup functions, type validation |
| `CLI.cpp` | After parsing, before checking: resolve all `#include` headers |
| `Checker.cpp` | Query CBindings for function/struct/enum validation |
| `IRGen.cpp` | Emit `declare` for each C function actually called |

**CHeaderResolver flow**:
1. Receive header path + include search paths (from `-I` flags)
2. Call `clang_parseTranslationUnit()` with the header
3. Walk the AST via `clang_visitChildren()`:
   - `CXCursor_FunctionDecl` → extract name, return type, params, variadic
   - `CXCursor_StructDecl` → extract name, fields
   - `CXCursor_EnumDecl` → extract name, values
   - `CXCursor_TypedefDecl` → extract alias → target
   - `CXCursor_MacroDefinition` → extract simple integer/string constants
4. Map each C type to TM type via CTypeMapper
5. Store in CBindings registry

**Lazy declaration**: IRGen only emits `declare` for C functions that are
actually called in the TM code — not for every function in the header.
This keeps the LLVM module small and compilation fast.

**Struct auto-import**: When a C function returns `struct timeval*`, the
resolver automatically imports `struct timeval` as a TM struct with mapped
field types. Available by name: `timeval`.

**Performance**: libclang parses headers once per compilation. The parsed
result is cached in the CBindings registry. All lookups are O(1) hash maps.
No runtime cost — everything resolved at compile time.

**Test**:
```
namespace Main;
#include <stdio.h>
#include <math.h>

fn main() int32 {
    printf(c"sqrt(2) = %f\n", sqrt(2.0));
    printf(c"sin(PI/2) = %f\n", sin(1.5707963));
    
    *void buf = malloc(100);
    sprintf(buf as *char, c"Allocated %d bytes", 100);
    puts(buf as *char);
    free(buf);
    
    ret 0;
}
```

---

## Execution Order Summary

| Step | Feature | Depends On | Estimated Complexity |
|------|---------|------------|---------------------|
| 1 | `extern` declarations | — | Medium |
| 2 | `c"..."` string literals | — | Small |
| 3 | C variadic `...` | Step 1 | Medium |
| 4 | `cstr`/`fromCStr` builtins | — | Small |
| 5 | `cstring` type alias | — | Trivial |
| 6 | `[N]T` fixed-size arrays | — | Medium |
| 7 | `union` type | — | Medium-Large |
| 8 | `float80`/`float128` | — | Small |
| 9 | `-l`/`-L`/`-I` CLI flags | — | Small |
| 10 | `#include` + CHeaderResolver | Steps 1-9 | Large |

Steps 1-3 are the critical path. After Step 3, users can call any C function
using manual `extern` declarations. Steps 4-9 add convenience and completeness.
Step 10 is the crown jewel that makes it all automatic.

---

## Performance Design

**Zero-cost call path for extern functions**:
```
TM source:  puts(c"Hello");
LLVM IR:    call i32 @puts(ptr @.cstr.0)
Machine:    callq puts@PLT
```
Identical to what clang would emit. No wrappers, no thunks, no indirection.

**String conversion cost**:
- `cstr(s)` — one `malloc` + one `memcpy` + null byte. O(n) where n = string length.
- `fromCStr(p)` — one `strlen`. O(n).
- `fromCStrLen(p, n)` — zero cost. Just wraps the pointer.

**Avoid in hot loops**: If calling printf in a loop, compute `cstr()` outside
the loop and reuse the pointer. The compiler does NOT auto-hoist malloc calls.

**Header parsing cost**: libclang parses `<stdio.h>` in ~5ms on a modern machine.
Headers are parsed once per compilation, not per file. For a project with 10 files
all including `<stdio.h>`, it's parsed only once.

---

## Error Messages

Each step must produce clear error messages:

| Scenario | Message |
|----------|---------|
| Calling undeclared extern | `undeclared function 'printf'. Use 'extern' or '#include <stdio.h>'` |
| Passing `string` to `*char` param | `cannot pass 'string' as '*char'. Use 'cstr(s)' to convert` |
| Passing `*char` to `string` param | `cannot pass '*char' as 'string'. Use 'fromCStr(p)' to convert` |
| Missing `-l` flag | Linker error from clang/gcc (passed through as-is) |
| Header not found | `cannot find header 'xyz.h'. Check '-I' include paths` |
| Union field access on wrong variant | No runtime check (like C — undefined behavior by design) |
