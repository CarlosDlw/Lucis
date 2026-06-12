# Changelog

All notable changes to the Lucis language and its compiler.

---

## v0.0.1 beta — Initial Release

First public release of the Lucis language compiler.

### Language Features

- **Type System**: `int8`–`int128`, `intinf`, `uint8`–`uint128`, `isize`, `usize`, `float32`–`float128`, `double`, `bool`, `char`, `string`, `cstring`, `void`
- **Variables**: `let` (immutable) and `var` (mutable) declarations with type inference
- **Functions**: Named functions, generic functions with monomorphization, variadic parameters with `...`
- **Control Flow**: `if`/`else if`/`else`, `for`-in, classic `for`, `while`, `do-while`, `switch` with pattern matching
- **Ranges**: `..` (exclusive) and `..=` (inclusive)
- **Data Types**: `struct`, `enum`, `union`, type aliases
- **Generics**: Parametric polymorphism with `<T>` syntax and specialization via monomorphization
- **Namespaces**: `namespace` declarations, `use` imports with selective and group syntax
- **Extend Blocks**: Add methods to any type via `extend`
- **Error Handling**: `try`/`catch`/`finally`, `throw`, `defer` for cleanup
- **Pointers**: Raw pointers (`*T`), `&` and `*` operators, arrow access (`->`)
- **Memory Management**: Manual with `std::mem`, `defer` for auto-cleanup, RAII-style `free()` on heap types
- **Concurrency**: `lock` blocks, `std::thread` module
- **FFI**: `extern` declarations, C interop via libclang, `cstring` type, ABI-compatible structs

### Standard Library

20+ modules: `log`, `io`, `math`, `str`, `conv`, `fmt`, `fs`, `path`, `process`, `os`, `time`, `random`, `mem`, `hash`, `bits`, `ascii`, `regex`, `encoding`, `crypto`, `compress`, `net`, `thread`, `test`

### Collections

- `vec<T>` — dynamic array
- `map<K, V>` — hash map
- `set<T>` — hash set

### Compiler

- ANTLR4 parser frontend
- LLVM 22 IR backend
- Optimization levels: `-o1`, `-o2`, `-o3`
- Direct binary output or IR inspection
- Linker integration with `-l` and `-L` flags
- Include paths with `-I` flag
