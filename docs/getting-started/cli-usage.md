# CLI Usage

This page documents all command-line options for the Lucis compiler (`lucis`).

## Synopsis

```
lucis <command> [args...]

Commands:
  build       Compile a Lucis project into a native binary
  run         Compile and execute a Lucis program
  check       Type-check a Lucis project without generating code
  test        Run the Lucis test suite (not yet implemented)
  help        Show help for a command
  helpc       C library reference helper
  init        Create a new Lucis project
```

Legacy positional mode (`lucis <file.lc> [<output>]`) is deprecated and will be
removed in a future release. Use `lucis build <file> [-o <output>]` instead.

## Global Flags

```
--help, -h               Show help
--version, -v            Show version
--print-builtins-path    Print the path to the builtins library
```

## CLI / Config Isolation

The CLI and `lucis.yaml` have a strict isolation rule:

- **Explicit file argument** (`lucis build foo.lc`): config is **completely ignored**.
  All behavior is controlled by CLI flags. This is the "standalone" mode.
- **No file argument** (`lucis build` from a project directory): config **drives the
  pipeline** — entrypoint, output binary name, output directory, optimization level,
  source directories, linker libraries, and include paths are taken from `lucis.yaml`.
  CLI flags override individual config values.

This ensures clean behavior regardless of whether you're in a project directory
or compiling a standalone file.

## init — Create a New Project

```
lucis init [path]
```

Creates a new Lucis project at the given path (directory name, `.` for current
directory, `../relative/path`, etc.). If the directory does not exist, it is
created automatically along with a `src/` directory and a starter `src/main.lc`.

A `lucis.yaml` project configuration file is generated with sensible defaults.
It controls the project name, binary output, source directories, build options,
and linker settings.

| Flag | Description |
|------|-------------|
| `path` | Project path or directory name (default: `.`) |

```
lucis init my-app
lucis init .
lucis init ../projects/hello
```

## build — Compile to Binary

```
lucis build [<file>] [-o <output>] [-O <level>] [--lto]
               [--emit-llvm] [--emit-asm] [--emit-bc] [--emit-obj] [--emit-bin]
               [--static] [--shared] [--fPIC]
               [--no-std] [--target <TRIPLE>] [--entry <SYMBOL>]
               [--link-arg <FLAG>] [--rpath <DIR>]
               [--nmagic] [--omagic] [--linker-script <FILE>] [--linker <PATH>]
               [-l <lib>] [-L <dir>] [-I <dir>] [-q] [-r]
```

Compiles the project to a native binary. If `<file>` is omitted, the compiler
looks for a `lucis.yaml` in the current directory and auto-resolves the
entrypoint from the configured source paths. Without `-o`, the output defaults
to `<input-stem>.out`. If `binary` is set in `lucis.yaml`, that name is used
verbatim (no `.out` appended). If `out_dir` is set, the binary is placed in
that directory (relative to the project root).

All emit flags (`--emit-llvm`, `--emit-asm`, `--emit-bc`, `--emit-obj`, `--emit-bin`) are
**CLI-only** — they are not configured in `lucis.yaml`. Without `-o`, text emits
(LLVM IR, assembly) print to **stdout**; bitcode, object, and binary emits use an
auto-generated file path. With `-o`, all emits write to the given file.

The `--emit-bin` flag requires a full build + link to produce an ELF executable
first, then runs `objcopy -O binary` to extract the raw binary. This is useful
for freestanding/kernel targets that need a flat binary (e.g. for bootloaders).

When at least one emit flag is active (except `--emit-bin`), the build stops after
generating the requested output — no linking or binary is produced. `--emit-bin`
always runs a full build + link before objcopy. With no emit flags, a normal
binary is produced.

| Flag | Description |
|------|-------------|
| `-o, --output <FILE>` | Output file path (binary, IR, asm, bitcode, or object) |
| `-O, --opt <LEVEL>` | Optimization level: `0`, `1`, `2`, `3`, `s`, `z`, or `fast` (default: `0`) |
| `--lto` | Enable Link Time Optimization (LTO) |
| `--emit-llvm` | Emit LLVM IR (`.ll`) to stdout or `-o` file |
| `--emit-asm` | Emit assembly (`.s`) to stdout or `-o` file |
| `--emit-bc` | Emit LLVM bitcode (`.bc`) |
| `--emit-obj` | Emit object file (`.o`) |
| `--emit-bin` | Emit raw binary (`.bin`) via `objcopy -O binary` |
| `-r, --recursive` | Include all modules in emit output (works with `--emit-*`) |
| `--no-std` | Build without standard library (freestanding/kernel) |
| `--target <TRIPLE>` | Set LLVM target triple (e.g. `x86_64-unknown-none`) |
| `--entry <SYMBOL>` | Set the entry point symbol (default: `main`; passed as `-Wl,-e`) |
| `--static` | Produce a statically linked executable |
| `--shared` | Produce a shared library (`.so`, `.dll`) |
| `--fPIC` | Generate position-independent code (PIC) |
| `--link-arg <FLAG>` | Pass argument directly to linker (repeatable) |
| `--rpath <DIR>` | Add runtime library search path |
| `-l, --link <LIB>` | Link against a library (repeatable) |
| `-L, --lib-path <DIR>` | Add library search path (repeatable) |
| `-I, --include <DIR>` | Add include search path (repeatable) |
| `--nmagic` | Suppress page alignment in linker (equivalent to `ld -n`) |
| `--omagic` | Set text segment writable (equivalent to `ld -N`) |
| `--linker-script <FILE>` | Use a custom linker script (passed as `-T` to the linker) |
| `--linker <PATH>` | Use a custom linker instead of the default clang/gcc (useful for cross-compilation with `ld`, `x86_64-elf-ld`, etc.) |
| `-q, --quiet` | Suppress pipeline logs |

```bash
# Standard build
lucis build main.lc -o ./main -O2

# Build a shared library (automatically enables --fPIC)
lucis build module.lc --shared -o libmodule.so

# Build a static binary
lucis build main.lc --static -o main_static

# Emit LLVM IR to stdout
lucis build main.lc --emit-llvm | less

# Emit Assembly to file
lucis build main.lc --emit-asm -o main.s

# Emit object file
lucis build main.lc --emit-obj -o main.o

# Size-optimized build with LTO
lucis build main.lc -Oz --lto

# Build a freestanding kernel with assembly boot code
lucis build main.lc boot.s --no-std --nmagic --target x86_64-unknown-none --linker-script linker.ld --linker ld --entry _start -o kernel.elf
```

**Optimization Levels:**
- `0-3`: Standard optimization levels.
- `s`: Optimize for size, balancing performance.
- `z`: Optimize aggressively for size.
- `fast`: Enable aggressive optimizations (O3 + fast-math).

**Linkage Control:**
- `--static`: Produce a statically linked executable.
    * **Note**: Requires static versions of system dependencies (e.g., `zlib-static`, `glibc-static`) installed on your system.
- `--shared`: Produce a shared library (`.so`, `.dll`).
- `--fPIC`: Generate position-independent code (PIC). Automatically enabled with `--shared`.
- `--nmagic`: Suppress page alignment in the linker (`ld -n`). Useful for kernels and bare-metal where section alignment constraints are undesirable.
- `--omagic`: Make the text segment writable (`ld -N`). Disables page alignment and allows self-modifying code in freestanding environments.

## run — Compile and Execute

```
lucis run [<file>] [-O <level>] [--lto] [-c] [-l <lib>] [-L <dir>] [-I <dir>] [-q] [-- args...]
```

Compiles and immediately executes the program — the compiled binary is cached
in `.lucis/cache/` and reused on subsequent runs when the source hasn't changed.
If `<file>` is omitted, the entrypoint is auto-resolved from `lucis.yaml`.

| Flag | Description |
|------|-------------|
| `-O, --opt <LEVEL>` | Optimization level: `0`, `1`, `2`, `3`, `s`, `z`, or `fast` |
| `--lto` | Enable Link Time Optimization |
| `-c, --clean` | Clear run cache before compiling |
| `-l, --link <LIB>` | Link against a library (repeatable) |
| `-L, --lib-path <DIR>` | Add library search path (repeatable) |
| `-I, --include <DIR>` | Add include search path (repeatable) |
| `-q, --quiet` | Suppress pipeline logs |
| `-- args...` | Arguments forwarded to the compiled program |

```bash
lucis run main.lc -O3
lucis run app.lc --lto -- arg1 arg2
lucis run app.lc -c -q              # clear cache before run
```

## check — Type Checking

```
lucis check [<file>] [-I <dir>] [-q]
```

Runs the parser and semantic checker without generating any code or binary.
If `<file>` is omitted, the entrypoint is auto-resolved from `lucis.yaml`.

| Flag | Description |
|------|-------------|
| `-I, --include <DIR>` | Add include search path (repeatable) |
| `-q, --quiet` | Suppress pipeline logs |

```bash
lucis check main.lc
lucis check -q
```

## test — Run Test Suite

```
lucis test
```

**Not yet implemented.** Prints a message and exits. A proper test runner with
test discovery, filtering, and per-test reporting is planned for a future release.

## help — Command Help

```
lucis help [command]
```

Shows general help or help for a specific command.

```bash
lucis help
lucis help build
```

## helpc — C Library Reference

```
lucis helpc <lib> [symbol] [flags]
```

Inspects C library headers and displays type information mapped to Lucis
equivalents.

```bash
lucis helpc raylib InitWindow
lucis helpc stdio printf
lucis helpc math sin --json
```

See [helpc](../tools/helpc.md) for the full reference.

## Project Configuration (lucis.yaml)

When a `lucis.yaml` file exists in the project root, the compiler and LSP use
it to define project boundaries and settings. The file is generated by
`lucis init` and can be edited manually.

```yaml
name: my-app                    # Project name (required)
version: "0.0.1"               # Project version

binary: my-app                  # Output binary name (default: <name>)
out_dir: build                  # Build output directory

source:                         # Source directories for use resolution
  - src/

build:                          # Build defaults (overridden by CLI flags)
  opt_level: O2                 # O0, O1, O2, O3, Os, Oz, Ofast
  lto: false                    # Enable Link Time Optimization
  static: false                 # Static linking
  shared: false                 # Shared library
  fpic: true                    # Position-independent code
  no_std: false                 # Build without standard library (freestanding)
  target: ""                    # LLVM target triple for cross-compilation
  code_model: ""                # Code model (e.g. "kernel")
  entry: ""                     # Entry point symbol (default: main)

run:                            # Run defaults (overridden by CLI flags)
  opt_level: O0
  lto: false
  args: []                      # Default program arguments

linker:                         # Linker settings
  libs: []                      # Libraries to link (-l)
  lib_paths: []                 # Library search paths (-L)

scripts:                        # Pre/post build hooks
  pre: []                       # Shell commands to run before compilation
  pos: []                       # Shell commands to run after successful link

includes: []                    # Include paths for C FFI (-I)
```

## Build Artifacts

The compiler creates a `.lucis/build/` directory in the project root for
intermediate object files:

```
project/
├── src/
│   └── main.lc
├── lucis.yaml
└── .lucis/
    ├── build/
    │   ├── src__main__main.o
    │   └── cache/
    │       ├── build_manifest.txt
    │       └── semantic.db
    └── cache/
        └── run-<hash>.bin
```

These files are reused across compilations. You can safely delete `.lucis/`
to force a clean rebuild.

## Compiler Pipeline

When you run `lucis build`, the compiler performs these steps:

1. **Resolve Project Root** — Walk up from the input file looking for `lucis.yaml` or `.git`
2. **Resolve Imports** — BFS import resolution starting from the entry point; only
   files referenced by `use` are opened (no directory scanning)
3. **Parse** — Parse each file using the ANTLR4 grammar
4. **Register** — Build a module registry for cross-file symbol resolution
5. **Resolve Headers** — Process `#include` directives and auto-discover C source files
6. **Check** — Run semantic analysis and type checking on all units
7. **Generate IR** — Emit LLVM intermediate representation for each unit
8. **Optimize** — Apply LLVM optimization passes (if `-O` is specified)
9. **Emit Objects** — Write `.o` object files to `.lucis/build/`
10. **Link** — Invoke the system linker to produce the final native binary

## Error Messages

```
lucis: unknown flag '--xyz'
```

Unknown command-line flag. Check the flag spelling.

```
lucis: no input file specified and no lucis.yaml found
```

No `.lc` file provided and no `lucis.yaml` found in the current directory or
any parent directory.

```
lucis: cannot find header '<mylib.h>'. Check '-I' include paths
```

A `#include` directive references a header that cannot be found. Add the
correct `-I` path.

## See Also

- [Installation](installation.md) — Build the compiler from source
- [Hello World](hello-world.md) — Your first Lucis program
- [Linking](../ffi/linking.md) — Detailed guide on linking with C libraries
