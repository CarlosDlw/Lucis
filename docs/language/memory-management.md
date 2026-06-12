# Memory Management

T uses manual memory management with three complementary mechanisms: `defer` for explicit cleanup scheduling, automatic cleanup for collection types, and the `std::mem` module for low-level allocation.

---

## `defer`

The `defer` statement schedules code to execute when the current function exits. Deferred statements run in **LIFO order** (last deferred runs first).

### Basic Usage

```
#include <stdlib.h>

fn example() void {
    *int32 ptr = malloc(4) as *int32;
    defer free(ptr as *void);

    *ptr = 42;
    println(*ptr);   // 42
    // free(ptr) runs here automatically
}
```

### LIFO Execution Order

```
fn example() void {
    defer println(1);
    defer println(2);
    defer println(3);
    println(0);
}
// Output: 0, 3, 2, 1
```

### With Any Statement

`defer` works with any statement or expression:

```
defer close(fd);
defer v.free();
defer println("cleanup done");
```

---

## Structural Blocks

Structural blocks are special block constructs for scope isolation, code injection, and RAII-style cleanup. There are three forms: **naked block** `{}`, **inline block** `#inline {}`, and **scope block** `#scope (...) {}`.

---

### Naked Block `{}`

A pair of braces creates a new lexical scope. Variables declared inside are not visible outside.

```lucis
fn main() int32 {
    int32 x = 1;
    {
        int32 y = 2;    // y is local to this block
        x = x + y;
    }
    // y is not accessible here
    println(x);         // 3
    ret 0;
}
```

Use naked blocks to limit variable lifetime or to avoid name collisions.

---

### Inline Block `#inline {}`

`#inline {}` injects the block's body into the enclosing scope. Variables declared inside become visible in the surrounding scope, as if written inline. This is useful for grouping logically related setup code without introducing a new scope.

```lucis
fn main() int32 {
    #inline {
        int32 result = compute();
        bool valid = result > 0;
    }
    // result and valid are visible here
    if valid {
        println(result);
    }
    ret 0;
}
```

---

### Scope Block `#scope (...) {}`

`#scope (callbacks...) {}` registers one or more cleanup callbacks that run automatically when the block exits, in **LIFO order**. This enables RAII-style resource management scoped to a block rather than a full function.

```lucis
#include <raylib.h>

fn main() int32 {
    InitWindow(800, 600, c"Demo");
    SetTargetFPS(60);

    while !WindowShouldClose() {
        #scope (EndDrawing()) {
            BeginDrawing();
            ClearBackground(RAYWHITE);
            DrawText(c"Hello!", 10, 10, 20, DARKGRAY);
        }
        // EndDrawing() is always called here, even if the body returns early
    }
    ret 0;
}
```

Multiple callbacks run in reverse order (LIFO):

```lucis
#scope (releaseB(), releaseA()) {
    acquireA();
    acquireB();
    // ... work ...
}
// releaseB() runs first, then releaseA()
```

`#scope` complements `defer` when cleanup should be bound to a specific block rather than function exit.

---

## Strings, Ownership, and `freeStr`

Lucis `string` values are not all equivalent from a lifetime perspective.

- String literals like `"hello"` do not need `freeStr`.
- Borrowed strings such as `Error.message` do not need `freeStr` by themselves.
- Owned strings returned by APIs that allocate a new buffer are automatically dropped when local scope exits.

Common sources of owned strings:

- `fromCStrCopy(...)`
- `sprintf(...)`
- string transformation methods that return a new `string`, such as `toUpper()`, `toLower()`, `trim()`, `reverse()`, `capitalize()`, `replace()`, `substring()`, `slice()`, `concat()`, and similar APIs that produce a fresh buffer

In most cases, if you store the result in a local variable, Lucis will auto-drop it at scope exit:

```lucis
use std::log::{ println };

fn main() int32 {
    string msg = "an error occurred".capitalize();
    println(msg);
    ret 0;
}
```

Use `freeStr` only when you need an early/manual release (before scope exit):

```lucis
use std::log::{ println };

fn main() int32 {
    string msg = "an error occurred".capitalize();
    freeStr(msg); // explicit early release
    println(msg);
    ret 0;
}
```

`freeStr` consumes ownership of its argument. After `freeStr(x)`, `x` is moved and cannot be used.
Do not call `freeStr` on borrowed strings unless you know the API returned owned memory.

---

## Automatic Cleanup for Heap-Backed Locals

`string`, `vec<T>`, `map<K, V>`, and `set<T>` locals are automatically cleaned up.
Cleanup happens both on function exit and lexical/control-flow exits where scope ends (for example, block exits and loop `break`/`continue` paths).

### Vec Auto-Cleanup

```
fn example() void {
    vec<int32> v;
    v.push(10);
    v.push(20);
    v.push(30);
    println(v.len());   // 3
    // v is automatically freed at function exit
}
```

### Map Auto-Cleanup

```
fn example() void {
    map<int32, int32> m;
    m.insert(1, 100);
    m.insert(2, 200);
    println(m.len());   // 2
    // m is automatically freed at function exit
}
```

### Set Auto-Cleanup

```
fn example() void {
    set<int32> s;
    s.add(1);
    s.add(2);
    s.add(3);
    println(s.len());   // 3
    // s is automatically freed at function exit
}
```

### Mixed Defer and Auto-Cleanup

Both mechanisms can coexist in the same function. Explicit `defer` statements run first (LIFO), then auto-cleanup runs:

```
fn example() void {
    vec<int32> v;
    v.push(1);
    defer println(99);
    vec<int32> w;
    w.push(2);
    w.push(3);
    println(v.len());   // 1
    println(w.len());   // 2
    // defer println(99) runs first
    // then v and w are auto-freed
}
```

### Explicit `.free()`

You can also call `.free()` manually or use `defer` for explicit control:

```
vec<int32> v = [1, 2, 3];
defer v.free();
// ... use v ...
```

---

## `std::mem` — Low-Level Memory Operations

For direct heap allocation and manipulation, import from `std::mem`:

```
use std::mem::alloc;
use std::mem::allocZeroed;
use std::mem::realloc;
use std::mem::free;
use std::mem::copy;
use std::mem::move;
use std::mem::set;
use std::mem::zero;
use std::mem::compare;
```

### Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `alloc` | `(usize) -> *void` | Allocate bytes (uninitialized) |
| `allocZeroed` | `(usize) -> *void` | Allocate zeroed bytes |
| `realloc` | `(*void, usize) -> *void` | Resize allocation |
| `free` | `(*void)` | Free allocation |
| `copy` | `(*void dst, *void src, usize n)` | Copy n bytes (non-overlapping) |
| `move` | `(*void dst, *void src, usize n)` | Copy n bytes (overlapping-safe) |
| `set` | `(*void, int32 byte, usize n)` | Fill n bytes with value |
| `zero` | `(*void, usize n)` | Zero out n bytes |
| `compare` | `(*void, *void, usize) -> int32` | Compare n bytes (like `memcmp`) |

### Example

```
use std::mem::{ alloc, allocZeroed, copy, compare, free };

*int32 p = alloc(4);
*p = 42;
println(*p);            // 42

*int32 q = allocZeroed(4);
println(*q);            // 0

copy(q, p, 4);
println(*q);            // 42

int32 cmp = compare(p, q, 4);
println(cmp);           // 0 (equal)

free(p);
free(q);
```

---

## C `malloc` / `free`

You can also use C's `malloc` and `free` directly via `#include` or `extern`:

```
#include <stdlib.h>

*int32 ptr = malloc(4) as *int32;
*ptr = 42;
free(ptr as *void);
```

Or with extern declarations:

```
extern *void malloc(usize size);
extern void free(*void ptr);
```

---

## Summary: When to Use What

| Mechanism | Use Case |
|-----------|----------|
| Auto-cleanup | Local `string`, `Vec`, `Map`, `Set` — no action needed |
| `defer` | Explicit cleanup of raw pointers, file handles, etc. |
| `{}` naked block | Isolate variable scope within a function |
| `#inline {}` | Inject code into parent scope (no scope boundary) |
| `#scope (...) {}` | RAII-style cleanup bound to a specific block |
| `v.free()` | Manual early cleanup of collections |
| `std::mem` | Low-level allocation, copy, compare |
| C `malloc`/`free` | Direct C interop |

---

## See Also

- [Pointers](pointers.md) — Pointer types, address-of, dereference
- [Generics](generics.md) — `vec<T>`, `map<K,V>`, `set<T>` methods
- [Modules](modules.md) — Importing `std::mem`

---

## Ownership Model (Current)

Lucis now treats heap-backed values (`string`, `vec`, `map`, `set`) as ownership-tracked values.

- Assignments and returns can transfer ownership.
- Using a value after it is moved triggers diagnostics (`use-after-move` / `double-move`).
- `freeStr` consumes its argument ownership.
- Collection cleanup now performs deep cleanup for string elements/keys/values.
- Ownership diagnostics are emitted with stable codes (for example `OWN001`, `OWN002`) in LSP.

For interop helpers:

- `fromCStr(...)` / `fromCStrLen(...)` are borrowed.
- `fromCStrCopy(...)`, `sprintf(...)`, and string transform APIs return owned strings.
