# Linking

When your Lucis program calls C functions, the compiler needs to link against the libraries that provide them. This page explains how linking works, the default libraries, and how to add custom ones.

---

## How Linking Works

After compiling your `.lc` files to machine code, the compiler invokes a linker (usually `clang` or `gcc`) to combine:

1. Your compiled Lucis code (object files)
2. The T runtime builtins (string, collections, etc.)
3. External C libraries (system or custom)

Into a single executable binary.

---

## Default Libraries

The compiler links these libraries automatically — you don't need to specify them:

| Library | Flag | Provides |
|---------|------|----------|
| libc | (implicit) | `printf`, `malloc`, `puts`, `strlen`, etc. |
| libm | `-lm` | `sqrt`, `sin`, `cos`, `pow`, `floor`, etc. |
| libz | `-lz` | Compression (used by `std::compress`) |
| libpthread | `-lpthread` | Threading (used by `spawn`, `Mutex`) |

Because these are linked by default, you can call math functions, use compression, and spawn threads without any extra flags.

---

## Linker Flags

### `-l` — Link a Library

The `-l` flag tells the linker to search for a shared or static library:

```bash
lucis main.lc ./main -lssl -lcrypto
```

This searches for `libssl.so` (or `libssl.a`) and `libcrypto.so` (or `libcrypto.a`) in the system's library search paths.

**Examples:**

```bash
# Link with SQLite
lucis main.lc ./main -lsqlite3

# Link with cURL
lucis main.lc ./main -lcurl

# Link with multiple libraries
lucis main.lc ./main -lssl -lcrypto -lcurl
```

### `-L` — Add Library Search Path

If the library is in a non-standard directory, use `-L` to add the path:

```bash
lucis main.lc ./main -L/opt/mylibs -lmylib
```

The linker searches the specified directory for `libmylib.so` or `libmylib.a`.

```bash
# Library in a local build directory
lucis main.lc ./main -L./build/lib -lmyengine

# Library in a custom prefix
lucis main.lc ./main -L/usr/local/lib -lspecial
```

### `-I` — Add Include Search Path

If your C headers are in a non-standard directory, use `-I` to add an include search path:

```bash
lucis main.lc ./main -I/opt/mylibs/include
```

This lets `#include <mylib.h>` find headers that aren't in `/usr/include`.

```bash
# Include path for a local project
lucis main.lc ./main -I./vendor/include -L./vendor/lib -lvendor

# Multiple include paths
lucis main.lc ./main -I/opt/ssl/include -I/opt/curl/include -lssl -lcurl
```

---

## Complete Workflow Example

### Using a System Library (SQLite)

1. Install the library:

```bash
# Ubuntu/Debian
sudo apt install libsqlite3-dev

# Fedora
sudo dnf install sqlite-devel

# macOS
brew install sqlite
```

2. Write your Lucis code with `extern` declarations or `#include`:

```
namespace SQLiteDemo;

extern int32 sqlite3_libversion_number();
extern *char sqlite3_libversion();

fn main() int32 {
    *char version = sqlite3_libversion();
    printf(c"SQLite version: %s\n", version);
    printf(c"Version number: %d\n", sqlite3_libversion_number());
    ret 0;
}
```

3. Compile and link:

```bash
lucis main.lc ./main -lsqlite3
./main
```

### Using a Local C Library

1. Create your C library:

```c
// mymath.h
#ifndef MYMATH_H
#define MYMATH_H

#define PI_APPROX 314159

int add(int a, int b);
int multiply(int a, int b);

#endif
```

```c
// mymath.c
#include "mymath.h"

int add(int a, int b) { return a + b; }
int multiply(int a, int b) { return a * b; }
```

2. Use it from Lucis with `#include "..."`:

```
namespace LocalLib;

#include <stdio.h>
#include "mymath.h"

fn main() int32 {
    int32 sum = add(10, 20);
    int32 prod = multiply(6, 7);
    printf(c"add = %d, multiply = %d\n", sum, prod);  // 30, 42

    int32 pi = PI_APPROX;
    printf(c"PI_APPROX = %d\n", pi);  // 314159

    ret 0;
}
```

3. Compile and run — the `.c` file is compiled automatically:

```bash
lucis main.lc ./main
./main
```

No `-l` flag needed. The compiler finds `mymath.c` next to `mymath.h` and compiles + links it automatically.

---

## Auto-Compilation of Local C Sources

When you use `#include "header.h"` with a local header, the compiler automatically checks if a matching `.c` file exists:

| Include Directive | Looks For | Action |
|-------------------|-----------|--------|
| `#include "mymath.h"` | `mymath.c` | Auto-compiles to `.o` and links |
| `#include "utils/helper.h"` | `utils/helper.c` | Auto-compiles to `.o` and links |
| `#include <stdio.h>` | (system) | No auto-compilation |

The auto-compilation uses the first available C compiler (`cc`, `clang`, or `gcc`) and places the resulting object file in the build directory.

This means a typical project structure with C interop looks like:

```
my_project/
├── main.lc              # Lucis source
├── native_lib.h         # C header (parsed by compiler)
├── native_lib.c         # C implementation (auto-compiled)
└── build/
    ├── main.o           # compiled Lucis code
    ├── c__native_lib.o  # auto-compiled C code
    └── main             # linked executable
```

---

## Static vs Dynamic Linking

By default, the linker prefers **dynamic linking** (shared libraries, `.so` files). This means the library must be present at runtime.

To force static linking of a specific library, the library must have a `.a` archive available:

```bash
# If only libmylib.a exists in the search path, it links statically
lucis main.lc ./main -L./lib -lmylib
```

The linker resolves in order of preference:
1. Shared library (`.so`) — loaded at runtime
2. Static library (`.a`) — embedded in the executable

---

## Troubleshooting

### "undefined reference to ..."

The function is declared (via `extern` or `#include`) but the library providing it isn't linked:

```bash
# Missing -lm for math functions
lucis main.lc ./main           # error: undefined reference to 'sqrt'
lucis main.lc ./main -lm       # works (but -lm is already default)
```

### "cannot find -lxxx"

The library isn't installed or isn't in the search path:

```bash
# Library not found
lucis main.lc ./main -lmissing    # error: cannot find -lmissing

# Fix: install it or add -L path
sudo apt install libmissing-dev
lucis main.lc ./main -lmissing    # works
```

### "fatal error: 'xxx.h' file not found"

The header isn't in the include search path:

```bash
# Header not found
lucis main.lc ./main              # error: 'mylib.h' not found

# Fix: add -I path
lucis main.lc ./main -I/opt/mylib/include
```

---

## See Also

- [FFI Overview](overview.md) — Introduction to FFI
- [Calling C Functions](calling-c.md) — `extern` and `#include` usage
- [CLI Usage](../getting-started/cli-usage.md) — All compiler flags
