# Overview

Lucis is a compiled systems programming language that targets LLVM IR. It is designed for developers who want low-level control with modern ergonomics — a language that feels familiar if you come from C, C++, Rust, or Go, but with a cleaner syntax and a batteries-included standard library.

## Design Goals

- **Performance first.** T compiles to native machine code via LLVM with full optimization support (`-o1`, `-o2`, `-o3`). There is no garbage collector, no runtime overhead, and no hidden allocations.

- **C interoperability.** Lucis can call any C function, include C headers, link against C libraries, and pass structs by value — all with zero glue code. The `extern`, `#include`, and `c"..."` syntax make FFI a first-class feature, not an afterthought.

- **Type-first syntax.** Declarations place the type before the name: `int32 x = 10;`. This makes code scannable and consistent across variables, parameters, and return types.

- **Rich standard library.** The standard library covers I/O, file system, networking, cryptography, regular expressions, compression, formatting, and more — all implemented as C builtins for performance.

- **Explicit over implicit.** There is no implicit type coercion, no hidden control flow, and no magic. Casts use `as`, type checks use `is`, and memory management is manual (with `defer` to help).

## Key Features

| Feature | Description |
|---|---|
| Native compilation | LLVM backend, produces native binaries for x86-64, ARM, and more |
| C FFI | `extern`, `#include`, `c"..."` literals, struct ABI compatibility |
| Generics | `vec<T>`, `map<K, V>`, `set<T>`, `Task<T>` |
| Concurrency | `spawn`, `await`, `Mutex`, `lock` statement |
| Error handling | `try`/`catch`/`finally`, `throw`, built-in `Error` type |
| Memory control | Manual allocation, `defer` cleanup, auto-cleanup for collections |
| Type methods | Methods on all primitive types via dot notation |
| List comprehensions | `[x * 2 \| for int32 x in items if x > 0]` |
| Structs with methods | `struct` + `extend` blocks with instance and static methods |

## Comparison with Other Languages

T sits in the same space as C, Zig, Odin, and V — compiled languages with manual memory management and C interop. Compared to these:

- **vs C:** Lucis has a richer type system (generics, methods on types, enums with namespace access), a standard library that goes beyond libc, and modern syntax (no header files, no preprocessor for Lucis code).
- **vs Zig/Odin:** T prioritizes simplicity and a batteries-included stdlib over compile-time metaprogramming. The learning curve is intentionally shallow.
- **vs Rust:** Lucis does not have a borrow checker or lifetime system. Memory management is explicit and manual, with `defer` and auto-cleanup as the primary safety tools.

## Hello World

```

use std::log::println;

fn main() int32 {
    println("Hello, world!");
    ret 0;
}
```

## See Also

- [Syntax](syntax.md) — General syntax rules
- [Types](types.md) — All built-in types
- [Hello World](../getting-started/hello-world.md) — Step-by-step first program
