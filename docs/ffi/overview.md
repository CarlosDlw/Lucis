# FFI Overview

T provides zero-cost interoperability with C. You can call any C function, use C structs, enums, and macros directly from Lucis code — without wrappers, bindings generators, or runtime overhead. The compiled output uses the exact same calling conventions and memory layout as C.

---

## Two Ways to Declare C Functions

T offers two mechanisms for accessing C functions, and you can mix them in the same file:

### 1. Manual `extern` Declarations

You write the function signature yourself. This is useful for one-off functions or when you don't have a header file:

```
extern int32 puts(*char s);
extern float64 sqrt(float64 x);

fn main() int32 {
    puts(c"Hello from C!");
    float64 val = sqrt(144.0);
    ret 0;
}
```

### 2. `#include` Directives

You include a C header file, and the compiler **automatically parses it** using libclang, extracting all functions, structs, enums, and macro constants:

```
#include <stdio.h>
#include <math.h>

fn main() int32 {
    printf(c"sqrt(144) = %.0f\n", sqrt(144.0));
    ret 0;
}
```

With `#include`, you don't need to write any `extern` declarations — the compiler reads the header and registers everything for you.

---

## What Gets Extracted from Headers

When you use `#include`, the compiler parses the C header with libclang and extracts:

| C Declaration | Available in Lucis as |
|---------------|-------------------|
| Function declarations | Callable functions (same as `extern`) |
| `struct` definitions | Usable struct types with all fields |
| `enum` definitions | Integer constants (e.g., `COLOR_RED`) |
| `#define` constants | Integer constants (numeric macros only) |
| `extern` global variables | Accessible global symbols |

Function-like macros (`#define FOO(x) ...`) and non-numeric macros are ignored.

---

## C String Literals

C functions expect null-terminated strings. Lucis provides the `c"..."` literal syntax for this:

```
extern int32 puts(*char s);

puts(c"Hello, World!");    // null-terminated, type is *char
```

Regular Lucis strings (`"..."`) are **not** null-terminated — they are length-prefixed. To convert between the two, use `cstr()`, `fromCStr()`, and `fromCStrLen()`.

---

## Type Correspondence

Lucis types map directly to C types with no conversion:

| C Type | T Type |
|--------|--------|
| `char` | `char` |
| `signed char` | `char` |
| `unsigned char` | `uint8` |
| `short` | `int16` |
| `unsigned short` | `uint16` |
| `int` | `int32` |
| `unsigned int` | `uint32` |
| `long` | `int64` |
| `unsigned long` | `uint64` |
| `float` | `float32` |
| `double` | `float64` |
| `long double` | `float80` |
| `void` | `void` |
| `void*` | `*void` |
| `char*` | `*char` (or `cstring`) |
| `T*` | `*T` |
| `size_t` | `usize` |

---

## Complete Example

This example demonstrates all core FFI features in a single file:

```
namespace FFIDemo;

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

fn main() int32 {
    // Call C functions directly from headers
    printf(c"sqrt(144) = %.0f\n", sqrt(144.0));
    printf(c"sin(pi/2) = %.6f\n", sin(1.5707963267948966));

    // Memory allocation via C stdlib
    *void buf = malloc(64 as usize);
    memset(buf, 65, 10 as usize);       // fill with 'A'
    free(buf);

    // String comparison
    int32 cmp = strcmp(c"hello", c"hello");
    printf(c"strcmp result: %d\n", cmp);  // 0

    // String length
    usize len = strlen(c"hello");
    printf(c"strlen: %d\n", len as int32);  // 5

    ret 0;
}
```

---

## FFI Topics

| Page | Description |
|------|-------------|
| [Calling C Functions](calling-c.md) | `extern` declarations, `#include` directives, variadic functions, local headers |
| [C Strings](c-strings.md) | `c"..."` literals, `cstring` type, `cstr()`, `fromCStr()`, `fromCStrLen()` |
| [Struct ABI](structs-abi.md) | Passing and returning C structs by value, System V x86-64 ABI |
| [Linking](linking.md) | `-l`, `-L`, `-I` flags, static and dynamic linking |

## See Also

- [Pointers](../language/pointers.md) — Pointer syntax and operations
- [Types](../language/types.md) — All built-in types
- [Functions](../language/functions.md) — Function declarations and variadic functions
