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

## Global Flags

```
--help, -h               Show help
--version, -v            Show version
--print-builtins-path    Print the path to the builtins library
```

## Config / CLI override

- **CLI flags override** matching `lucis.yaml` values when both are present.
- **`--ignore-config`** skips config detection entirely (mutually exclusive with `--config`).
- **`--config <FILE>`** loads a specific config file instead of auto-detecting.

## init — Create a New Project

```
lucis init [path]
```

| Flag | Description |
|------|-------------|
| `path` | Project path or directory name (default: `.`) |

```
lucis init my-app
lucis init .
```

## build — Compile to Binary

### Flags grouped by stage

```
lucis build [<file>] [flags]
```

**Compilation:**
| Flag | Description |
|------|-------------|
| `-O, --opt <LEVEL>` | Optimization: `0`, `1`, `2`, `3`, `s`, `z`, `fast` (default: `0`) |
| `--target <TRIPLE>` | LLVM target triple (e.g. `x86_64-unknown-none`) |
| `--no-std` | Build without standard library (freestanding/kernel) |
| `--lto` | Enable Link Time Optimization |
| `--fPIC` | Generate position-independent code |
| `-I, --include <DIR>` | Add include search path (repeatable) |

**Assembly:**
| Flag | Description |
|------|-------------|
| `--asm <FILE>` | Assembly source file `.s`/`.asm` (repeatable) |
| `--assembler <nasm\|as>` | Assembler program (default: try nasm, fallback as) |
| `--assembler-arg <FLAG>` | Flag for the assembler (repeatable) |

**Direct inputs:**
| Flag | Description |
|------|-------------|
| `--obj <FILE>` | Pre-compiled object file `.o` (repeatable) |
| `--lib <FILE>` | Library file `.a`/`.so` (repeatable) |
| `-l, --link <LIB>` | Link against a system library (repeatable) |
| `-L, --lib-path <DIR>` | Add library search path (repeatable) |

**Link:**
| Flag | Description |
|------|-------------|
| `--linker <PATH>` | Linker program (default: gcc/clang for host, ld for freestanding) |
| `--linker-script <FILE>` | Linker script (`-T`) |
| `--linker-entry <SYMBOL>` | Entry point symbol (`-e`) |
| `--linker-nmagic` | Suppress page alignment in linker (`-n`) |
| `--linker-omagic` | Set text segment writable (`-N`) |
| `--linker-gc-sections` | Garbage collect unused sections at link time |
| `--linker-arg <FLAG>` | Pass raw argument to linker (repeatable) |
| `--rpath <DIR>` | Add runtime library search path |
| `--static` | Produce a statically linked executable |
| `--shared` | Produce a shared library |

**Post-process:**
| Flag | Description |
|------|-------------|
| `--strip` | Strip debug/symbol info from output binary |
| `--emit-llvm` | Emit LLVM IR (`.ll`) |
| `--emit-asm` | Emit assembly (`.s`) from LLVM |
| `--emit-bc` | Emit LLVM bitcode (`.bc`) |
| `--emit-obj` | Emit object file (`.o`) and stop |
| `--emit-bin` | Emit raw binary (`.bin`) via objcopy |
| `-r, --recursive` | Include all modules in emit output |

**Output:**
| Flag | Description |
|------|-------------|
| `-o, --output <FILE>` | Output path (default: `<input>.out`) |

**General:**
| Flag | Description |
|------|-------------|
| `-q, --quiet` | Suppress pipeline logs |
| `--config <FILE>` | Path to `lucis.yaml` configuration |
| `--ignore-config` | Ignore `lucis.yaml`, use CLI flags only |

**Deprecated aliases (accepted with warning):**
| Old flag | Use instead |
|----------|-------------|
| `--entry <SYMBOL>` | `--linker-entry <SYMBOL>` |
| `--nmagic` | `--linker-nmagic` |
| `--omagic` | `--linker-omagic` |
| `--gc-sections` | `--linker-gc-sections` |
| `--link-arg <FLAG>` | `--linker-arg <FLAG>` |

### Examples

```bash
# Standard build
lucis build main.lc -o app -O2

# Shared library
lucis build lib.lc --shared -o libfoo.so

# Static binary
lucis build main.lc --static -o main_static

# Kernel / bare-metal with assembly
lucis build main.lc --asm boot.s --assembler nasm \
  --target x86_64-unknown-none --no-std \
  --linker ld --linker-script linker.ld --linker-entry _start --linker-nmagic \
  -o kernel.elf

# Using lucis.yaml from a project directory
lucis build

# Build without config (ad-hoc)
lucis build --ignore-config main.lc -o app

# Emit LLVM IR to stdout
lucis build main.lc --emit-llvm | less
```

## run — Compile and Execute

```
lucis run [<file>] [-O <level>] [--lto] [-c] [-l <lib>] [-L <dir>] [-I <dir>]
          [--ignore-config] [-q] [-- args...]
```

| Flag | Description |
|------|-------------|
| `-O, --opt <LEVEL>` | Optimization level |
| `--lto` | Enable Link Time Optimization |
| `-c, --clean` | Clear run cache before compiling |
| `-l, --link <LIB>` | Link against a library (repeatable) |
| `-L, --lib-path <DIR>` | Add library search path (repeatable) |
| `-I, --include <DIR>` | Add include search path (repeatable) |
| `--ignore-config` | Ignore `lucis.yaml` |
| `-q, --quiet` | Suppress pipeline logs |
| `-- args...` | Arguments forwarded to the compiled program |

```bash
lucis run main.lc -O3
lucis run --lto -- arg1 arg2
lucis run -c -q
```

## check — Type Checking

```
lucis check [<file>] [-I <dir>] [--ignore-config] [-q]
```

| Flag | Description |
|------|-------------|
| `-I, --include <DIR>` | Add include search path (repeatable) |
| `--ignore-config` | Ignore `lucis.yaml` |
| `-q, --quiet` | Suppress pipeline logs |

```bash
lucis check main.lc
lucis check -q
```

## help — Command Help

```
lucis help [command]
```

```bash
lucis help build
```

---

## Project Configuration (lucis.yaml)

Every CLI flag has a corresponding `lucis.yaml` key, and vice‑versa.
The file is generated by `lucis init` and can be edited manually.

### Full schema

```yaml
name: my-app                    # Project name (required)
version: "0.1.0"               # Project version

# Tool paths (cross-compilation, override PATH)
tools:
  nasm: /usr/bin/nasm           # Assembler path
  ld: /usr/bin/ld.bfd           # Linker path
  objcopy: /usr/bin/objcopy     # Used by --strip, --emit-bin

# Assembly stage
assembly:
  files:                        # --asm (repeatable)
    - boot.s
  assembler: nasm               # --assembler <nasm|as>
  flags:                        # --assembler-arg (repeatable)
    - -f elf64

# Source directories
source:
  - src/

# Compiler options
build:
  target: ""                    # --target
  opt_level: O2                 # -O (default: O0)
  no_std: false                 # --no-std
  lto: false                    # --lto
  static: false                 # --static
  shared: false                 # --shared
  fpic: true                    # --fPIC
  code_model: ""                # LLVM code model
  include_paths:                # -I (repeatable)
    - include/
  defines:                      # Preprocessor defines (-D)
    DEBUG: "1"
    VERSION: '"0.1.0"'

# Pre-compiled inputs
inputs:
  objects:                      # --obj (repeatable)
    - lib/foo.o
  static_libs:                  # --lib (repeatable)
    - lib/libdriver.a
  shared_libs:                  # --lib for .so
    - lib/libexternal.so

# Linker options
linker:
  program: ""                   # --linker (default: gcc/clang/ld)
  script: ""                    # --linker-script
  entry: ""                     # --linker-entry
  libs: []                      # -l (repeatable)
  lib_paths: []                 # -L (repeatable)
  flags: []                     # --linker-nmagic, --linker-omagic, etc.
  args: []                      # --linker-arg (repeatable)

# Output / emit
output:
  path: ""                      # -o
  strip: false                  # --strip
  emit_bin: false               # --emit-bin
  emit_llvm: false              # --emit-llvm
  emit_asm: false               # --emit-asm
  emit_bc: false                # --emit-bc
  emit_obj: false               # --emit-obj

# Pre/post build scripts
scripts:
  env:                          # Environment variables for scripts
    LUCIS_ARCH: x86_64
  pre: []                       # Commands before build
  pos: []                       # Commands after successful link

# Run defaults (lucis run)
run:
  opt_level: O0
  lto: false
  args: []
```

### Key mapping: yaml ↔ CLI

| Yaml key | CLI flag |
|----------|----------|
| `assembly.files[]` | `--asm <file>` |
| `assembly.assembler` | `--assembler <nasm\|as>` |
| `assembly.flags[]` | `--assembler-arg <flag>` |
| `build.target` | `--target <triple>` |
| `build.opt_level` | `-O <level>` |
| `build.no_std` | `--no-std` |
| `build.lto` | `--lto` |
| `build.static` | `--static` |
| `build.shared` | `--shared` |
| `build.fpic` | `--fPIC` |
| `build.include_paths[]` | `-I <dir>` |
| `inputs.objects[]` | `--obj <file>` |
| `inputs.static_libs[]` | `--lib <file>` |
| `inputs.shared_libs[]` | `--lib <file>` |
| `linker.program` | `--linker <path>` |
| `linker.script` | `--linker-script <file>` |
| `linker.entry` | `--linker-entry <sym>` |
| `linker.libs[]` | `-l <lib>` |
| `linker.lib_paths[]` | `-L <dir>` |
| `linker.flags[]` | `--linker-nmagic`, `--linker-omagic`, `--linker-gc-sections` |
| `linker.args[]` | `--linker-arg <flag>` |
| `output.path` | `-o <file>` |
| `output.strip` | `--strip` |
| `output.emit_bin` | `--emit-bin` |
| `output.emit_llvm` | `--emit-llvm` |
| `output.emit_asm` | `--emit-asm` |
| `output.emit_bc` | `--emit-bc` |
| `output.emit_obj` | `--emit-obj` |
| `tools.nasm` | `--assembler` (fallback) |
| `tools.ld` | `--linker` (fallback) |

### Pre/post scripts

Scripts run as shell commands with these environment variables set:

| Variable | Set by | Available in |
|----------|--------|-------------|
| `LUCIS_PROJECT_ROOT` | Compiler | pre, pos |
| `LUCIS_BUILD_DIR` | Compiler | pre, pos |
| `LUCIS_TARGET` | Compiler | pre, pos |
| `LUCIS_OUTPUT` | Compiler | pos only |
| `LUCIS_*` (from `scripts.env`) | Config | pre, pos |

## Build Pipeline

```
assembly  →  compilation (LC → LLVM IR)  →  link  →  post-process
  │                │                           │          │
 nasm/as         lucis                      ld/gcc    objcopy/strip
```

1. **Assembly** — `.s`/`.asm` files are assembled with `nasm` or `as`
2. **Compilation** — `.lc` sources are compiled to LLVM IR, optimized, and emitted as objects
3. **Link** — Objects + libraries are linked into an ELF/PE/Mach-O binary
4. **Post-process** — Optional `objcopy` for strip or raw binary emit

## Build Artifacts

```
project/
├── src/
│   └── main.lc
├── lucis.yaml
└── .lucis/
    └── build/
        ├── __main.o
        └── cache/
            ├── build_manifest.txt
            └── semantic.db
```

## Error Messages

```
lucis: unknown flag '--xyz'
```
Unknown command-line flag.

```
lucis: no input file specified and no lucis.yaml found
```
No `.lc` file provided and no config found.

```
lucis: error: --ignore-config and --config are mutually exclusive
```
Both flags cannot be used together.

## See Also

- [Installation](installation.md) — Build the compiler from source
- [Hello World](hello-world.md) — Your first Lucis program
- [Linking](../ffi/linking.md) — Detailed guide on linking with C libraries
