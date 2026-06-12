# std::log

Output functions for printing to stdout and stderr.

## Import

```
use std::log::println;
use std::log::{ println, print, eprint, eprintln, dbg, sprintf };
```

## Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `println` | `(T) -> void` | Print value followed by newline to stdout |
| `print` | `(T) -> void` | Print value to stdout (no newline) |
| `eprint` | `(T) -> void` | Print value to stderr (no newline) |
| `eprintln` | `(T) -> void` | Print value followed by newline to stderr |
| `dbg` | `(T) -> void` | Debug print with type and value info |
| `sprintf` | `(string, ...) -> string` | Format string with values and return result |

All print functions accept any primitive type (`int32`, `float64`, `bool`, `char`, `string`, etc.) and also C strings (`*char`).

## Examples

```
use std::log::{ println, print, dbg, sprintf };

*char name = c"Carlos";
println(name);        // Carlos\n
println("hello");     // hello\n
println(42);          // 42\n
println(3.14);        // 3.14\n
println(true);        // true\n

print("no newline");

dbg(42);              // debug output with type info

string msg = sprintf("x = {}, y = {}", 10, 3.14);
println(msg);         // x = 10, y = 3.14
```

## See Also

- [std::fmt](fmt.md) — Advanced string formatting
- [std::io](io.md) — Input functions
