# std::bits

Bitwise utility functions for integer manipulation.

## Import

```
use std::bits::{ popcount, ctz, clz, rotl, rotr, isPow2, nextPow2 };
```

## Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `popcount` | `(int32) -> int32` | Count set bits (1s) |
| `ctz` | `(int32) -> int32` | Count trailing zeros |
| `clz` | `(int32) -> int32` | Count leading zeros |
| `rotl` | `(int32, int32) -> int32` | Rotate bits left |
| `rotr` | `(int32, int32) -> int32` | Rotate bits right |
| `bswap` | `(int32) -> int32` | Reverse byte order |
| `isPow2` | `(int32) -> bool` | Check if power of two |
| `nextPow2` | `(int32) -> int32` | Next power of two ≥ value |
| `bitReverse` | `(int32) -> int32` | Reverse all bits |
| `extractBits` | `(int32, int32, int32) -> int32` | Extract bit field (value, start, length) |
| `setBit` | `(int32, int32) -> int32` | Set bit at position |
| `clearBit` | `(int32, int32) -> int32` | Clear bit at position |
| `toggleBit` | `(int32, int32) -> int32` | Toggle bit at position |
| `testBit` | `(int32, int32) -> bool` | Test bit at position |

## Example

```
use std::bits::{ popcount, ctz, clz, isPow2, nextPow2, setBit, testBit };
use std::log::println;

println(popcount(0b1010_1010));    // 4
println(ctz(0b1000));              // 3
println(clz(1));                   // 31
println(isPow2(64));               // true
println(isPow2(65));               // false
println(nextPow2(50));             // 64

int32 flags = 0;
flags = setBit(flags, 3);         // set bit 3
println(testBit(flags, 3));        // true
```

## See Also

- [std::math](math.md) — Mathematical functions
- [std::hash](hash.md) — Hashing utilities
