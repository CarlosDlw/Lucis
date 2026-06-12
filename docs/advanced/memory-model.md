# Memory Model

Lucis uses a straightforward memory model: local variables live on the stack, heap-backed values (`string`, `vec`, `map`, `set`) allocate dynamic storage when needed, and cleanup is handled through explicit `defer` plus automatic scope-based cleanup. There is no garbage collector.

This page explains how each part works and what the compiler does behind the scenes.

---

## Stack Allocation

All local variables are stack-allocated using LLVM `alloca` instructions. This includes scalars, fixed-size arrays, struct values, and pointers to heap data.

```
fn main() int32 {
    int32 x = 42;                   // 4 bytes on stack
    float64 pi = 3.14159;           // 8 bytes on stack
    bool flag = true;               // 1 byte on stack
    int32[5] arr = [1, 2, 3, 4, 5]; // 20 bytes on stack (contiguous)

    ret 0;
}
```

The compiler tracks each local variable through an internal table that stores:
- The `alloca` pointer (where the variable lives on the stack)
- Type information (used for type-checking and cleanup decisions)
- Array dimensions (0 for scalar, 1 for `[]T`, 2 for `[][]T`, etc.)

### Generated LLVM IR

For the code above, the compiler emits:

```llvm
%x   = alloca i32
%pi  = alloca double
%flag = alloca i1
%arr = alloca [5 x i32]

store i32 42, i32* %x
store double 3.14159, double* %pi
store i1 true, i1* %flag
; array elements stored individually
```

**Key points:**
- Stack allocation is instant (just a pointer bump)
- Size is determined at compile time
- Lifetime is from function entry to function exit
- No manual freeing needed

### Structs on the Stack

Custom structs are also stack-allocated by default:

```
struct Point {
    float64 x;
    float64 y;
}

fn main() int32 {
    Point p = Point { x: 10.0, y: 20.0 };  // 16 bytes on stack
    ret 0;
}
```

```llvm
%p = alloca %struct.Point    ; { double, double }
; fields stored via GEP (getelementptr) instructions
```

---

## Heap Allocation

Collections (`vec`, `map`, `set`) allocate their internal data on the heap. The collection header (pointer + metadata) lives on the stack, but the element storage is heap-allocated via `malloc`.

### vec\<T\> Layout

A `vec<T>` is a 3-field struct:

```
┌─────────────────────────────────────────────┐
│  vec<T> header (stack)                      │
│  ┌──────────┬──────────┬──────────┐         │
│  │ ptr      │ len      │ cap      │         │
│  │ (void*)  │ (size_t) │ (size_t) │         │
│  └────┬─────┴──────────┴──────────┘         │
│       │                                     │
│       ▼  heap                               │
│  ┌────┬────┬────┬────┬────┬────┬────┬────┐  │
│  │ e0 │ e1 │ e2 │ e3 │    │    │    │    │  │
│  └────┴────┴────┴────┴────┴────┴────┴────┘  │
│  ◄─── len = 4 ──►    ◄── unused ──►        │
│  ◄──────── cap = 8 ────────────────►        │
└─────────────────────────────────────────────┘
```

- **ptr**: Pointer to heap-allocated element array
- **len**: Number of elements currently stored
- **cap**: Total allocated capacity

**Growth strategy:** Initial capacity is 8. When full, capacity doubles (growth factor 2x). Reallocation uses `realloc`, which may copy data to a new location.

```
vec<int32> v = [1, 2, 3];    // cap=8, len=3
v.push(4);                    // cap=8, len=4
v.push(5);                    // cap=8, len=5
// ... push until 8 ...
v.push(9);                    // cap=16, len=9 (doubled)
```

### map\<K, V\> Layout

A `map<K, V>` uses open addressing with quadratic probing:

```
┌──────────────────────────────────────────────────────┐
│  map<K, V> header (stack)                            │
│  ┌────────┬──────┬────────┬────────┐                 │
│  │ states │ keys │ values │ hashes │                  │
│  │ (ptr)  │(ptr) │ (ptr)  │ (ptr)  │                  │
│  ├────────┼──────┼────────┼────────┤                 │
│  │ len    │ cap  │key_size│val_size│                  │
│  │(usize) │(usize)│(usize)│(usize)│                  │
│  └────────┴──────┴────────┴────────┘                 │
│                                                      │
│  Heap arrays (4 separate allocations):               │
│  states:  [0, 1, 0, 0, 1, 2, 0, ...]  (0=empty,    │
│                                         1=occupied,  │
│                                         2=deleted)   │
│  keys:    [_, k0, _, _, k1, _, _, ...]               │
│  values:  [_, v0, _, _, v1, _, _, ...]               │
│  hashes:  [_, h0, _, _, h1, _, _, ...]               │
└──────────────────────────────────────────────────────┘
```

- Initial capacity: 16
- Resize when load factor exceeds 75%
- Type-agnostic storage: `key_size` and `val_size` determine element stride

### set\<T\> Layout

A `set<T>` is similar to `map` but without the values array:

```
┌──────────────────────────────────────────┐
│  set<T> header (stack)                   │
│  ┌────────┬──────┬────────┐              │
│  │ states │ keys │ hashes │              │
│  │ (ptr)  │(ptr) │ (ptr)  │              │
│  ├────────┼──────┼────────┤              │
│  │ len    │ cap  │key_size│              │
│  │(usize) │(usize)│(usize)│              │
│  └────────┴──────┴────────┘              │
└──────────────────────────────────────────┘
```

### String Layout

Strings are immutable fat pointers:

```
┌─────────────────────────┐
│  string (stack)         │
│  ┌──────┬──────┐        │
│  │ ptr  │ len  │        │
│  │(char*)│(usize)│       │
│  └──┬───┴──────┘        │
│     │                   │
│     ▼  read-only data   │
│  "hello world"          │
└─────────────────────────┘
```

- **ptr**: Pointer to UTF-8 character data
- **len**: Length in bytes
- String literals are stored in the binary's read-only data section
- Operations like concatenation return new strings — no in-place mutation
- No explicit `.free()` needed for string literals

---

## Defer Mechanism

The `defer` statement schedules cleanup code to run at function exit, in LIFO (last-in-first-out) order. This guarantees resources are freed even if the function exits early via `ret` or an exception.

### How It Works

When the compiler encounters a `defer` statement, it does **not** emit any code immediately. Instead, it pushes the statement onto an internal defer stack. At every return point in the function, the compiler emits all deferred statements in reverse order.

```
fn processData() void {
    vec<int32> v = [1, 2, 3];
    defer v.free();              // pushed first

    int32* ptr = alloc(4);
    defer free(ptr);             // pushed second

    *ptr = 42;
    println(*ptr);

    // At function exit, defer stack unwinds:
    // 1. free(ptr)     ← last deferred, runs first
    // 2. v.free()      ← first deferred, runs last
}
```

### Defer Stack Internals

The compiler maintains a vector of deferred statements:

```
deferStack_ = [
    { callCtx: v.free() },     // index 0
    { callCtx: free(ptr) },    // index 1
]
```

At every `ret` statement, the compiler iterates this list in reverse (`rbegin` → `rend`), emitting the IR for each deferred call. This means the deferred code is **duplicated** at each return point — there's no shared cleanup block.

### Multiple Return Paths

Defer works correctly even with early returns:

```
fn readFile(string path) int32 {
    int32 fd = open(path);
    defer close(fd);             // guaranteed to run

    string data = read(fd);
    if data.len() == 0 {
        ret -1;                  // close(fd) emitted here
    }

    process(data);
    ret 0;                       // close(fd) emitted here too
}
```

The compiler duplicates the deferred cleanup at **both** return points, ensuring `close(fd)` always executes regardless of which path is taken.

---

## Automatic Cleanup

In addition to explicit `defer`, the compiler automatically inserts cleanup calls for heap-backed local values (`string`, `vec`, `map`, `set`). This means you don't need to manually call `.free()` or `freeStr()` for normal local-scope lifetimes.

### How It Works

Before return and at block/control-flow scope exits, after deferred cleanups when applicable, the compiler scans locals that are leaving scope and emits the type-specific cleanup calls.

| Type | Cleanup function called |
|------|-------------------------|
| `string` | `lucis_freeStr()` |
| `vec<int32>` | `lucis_vec_free_i32()` |
| `vec<string>` | `lucis_vec_free_str()` |
| `map<string, int32>` | `lucis_map_free_str_i32()` |
| `set<float64>` | `lucis_set_free_f64()` |

### Cleanup Order

The full cleanup sequence at every return point is:

1. **Explicit defers** — emitted in reverse order (LIFO)
2. **Auto cleanup** — compiler-generated free calls for all collections

```
fn example() void {
    vec<int32> a = [1, 2, 3];
    map<string, int32> m;
    defer println("goodbye");

    // At ret:
    // 1. println("goodbye")          ← defer
    // 2. lucis_vec_free_i32(&a)     ← auto cleanup
    // 3. lucis_map_free_str_i32(&m) ← auto cleanup
}
```

### Skip Variable

When a function returns a collection value, the compiler skips auto-cleanup for that specific variable to avoid freeing memory that's being returned to the caller:

```
fn createVec() vec<int32> {
    vec<int32> result = [1, 2, 3];
    ret result;   // result is NOT auto-freed — ownership transfers to caller
}
```

---

## Exception Handling Memory

T uses `setjmp`/`longjmp` for exception handling, with a thread-local stack of exception handler frames.

### Error Structure

```
┌─────────────────────────────┐
│  lucis_error               │
│  ┌─────────────────────┐    │
│  │ message (string)    │    │
│  │   ptr + len         │    │
│  ├─────────────────────┤    │
│  │ file (string)       │    │
│  │   ptr + len         │    │
│  ├─────────────────────┤    │
│  │ line (int32)        │    │
│  └─────────────────────┘    │
└─────────────────────────────┘
```

### Handler Frame Stack

Each `try` block pushes a frame onto a thread-local linked list:

```
Thread-local eh_stack:

┌──────────┐    ┌──────────┐    ┌──────────┐
│ frame 3  │───►│ frame 2  │───►│ frame 1  │───► NULL
│ jmp_buf  │    │ jmp_buf  │    │ jmp_buf  │
│ error    │    │ error    │    │ error    │
│ active   │    │ active   │    │ active   │
└──────────┘    └──────────┘    └──────────┘
     ▲
     │
  eh_stack (head)
```

### Flow

1. **`try` block** — pushes a new frame, calls `setjmp()` to save the CPU state
2. **Normal execution** — code runs normally; frame is popped at end of `try`
3. **`panic()` called** — fills the top frame's error fields, calls `longjmp()` to jump back to the saved state
4. **`catch` block** — receives the error, frame is popped

```
try {
    // setjmp() saved here
    panic("something went wrong");
    // longjmp() jumps back to setjmp location
} catch (Error e) {
    println(e.message);   // "something went wrong"
    println(e.file);      // "main.lc"
    println(e.line);      // line number of panic()
}
```

### Defer and Exceptions

Deferred statements execute even when an exception occurs. Before `longjmp` unwinds to the catch handler, the compiler ensures all defers in the current scope run:

```
try {
    vec<int32> data = [1, 2, 3];
    defer data.free();

    panic("error");
    // data.free() executes before jumping to catch
} catch (Error e) {
    println(e.message);   // data was already freed
}
```

---

## Memory Layout Summary

| Type | Location | Size (bytes) | Cleanup |
|------|----------|-------------|---------|
| `int8` / `uint8` | Stack | 1 | None |
| `int16` / `uint16` | Stack | 2 | None |
| `int32` / `uint32` | Stack | 4 | None |
| `int64` / `uint64` | Stack | 8 | None |
| `float32` | Stack | 4 | None |
| `float64` | Stack | 8 | None |
| `bool` | Stack | 1 | None |
| `char` | Stack | 1 | None |
| `string` | Stack (fat ptr) | 16 | Auto-drop for owned locals; borrowed/literals are not freed |
| `int32[N]` | Stack | N × 4 | None |
| `struct` | Stack | Sum of fields + padding | None |
| `vec<T>` | Stack header + heap data | 24 + (cap × elem_size) | Auto or `defer v.free()` |
| `map<K,V>` | Stack header + heap data | 64 + (cap × (1 + key + val + 8)) | Auto or `defer m.free()` |
| `set<T>` | Stack header + heap data | 48 + (cap × (1 + key + 8)) | Auto or `defer s.free()` |
| Pointer (`T*`) | Stack | 8 | Manual (`free()`) |

---

## Best Practices

**Use `defer` for non-collection resources:**
```
int32 fd = open("file.txt");
defer close(fd);
// use fd...
```

**Let auto-cleanup handle collections when possible:**
```
fn compute() void {
    vec<int32> v = [1, 2, 3];
    // no need for defer v.free() — auto-cleanup handles it
    println(v.len());
}
```

**Use `defer` when you need guaranteed ordering:**
```
fn transaction() void {
    lock(mutex);
    defer unlock(mutex);       // always unlocked, even on panic

    vec<int32> data = loadData();
    defer data.free();         // freed before unlock

    process(data);
}
```

**Avoid returning pointers to stack-allocated data:**
```
// BAD — pointer to stack memory is invalid after return
fn bad() -> int32* {
    int32 x = 42;
    ret &x;            // dangling pointer!
}

// GOOD — return by value
fn good() -> int32 {
    int32 x = 42;
    ret x;
}
```

---

## Ownership State Tracking

The checker tracks ownership state for drop-tracked locals:

- `Owned`: value must be dropped or moved.
- `Borrowed*`: non-owning reference semantics.
- `Moved`: value has been transferred and cannot be used.
- `Dropped`: value was explicitly consumed (for example via `freeStr`).

This state is now used to emit compile-time diagnostics:

- use-after-move
- double-move
- ownership conflicts in assignment/call/return paths

In LSP, ownership diagnostics are tagged with stable codes (for example `OWN001`, `OWN002`) for easier filtering and tooling integration.
