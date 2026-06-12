# Concurrency

Lux provides thread-based concurrency with `spawn`, `await`, `Task<T>`, `Mutex`, and the `lock` statement. Threads are implemented using pthreads under the hood.

---

## `spawn` — Create a Task

The `spawn` keyword launches a function call on a new thread and returns a `Task<T>`:

```
use std::thread::Task;

fn compute(int32 a, int32 b) int32 {
    ret a + b;
}

Task<int32> t = spawn compute(10, 20);
```

The spawned function runs concurrently. `T` in `Task<T>` matches the return type of the function.

---

## `await` — Wait for Result

The `await` keyword blocks until the task completes and returns its result:

```
Task<int32> t = spawn compute(10, 20);
int32 result = await t;
println(result);   // 30
```

---

## Parallel Execution

Multiple tasks can run in parallel:

```
fn heavyWork() int32 {
    int32 sum = 0;
    for int32 i = 0; i < 1000; i++ {
        sum += i;
    }
    ret sum;
}

Task<int32> t1 = spawn heavyWork();
Task<int32> t2 = spawn heavyWork();

int32 r1 = await t1;
int32 r2 = await t2;
println(r1);   // 499500
println(r2);   // 499500
```

Both calls to `heavyWork()` execute concurrently on separate threads.

---

## `Mutex` — Mutual Exclusion

A `Mutex` protects shared state from concurrent access:

```
use std::thread::Mutex;

Mutex mtx = Mutex();
```

---

## `lock` — Critical Section

The `lock` statement acquires the mutex for the duration of a block:

```
Mutex mtx = Mutex();
int32 counter = 0;

lock(mtx) {
    counter += 1;
}

println(counter);   // 1
```

The mutex is automatically released when the block exits.

---

## Thread Utilities

The `std::thread` module provides additional utilities:

```
use std::thread::cpuCount;
use std::thread::threadId;
use std::thread::yield;
```

| Function | Signature | Description |
|----------|-----------|-------------|
| `cpuCount` | `() -> uint32` | Number of available CPU cores |
| `threadId` | `() -> uint64` | Current thread's ID |
| `yield` | `()` | Yield the current thread's time slice |

```
uint32 cores = cpuCount();
println(cores);   // e.g., 8

uint64 tid = threadId();
println(tid);     // thread ID

yield();          // let other threads run
```

---

## Complete Example

```
namespace ConcurrencyDemo;

use std::log::println;
use std::thread::{ Task, Mutex, cpuCount };

fn compute(int32 a, int32 b) int32 {
    ret a + b;
}

fn main() int32 {
    println(cpuCount());

    // Spawn parallel tasks
    Task<int32> t1 = spawn compute(10, 20);
    Task<int32> t2 = spawn compute(30, 40);

    int32 r1 = await t1;
    int32 r2 = await t2;
    println(r1);   // 30
    println(r2);   // 70

    // Protected shared state
    Mutex mtx = Mutex();
    int32 counter = 0;

    lock(mtx) {
        counter += r1 + r2;
    }

    println(counter);   // 100

    ret 0;
}
```

---

## See Also

- [Generics](generics.md) — `Task<T>` type parameter
- [Functions](functions.md) — Function declarations used with `spawn`
- [Memory Management](memory-management.md) — `defer` for cleanup in concurrent code
