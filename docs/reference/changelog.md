# Changelog

All notable changes to the Lucis language and its compiler.

---

## Unreleased — Native Test Framework

### Language Features

- **`#[test]` attribute**: marks a function as a test case, with optional
  display name, `skip` / `skip("reason")`, `expected_fail` and per-test
  `timeout(N)` (ms). Validated at compile time (plain, non-generic,
  non-comptime, parameterless function returning `void` or `int32`).

### CLI

- **`lucis test`**: discovers `#[test]` functions through the entry import
  graph, builds a test binary and runs it with process isolation per test.
  Flags: `-t/--test`, `--skip`, `--exact`, `--list`, `--timeout`, `-j`,
  `-v`, plus compile flags.
- Exit codes: `0` pass (or no tests), `1` failure/compile error, `2` usage.

### Internal

- `TestAttribute` (attribute parsing), `BinaryBuilder` (shared
  pipeline→IR→link core extracted from `RunCommand`), IRGen test-mode
  (`synthesizeTestMain`) and the C harness `src/builtins/test/test_runner.c`
  (fork/pipe isolation, output capture, timeouts via `setitimer`, parallel
  `-j` with deterministic reporting).
- `make test-framework` validates the framework (positive + negative cases in
  `test_framework/`).

---

## v0.0.2-beta — Path-Based Imports

### Breaking Changes

- **`namespace` keyword removed.** Files no longer declare `namespace X;`. The file path relative to project root is the module identity.
- **`use` is path-based.** `use lib::math::add` resolves to `lib/math.lc` (searched in project root + `sourcePaths` from `lucis.yaml`).
- **No directory scanning.** The compiler uses BFS import resolution from the entry point — only files referenced by `use` are opened.

### Migration

```lucis
// Before
namespace Main;
use std::log::println;

// After
use std::log::println;
```

### Internal

- `NamespaceRegistry` → `ModuleRegistry` (keyed by file path)
- `ProjectScanner` removed
- Pipeline/LSP use BFS import resolution
- `lucis.yaml` `source` paths used as search prefixes

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
- **Modules**: import system via `use` declarations with selective and group syntax
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
