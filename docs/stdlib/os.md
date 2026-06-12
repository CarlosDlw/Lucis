# std::os

Low-level operating system primitives and POSIX-like functions.

## Import

```
use std::os::{ getpid, hostname, errno, strerror, kill };
```

## Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `getpid` | `() -> int32` | Current process ID |
| `getppid` | `() -> int32` | Parent process ID |
| `getuid` | `() -> int32` | Current user ID |
| `getgid` | `() -> int32` | Current group ID |
| `hostname` | `() -> string` | System hostname |
| `pageSize` | `() -> int32` | Memory page size in bytes |
| `errno` | `() -> int32` | Last OS error code |
| `strerror` | `(int32) -> string` | Error string for error code |
| `kill` | `(int32, int32) -> int32` | Send signal to process |
| `dup` | `(int32) -> int32` | Duplicate file descriptor |
| `dup2` | `(int32, int32) -> int32` | Duplicate fd to target fd |
| `closeFd` | `(int32) -> int32` | Close file descriptor |

## Example

```
use std::os::{ getpid, getppid, getuid, hostname, pageSize, errno, strerror };
use std::log::println;

println(getpid());                 // 12345
println(getppid());                // 12300
println(getuid());                 // 1000
println(hostname());               // my-machine
println(pageSize());               // 4096

int32 err = errno();
println(strerror(err));            // Success
```

## See Also

- [std::process](process.md) — Higher-level process functions
- [std::mem](mem.md) — Memory management
