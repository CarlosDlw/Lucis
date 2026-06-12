# std::thread

Threading utilities and concurrency primitives.

## Import

```
use std::thread::{ cpuCount, threadId, yield };
```

## Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `cpuCount` | `() -> int32` | Number of available CPU cores |
| `threadId` | `() -> int64` | Current thread's unique ID |
| `yield` | `() -> void` | Yield execution to other threads |

## Concurrency Primitives

The primary concurrency model uses `spawn`, `await`, `Task<T>`, and `Mutex` — these are language builtins, not part of `std::thread`.

```
// spawn a task
Task<int32> t = spawn computeResult();
int32 result = await t;

// mutex for shared state
Mutex<int32> counter = Mutex(0);
lock (counter) |*value| {
    *value = *value + 1;
}
```

See the [Concurrency guide](../language/concurrency.md) for full details on `spawn`, `await`, `Task<T>`, and `Mutex`.

## Example

```
use std::thread::{ cpuCount, threadId, yield };
use std::log::println;

println(cpuCount());               // 8
println(threadId());               // 140234567890

// cooperative yield
yield();
```

## See Also

- [Concurrency](../language/concurrency.md) — spawn, await, Task, Mutex
- [std::time](time.md) — Sleep functions
