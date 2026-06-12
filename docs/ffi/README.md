# FFI — Foreign Function Interface

Lux provides native C interoperability through a zero-cost FFI system.
C functions can be called directly from LuxM code with no wrappers, thunks,
or runtime overhead — the generated machine code is identical to what a
C compiler would produce.

## Table of Contents

- [extern declarations](#extern-declarations)
- [C string literals](#c-string-literals)
- [C variadic functions](#c-variadic-functions)
- [String conversion builtins](#string-conversion-builtins)
- [cstring type alias](#cstring-type-alias)
- [Fixed-size arrays](#fixed-size-arrays)
- [Union type](#union-type)
- [Extended float types](#extended-float-types)
- [#include directives](#include-directives)
- [Local includes with auto-compile](#local-includes-with-auto-compile)
- [Pointer arithmetic](#pointer-arithmetic)
- [#define macro constants](#define-macro-constants)
- [C global variables](#c-global-variables)
- [Callbacks](#callbacks)
- [Struct by-value ABI](#struct-by-value-abi)
- [Enum type compatibility](#enum-type-compatibility)
- [CLI linker flags](#cli-linker-flags)
- [Type mapping](#type-mapping)
- [Linking C libraries](#linking-c-libraries)

---

## extern declarations

Use `extern` to declare C functions without a body. The compiler generates
a direct `declare` in LLVM IR, and the linker resolves it from libc or
any linked library.

```
extern int32 puts(*char s);
extern float64 sqrt(float64 x);
extern *void malloc(usize size);
extern void free(*void ptr);
```

### Rules

- Extern functions use their **exact C name** — no namespace mangling.
- The return type and parameter types must match the C function's ABI.
- Parameter names are optional: `extern int32 abs(int32);` is valid.
- Extern declarations are file-scoped: each file that uses a C function
  must declare it (or use `#include` when available).

### Generated LLVM IR

```
extern int32 puts(*char s);
```

Produces:

```llvm
declare i32 @puts(ptr)
```

Identical to what `clang` generates for the same declaration.

---

## C string literals

Use `c"..."` to create null-terminated C string constants. These produce
a `*char` (pointer to char) — not a TM `string`.

```
puts(c"Hello, World!");
printf(c"Value: %d\n", 42);

*char msg = c"Static C string";
```

### Differences from LuxM strings

| Feature | TM `string` | C `c"..."` |
|---------|-------------|------------|
| Type | `string` (`{ptr, len}` struct) | `*char` (raw pointer) |
| Null-terminated | No | Yes |
| Length tracking | Built-in `.len` | Must use `strlen()` |
| Use case | TM standard library | C function arguments |

### Escape sequences

C string literals support the same escape sequences as regular strings:

| Escape | Character |
|--------|-----------|
| `\n` | Newline |
| `\t` | Tab |
| `\r` | Carriage return |
| `\\` | Backslash |
| `\"` | Double quote |
| `\0` | Null byte |
| `\xHH` | Hex byte |

---

## C variadic functions

C-style variadic functions are declared with `...` after the fixed parameters:

```
extern int32 printf(*char fmt, ...);
extern int32 sprintf(*char buf, *char fmt, ...);
extern int32 scanf(*char fmt, ...);
```

### Argument promotion

When passing arguments to the variadic portion of a C function, the compiler
automatically applies **C default argument promotions**:

| Lux type | Promoted to | Reason |
|---------|------------|--------|
| `float32` | `float64` | C promotes `float` → `double` in varargs |
| `int8`, `int16` | `int32` | C promotes small ints → `int` in varargs |
| `bool` | `int32` | `_Bool` → `int` in C |

Fixed parameters (before `...`) are **not** promoted — they are passed
with exact type matching.

### Example

```
extern int32 printf(*char fmt, ...);

fn main() int32 {
    printf(c"Integer: %d\n", 42);
    printf(c"Float: %f\n", 3.14);
    printf(c"Multiple: %s is %d years old\n", c"Alice", 30);
    printf(c"Hex: 0x%x\n", 255);
    ret 0;
}
```

---

## String conversion builtins

Four global builtins handle conversion between TM `string` and C `*char`.
They are always available — no `use` import needed.

### `cstr(s)` — string → *char

Allocates a new null-terminated C string from a TM string. The caller
must `free()` the returned pointer when done.

```
string name = "Lux";
*char cname = cstr(name);
puts(cname);
free(cname as *void);
```

- **Cost**: `malloc(len + 1)` + `memcpy` + null terminator. O(n).
- **Ownership**: The caller owns the returned pointer.

### `fromCStr(p)` — *char → string

Creates a Lux string from a null-terminated C string. Computes the length
via `strlen()`.

```
*char raw = c"Hello from C";
string s = fromCStr(raw);
// s.len == 12
```

- **Cost**: One `strlen()` call. O(n).
- **Ownership**: The Lux string wraps the original pointer (zero-copy).

### `fromCStrCopy(p)` — *char → string (owned copy)

Creates a Lux string by copying a null-terminated C string into newly
allocated memory owned by Lux.

```
*char raw = c"Hello from C";
string s = fromCStrCopy(raw);
// s remains valid even if raw is later freed
```

- **Cost**: `strlen()` + `malloc(len + 1)` + `memcpy`. O(n).
- **Ownership**: The returned Lux string owns its copied buffer.

### `fromCStrLen(p, len)` — *char + length → string

Creates a Lux string from a pointer and explicit length. Zero-cost — no
memory allocation or scanning.

```
string s = fromCStrLen(c"Hello World!!!!", 5);
// s contains "Hello" (length 5)
```

- **Cost**: Zero. Just wraps the pointer and length.

### `freeStr(s)` — free owned Lux string buffer

Frees the backing memory of a Lux string previously created by
`fromCStrCopy()`.

```
*char raw = c"Hello from C";
string s = fromCStrCopy(raw);
freeStr(s);
```

- **Cost**: One `free()` call. O(1).
- **Ownership**: Only valid for strings that own heap memory from `fromCStrCopy()`.

---

## cstring type alias

`cstring` is a built-in type alias for `*char`. It makes FFI code more
readable when working with C strings.

```
cstring greeting = c"Hello!";
puts(greeting);

cstring name = cstr("Lux");
puts(name);
free(name as *void);
```

`cstring` is fully interchangeable with `*char` — they are the same type
at the LLVM level.

---

## Fixed-size arrays

`[N]T` declares a fixed-size array of N elements of type T. These map
directly to LLVM's `[N x T]` array type.

```
[5]int32 nums = [10, 20, 30, 40, 50];
[3]float64 coords = [1.0, 2.5, 3.7];
[6]char hello = ['H', 'e', 'l', 'l', 'o', '\0'];
```

### Array decay

When passing a `[N]T` array to a function expecting a pointer (`*T`),
the array automatically decays to a pointer to its first element — just
like in C.

```
extern int32 puts(*char s);

[6]char msg = ['H', 'e', 'l', 'l', 'o', '\0'];
puts(&msg as *char);  // array decays to *char
```

### LLVM representation

```
[5]int32 nums = [10, 20, 30, 40, 50];
```

Produces:

```llvm
%nums = alloca [5 x i32]
```

---

## Union type

A `union` declares a type where all fields share the same memory region.
The union occupies the size of its largest field — identical to C unions.

```
union IntOrFloat {
    int32 i;
    float32 f;
}

fn main() int32 {
    IntOrFloat u = IntOrFloat { i: 42 };
    printf(c"i = %d\n", u.i);

    u.f = 3.14 as float32;
    printf(c"f = %f\n", u.f as float64);

    // After writing to .f, reading .i gives the raw bit pattern
    // (undefined behavior by design, same as C)
    ret 0;
}
```

### Rules

- Union literals initialize **exactly one** field: `Name { field: value }`.
- Field access uses dot syntax: `u.i`, `u.f`.
- Field assignment uses dot syntax: `u.f = 3.14`.
- Arrow access works with pointers to unions: `ptr->field`.
- Reading a field that wasn't the last one written is **undefined behavior**
  (same semantics as C — no runtime check).

### LLVM representation

A union with fields `int32 i` and `float32 f` (both 4 bytes) becomes:

```llvm
%IntOrFloat = type { [4 x i8] }
```

A union with a pointer field (8 bytes on 64-bit) becomes:

```llvm
%Value = type { [8 x i8] }
```

Field access uses the alloca pointer directly with the target field type —
opaque pointers in LLVM make this straightforward.

---

## Extended float types

`float80` and `float128` provide extended precision floating-point types
for C interoperability.

| Lux type | C equivalent | LLVM type | Precision |
|---------|-------------|-----------|-----------|
| `float80` | `long double` (x86) | `x86_fp80` | ~18-19 decimal digits |
| `float128` | `_Float128` / `__float128` | `fp128` | ~33-36 decimal digits |

### Usage

```
float80 x = 3.14159265358979323846 as float80;
float128 y = 2.71828182845904523536 as float128;

// Cast to float64 for printf
printf(c"x = %.15f\n", x as float64);
printf(c"y = %.15f\n", y as float64);
```

### Cast rules

All float types can be cast between each other using `as`:

```
float32 a = 1.5 as float32;
float80 b = a as float80;    // fpext: no precision loss
float64 c = b as float64;    // fptrunc: may lose precision
float128 d = c as float128;  // fpext: no precision loss
```

### Platform notes

- `float80` maps to x86 extended precision (`x86_fp80`). Available on
  x86/x86-64 only. On ARM/RISC-V it may not be hardware-supported.
- `float128` maps to IEEE 754 quad precision (`fp128`). Software-emulated
  on most platforms — slower than hardware float types.

---

## #include directives

Use `#include` to import C header files directly into Lux code. The compiler
parses the header with libclang at compile time and extracts all functions,
structs, enums, and typedefs — making them available as if they were
native Lux declarations.

```
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

fn main() int32 {
    printf(c"sqrt(144) = %f\n", sqrt(144.0));
    *void buf = malloc(64);
    memset(buf, 65, 3);
    printf(c"%c%c%c\n", *(buf as *char), *(buf as *char), *(buf as *char));
    free(buf);
    ret 0;
}
```

### System headers vs local headers

| Syntax | Search path | Example |
|--------|------------|--------|
| `#include <header.h>` | System include directories | `#include <stdio.h>` |
| `#include "header.h"` | Current file directory, then system | `#include "mylib.h"` |

System include directories are auto-discovered:
- `/usr/include`
- `/usr/local/include`
- `/usr/lib/clang/<version>/include`

### What gets extracted

| C declaration | TM availability | Example |
|---------------|----------------|--------|
| Functions | Callable directly | `printf(c"hi\n");` |
| Structs | Usable as types with field access | `timespec ts;` |
| Enums | Constants usable as `int32` values | `int32 v = SEEK_SET;` |
| Typedefs | Usable as type names | `size_t len = 42;` |

### Lazy function declaration

C functions from `#include` headers are declared lazily — the LLVM
`declare` is only emitted when the function is actually called. This
keeps the generated IR clean even when including large headers like
`<stdlib.h>` that export hundreds of functions.

### Struct extraction

C structs are fully mapped to Lux struct types with all their fields.
Field types are recursively resolved, including nested structs and
pointer-to-struct types.

```
#include <time.h>

fn main() int32 {
    timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 0;
    ret 0;
}
```

Circular struct references (e.g. a linked list node with `*Node next`)
are handled correctly via a skeleton-first registration pattern.

### Enum extraction

C enum constants are extracted as global `int32` values. They can be
used directly in expressions without any prefix or qualification.

```
#include <stdio.h>

fn main() int32 {
    // SEEK_SET, SEEK_CUR, SEEK_END from <stdio.h>
    int32 origin = SEEK_SET;
    printf(c"SEEK_SET = %d\n", origin);
    ret 0;
}
```

### Typedef extraction

C typedefs are registered as type aliases. The underlying type is
fully resolved — `size_t` becomes `uint64` (on 64-bit), `time_t`
becomes `int64`, etc.

### Differences from `extern`

| Feature | `extern` | `#include` |
|---------|---------|------------|
| Scope | Single function | Entire header |
| Types | Must be written manually | Auto-extracted |
| Structs | Not supported | Fully supported |
| Enums | Not supported | Fully supported |
| Typedefs | Not supported | Fully supported |
| Overhead | None (compile-time) | Slight (libclang parse) |

Use `extern` for quick one-off C function calls. Use `#include` when
you need access to structs, enums, or many functions from a C library.

---

## Local includes with auto-compile

When you use `#include "header.h"` with a local header, the compiler
automatically looks for a matching `.c` source file in the same
directory. If found, it compiles the C source into an object file
inside `.lux/build/` and links it into the final binary — no manual
compilation step required.

```
#include <stdio.h>
#include "mymath.h"

fn main() int32 {
    int32 sum = add(10, 20);
    printf(c"sum = %d\n", sum);
    ret 0;
}
```

With `mymath.h`:

```c
#ifndef MYMATH_H
#define MYMATH_H

#define PI_APPROX 314159

int add(int a, int b);
int multiply(int a, int b);

#endif
```

And `mymath.c`:

```c
#include "mymath.h"

int add(int a, int b) {
    return a + b;
}

int multiply(int a, int b) {
    return a * b;
}
```

Running `lux main.lx ./main` will:

1. Parse `mymath.h` via libclang to extract `add`, `multiply`, and `PI_APPROX`
2. Find `mymath.c` in the same directory as `mymath.h`
3. Compile `mymath.c` → `.lux/build/c__mymath.o` using the system C compiler
4. Link `c__mymath.o` together with the TM object file into the final binary

### Build directory layout

```
project/
├── main.lx
├── mymath.h
├── mymath.c
└── .lux/
    ├── build/
    │   ├── MyNamespace__main.o    # compiled TM code
    │   └── c__mymath.o            # auto-compiled C code
    └── cache/
```

### How it works

| Step | Action |
|------|--------|
| Header resolution | `#include "mymath.h"` → libclang parses with `-I<source dir>` |
| Source discovery | Replaces `.h` → `.c` and checks if the file exists |
| Compilation | Invokes `cc`, `clang`, or `gcc` (first available) |
| Deduplication | Each `.c` file is compiled only once, even if included from multiple `.lx` files |
| Linking | Compiled `.o` files are appended to the linker command automatically |

### Notes

- The `.c` file must be in the **same directory** as the `.h` file.
- If no `.c` file exists (header-only library), nothing extra is compiled.
- All extracted declarations (functions, structs, enums, typedefs, macros)
  work identically to system `#include` headers.
- Additional `-I` paths passed via CLI are forwarded to the C compiler.

---

## Pointer arithmetic

Pointer arithmetic works the same as in C. Adding an integer to a pointer
advances it by that many elements (scaled by the element size). Subtracting
two pointers yields the number of elements between them.

### Supported operations

| Expression | Result type | Description |
|-----------|------------|-------------|
| `ptr + n` | `*T` | Advance pointer by `n` elements |
| `n + ptr` | `*T` | Same as above (commutative) |
| `ptr - n` | `*T` | Move pointer back by `n` elements |
| `ptr - ptr` | `int64` | Element count between two pointers |

### Example

```
extern *void malloc(usize size);
extern void free(*void ptr);
extern int32 printf(*char fmt, ...);

fn main() int32 {
    // Allocate space for 5 int32 values
    *int32 buf = malloc(20) as *int32;

    // Write values using pointer arithmetic
    *buf = 10;
    *int32 second = buf + 1;
    *second = 20;
    *int32 third = buf + 2;
    *third = 30;

    // Read values
    printf(c"%d %d %d\n", *buf, *second, *third);

    free(buf as *void);
    ret 0;
}
```

Output: `10 20 30`

### Stride

The compiler automatically scales pointer offsets by the size of the
pointee type. `buf + 1` where `buf` is `*int32` advances by 4 bytes
(the size of `int32`), not 1 byte.

If the pointee type cannot be resolved, the compiler falls back to
byte-level arithmetic (stride of 1).

---

## #define macro constants

Integer `#define` constants from C headers are automatically extracted
and available as `int32` values in Lux code. This covers the vast majority
of C API constants: error codes, flags, sizes, and sentinel values.

```
#include <stdio.h>
#include <stdlib.h>

fn main() int32 {
    int32 eof = EOF;             // -1
    int32 bs  = BUFSIZ;          // 8192
    int32 ss  = SEEK_SET;        // 0
    int32 es  = EXIT_SUCCESS;    // 0
    int32 ef  = EXIT_FAILURE;    // 1

    printf(c"EOF=%d BUFSIZ=%d SEEK_SET=%d\n", eof, bs, ss);
    ret es;
}
```

### What gets extracted

The compiler evaluates simple integer constant expressions in macro
bodies, including:

| Pattern | Example |
|---------|---------|
| Integer literals | `#define FOO 42` |
| Hex/octal literals | `#define MASK 0xFF` |
| Character literals | `#define NUL '\0'` |
| Negation | `#define ERR (-1)` |
| Bitwise operators | `#define FLAG (1 << 3)` |
| Bitwise OR/AND | `#define MODE (O_RDWR \| O_CREAT)` |
| Type casts | `#define NULL ((void*)0)` |

### What is skipped

- **Function-like macros**: `#define MAX(a,b) ...` — skipped (not constants)
- **String macros**: `#define VERSION "1.0"` — skipped (not integer)
- **Internal macros**: Names starting with `_` are skipped
- **Complex expressions**: Macros referencing other macros or function calls

### NULL

`NULL` is detected as a special case (`(void*)0` pattern) and treated
as a zero-valued integer constant.

---

## C global variables

Extern global variables from C headers are automatically extracted and
accessible as regular variables in TM code. The compiler declares them
as LLVM `external global` symbols, resolved by the linker.

```
#include <stdio.h>

fn main() int32 {
    // stdout, stderr are extern FILE* globals
    fprintf(stdout, c"Hello to stdout\n");
    fprintf(stderr, c"Hello to stderr\n");
    fputs(c"Another line\n", stdout);
    ret 0;
}
```

### Supported operations

| Operation | Example | Description |
|-----------|---------|-------------|
| Read | `fprintf(stdout, ...)` | Load the global and pass it |
| Assign | `myGlobal = 42;` | Store a new value to the global |

### What gets extracted

Only variables with **external linkage** (`extern`) are extracted.
Static, file-local, or internal variables are skipped. Names starting
with `_` are also skipped to avoid compiler internals.

### Common globals

| Variable | Header | Type | Description |
|----------|--------|------|-------------|
| `stdin` | `<stdio.h>` | `*FILE` | Standard input stream |
| `stdout` | `<stdio.h>` | `*FILE` | Standard output stream |
| `stderr` | `<stdio.h>` | `*FILE` | Standard error stream |

### Platform note

On some platforms (e.g. glibc), `errno` is a macro that expands to
`(*__errno_location())`, not a true global variable. Such macros are
not captured as globals. Use the `errno` function wrapper or access
it through C helper functions if needed.

---

## Callbacks

Lux functions can be passed directly as C function pointer arguments.
This enables use of C APIs that accept callbacks: `qsort`, `bsearch`,
`atexit`, `signal`, `pthread_create`, etc.

```
#include <stdlib.h>
#include <stdio.h>

fn compareInts(*void a, *void b) int32 {
    *int32 ia = a as *int32;
    *int32 ib = b as *int32;
    ret *ia - *ib;
}

fn main() int32 {
    *void buf = malloc(20 as usize);
    *int32 arr = buf as *int32;
    *arr = 5;
    *(arr + 1) = 3;
    *(arr + 2) = 1;

    qsort(buf, 3 as usize, 4 as usize, compareInts);
    printf(c"%d %d %d\n", *arr, *(arr+1), *(arr+2));
    // Output: 1 3 5

    free(buf);
    ret 0;
}
```

### How it works

When a function name is used as an expression argument, it evaluates
to the function's address (an LLVM `ptr`). Since LLVM uses opaque
pointers, no type conversion is needed — the pointer is passed directly
to the C function.

### Function pointer type compatibility

The checker treats function types and function pointer types as
compatible. If a C function expects `int (*)(const void*, const void*)`
(mapped to `*fn(*void,*void)->int32`), passing a TM function with
matching signature works without casts.

### Common use cases

| C function | Callback signature | Purpose |
|------------|-------------------|----------|
| `qsort` | `int32 cmp(*void, *void)` | Sort comparison |
| `bsearch` | `int32 cmp(*void, *void)` | Binary search comparison |
| `atexit` | `void cleanup()` | Exit handler |
| `signal` | `void handler(int32)` | Signal handler |

### Rules

- The callback function must be defined at **module level** (not inside
  another function).
- The callback's signature must match what the C function expects at
  the ABI level.
- Namespace mangling is handled transparently — passing `compareInts`
  resolves to the mangled LLVM name automatically.

---

## Struct by-value ABI

C structs can be passed to and returned from C functions **by value**.
The compiler implements x86-64 System V ABI classification to ensure
correct register/stack usage when calling C code.

```
#include <stdio.h>
#include "structs.h"

fn main() int32 {
    Point p = make_point(10, 20);
    int32 s = point_sum(p);
    printf(c"sum = %d\n", s);  // 30

    Vec4 v = make_vec4(1, 2, 3, 4);
    int64 total = vec4_sum(v);
    printf(c"total = %ld\n", total);  // 10
    ret 0;
}
```

### ABI classification

The compiler classifies struct arguments and return values according
to their size, matching what Clang/GCC produce:

| Size | ABI class | Coercion | Example |
|------|-----------|----------|---------|
| ≤ 8 bytes | INTEGER (1 reg) | Struct → `iN` | `Point {i32,i32}` → `i64` |
| 9–16 bytes | INTEGER (2 regs) | Struct → `{i64, iM}` | `Pair64 {i64,i64}` → `{i64, i64}` |
| > 16 bytes | MEMORY | `sret` (return) / `byval` (param) | `Vec4 {i64,i64,i64,i64}` → pointer |

### How it works

1. **Declaration**: When a C function with struct params/return is first
   called, `declareCFunction` classifies each type using `ABIClassifier`
   and generates the coerced LLVM function signature.

2. **Call site — arguments**: Struct values are stored to a temporary
   alloca, then loaded as the coerced integer type(s) before being passed.

3. **Call site — return**: The coerced return value (e.g. `i64`) is
   stored to a temporary alloca and loaded back as the original struct type.

4. **Large structs (sret)**: A hidden first parameter (pointer to caller-
   allocated space) is added. The callee writes the return value there.

### Nested structs

Nested structs are classified by their **total size**, not by their
nesting structure. A `Rect { Point origin; Point size; }` is 16 bytes
and uses 2-register passing, same as `Pair64`.

### Notes

- Classification follows the **x86-64 System V** ABI (Linux/macOS).
- Only struct types trigger ABI coercion — scalars and pointers pass
  through unchanged.
- The coercion is transparent to TM code: you write `Point p = make_point(10, 20);`
  and the compiler handles all the register packing behind the scenes.

---

## Enum type compatibility

C enum types are fully supported as function parameter and return types.
When a C function uses an enum type (e.g. `Color`), Lux maps it to
the underlying integer type (typically `uint32` on most platforms) and
allows implicit conversion between enum constants and integer variables.

```
#include <stdio.h>
#include "colors.h"

fn main() int32 {
    // Enum constants are usable directly
    int32 r = COLOR_RED;     // 0
    int32 g = COLOR_GREEN;   // 1

    // Pass enum to C function that takes Color parameter
    *char name = color_name(COLOR_GREEN);
    printf(c"color: %s\n", name);  // "green"

    // Receive enum from C function return
    int32 next = next_color(COLOR_BLUE);
    printf(c"next: %d\n", next);  // 0 (RED)
    ret 0;
}
```

### How it works

| Step | Action |
|------|--------|
| Header parsing | `CXType_Enum` is resolved to the underlying integer type via `clang_getEnumDeclIntegerType` |
| Constants | Enum values are extracted as named integer constants (same as `#define`) |
| Type checking | `Enum ↔ Integer` assignment is allowed implicitly in both directions |
| Code generation | Enum values compile to plain integer constants — zero overhead |

### Notes

- Enum constants are available by their C name without any prefix or
  qualification: `COLOR_RED`, not `Color::COLOR_RED`.
- The underlying integer type is determined by the C compiler (usually
  `unsigned int` → `uint32`), not hardcoded.
- Both `typedef enum { ... } Name;` and `enum Name { ... };` styles
  are supported.

---

## CLI linker flags

When linking against external C libraries, use the following flags on
the `lux` command line:

| Flag | Description | Example |
|------|-------------|---------|
| `-lname` | Link against library `name` | `-lSDL2` |
| `-Lpath` | Add `path` to the library search path | `-L/opt/lib` |
| `-Ipath` | Add `path` to include search path (reserved for future use) | `-I/opt/include` |

### Usage

```bash
# Link against SDL2 installed in a custom location
lux build main.lx -o ./game -lSDL2 -L/opt/sdl2/lib

# Link against multiple libraries
lux build main.lx -o ./app -lcurl -lssl -lcrypto

# Default libraries (-lm, -lz, -lpthread) are always linked automatically
lux build main.lx -o ./main
```

### Notes

- `-l` and `-L` flags are appended directly to the clang/gcc linker
  command — no wrapping or translation.
- The compiler always links `-lm`, `-lz`, and `-lpthread` by default.
  You do not need to specify these manually.
- `-I` is reserved for the upcoming `#include` directive (Step 10).
  Currently it is accepted and stored but not used.

---

## Type mapping

| TM type | C equivalent | LLVM IR |
|---------|-------------|---------|
| `int8` | `int8_t` / `char` | `i8` |
| `int16` | `int16_t` / `short` | `i16` |
| `int32` | `int32_t` / `int` | `i32` |
| `int64` | `int64_t` / `long long` | `i64` |
| `usize` | `size_t` | `i64` (on 64-bit) |
| `isize` | `ssize_t` / `ptrdiff_t` | `i64` (on 64-bit) |
| `float32` | `float` | `float` |
| `float64` | `double` | `double` |
| `float80` | `long double` (x86) | `x86_fp80` |
| `float128` | `_Float128` / `__float128` | `fp128` |
| `bool` | `_Bool` | `i1` |
| `char` | `char` | `i8` |
| `void` | `void` | `void` |
| `*T` | `T*` | `ptr` |
| `*void` | `void*` | `ptr` |
| `*char` | `char*` / `const char*` | `ptr` |
| `cstring` | `char*` (alias for `*char`) | `ptr` |
| `[N]T` | `T[N]` | `[N x T]` |
| `union U` | `union U` | `{ [max_size x i8] }` |

### Pointer types

All pointer types in TM map to LLVM's opaque `ptr` type. The type after
`*` tells the TM checker what the pointer points to, but at the LLVM
level all pointers are interchangeable (opaque pointer semantics).

```
*int32 p;     // int32_t*
*void  raw;   // void*
*char  str;   // char*
**int32 pp;   // int32_t**
```

---

## Linking C libraries

Functions from `libc` (like `puts`, `printf`, `malloc`, `free`) are always
available — the compiler links against libc by default.

For math functions (`sqrt`, `sin`, `cos`, etc.), the compiler links with
`-lm` by default, so no extra flags are needed.

### Currently auto-linked

| Library | Flag | Functions |
|---------|------|-----------|
| libc | (always) | `puts`, `printf`, `malloc`, `free`, `memset`, etc. |
| libm | `-lm` | `sqrt`, `sin`, `cos`, `pow`, `log`, etc. |
| libz | `-lz` | `compress`, `uncompress`, etc. |
| libpthread | `-lpthread` | `pthread_*` functions |

---

## Performance

All extern function calls are **zero-cost**. The generated code is
identical to what a C compiler would produce:

```
Lux source:     puts(c"Hello");
LLVM IR:        call i32 @puts(ptr @.cstr.0)
x86-64 asm:     callq puts@PLT
```

No wrappers. No thunks. No indirection. Calling a C function from Lux
is exactly as fast as calling it from C.
