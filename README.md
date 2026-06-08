# Lux

A compiled systems programming language that targets LLVM IR. Lux combines the performance and control of C with modern syntax, user-defined generics, a rich standard library, and native C interoperability — with zero runtime overhead.

```lux
namespace Main;

use std::log::println;

int32 main() {
    println("Hello, world!");
    ret 0;
}
```

---

## Table of Contents

- [Lux](#lux)
- [Features](#features)
- [Building](#building)
- [Install dependencies](#install-dependencies)
- [Configure and build (recommended)](#configure-and-build-recommended)
- [Alternative: pure CMake](#alternative-pure-cmake)
- [Regenerate grammar manually (optional)](#regenerate-grammar-manually-optional)
- [Dependency discovery notes](#dependency-discovery-notes)
- [Troubleshooting](#troubleshooting)
- [Usage](#usage)
- [Quick Example](#quick-example)
- [Documentation](#documentation)
- [Getting Started](#getting-started)
- [Language Guide](#language-guide)
- [Standard Library](#standard-library)
- [Advanced Topics](#advanced-topics)
- [FFI](#ffi)
- [Reference](#reference)
- [Final Notes](#final-notes)
- [License](#license)

---

## Features

- **Native compilation** — LLVM backend targeting x86-64, ARM, and more; full optimization pipeline (`-O1` through `-O3`, `-Os`, `-Oz`, `-Ofast`) and Link Time Optimization (`--lto`)
- **C FFI** — call any C function with `extern`, include headers with `#include`, pass structs by value; zero glue code
- **User-defined generics** — monomorphized `struct Node<T>`, `extend Node<T>` methods, generic functions `T max<T>(T a, T b)`, and type inference from call arguments when possible
- **Built-in collections** — `vec<T>`, `map<K, V>`, `set<T>` with full method suites
- **Concurrency** — `spawn`/`await`, `Task<T>`, `Mutex`, `lock` statement
- **Error handling** — `try`/`catch`/`finally`, `throw`, built-in `Error` type
- **Type methods** — dot notation on all primitive types, structs, and arrays
- **Rich enums** — unit, tuple payload, named payload, and generic enums (e.g., `Result<V, E>`)
- **Enum variant checks** — `is` supports type checks, variant identity checks, and variant binding like `value is Result<int32, string>::Err(msg)`
- **Manual memory** — explicit allocation, `defer` cleanup, auto-cleanup for collections
- **List comprehensions** — `[x * 2 | for int32 x in items if x > 0]`
- **LSP support** — semantic highlighting, hover, completions, and signature help via the included language server

---

## Building

Dependencies:

| Dependency | Version |
|---|---|
| CMake | 3.20+ |
| GCC or Clang | C++17 (GCC 9+ / Clang 10+) |
| LLVM | 15+ (18+ recommended) |
| ANTLR4 C++ Runtime | 4.13+ |
| libclang | any |
| zlib, pthreads | any |

### Install dependencies

Install the equivalent development packages for your distro. Typical package names:

- Ubuntu/Debian: `cmake`, `build-essential`, `llvm-dev`, `libclang-dev`, `libantlr4-runtime-dev`, `zlib1g-dev`, `pkg-config`
- Arch Linux: `cmake`, `base-devel`, `llvm`, `clang`, `antlr4-runtime`, `zlib`, `pkgconf`
- Fedora: `cmake`, `gcc-c++`, `llvm-devel`, `clang-devel`, `antlr4-cpp-runtime-devel`, `zlib-devel`, `pkgconf-pkg-config`

**Note for Static Linkage (--static):**
If you plan to use the `--static` flag, you must also install the static library versions for system dependencies:
- Ubuntu/Debian: `zlib1g-static`
- Arch Linux: `zlib-static`
- Fedora: `zlib-static`
- (Ensure `glibc-static` is also installed on Fedora/RedHat based systems)

### Configure and build (recommended)

Use the project Makefile (portable wrappers around CMake):

```bash
git clone https://github.com/CarlosDlw/Lux.git
cd Lux
make configure BUILD_TYPE=Debug SANITIZERS=ON
make build
```

The compiled binary is at `build/lux`.

Useful configure variants:

```bash
# Release build without sanitizers
make configure BUILD_TYPE=Release SANITIZERS=OFF

# Force Ninja generator
make configure GENERATOR="Ninja"

# Pass extra CMake hints (custom prefixes/toolchains)
make configure CMAKE_FLAGS="-DCMAKE_PREFIX_PATH=/opt/custom"
```

### Alternative: pure CMake

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DLUX_ENABLE_SANITIZERS=ON
cmake --build build --parallel
```

### Regenerate grammar manually (optional)

Grammar generation is available as a manual Make target and is not part of the normal build pipeline.

```bash
# Uses the default jar in project root
make grammar

# Or pass an explicit jar path
make grammar ANTLR_JAR=/path/to/antlr-4.13.2-complete.jar
```

This regenerates C++ ANTLR sources into `src/generated/` from:

- `grammar/LuxLexer.g4`
- `grammar/LuxParser.g4`

Use this only when grammar files change or when you intentionally want to refresh generated sources.

### Dependency discovery notes

- The build system prefers pkg-config when available.
- It also searches common system locations for ANTLR4 runtime and libclang.
- If LLVM is installed in a non-standard prefix, pass `-DLLVM_DIR=...` or include it in `CMAKE_PREFIX_PATH`.

### Troubleshooting

If CMake reports missing ANTLR4 runtime or libclang:

1. Ensure development packages are installed (headers + libraries, not only runtime binaries).
2. Check pkg-config visibility:
    - `pkg-config --cflags --libs antlr4-runtime`
    - `pkg-config --cflags --libs libclang`
3. If pkg-config cannot find them, export paths:
    - `export PKG_CONFIG_PATH=/your/prefix/lib/pkgconfig:$PKG_CONFIG_PATH`
4. Provide explicit CMake hints:
    - `-DCMAKE_PREFIX_PATH=/your/prefix`
    - `-DLLVM_DIR=/your/prefix/lib/cmake/llvm`

---

## Usage

```
lux build <file> [-o <out>] [-O <lvl>] [--lto] [--emit-...]  Compile to binary
lux run   <file> [-O <lvl>] [--lto] [-- args...]             JIT execution
lux check <file> [-I <dir>]                                   Type-check only
lux test  [filter] [-q]                                       Run test suite
lux help  [command]                                           Show help
lux helpc <lib> [symbol]                                      C library reference
```

```bash
# Compile to binary with LTO and size optimization
lux build main.lx -o ./main -Oz --lto

# Emit Assembly to file
lux build main.lx --emit-asm -o main.s

# Show LLVM IR in terminal
lux build main.lx --emit-llvm

# Run via JIT with O3 optimization
lux run main.lx -O3

# Type-check only
lux check main.lx

# Run the test suite
lux test -q
```

---

## Quick Example

```lux
namespace Main;

use std::log::println;

struct Node<T> {
    T value;
}

extend Node<T> {
    Node<T> create(T val) {
        ret Node<T> { value: val };
    }

    T getValue(&self) {
        ret self.value;
    }
}

T max<T>(T a, T b) {
    ret a > b ? a : b;
}

int32 main() {
    Node<int32> n = Node::create(42);
    int32 val = n.getValue();
    int32 m = max(3, 7);
    println(val);   // 42
    println(m);     // 7
    ret 0;
}
```

---

## Documentation

### Getting Started

| | |
|---|---|
| [Installation](docs/getting-started/installation.md) | Build the compiler from source |
| [Hello World](docs/getting-started/hello-world.md) | Your first Lux program, step by step |
| [CLI Usage](docs/getting-started/cli-usage.md) | Compiler flags, options, and output modes |
| [Editor Setup](docs/getting-started/editor-setup.md) | Syntax highlighting and LSP tooling |

### Language Guide

#### Basics

| | |
|---|---|
| [Overview](docs/language/overview.md) | Philosophy, goals, and design principles |
| [Syntax](docs/language/syntax.md) | Semicolons, blocks, comments, identifiers |
| [Types](docs/language/types.md) | Integers, floats, bool, char, string, void, pointers |
| [Variables](docs/language/variables.md) | Declaration, initialization, scope |
| [Operators](docs/language/operators.md) | Arithmetic, comparison, logical, bitwise, precedence |

#### Control Flow

| | |
|---|---|
| [Control Flow](docs/language/control-flow.md) | `if`/`else`, `switch`, `for`, `while`, `loop`, `break`, `continue` |
| [Ranges](docs/language/ranges.md) | `..`, `..=` in loops and comprehensions |

#### Functions

| | |
|---|---|
| [Functions](docs/language/functions.md) | Declaration, parameters, return values, variadic, function pointers |

#### Data Structures

| | |
|---|---|
| [Arrays](docs/language/arrays.md) | Fixed-size `[N]T`, indexing, methods, list comprehensions |
| [Structs](docs/language/structs.md) | Definition, instantiation, field access, `extend` methods |
| [Enums](docs/language/enums.md) | Definition, variants, `::` access |
| [Unions](docs/language/unions.md) | Definition, shared memory layout, generic unions |
| [Tuples](docs/language/tuples.md) | Lightweight anonymous product types |

#### Type System

| | |
|---|---|
| [Type Aliases](docs/language/type-aliases.md) | The `type` keyword |
| [Generics](docs/language/generics.md) | Built-in and user-defined generic types, monomorphization, constraints |
| [Expressions](docs/language/expressions.md) | Ternary, null coalescing, `as`, `is`, `sizeof`, `typeof` |

#### Modules and Namespaces

| | |
|---|---|
| [Modules](docs/language/modules.md) | `use` / `import` system and module resolution |
| [Namespaces](docs/language/namespaces.md) | Namespace declaration, resolution, multi-file projects |

#### Error Handling

| | |
|---|---|
| [Error Handling](docs/language/error-handling.md) | `try`/`catch`/`finally`, `throw`, `Error`, `panic`, `assert` |

#### Pointers and Memory

| | |
|---|---|
| [Pointers](docs/language/pointers.md) | Pointer types, `&`, `*`, `->`, null, pointer arithmetic |
| [Memory Management](docs/language/memory-management.md) | `defer`, auto-cleanup, manual allocation via `std::mem` |

#### Concurrency

| | |
|---|---|
| [Concurrency](docs/language/concurrency.md) | `spawn`, `await`, `Task<T>`, `Mutex`, `lock` |

### Standard Library

| Module | Description |
|---|---|
| [std::log](docs/stdlib/log.md) | `println`, `print`, `eprint`, `dbg`, `sprintf` |
| [std::io](docs/stdlib/io.md) | `readLine`, `prompt`, `readChar`, `readInt`, `readFloat` |
| [std::fmt](docs/stdlib/fmt.md) | `lpad`, `rpad`, `center`, `hex`, `bin`, `commas`, `humanBytes` |
| [std::str](docs/stdlib/string.md) | `split`, `join`, `parseInt`, `parseFloat`, trim, replace |
| [std::conv](docs/stdlib/conv.md) | `itoa`, `atoi`, `ftoa`, `toHex`, `toBinary` |
| [Vec\<T\>](docs/stdlib/collections/vec.md) | Dynamic growable array |
| [Map\<K, V\>](docs/stdlib/collections/map.md) | Open-addressing hash map |
| [Set\<T\>](docs/stdlib/collections/set.md) | Open-addressing hash set |
| [std::fs](docs/stdlib/fs.md) | Read, write, mkdir, list, remove |
| [std::path](docs/stdlib/path.md) | `join`, `parent`, `extension`, `normalize` |
| [std::process](docs/stdlib/process.md) | `exec`, env, pid, platform |
| [std::os](docs/stdlib/os.md) | `getpid`, hostname, errno, signals |
| [std::math](docs/stdlib/math.md) | `PI`, `E`, `sqrt`, `sin`, `cos`, `pow`, `clamp` |
| [std::random](docs/stdlib/random.md) | `randInt`, `randFloat`, `randBool` |
| [std::bits](docs/stdlib/bits.md) | `popcount`, `rotl`, `rotr`, `setBit`, `testBit` |
| [std::time](docs/stdlib/time.md) | `now`, `sleep`, `year`, `formatTime` |
| [std::net](docs/stdlib/net.md) | TCP/UDP: `tcpConnect`, `tcpSend`, `udpBind` |
| [std::crypto](docs/stdlib/crypto.md) | `md5`, `sha256`, `sha512`, `hmacSha256`, `randomBytes` |
| [std::encoding](docs/stdlib/encoding.md) | Base64, URL encoding |
| [std::compress](docs/stdlib/compress.md) | `gzipCompress`, `deflate`, `inflate` |
| [std::regex](docs/stdlib/regex.md) | `match`, `find`, `regexReplace`, `regexSplit` |
| [std::ascii](docs/stdlib/ascii.md) | `isAlpha`, `isDigit`, `toUpper`, `toLower` |
| [std::hash](docs/stdlib/hash.md) | `hashString`, `hashInt`, `hashCombine`, `crc32` |
| [std::mem](docs/stdlib/mem.md) | `alloc`, `free`, `copy`, `move`, `zero`, `compare` |
| [std::thread](docs/stdlib/thread.md) | `cpuCount`, `threadId`, `yield` |
| [std::test](docs/stdlib/test.md) | `assertEqual`, `assertNear`, `fail`, `skip` |

### Advanced Topics

| | |
|---|---|
| [Compiler Internals](docs/advanced/compiler-internals.md) | Parser, checker, IR builder, optimizer pipeline |
| [Memory Model](docs/advanced/memory-model.md) | Stack layout, heap, pointer semantics |
| [Optimization](docs/advanced/optimization.md) | LLVM pass pipeline, `-o1`/`-o2`/`-o3` |
| [Extending Lux](docs/advanced/extending.md) | Adding builtins and extending the compiler |

### FFI

| | |
|---|---|
| [FFI Overview](docs/ffi/overview.md) | C interop architecture |
| [Calling C](docs/ffi/calling-c.md) | `extern`, `#include`, calling conventions |
| [Structs & ABI](docs/ffi/structs-abi.md) | Passing structs by value, layout compatibility |
| [C Strings](docs/ffi/c-strings.md) | `c"..."` literals, null termination, conversions |
| [Linking](docs/ffi/linking.md) | Linking against static and shared libraries |

### Reference

| | |
|---|---|
| [Grammar](docs/reference/grammar.md) | Full ANTLR4 grammar reference |
| [Keywords](docs/reference/keywords.md) | Reserved keywords |
| [Operator Precedence](docs/reference/operator-precedence.md) | Full precedence table |
| [Builtins](docs/reference/builtins.md) | Built-in functions and operators |
| [Type Methods](docs/reference/type-methods.md) | Methods available on all types |
| [Changelog](docs/reference/changelog.md) | Version history |

---

## Final Notes

Lux is actively evolving. If a build fails on your platform, open an issue with:

- distro and version
- compiler and CMake versions
- full CMake configure output

This helps keep cross-distro support reliable and fast to maintain.

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE).
