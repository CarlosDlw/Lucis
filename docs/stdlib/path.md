# std::path

Path manipulation functions for constructing and analyzing file paths.

## Import

```
use std::path::{ join, parent, fileName, extension, normalize };
```

## Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `join` | `(string, string) -> string` | Join two path segments |
| `joinAll` | `(vec<string>) -> string` | Join multiple path segments |
| `parent` | `(string) -> string` | Parent directory |
| `fileName` | `(string) -> string` | File name with extension |
| `stem` | `(string) -> string` | File name without extension |
| `extension` | `(string) -> string` | File extension (without dot) |
| `isAbsolute` | `(string) -> bool` | Whether path is absolute |
| `isRelative` | `(string) -> bool` | Whether path is relative |
| `normalize` | `(string) -> string` | Resolve `.` and `..` segments |
| `toAbsolute` | `(string) -> string` | Convert to absolute path |
| `separator` | `() -> char` | Path separator (`/` on Unix) |
| `withExtension` | `(string, string) -> string` | Replace extension |
| `withFileName` | `(string, string) -> string` | Replace file name |

## Example

```
use std::path::{ join, parent, fileName, stem, extension, normalize };
use std::log::println;

println(join("/home/user", "docs"));             // /home/user/docs
println(parent("/home/user/file.txt"));           // /home/user
println(fileName("/home/user/file.txt"));         // file.txt
println(stem("/home/user/file.txt"));             // file
println(extension("/home/user/file.txt"));        // txt
println(normalize("/home/user/../docs/./f.txt")); // /home/docs/f.txt
```

## See Also

- [std::fs](fs.md) — File system operations
