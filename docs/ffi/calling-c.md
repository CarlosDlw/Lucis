# Calling C Functions

Lux provides two ways to call C functions: manual `extern` declarations and automatic `#include` directives. Both produce identical machine code — there is no performance difference.

---

## Extern Declarations

The `extern` keyword declares a C function that exists in an external library. You provide the function signature, and the compiler generates a direct call without any wrapper:

```
extern int32 puts(*char s);

fn main() int32 {
    puts(c"Hello from C!");
    ret 0;
}
```

### Syntax

```
extern ReturnType functionName(ParamType1 paramName1, ParamType2 paramName2, ...);
```

- The return type comes first, followed by the function name and parameters.
- Parameter names are optional — only the types matter for linking.
- The declaration ends with a semicolon.
- Extern declarations must be at the top level (not inside functions).

### Multiple Declarations

You can declare as many extern functions as you need:

```
extern int32 puts(*char s);
extern int32 printf(*char fmt, ...);
extern float64 sqrt(float64 x);
extern float64 sin(float64 x);
extern float64 cos(float64 x);
extern *void malloc(usize size);
extern void free(*void ptr);
extern int32 strcmp(*char s1, *char s2);
extern usize strlen(*char s);
```

### Variadic C Functions

C functions that accept a variable number of arguments (like `printf`) use `...` after the last named parameter:

```
extern int32 printf(*char fmt, ...);

fn main() tm {
    printf(c"Integer: %d\n", 42);
    printf(c"Float: %.4f\n", 3.1415);
    printf(c"String: %s\n", c"hello");
    printf(c"Multiple: %d + %d = %d\n", 3, 4, 7);
    ret 0;
}
```

The `...` in an extern declaration is different from Lux's own variadic syntax (`int32 ...values`). C variadic arguments are not type-checked by the compiler — the types must match the format string.

---

## Include Directives

The `#include` directive tells the compiler to parse a C header file and automatically register all the functions, structs, enums, and macro constants it finds. This is the recommended approach when working with C libraries that provide headers.

### System Headers

System headers are included with angle brackets:

```
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

fn main() int32 {
    printf(c"sqrt(144) = %.0f\n", sqrt(144.0));

    *void buf = malloc(64 as usize);
    free(buf);

    usize len = strlen(c"hello");
    printf(c"strlen = %d\n", len as int32);

    ret 0;
}
```

No `extern` declarations are needed — the compiler reads the header with libclang and extracts every function declaration it finds.

The compiler automatically discovers system include paths by querying the system C compiler. It searches standard directories like `/usr/include`, `/usr/local/include`, and the compiler's built-in header path.

### Local Headers

Local headers are included with double quotes and a relative path from the source file:

```
#include "mymath.h"

fn main() int32 {
    int32 sum = add(10, 20);
    ret 0;
}
```

Where `mymath.h` is a C header in the same directory:

```c
// mymath.h
#ifndef MYMATH_H
#define MYMATH_H

#define PI_APPROX 314159

int add(int a, int b);
int multiply(int a, int b);

#endif
```

And `mymath.c` is the implementation:

```c
// mymath.c
#include "mymath.h"

int add(int a, int b) {
    return a + b;
}

int multiply(int a, int b) {
    return a * b;
}
```

### Auto-Compilation of Local C Sources

When you `#include "header.h"`, the compiler automatically looks for a matching `.c` file with the same name (e.g., `mymath.c` for `mymath.h`). If it finds one, it:

1. Compiles the `.c` file to an object file using the system C compiler
2. Links the resulting object file into the final executable

This means you don't need a separate build step for your C code. Just place the `.h` and `.c` files next to your `.lx` file:

```
my_project/
├── main.lx          # your Lux code
├── mymath.h         # C header
└── mymath.c         # C implementation (auto-compiled)
```

```bash
lux main.lx ./main   # compiles mymath.c automatically
./main
```

### Mixing System and Local Headers

You can use both in the same file:

```
#include <stdio.h>
#include "mymath.h"

fn main() int32 {
    int32 result = add(10, 20);
    printf(c"add(10, 20) = %d\n", result);
    ret 0;
}
```

---

## What Gets Extracted

When the compiler parses a C header, it extracts and makes available:

### Functions

All function declarations become callable. The compiler maps C types to Lux types automatically:

```c
// In C header:
int add(int a, int b);
double sqrt(double x);
void* malloc(size_t size);
```

```
// In Lux code (no extern needed):
int32 sum = add(10, 20);       // int → int32
float64 s = sqrt(144.0);       // double → float64
*void p = malloc(64 as usize); // size_t → usize, void* → *void
```

### Structs

Struct definitions become usable types with all their fields:

```c
// In C header:
typedef struct {
    int x;
    int y;
} Point;

Point make_point(int x, int y);
int point_sum(Point p);
```

```
// In Lux code:
Point p = make_point(10, 20);
printf(c"x=%d y=%d\n", p.x, p.y);

int32 sum = point_sum(p);
```

### Enums

Enum constants become integer constants:

```c
// In C header:
typedef enum {
    COLOR_RED   = 0,
    COLOR_GREEN = 1,
    COLOR_BLUE  = 2
} Color;

const char* color_name(Color c);
```

```
// In Lux code:
int32 r = COLOR_RED;     // 0
int32 g = COLOR_GREEN;   // 1
int32 b = COLOR_BLUE;    // 2

*char name = color_name(COLOR_GREEN);
puts(name);              // "green"
```

### Macro Constants

Numeric `#define` constants become integer constants:

```c
// In C header:
#define PI_APPROX 314159
#define MAX_ITEMS 100
#define FLAGS     (1 << 3)    // expression macros are evaluated
```

```
// In Lux code:
int32 pi = PI_APPROX;     // 314159
int32 max = MAX_ITEMS;     // 100
```

The compiler evaluates constant expressions in macros (arithmetic, bit shifts, bitwise operators). Function-like macros (`#define FOO(x)`) are not supported.

---

## Mixing `extern` and `#include`

You can use both approaches in the same file. This is useful when a system header declares most functions but you need to add a specific declaration:

```
#include <stdio.h>

// Additional function not in stdio.h
extern float64 custom_sqrt(float64 x);

fn main() int32 {
    printf(c"result = %.2f\n", custom_sqrt(2.0));
    ret 0;
}
```

---

## Name Conflicts Between C and Lux

When a C header and a Lux `use` import declare a function with the same name (e.g., `sprintf`), the **last declaration wins** based on the order they appear in the preamble.

### Lux Overrides C (common case)

If `use` appears **after** `#include`, the Lux stdlib version takes precedence:

```
#include <stdio.h>          // declares C sprintf (returns int32)
use std::log::sprintf;      // ← overrides: Lux sprintf (returns string)

fn main() int32 {
    // Uses Lux sprintf — returns a formatted string
    string msg = sprintf("x={}, y={}", 10, 20);
    println(msg);   // x=10, y=20
    ret 0;
}
```

### C Overrides Lux

If `#include` appears **after** `use`, the C version takes precedence:

```
use std::log::sprintf;      // declares Lux sprintf (returns string)
#include <stdio.h>          // ← overrides: C sprintf (returns int32)

fn main() int32 {
    // Uses C sprintf — writes to a char buffer
    // string msg = sprintf(...);  ← compile error: C sprintf returns int32
    ret 0;
}
```

### Recommended Pattern

Place `#include` directives **before** `use` declarations. This way, Lux stdlib functions naturally override any C functions with the same name:

```
#include <stdio.h>
#include <math.h>
#include <string.h>

use std::log::{ println, sprintf };
use Utils::{ doubleVal, square };

fn main() int32 {
    // printf → C version (no Lux override)
    printf(c"hello %s\n", c"world");

    // sprintf → Lux version (overrides C)
    string msg = sprintf("val={}", 42);
    println(msg);

    ret 0;
}
```

> **Note:** This override rule applies only to functions. C structs, enums, typedefs, and macro constants are not affected by `use` imports.

---

## Common Patterns

### Calling `printf` with Format Specifiers

```
extern int32 printf(*char fmt, ...);

fn main() int32 {
    int32 age = 25;
    float64 pi = 3.14159;
    *char name = c"Alice";

    printf(c"Name: %s\n", name);
    printf(c"Age: %d\n", age);
    printf(c"Pi: %.5f\n", pi);
    printf(c"Hex: 0x%x\n", 255);
    printf(c"Char: %c\n", 65);     // 'A'

    ret 0;
}
// Output:
// Name: Alice
// Age: 25
// Pi: 3.14159
// Hex: 0xff
// Char: A
```

### Memory Allocation with `malloc`/`free`

```
extern *void malloc(usize size);
extern void free(*void ptr);
extern *void memset(*void s, int32 c, usize n);

fn main() int32 {
    // Allocate 10 integers
    *int32 nums = malloc(10 * 4 as usize) as *int32;
    defer free(nums as *void);

    // Fill with zeros
    memset(nums as *void, 0, 40 as usize);

    // Write values
    *nums = 42;

    ret 0;
}
```

### Using Math Functions

```
extern float64 sqrt(float64 x);
extern float64 sin(float64 x);
extern float64 cos(float64 x);
extern float64 pow(float64 base, float64 exp);

fn main() int32 {
    float64 pi = 3.14159265358979323846;

    float64 s = sqrt(144.0);          // 12.0
    float64 sn = sin(pi / 2.0);       // 1.0
    float64 cs = cos(0.0);            // 1.0
    float64 p = pow(2.0, 10.0);       // 1024.0

    ret 0;
}
```

---

## See Also

- [FFI Overview](overview.md) — Introduction and type correspondence table
- [C Strings](c-strings.md) — String conversion between T and C
- [Struct ABI](structs-abi.md) — Passing structs to and from C functions
- [Linking](linking.md) — Linking with external libraries
