# CLI Usage

This page documents all command-line options for the Lux compiler (`lux`).

## Synopsis

```
lux <command> [args...]

Commands:
  build       Compile a Lux project into a native binary
  run         Compile and execute a Lux program via JIT
  check       Type-check a Lux project without generating code
  test        Run the Lux test suite
  help        Show help for a command
  helpc       C library reference helper
```

Legacy positional mode (`lux <file.lx> [<output>]`) is deprecated and will be
removed in a future release. Use `lux build <file> [-o <output>]` instead.

## Global Flags

```
--help, -h     Show help
--version, -v  Show version
## build — Compile to Binary

```
lux build <file> [-o <output>] [-O <level>] [--lto]
               [--emit-llvm] [--emit-asm] [--emit-bc] [--emit-obj]
               [--static] [--shared] [--fPIC]
               [-l <lib>] [-L <dir>] [-I <dir>] [-q]
```

Compiles the project to a native binary. Without `-o`, the output defaults to
`<input-stem>.out`.

| Flag | Description |
|------|-------------|
| `-o, --output <FILE>` | Output file path (binary, IR, asm, bitcode, or object) |
| `-O, --opt <LEVEL>` | Optimization level: `0`, `1`, `2`, `3`, `s`, `z`, or `fast` (default: `0`) |
| `--lto` | Enable Link Time Optimization (LTO) |
| `--emit-llvm` | Emit LLVM IR (`.ll`) |
| `--emit-asm` | Emit assembly (`.s`) |
| `--emit-bc` | Emit LLVM bitcode (`.bc`) |
| `--emit-obj` | Emit object file (`.o`) |
| `--static` | Produce a statically linked executable |
| `--shared` | Produce a shared library (`.so`, `.dll`) |
| `--fPIC` | Generate position-independent code (PIC) |
| `-l, --link <LIB>` | Link against a library (repeatable) |
| `-L, --lib-path <DIR>` | Add library search path (repeatable) |
| `-I, --include <DIR>` | Add include search path (repeatable) |
| `-q, --quiet` | Suppress pipeline logs |
...
# Build a shared library (automatically enables --fPIC)
lux build module.lx --shared -o libmodule.so

# Build a static binary
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

**Emit Modes:**
By default, `--emit-llvm` and `--emit-asm` print to **stdout**. If `-o` is provided, the output is redirected to the specified file.
`--emit-obj` requires `-o` to specify the object file path; otherwise, it writes to the build directory.

```bash
# Standard build
lux build main.lx -o ./main -O2

# Build a shared library (automatically enables --fPIC)
lux build module.lx --shared -o libmodule.so

# Build a static binary
lux build main.lx --static -o main_static

# Emit Assembly to file
lux build main.lx --emit-asm -o main.s

# Emit LLVM IR to stdout
lux build main.lx --emit-llvm | less

# Size-optimized build with LTO
lux build main.lx -Oz --lto
```

## run — JIT Execution

```
lux run <file> [-O <level>] [--lto] [-l <lib>] [-L <dir>] [-I <dir>] [-q] [-- args...]
```

Compiles and immediately executes the program via LLVM JIT — no binary is
written to disk.

| Flag | Description |
|------|-------------|
| `-O, --opt <LEVEL>` | Optimization level: `0`, `1`, `2`, `3`, `s`, `z`, or `fast` |
| `--lto` | Enable Link Time Optimization for JIT |

```bash
lux run main.lx -O3
lux run app.lx --lto -- arg1 arg2
```

## check — Type Checking

```
lux check <file> [-I <dir>] [-q]
```

Runs the parser and semantic checker without generating any code or binary.

```bash
lux check main.lx
lux check main.lx -q
```

## test — Run Test Suite

```
lux test [filter] [-l] [-q]
```

Runs the Lux project's test suite (expects a `tests/main.lx` in the project).

| Flag | Description |
|------|-------------|
| `filter` | Optional substring match to filter tests |
| `-l, --list` | List available test files |
| `-q, --quiet` | Suppress output |

```bash
lux test
lux test -q
lux test structs
```

## help — Command Help

```
lux help [command]
```

Shows general help or help for a specific command.

```bash
lux help
lux help build
```

## helpc — C Library Reference

```
lux helpc <lib> [symbol] [flags]
```

Inspects C library headers and displays type information mapped to Lux
equivalents.

```bash
lux helpc raylib InitWindow
lux helpc stdio printf
lux helpc math sin --json
```

See [helpc](../tools/helpc.md) for the full reference.

## Project Scanning

When you run `lux build <file>` or `lux run <file>`, the compiler scans the
directory containing `<file>` for all `.lx` files. Every `.lx` file must have a
`namespace` declaration. Files are compiled together to support cross-file
namespace resolution.

## Build Artifacts

The compiler creates a `.lux/build/` directory in the project root for
intermediate object files:

```
project/
├── main.lx
├── utils.lx
└── .lux/
    └ build/
      ├── Main__main.o
      └── Utils__utils.o
```

These files are reused across compilations. You can safely delete `.lux/build/`
to force a clean rebuild.

## Compiler Pipeline

When you run `lux build`, the compiler performs these steps:

1. **Scan** — Discover all `.lx` files in the project directory
2. **Parse** — Parse each file using the ANTLR4 grammar
3. **Register** — Build a namespace registry for cross-file module resolution
4. **Resolve Headers** — Process `#include` directives
5. **Compile C** — Compile local `.c` files to object files
6. **Check** — Run semantic analysis and type checking
7. **Generate IR** — Emit LLVM intermediate representation
8. **Optimize** — Apply LLVM optimization passes (if `-O` is specified)
9. **Emit Objects** — Write `.o` object files to `.lux/build/`
10. **Link** — Invoke the system linker to produce the final native binary

## Error Messages

```
lux: unknown flag '--xyz'
```

Unknown command-line flag. Check the flag spelling.

```
lux: no .lx files found in '/path/to/project'
```

No Lux source files found in the directory containing your input file.

```
lux: file 'module.lx' is missing a 'namespace' declaration
```

Every `.lx` file must begin with a `namespace` declaration.

```
lux: cannot find header '<mylib.h>'. Check '-I' include paths
```

A `#include` directive references a header that cannot be found. Add the
correct `-I` path.

## See Also

- [Installation](installation.md) — Build the compiler from source
- [Hello World](hello-world.md) — Your first Lux program
- [Linking](../ffi/linking.md) — Detailed guide on linking with C libraries
