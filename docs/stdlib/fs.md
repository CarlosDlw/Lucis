# std::fs

File system operations for reading, writing, and manipulating files and directories.

## Import

```
use std::fs::{ readFile, writeFile, exists, mkdir, listDir };
```

## Functions

### File I/O

| Function | Signature | Description |
|----------|-----------|-------------|
| `readFile` | `(string) -> string` | Read entire file as string |
| `writeFile` | `(string, string) -> void` | Write string to file (creates/overwrites) |
| `appendFile` | `(string, string) -> void` | Append string to file |
| `readBytes` | `(string) -> vec<uint8>` | Read file as byte vector |
| `writeBytes` | `(string, vec<uint8>) -> void` | Write bytes to file |

### Queries

| Function | Signature | Description |
|----------|-----------|-------------|
| `exists` | `(string) -> bool` | Whether path exists |
| `isFile` | `(string) -> bool` | Whether path is a regular file |
| `isDir` | `(string) -> bool` | Whether path is a directory |
| `fileSize` | `(string) -> int64` | File size in bytes |

### File Operations

| Function | Signature | Description |
|----------|-----------|-------------|
| `remove` | `(string) -> bool` | Delete a file |
| `rename` | `(string, string) -> bool` | Rename/move a file |

### Directory Operations

| Function | Signature | Description |
|----------|-----------|-------------|
| `mkdir` | `(string) -> bool` | Create a directory |
| `mkdirAll` | `(string) -> bool` | Create directory and parents |
| `removeDir` | `(string) -> bool` | Remove empty directory |
| `listDir` | `(string) -> vec<string>` | List directory entries |

### Working Directory

| Function | Signature | Description |
|----------|-----------|-------------|
| `cwd` | `() -> string` | Current working directory |
| `setCwd` | `(string) -> bool` | Change working directory |
| `tempDir` | `() -> string` | System temp directory |

## Example

```
use std::fs::{ writeFile, readFile, exists, mkdir, remove, removeDir };
use std::log::println;

mkdir("/tmp/myapp");
writeFile("/tmp/myapp/data.txt", "hello world");

println(exists("/tmp/myapp/data.txt"));    // true
println(readFile("/tmp/myapp/data.txt"));   // hello world

remove("/tmp/myapp/data.txt");
removeDir("/tmp/myapp");
```

## See Also

- [std::path](path.md) — Path manipulation
- [std::io](io.md) — stdin/stdout I/O
