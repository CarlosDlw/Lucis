# Modules

T uses a `use` declaration system to import symbols from the standard library and user-defined modules.

---

## Importing from the Standard Library

The standard library is organized into modules under `std::`. Individual symbols are imported with `use`:

```
use std::log::println;
use std::math::sqrt;
use std::mem::alloc;
```

### Grouped Imports

Multiple symbols from the same module can be imported in a single `use` declaration with braces:

```
use std::log::{ println, print, dbg };
```

> **Note:** `vec`, `map`, and `set` are native keywords — no import required.

### Available Standard Library Modules

| Module | Description |
|--------|-------------|
| `std::log` | Output: `println`, `print`, `eprint`, `eprintln`, `dbg`, `sprintf` |
| `std::io` | Input: `readLine`, `readInt`, `prompt`, etc. |
| `std::math` | Math functions and constants: `PI`, `sqrt`, `sin`, etc. |
| `std::str` | String utilities: `split`, `parseInt`, `parseFloat`, etc. |
| `std::mem` | Memory: `alloc`, `free`, `copy`, `zero`, etc. |
| `std::random` | Random: `randInt`, `randFloat`, `randBool`, etc. |
| `std::time` | Time: `now`, `sleep`, `clock`, etc. |
| `std::fs` | File system: `readFile`, `writeFile`, `exists`, etc. |
| `std::process` | Process: `exit`, `env`, `exec`, etc. |
| `std::conv` | Conversion: `itoa`, `atoi`, `toHex`, etc. |
| `std::hash` | Hashing: `hashString`, `crc32`, etc. |
| `std::bits` | Bit ops: `popcount`, `ctz`, `clz`, etc. |
| `std::ascii` | ASCII: `isAlpha`, `toUpper`, etc. |
| `std::path` | Path ops: `join`, `parent`, `extension`, etc. |
| `std::fmt` | Formatting: `lpad`, `hex`, `commas`, etc. |
| `std::regex` | Regex: `match`, `find`, `regexReplace`, etc. |
| `std::encoding` | Encoding: `base64EncodeStr`, `urlEncode`, etc. |
| `std::os` | OS: `getpid`, `hostname`, `errno`, etc. |
| `std::test` | Testing: `assertEqual`, `assertTrue`, `log`, etc. |
| `std::crypto` | Crypto: `sha256String`, `hmacSha256`, etc. |
| `std::compress` | Compression: `gzipCompress`, `deflate`, etc. |
| `std::net` | Networking: `tcpConnect`, `tcpSend`, etc. |
| `std::thread` | Threading: `cpuCount`, `Task`, `Mutex`, etc. |

> **Note:** `vec<T>`, `map<K,V>`, and `set<T>` are native keywords — no import needed.

---

## Importing from User Modules

User-defined functions can be imported from other `.lc` files using the namespace name:

```
// math.lc
namespace Math;

fn add(int32 a, int32 b) int32 {
    ret a + b;
}

fn multiply(int32 a, int32 b) int32 {
    ret a * b;
}
```

```
// main.lc
namespace Main;

use Math::add;
use Math::multiply;

fn main() int32 {
    println(add(3, 4));       // 7
    println(multiply(5, 6));  // 30
    ret 0;
}
```

Both files are passed to the compiler together. The `use` declaration resolves symbols by namespace name, not by file path.

---

## Including C Headers

C headers are included with the `#include` directive (not `use`):

```
#include <stdio.h>       // System header
#include "mylib.h"       // Local header
```

Functions, structs, enums, and constants from the header are automatically parsed via libclang and made available in scope.

`use` and `#include` can appear in any order. When a C header and a `use` import declare a function with the same name, the **last declaration in file order wins**. The recommended pattern is to place `#include` before `use`, so Lucis stdlib functions naturally override C equivalents:

```
#include <stdio.h>              // C sprintf (int32)
use std::log::{ println, sprintf };  // Lucis sprintf (string) ← wins

fn main() int32 {
    string msg = sprintf("val={}", 42);
    println(msg);
    ret 0;
}
```

See [Calling C Functions](../ffi/calling-c.md#name-conflicts-between-c-and-lucis) for details.

---

## Global Builtins

Some functions are available without any `use` import:

| Function | Description |
|----------|-------------|
| `exit(int32)` | Exit the program |
| `panic(string)` | Abort with message |
| `assert(bool)` | Abort if false |
| `assertMsg(bool, string)` | Abort if false, with message |
| `unreachable()` | Mark unreachable code |
| `toString(T)` | Convert any primitive to string |
| `toInt(string)` | Parse string to int64 |
| `toFloat(string)` | Parse string to float64 |
| `toBool(string)` | Parse string to bool |
| `cstr(string)` | Convert to C string `*char` |
| `fromCStr(*char)` | Convert from C string |
| `fromCStrCopy(*char)` | Convert from C string with owned copy |
| `fromCStrLen(*char, usize)` | Convert from C string with length |
| `freeStr(string)` | Free an owned string buffer, including values returned by `fromCStrCopy`, `sprintf`, and string methods that allocate a new string |

---

## See Also

- [Namespaces](namespaces.md) — Namespace declaration and multi-file projects
- [Syntax](syntax.md) — General syntax rules
