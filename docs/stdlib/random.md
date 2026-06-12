# std::random

Pseudorandom number generation functions.

## Import

```
use std::random::{ seed, seedTime, randInt, randFloat, randBool, uuid_v4 };
```

## Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `seed` | `(int64) -> void` | Set PRNG seed to specific value |
| `seedTime` | `() -> void` | Seed PRNG from current time |
| `randInt` | `() -> int64` | Random 64-bit integer |
| `randIntRange` | `(int64, int64) -> int64` | Random integer in [min, max] |
| `randUint` | `() -> uint64` | Random unsigned 64-bit integer |
| `randFloat` | `() -> float64` | Random float in [0.0, 1.0) |
| `randFloatRange` | `(float64, float64) -> float64` | Random float in [min, max) |
| `randBool` | `() -> bool` | Random boolean |
| `randChar` | `() -> char` | Random printable ASCII character |
| `uuid_v4` | `() -> string` | Random UUID v4 string |

## Example

```
use std::random::{ seedTime, randInt, randIntRange, randFloat, randBool, uuid_v4 };
use std::log::println;

seedTime();

println(randInt());                // -1234567890
println(randIntRange(1, 100));     // 42
println(randFloat());              // 0.731284
println(randBool());               // true
println(randIntRange(0, 6) + 1);   // dice roll: 1-6
println(uuid_v4());                // "123e4567-e89b-42d3-a456-426614174000"
```

> **Note:** Call `seedTime()` or `seed(n)` before generating random numbers. Without seeding, output is deterministic.

## See Also

- [std::math](math.md) — Mathematical functions
- [std::crypto](crypto.md) — Cryptographic random bytes
