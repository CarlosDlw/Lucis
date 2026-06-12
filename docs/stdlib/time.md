# std::time

Functions for time measurement, sleeping, and date/time access.

## Import

```
use std::time::{ now, sleep, clock, year, month, day, formatTime };
```

## Functions

### Current Time

| Function | Signature | Description |
|----------|-----------|-------------|
| `now` | `() -> int64` | Current time in milliseconds since epoch |
| `nowNanos` | `() -> int64` | Current time in nanoseconds |
| `nowMicros` | `() -> int64` | Current time in microseconds |
| `clock` | `() -> float64` | High-resolution clock in seconds |
| `timestamp` | `() -> int64` | Unix timestamp in seconds |

### Date Components

| Function | Signature | Description |
|----------|-----------|-------------|
| `year` | `() -> int32` | Current year |
| `month` | `() -> int32` | Current month (1-12) |
| `day` | `() -> int32` | Current day of month (1-31) |
| `hour` | `() -> int32` | Current hour (0-23) |
| `minute` | `() -> int32` | Current minute (0-59) |
| `second` | `() -> int32` | Current second (0-59) |
| `weekday` | `() -> int32` | Day of week (0=Sunday, 6=Saturday) |

### Utilities

| Function | Signature | Description |
|----------|-----------|-------------|
| `sleep` | `(int64) -> void` | Sleep for milliseconds |
| `sleepMicros` | `(int64) -> void` | Sleep for microseconds |
| `elapsed` | `(int64) -> int64` | Milliseconds since given timestamp |
| `formatTime` | `(string) -> string` | Format current time with strftime pattern |
| `parseTime` | `(string, string) -> int64` | Parse time string to timestamp |

## Example

```
use std::time::{ now, clock, sleep, year, month, day, formatTime, elapsed };
use std::log::println;

float64 start = clock();

println(year());                   // 2025
println(month());                  // 6
println(day());                    // 27

int64 t = now();
sleep(100);
println(elapsed(t));               // ~100

println(formatTime("%Y-%m-%d"));   // 2025-06-27

float64 end = clock();
println(end - start);              // elapsed seconds
```

## See Also

- [std::process](process.md) — Process control
