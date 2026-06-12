# std::mem

Low-level memory management functions.

## Import

```
use std::mem::{ alloc, free, copy, zero, set };
```

## Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `alloc` | `(int64) -> *uint8` | Allocate N bytes (uninitialized) |
| `allocZeroed` | `(int64) -> *uint8` | Allocate N bytes (zeroed) |
| `realloc` | `(*uint8, int64) -> *uint8` | Resize allocation |
| `free` | `(*uint8) -> void` | Free allocated memory |
| `copy` | `(*uint8, *uint8, int64) -> void` | Copy N bytes (dest, src, count) |
| `move` | `(*uint8, *uint8, int64) -> void` | Move N bytes (handles overlap) |
| `set` | `(*uint8, uint8, int64) -> void` | Fill N bytes with value |
| `zero` | `(*uint8, int64) -> void` | Zero N bytes |
| `compare` | `(*uint8, *uint8, int64) -> int32` | Compare N bytes (like memcmp) |

## Example

```
use std::mem::{ alloc, free, copy, zero, set };
use std::log::println;

// Allocate 100 bytes
*uint8 buf = alloc(100);
defer free(buf);

// Zero the buffer
zero(buf, 100);

// Fill with 0xFF
set(buf, 0xFF as uint8, 100);

// Copy data
*uint8 buf2 = alloc(100);
defer free(buf2);
copy(buf2, buf, 100);
```

> **Note:** For most use cases, prefer `vec<T>`, `map<K,V>`, or `set<T>` which handle allocation automatically. Use `std::mem` only when you need direct control over memory.

## See Also

- [std::os](os.md) — OS primitives
- [Memory Management](../language/memory-management.md) — Defer and auto-cleanup
