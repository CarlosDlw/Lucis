# Changelog

All notable changes to the Lucis language and its compiler.

---

## Unreleased — import aliases: deep cross-module support + LSP coverage

- **Methods of imported types work with `as`** (`use lib::Point as MyPoint;`
  → `p.norm()`): the checker's extend-block registration (B.5) compared
  the local binding against the exported name; it now uses
  `importedSymbolName`. Same for imported `impl` blocks (B.7) and trait
  pre-registration when the impl's trait is not explicitly imported.
- **Imported structs no longer create opaque skeletons**: pending maps in
  the checker and IRGen are keyed by the exported name (an alias-keyed
  skeleton produced an empty `%MyPoint` LLVM type → `alloca` crash), and
  `TypeInfo::toLLVMType` builds struct/union bodies self-contained from
  their fields — type aliases of structs (`type A = Point;`) work even
  when the LLVM type was not pre-created.
- **Generic templates and constants imported under aliases**:
  `use lib::Box as Bx; Bx<int32>` and `[IMPORTED_CONST]` array
  dimensions work. The checker's full constant evaluator persists
  integer values in the `ModuleRegistry` (const cache write); the IRGen
  reads them for `[N]T` sizes — no duplicated evaluation.
- **Nested array literals**: ragged shapes (`[[1,2],[3]]`) now produce a
  clear diagnostic instead of an IR-generator crash; the IRGen stays
  defensive (never feeds `InsertValue` mismatched types).
- **Fixed a pre-existing bug**: trait impl methods were forward-declared
  but never emitted (their bodies were skipped as "already lowered"),
  breaking `make test`'s `trait_methods` at link time.
- **LSP**: new `lsp_tests/test_import_aliases.py` (23 cases) pins
  completion, hover, go-to-definition, signature help and semantic
  tokens for every importable symbol kind under aliases (stdlib `.lc`
  variadic, builtins, user functions, structs+methods, traits,
  constants, type aliases, enums). This surfaced and fixed a
  signature-help bug: `analyzeCallSite` failed when the cursor sat
  inside a string literal (`output("…")` never found its `(`).

## Unreleased — `use` import aliases (`as`) + trailing commas

- **Import aliases everywhere**: `use module::func as local;` and group
  aliases `use module::{ func as local, other };` now work for stdlib
  builtins, stdlib `.lc` functions (variadic `printf` included),
  user-module functions, constants and types. Builtins dispatch on the
  exported name while the source uses the local binding
  (`ImportResolver` gained `isExportedSymbol` for the translated name).
- **Module imports of any depth**: `use module;`, `use fold::module;`
  and `use std::module;` import a namespace; members are accessed
  qualified (`module::CONST`) or via `use fold::module::{ sym }`.
  The module-vs-symbol ambiguity of `use a::b;` is resolved at
  check time (stdlib symbol → full-path module → symbol → fallback),
  and the import BFS now loads the full-path module when it exists.
- **Single normalized model**: new `src/imports/UseParser.{h,cpp}`
  (`UseBinding` + token-level `parseUseDeclWithTokens`) consumed by the
  checker, IRGen, pipeline and all LSP providers — no more per-file
  re-interpretation of the four `use` forms.
- **Trailing commas**: array literals (`[1, 2, 3,]`) and `use` groups.
- Tests: `tests/import_alias_trailing_array.lc` + `tests/fold/module.lc`
  (stdlib/builtin/user aliases, module members, empty/flat arrays).
  Known pre-existing bug found while testing (not caused here): nested
  array literals `[[1,2],[3]]` crash the IR generator.

## Unreleased — fix: `va_arg_string` returns an owned copy (UAF)

- **Fixed a use-after-free** that corrupted strings on their second use
  when they came from `fn main(vec<string> args)`: e.g. two `printf`
  calls with the same argument produced garbage on the second. The
  runtime tracks owned string allocations in a registry and only frees
  registered pointers — so auto-drop of a *view* temporaries was safe
  for literals (rodata, not registered) but freed the *slot of a live
  `vec<string>`* when the view came from `va_arg_string` (registered
  heap → dropped → dangling every other reference to it).
- `lucis::unsafe::va_arg_string` now copies the C string into tracked
  memory (`lucis_fromCStrCopy`) before returning: auto-drop of the
  temporary frees only the copy, never the caller's buffer (a `vec`
  element, `argv`, a struct field, …). One extra allocation per string
  read from the variadic list — the safe trade.
- Docs updated in `docs/language/intrinsics/unsafe.md`.
- Found via `local_projects/lrelf` (the ELF-reader CLI): the error path
  lost the file name because the second use of the argument was
  dangling. The same reproduction (`printf` a `vec<string>` element
  twice) was stable with rodata literals and only broke for registered
  heap — confirming the tracking mechanism.

## Unreleased — LSP flat-latency symbol resolution (Phase B, step 6)

- The last inline `topLevelDecl()` sweeps are gone. `FileSymbolIndex`
  gained `enclosingDeclAt()` (top-level spans); go-to-definition and
  hover now resolve the declaration under the cursor through the index
  (plus `enclosingFunctionAt`/`enclosingExtendMethodAt` for body walks)
  instead of scanning the whole file per request.
- Measured on synthetic files (function-heavy, parse cached):
  go-to-definition 200/2000/5000 functions = 0.3/0.5/1.0 ms (was
  0.6/4.8/11.7 ms — ~90% faster on large files, latency now nearly
  flat); hover 0.3/0.8/2.1 ms.

## Unreleased — LSP flat-latency symbol resolution (Phase B, step 6)

- The last inline `topLevelDecl()` sweeps are gone. `FileSymbolIndex`
  gained `enclosingDeclAt()` (top-level spans); go-to-definition and
  hover now resolve the declaration under the cursor through the index
  (plus `enclosingFunctionAt`/`enclosingExtendMethodAt` for body walks)
  instead of scanning the whole file per request.
- Measured on synthetic files (function-heavy, parse cached):
  go-to-definition 200/2000/5000 functions = 0.3/0.5/1.0 ms (was
  0.6/4.8/11.7 ms — ~90% faster on large files, latency now nearly
  flat); hover 0.3/0.8/2.1 ms.

## Unreleased — LSP indexed type inference in completion (Phase B, step 5)

- The static inference helpers of `CompletionProvider` — the
  `find*DeclForInference` set, `lookupFuncReturnType`, the enum/extend/
  bitstruct lookups inside `inferExprTypeName`, and the `Self`
  resolution in `collectLocalsFromStmts` — now go through the
  server-cached `FileSymbolIndex` via `FuncLookupCtx::index` (the
  linear sweeps remain as fallback). A redundant enum re-scan in the
  match-expr path was removed.
- **Measured finding**: completion latency on large files is NOT scan
  bound anymore — it is result-volume bound. After `receiver.()` on an
  unresolvable receiver, the provider returns the whole symbol world
  (2,137 items ≈ 467 KB ≈ 37 ms in a 2,000-function file). Capping or
  narrowing that fallback is the next target.

## Unreleased — LSP indexed type inference in completion (Phase B, step 5)

- The static inference helpers of `CompletionProvider` — the
  `find*DeclForInference` set, `lookupFuncReturnType`, the enum/extend/
  bitstruct lookups inside `inferExprTypeName`, and the `Self`
  resolution in `collectLocalsFromStmts` — now go through the
  server-cached `FileSymbolIndex` via `FuncLookupCtx::index` (the
  linear sweeps remain as fallback). A redundant enum re-scan in the
  match-expr path was removed.
- **Measured finding**: completion latency on large files is NOT scan
  bound anymore — it is result-volume bound. After `receiver.()` on an
  unresolvable receiver, the provider returns the whole symbol world
  (2,137 items ≈ 467 KB ≈ 37 ms in a 2,000-function file). Capping or
  narrowing that fallback is the next target.

## Unreleased — LSP symbol-indexed finders in hover & completion (Phase B, step 4)

- The same-file `find*` helpers of `HoverProvider` (8) and
  `CompletionProvider` (7, incl. `findEnclosingMethod`) now consult the
  server-cached `FileSymbolIndex` instead of re-scanning `topLevelDecl()`
  per request, matching what go-to-definition got in step 1. The linear
  sweeps remain as fallback for direct provider calls.
- `FileSymbolIndex` grew `enclosingExtendMethodAt()` (extend-method spans)
  for `findEnclosingMethod`.
- Remaining completion cost (measured ~37 ms on a 2,000-function file)
  is dominated by the static collector/inference helpers that walk the
  tree per request — the next target.

## Unreleased — LSP symbol-indexed finders in hover & completion (Phase B, step 4)

- The same-file `find*` helpers of `HoverProvider` (8) and
  `CompletionProvider` (7, incl. `findEnclosingMethod`) now consult the
  server-cached `FileSymbolIndex` instead of re-scanning `topLevelDecl()`
  per request, matching what go-to-definition got in step 1. The linear
  sweeps remain as fallback for direct provider calls.
- `FileSymbolIndex` grew `enclosingExtendMethodAt()` (extend-method spans)
  for `findEnclosingMethod`.
- Remaining completion cost (measured ~37 ms on a 2,000-function file)
  is dominated by the static collector/inference helpers that walk the
  tree per request — the next target.

## Unreleased — LSP cached document index (Phase B, step 3)

- The token index, file symbol index and parsed doc-comments are now
  combined into a single `DocumentIndex` cached per URI by the server
  (rebuilt only when the document text changes).
- Go-to-definition reuses the cached symbol index (it no longer rebuilds
  one per request), and hover reuses the cached doc-comments (the whole
  source was re-scanned on every hover request). Measured on a
  2,000-function fixture with doc comments: go-to-definition
  6.7 ms → 4.6 ms, hover 4.5 ms → 3.9 ms.

## Unreleased — LSP cached document index (Phase B, step 3)

- The token index, file symbol index and parsed doc-comments are now
  combined into a single `DocumentIndex` cached per URI by the server
  (rebuilt only when the document text changes).
- Go-to-definition reuses the cached symbol index (it no longer rebuilds
  one per request), and hover reuses the cached doc-comments (the whole
  source was re-scanned on every hover request). Measured on a
  2,000-function fixture with doc comments: go-to-definition
  6.7 ms → 4.6 ms, hover 4.5 ms → 3.9 ms.

## Unreleased — LSP token index under the cursor (Phase B, step 2)

- New `TokenIndex`: tokens of a document indexed by line, built once per
  parse and cached by the server (same source-comparison policy as the
  `ParseCache`). `hover` and go-to-definition replace their per-request
  linear scan of the whole token stream with an O(tokens-on-line)
  lookup; the index is rebuilt only when the document text changes.
- The index is invalidated together with the parse cache on
  didOpen/didSave/didClose; didChange goes through the same
  source-comparison path as `getOrParse`.
- Remaining Phase B targets: the per-request `parseDocComments` re-scan
  and the enclosing-function body walks in hover, plus the same-file
  `find*` sweeps of HoverProvider/CompletionProvider (FileSymbolIndex
  already covers DefinitionProvider).

## Unreleased — LSP shared file symbol index (Phase B, step 1)

- New `FileSymbolIndex`: a per-file index of top-level declarations
  (functions, structs, enums, unions, bitstructs, type aliases, extend
  blocks, externs, consts) plus O(log n) enclosing-function lookup,
  built once per parse instead of re-scanning `tree->topLevelDecl()`
  on every request.
- `DefinitionProvider` (go-to-definition) now resolves same-file symbols
  through the index; every `find*` helper that used a linear sweep
  consults it first. Behaviour is identical — lookups keep
  "first declaration in source order" semantics, and the linear sweep
  remains as fallback.
- Next step (Phase B, step 2): the providers' per-request linear token
  scan (finding the token under the cursor) — the dominant remaining
  cost in go-to-definition on large files.

## Unreleased — LSP incremental project rebuild (Phase C)

- **Project files are now cached by mtime**: `ProjectContext` re-parses a
  file only when it actually changed on disk since the last rebuild.
  Import chains, the project scan, and transitive stdlib modules are
  reused across `didOpen`/`didSave` rebuilds, so editing latency no
  longer grows with project size. Measured on the traits fixture:
  cold rebuild 47 ms → warm rebuild 3 ms.
- **Fixed stale import-error diagnostics**: `importErrors_` was never
  cleared between rebuilds, so a circular-import error kept being
  re-published for the file even after the cycle was fixed. The vector
  is now reset at the start of every build.
- **No duplicate module registrations**: the anchor document is
  canonicalized up front, so the BFS and the directory scan always agree
  on a single path for it.
- Removed the dead `keepAlive`/`keptUnits_` API (superseded by
  `ExportedSymbol::treeAnchor`).

## Unreleased — LSP crash on malformed `impl` (parse error)

- **Fixed a pre-existing LSP SIGSEGV**: opening a document whose source
  contains an incomplete `impl` (e.g. `impl Hashable` with no
  `for Type { ... }`) crashed the server during the project rebuild. The
  parser recovers from the syntax error but leaves the `implDecl` tree
  node with fewer than two `typeSpec`s; `ModuleRegistry` and the checker
  dereferenced the missing one unconditionally.
- All `impl` consumers now tolerate malformed nodes (syntax errors are
  reported by the parser, never by a segfault): `ModuleRegistry::registerFile`,
  `Checker::checkImplDecl`, the imported-impl matching in `Checker_driver`,
  and `declareImplMethods` in `IRGen_decl`. Closing such a document no
  longer kills `lucis lsp`.

## Unreleased — `[]uint8` ↔ `string` casts

- **Zero-cost byte/string views**: `[]uint8 as string` and `string as []uint8`
  (ABI-identical `{ptr, len}`) — the natural bridge for `#[fuzz]` targets
  that parse text. Also fixes `[]T x = <slice>` variable declarations, which
  silently discarded the initializer (variable became `{null, 0}`).

## Unreleased — Project-wide test discovery

- **`lucis test` without an explicit file** now discovers tests beyond the
  entry's import graph: conventional test directories (`tests/`, `test/`,
  `specs/`, `spec/`, `unit/` — any `.lc` containing `#[test]`) and files
  named `*.test.lc` / `*_test.lc` anywhere in the project. Discovered files
  become extra pipeline entries; results are aggregated into a single test
  binary with deduplicated units and tests.

## Unreleased — LSP typing latency (Phase A)

- `textDocument/didChange` parses the document once (the cached
  `ParseResult` also feeds the publish) and **defers diagnostics** behind a
  debounce window: the full semantic check no longer runs per keystroke,
  only when the typing pauses (flushed by the next dispatch).
- `publishDiagnostics` caches results per source text, so unchanged text is
  republished without re-running the checker.
- `lsp_tests/test_diagnostics.py` added earlier in the hardening step;
  diagnostics content and the didOpen/didChange/didSave lifecycle are now
  covered.

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

## Unreleased — Fuzzing (`lucis fuzz`)

### Language Features

- **`#[fuzz]` attribute**: marks a function of type `fn([]uint8) void` as
  the program's fuzz target (cargo-fuzz style); at most one per program.

### CLI

- **`lucis fuzz`**: `init` (scaffold `fuzz/`), `run <target>` (AFL-style
  loop), `list`, `repro <input>` (replay a saved crash). Flags: `--corpus`,
  `--crash-dir`, `--iters`, `--timeout`, `--seed`, `-j` (parallel workers),
  `-v`. Exit codes: `0` no crashes, `1` crash(es) found/compile error,
  `2` usage.

### Internal

- IRGen fuzz-mode instruments every user basic block with
  `lucis_fuzz_trace` coverage hooks and synthesizes `main` +
  `lucis_fuzz_dispatch`; the C runner (`src/builtins/fuzz/fuzz_runner.c`)
  implements the coverage-guided loop (shared bitmap via fork-inherited
  mmap, havoc mutation stack, per-input isolation, delta-debugging
  minimization, crash capture with output).
- `make fuzz-example` validates end-to-end: a seeded bug must be found and
  minimized to 1 byte (`fuzz_example/`).

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
