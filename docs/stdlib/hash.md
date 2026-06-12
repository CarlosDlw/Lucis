# std::hash

Hashing utility functions for general-purpose and data structure use.

## Import

```
use std::hash::{ hashString, hashInt, hashCombine, hashBytes, crc32 };
```

## Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `hashString` | `(string) -> uint64` | Hash a string |
| `hashInt` | `(int64) -> uint64` | Hash an integer |
| `hashCombine` | `(uint64, uint64) -> uint64` | Combine two hashes |
| `hashBytes` | `(*uint8, int64) -> uint64` | Hash raw byte buffer |
| `crc32` | `(string) -> uint32` | CRC-32 checksum |

## Example

```
use std::hash::{ hashString, hashInt, hashCombine, crc32 };
use std::log::println;

uint64 h1 = hashString("hello");
uint64 h2 = hashInt(42);
uint64 combined = hashCombine(h1, h2);

println(h1);
println(h2);
println(combined);
println(crc32("hello world"));
```

## See Also

- [std::crypto](crypto.md) — Cryptographic hashing (MD5, SHA)
- [std::bits](bits.md) — Bitwise utilities
