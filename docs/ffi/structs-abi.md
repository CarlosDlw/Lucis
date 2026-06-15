# Struct ABI

When you pass or return C structs by value, T follows the **System V AMD64 ABI** — the same calling convention used by C compilers on x86-64 Linux and macOS. This means struct-heavy C APIs work correctly without any manual effort.

This page explains how struct passing works, why it matters, and how to use it.

---

## The Short Version

You don't need to think about any of this for normal usage. Just include a C header, call functions that take or return structs, and it works:

```
#include <stdio.h>
#include "structs.h"

fn main() int32 {
    Point p = make_point(10, 20);
    int32 sum = point_sum(p);
    printf(c"sum = %d\n", sum);    // 30
    ret 0;
}
```

The compiler handles all the ABI details automatically. The rest of this page explains what happens behind the scenes.

---

## How Struct Passing Works

The System V x86-64 ABI defines rules for how structs are passed to and returned from functions, based on the struct's size:

### Small Structs (≤ 8 bytes)

Structs that fit in a single CPU register are packed into one integer and passed directly in a register:

```c
// C header
typedef struct {
    int x;    // 4 bytes
    int y;    // 4 bytes
} Point;      // total: 8 bytes → fits in one register

Point make_point(int x, int y);
int point_sum(Point p);
```

```
// Lucis code
#include "structs.h"

Point p = make_point(10, 20);    // returned in rax register
int32 s = point_sum(p);          // passed in rdi register
```

Behind the scenes, the compiler coerces the 8-byte struct to an `i64`, passes/returns it as a single integer, then unpacks it back into the struct fields.

### Medium Structs (9–16 bytes)

Structs between 9 and 16 bytes are split across two registers:

```c
// C header
typedef struct {
    long a;    // 8 bytes
    long b;    // 8 bytes
} Pair64;      // total: 16 bytes → two registers

Pair64 make_pair64(long a, long b);
long pair64_sum(Pair64 p);
```

```
// Lucis code
#include "structs.h"

Pair64 pr = make_pair64(100, 200);    // returned in rax + rdx
int64 sum = pair64_sum(pr);           // passed in rdi + rsi
```

The compiler splits the struct into two parts: the first 8 bytes go in one register, the remaining bytes go in the other.

### Large Structs (> 16 bytes)

Structs larger than 16 bytes don't fit in registers. They are passed **indirectly** via a hidden pointer:

```c
// C header
typedef struct {
    long w;    // 8 bytes
    long x;    // 8 bytes
    long y;    // 8 bytes
    long z;    // 8 bytes
} Vec4;        // total: 32 bytes → too big for registers

Vec4 make_vec4(long w, long x, long y, long z);
long vec4_sum(Vec4 v);
```

```
// Lucis code
#include "structs.h"

Vec4 v = make_vec4(1, 2, 3, 4);    // caller allocates space, passes hidden ptr
int64 sum = vec4_sum(v);           // struct passed via byval pointer
```

For **return values**, the compiler allocates stack space in the caller and passes a hidden pointer as the first argument (called `sret`). The callee writes the result into that memory.

For **parameters**, the compiler passes a pointer to a copy of the struct (with the `byval` attribute), so the callee cannot modify the caller's original.

### Size Classification Summary

| Struct Size | Register Strategy | What Happens |
|-------------|-------------------|--------------|
| ≤ 8 bytes | 1 register | Coerced to `i64` |
| 9–16 bytes | 2 registers | Split into `{i64, iN}` |
| > 16 bytes | Hidden pointer | `sret` for returns, `byval` for parameters |

---

## Nested Structs

Nested structs work correctly — the compiler calculates the total size including all sub-structs:

```c
// C header
typedef struct {
    int x;
    int y;
} Point;       // 8 bytes

typedef struct {
    Point origin;
    Point size;
} Rect;         // 16 bytes → two registers
```

```
// Lucis code
#include "structs.h"

Rect r = make_rect(5, 10, 20, 30);
int32 area = rect_area(r);
printf(c"area = %d\n", area);    // 600
```

The `Rect` struct (16 bytes) is classified as medium and split across two registers, with `origin` in the first and `size` in the second.

---

## Complete Example

This example tests all three size categories — small, medium, large, and nested:

```c
// structs.h
#ifndef STRUCTS_H
#define STRUCTS_H

typedef struct { int x; int y; } Point;             // 8 bytes
typedef struct { long a; long b; } Pair64;           // 16 bytes
typedef struct { long w; long x; long y; long z; } Vec4;  // 32 bytes
typedef struct { Point origin; Point size; } Rect;   // 16 bytes

Point make_point(int x, int y);
Pair64 make_pair64(long a, long b);
Vec4 make_vec4(long w, long x, long y, long z);
Rect make_rect(int ox, int oy, int sx, int sy);

int point_sum(Point p);
long pair64_sum(Pair64 p);
long vec4_sum(Vec4 v);
int rect_area(Rect r);

#endif
```

```c
// structs.c
#include "structs.h"

Point make_point(int x, int y) {
    Point p; p.x = x; p.y = y; return p;
}

Pair64 make_pair64(long a, long b) {
    Pair64 p; p.a = a; p.b = b; return p;
}

Vec4 make_vec4(long w, long x, long y, long z) {
    Vec4 v; v.w = w; v.x = x; v.y = y; v.z = z; return v;
}

Rect make_rect(int ox, int oy, int sx, int sy) {
    Rect r;
    r.origin.x = ox; r.origin.y = oy;
    r.size.x = sx; r.size.y = sy;
    return r;
}

int point_sum(Point p) { return p.x + p.y; }
long pair64_sum(Pair64 p) { return p.a + p.b; }
long vec4_sum(Vec4 v) { return v.w + v.x + v.y + v.z; }
int rect_area(Rect r) { return r.size.x * r.size.y; }
```

```
// main.lc

#include <stdio.h>
#include "structs.h"

fn main() int32 {
    // Small struct (8 bytes — 1 register)
    Point p = make_point(10, 20);
    int32 ps = point_sum(p);
    printf(c"point_sum = %d\n", ps);         // 30

    // Medium struct (16 bytes — 2 registers)
    Pair64 pr = make_pair64(100, 200);
    int64 prs = pair64_sum(pr);
    printf(c"pair64_sum = %ld\n", prs);      // 300

    // Large struct (32 bytes — sret/byval)
    Vec4 v = make_vec4(1, 2, 3, 4);
    int64 vs = vec4_sum(v);
    printf(c"vec4_sum = %ld\n", vs);         // 10

    // Nested struct (16 bytes — 2 registers)
    Rect r = make_rect(5, 10, 20, 30);
    int32 area = rect_area(r);
    printf(c"rect_area = %d\n", area);       // 600

    ret 0;
}
```

```bash
lucis main.lc ./main
./main
```

```
point_sum = 30
pair64_sum = 300
vec4_sum = 10
rect_area = 600
```

---

## Accessing struct fields

Once you have a C struct in Lucis, you access fields with dot notation, just like Lucis structs:

```
Point p = make_point(10, 20);
printf(c"x=%d, y=%d\n", p.x, p.y);

// You can also modify fields
p.x = 99;
printf(c"x=%d\n", p.x);    // 99
```

Pointers to C structs use arrow notation:

```
*Point pp = &p;
printf(c"x=%d\n", pp->x);    // 99
```

---

## Limitations

- **Only System V x86-64 ABI** is currently supported. Windows x64 (MS ABI) is not yet implemented.
- **Structs with floating-point fields** may use SSE registers in the real ABI, but T currently classifies all N-byte structs as integer register candidates. This works correctly for integer-only structs.
- **Unions** included in C headers are supported but follow a simplified layout.

---

## See Also

- [FFI Overview](overview.md) — Type correspondence table
- [Calling C Functions](calling-c.md) — `extern` and `#include` usage
- [Structs](../language/structs.md) — Lucis struct declarations and methods
- [Linking](linking.md) — Linking with external libraries
