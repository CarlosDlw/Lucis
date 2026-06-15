# The Lucis Language

Lucis is a compiled systems programming language that targets LLVM IR. It combines the performance and control of C with modern syntax, a rich standard library, and native C interoperability.

```
use std::log::println;

fn main() int32 {
    println("Hello, world!");
    ret 0;
}
```

---

## Getting Started

Learn how to install, configure, and write your first Lucis program.

- [Installation](getting-started/installation.md) — Build the compiler from source
- [Hello World](getting-started/hello-world.md) — Your first Lucis program, step by step
- [Editor Setup](getting-started/editor-setup.md) — Syntax highlighting and tooling (planned)
- [CLI Usage](getting-started/cli-usage.md) — Compiler flags, options, and output modes

---

## Language Guide

The complete guide to T's syntax and semantics, from basics to advanced features.

### Basics

- [Overview](language/overview.md) — Philosophy, goals, and design principles
- [Syntax](language/syntax.md) — General syntax rules: semicolons, blocks, comments, identifiers
- [Types](language/types.md) — All built-in types: integers, floats, bool, char, string, void, pointers
- [Variables](language/variables.md) — Declaration, initialization, scope, uninitialized variables
- [Operators](language/operators.md) — Arithmetic, comparison, logical, bitwise, assignment, precedence

### Control Flow

- [Control Flow](language/control-flow.md) — if/else, switch, for, while, loop, do-while, break, continue
- [Ranges](language/ranges.md) — Range expressions (`..`, `..=`) and their usage in loops and comprehensions

### Functions

- [Functions](language/functions.md) — Declaration, parameters, return values, variadic functions, function pointers

### Data Structures

- [Arrays](language/arrays.md) — Fixed-size arrays (`[N]T`), indexing, methods, list comprehensions
- [Structs](language/structs.md) — Definition, instantiation, field access, methods via `extend`
- [Enums](language/enums.md) — Definition, variants, access via `::`
- [Unions](language/unions.md) — Definition, usage, shared memory layout

### Type System

- [Type Aliases](language/type-aliases.md) — The `type` keyword and aliasing rules
- [Generics](language/generics.md) — Generic type parameters (`vec<T>`, `map<K, V>`)
- [Expressions](language/expressions.md) — Expression types, ternary, null coalescing, cast (`as`), type check (`is`), `sizeof`, `typeof`

### Modules

- [Modules](language/modules.md) — The `use` / `import` system and module resolution
- [Namespaces](language/namespaces.md) — Namespace declaration, resolution, and multi-file projects

### Error Handling

- [Error Handling](language/error-handling.md) — try/catch/finally, throw, the Error struct, panic, assert

### Pointers and Memory

- [Pointers](language/pointers.md) — Pointer types, address-of, dereference, arrow notation, null, pointer arithmetic
- [Memory Management](language/memory-management.md) — `defer`, auto-cleanup for collections, manual allocation via `std::mem`

### Concurrency

- [Concurrency](language/concurrency.md) — `spawn`, `await`, `Task<T>`, `Mutex`, `lock` statement

---

## Standard Library

Lucis's standard library provides modules for I/O, math, strings, file system, networking, and more.

- [Overview](stdlib/overview.md) — How the stdlib is organized and how to import

### Core Modules

- [std::log](stdlib/log.md) — `println`, `print`, `eprint`, `eprintln`, `dbg`, `sprintf`
- [std::io](stdlib/io.md) — `readLine`, `prompt`, `readChar`, `readInt`, `readFloat`, and more
- [std::fmt](stdlib/fmt.md) — String formatting: `lpad`, `rpad`, `center`, `hex`, `bin`, `commas`, `humanBytes`
- [std::str](stdlib/string.md) — String utility functions: `split`, `join`, `parseInt`, `parseFloat`, and more
- [std::conv](stdlib/conv.md) — Type conversions: `itoa`, `atoi`, `ftoa`, `toHex`, `toBinary`

### Collections

- [Vec\<T\>](stdlib/collections/vec.md) — Dynamic growable array
- [Map\<K, V\>](stdlib/collections/map.md) — Open-addressing hash map
- [Set\<T\>](stdlib/collections/set.md) — Open-addressing hash set

### System

- [std::fs](stdlib/fs.md) — File system operations: read, write, mkdir, list, remove
- [std::path](stdlib/path.md) — Path manipulation: join, parent, extension, normalize
- [std::process](stdlib/process.md) — Process control: exec, env, pid, platform
- [std::os](stdlib/os.md) — OS-level operations: getpid, hostname, errno, signals

### Math and Science

- [std::math](stdlib/math.md) — Math functions and constants: PI, E, sqrt, sin, cos, pow, clamp
- [std::random](stdlib/random.md) — Random number generation: `randInt`, `randFloat`, `randBool`
- [std::bits](stdlib/bits.md) — Bitwise utilities: `popcount`, `rotl`, `rotr`, `setBit`, `testBit`

### Time

- [std::time](stdlib/time.md) — Date, time, sleep: `now`, `sleep`, `year`, `formatTime`

### Networking

- [std::net](stdlib/net.md) — TCP/UDP networking: `tcpConnect`, `tcpSend`, `udpBind`

### Security

- [std::crypto](stdlib/crypto.md) — Hashing: `md5`, `sha256`, `sha512`, `hmacSha256`, `randomBytes`
- [std::encoding](stdlib/encoding.md) — Base64, URL encoding: `base64Encode`, `urlEncode`
- [std::compress](stdlib/compress.md) — Compression: `gzipCompress`, `deflate`, `inflate`

### Text Processing

- [std::regex](stdlib/regex.md) — Regular expressions: `match`, `find`, `regexReplace`, `regexSplit`
- [std::ascii](stdlib/ascii.md) — ASCII character queries: `isAlpha`, `isDigit`, `toUpper`, `toLower`

### Hashing

- [std::hash](stdlib/hash.md) — Hash functions: `hashString`, `hashInt`, `hashCombine`, `crc32`

### Memory

- [std::mem](stdlib/mem.md) — Low-level memory: `alloc`, `free`, `copy`, `move`, `zero`, `compare`

### Threading

- [std::thread](stdlib/thread.md) — Threading utilities: `cpuCount`, `threadId`, `yield`

### Testing

- [std::test](stdlib/test.md) — Testing framework: `assertEqual`, `assertNear`, `fail`, `skip`

---

## C Interoperability (FFI)

Lucis has first-class support for calling C libraries and linking with native code.

- [Overview](ffi/overview.md) — How FFI works in T
- [Calling C Functions](ffi/calling-c.md) — `extern` declarations and `#include` directives
- [C Strings](ffi/c-strings.md) — `c"..."` literals, `cstring` type, conversion builtins
- [Struct ABI](ffi/structs-abi.md) — Struct by-value passing with x86-64 System V ABI
- [Linking](ffi/linking.md) — `-l` and `-L` flags, dynamic and static linking

---

## Tools

Built-in utilities that ship with the compiler.

- [helpc](tools/helpc.md) — C library reference: inspect functions, structs, enums, macros, and typedefs from any C header

---

## Advanced

Deeper topics for power users and contributors.

- [Memory Model](advanced/memory-model.md) — Stack vs heap, allocation strategy, `defer`, auto-cleanup
- [Compiler Internals](advanced/compiler-internals.md) — Pipeline: parse → check → LLVM IR → machine code
- [Optimization](advanced/optimization.md) — LLVM optimization passes and performance hints
- [Extending T](advanced/extending.md) — How to add builtins, stdlib modules, and language features

---

## Reference

Quick-reference tables and formal specifications.

- [Grammar](reference/grammar.md) — Formal grammar summary from the `.g4` files
- [Keywords](reference/keywords.md) — Complete list of reserved keywords
- [Global Builtins](reference/builtins.md) — `exit`, `panic`, `assert`, `toString`, `cstr`, `sizeof`, `typeof`
- [Type Methods](reference/type-methods.md) — All methods available on each built-in type
- [Operator Precedence](reference/operator-precedence.md) — Full precedence table from highest to lowest
- [Changelog](reference/changelog.md) — Version history and breaking changes
