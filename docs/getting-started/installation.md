# Installation

This page covers how to build the Lux compiler (`lux`) from source on Linux.

## Prerequisites

T compiles to native code via LLVM. You need the following system dependencies installed before building:

| Dependency | Minimum Version | Purpose |
|---|---|---|
| **CMake** | 3.20+ | Build system |
| **C++ Compiler** | C++17 support (GCC 9+ or Clang 10+) | Compiles the compiler itself |
| **LLVM** | 15+ (recommended 18+) | Backend code generation and optimization |
| **ANTLR4 C++ Runtime** | 4.13+ | Parser infrastructure |
| **libclang** | — | C header parsing for FFI |
| **zlib** | — | Compression support (stdlib) |
| **pthreads** | — | Threading support (stdlib) |

### Installing Dependencies

**Arch Linux:**

```bash
sudo pacman -S cmake llvm clang lib32-llvm-libs zlib
```

For ANTLR4 C++ runtime on Arch, you may need to install from AUR:

```bash
yay -S antlr4-runtime-cpp
```

**Ubuntu / Debian:**

```bash
sudo apt install cmake llvm-dev libclang-dev libantlr4-runtime-dev zlib1g-dev
```

**Fedora:**

```bash
sudo dnf install cmake llvm-devel clang-devel antlr4-cpp-runtime-devel zlib-devel
```

## Building from Source

### 1. Clone the Repository

```bash
git clone https://github.com/yourusername/lux.git
cd lux
```

### 2. Build with Make (Recommended)

The project includes a Makefile that wraps the CMake workflow:

```bash
make build
```

This automatically runs `cmake -B build -DCMAKE_BUILD_TYPE=Debug` on the first invocation, then compiles the project in parallel.

### 3. Build Manually with CMake

If you prefer running CMake directly:

```bash
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . --parallel $(nproc)
```

### 4. Verify the Build

After a successful build, the compiler binary is located at `build/lux`:

```bash
./build/lux version
```

Expected output:

```
lux v0.0.1 beta
```

## Makefile Targets

| Target | Description |
|---|---|
| `make build` | Compile the project (auto-configures on first run) |
| `make configure` | Run CMake configuration only |
| `make rebuild` | Clean and recompile everything from scratch |
| `make clean` | Remove the `build/` directory |
| `make run FILE=path.lx` | Build and run a `.lx` file |
| `make help` | List all available targets |

## Troubleshooting

### ANTLR4 Runtime Not Found

```
ANTLR4 C++ runtime not found.
Install with: sudo apt install libantlr4-runtime-dev
```

The ANTLR4 C++ runtime headers and library must be installed. The build system searches `/usr/include/antlr4-runtime`, `/usr/local/include`, `/usr/lib`, and `/usr/local/lib`. If your installation is in a non-standard path, set the `CMAKE_PREFIX_PATH` variable:

```bash
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/antlr4 ..
```

### libclang Not Found

```
libclang not found.
Install with: sudo pacman -S clang  (or equivalent for your distro)
```

Make sure the `clang-c/Index.h` header and `libclang.so` library are installed. On Debian-based systems, install `libclang-dev`.

### LLVM Version Mismatch

The compiler supports LLVM 15 and above. LLVM 18+ is recommended for the best compatibility. If you have multiple LLVM versions installed, you can specify which one to use:

```bash
cmake -B build -DLLVM_DIR=/usr/lib/llvm-18/lib/cmake/llvm ..
```

## See Also

- [Hello World](hello-world.md) — Write and run your first Lux program
- [CLI Usage](cli-usage.md) — Compiler flags and options
