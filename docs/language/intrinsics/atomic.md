# `lucis::atomic` — Atomic Operations

The `atomic` namespace provides **atomic memory operations** with sequential consistency. These are essential for lock-free concurrent programming.

> **Always available** — no `use lucis::atomic` declaration needed.

All operations are **sequentially consistent** (seq_cst), establishing a single total order across all threads.

---

## Load & Store

### `lucis::atomic::load\<T\>(ptr) -> T`

```lucis
int32 val = lucis::atomic::load<int32>(&counter);
```

Atomically reads a value of type `T` from `ptr` with seq_cst ordering.

| Param | Type | Description |
|-------|------|-------------|
| `<T>` | type | Value type to load |
| `ptr` | `*_any` | Pointer to read from |

### `lucis::atomic::store\<T\>(ptr, val)`

```lucis
lucis::atomic::store<int32>(&counter, 42);
```

Atomically writes `val` of type `T` to `ptr` with seq_cst ordering.

---

## Fetch-and-Op (Read-Modify-Write)

These atomically modify a value and return the **old** value. They lower to LLVM `atomicrmw` instructions.

### `lucis::atomic::add\<T\>(ptr, val) -> T`

```lucis
int32 old = lucis::atomic::add<int32>(&counter, 1);
```

Atomically adds `val` to `*ptr`. Returns the old value.

### `lucis::atomic::sub\<T\>(ptr, val) -> T`

```lucis
int32 old = lucis::atomic::sub<int32>(&counter, 1);
```

Atomically subtracts `val` from `*ptr`. Returns the old value.

### `lucis::atomic::bit_and\<T\>(ptr, val) -> T`

```lucis
int32 old = lucis::atomic::bit_and<int32>(&flags, 0xFE);
```

Atomically ANDs `val` with `*ptr`. Returns the old value.

### `lucis::atomic::bit_or\<T\>(ptr, val) -> T`

```lucis
int32 old = lucis::atomic::bit_or<int32>(&flags, 0x01);
```

Atomically ORs `val` with `*ptr`. Returns the old value.

### `lucis::atomic::bit_xor\<T\>(ptr, val) -> T`

```lucis
int32 old = lucis::atomic::bit_xor<int32>(&flags, 0xFF);
```

Atomically XORs `val` with `*ptr`. Returns the old value.

---

## Exchange & CAS

### `lucis::atomic::exchange\<T\>(ptr, val) -> T`

```lucis
int32 old = lucis::atomic::exchange<int32>(&data, 42);
```

Atomically replaces `*ptr` with `val` and returns the old value (atomic swap).

### `lucis::atomic::cas\<T\>(ptr, expected, desired) -> bool`

```lucis
bool swapped = lucis::atomic::cas<int32>(&data, 0, 42);
```

Atomically compares `*ptr` with `expected` and, if equal, replaces with `desired` (weak compare-and-swap). Returns `true` if the swap occurred, `false` otherwise.

> **Weak CAS** may fail spuriously even when `*ptr == expected`. Use it in a retry loop:

```lucis
fn atomic_increment(*int32 ptr) void {
    loop {
        int32 old = lucis::atomic::load<int32>(ptr);
        if lucis::atomic::cas<int32>(ptr, old, old + 1) { break; }
    }
}
```

---

## Thread Fences

Sequentially-consistent memory fences are available in `lucis::sys`:

| Function | Ordering |
|----------|----------|
| `lucis::sys::fence_acquire()` | Acquire |
| `lucis::sys::fence_release()` | Release |
| `lucis::sys::fence_acq_rel()` | Acquire+Release |
| `lucis::sys::fence_seq_cst()` | Sequentially Consistent |

---

## Example: Lock-Free Counter

```lucis
fn main() int32 {
    int32 counter = 0;

    // Parallel increment (would use spawn in real code)
    int32 old = lucis::atomic::add<int32>(&counter, 1);
    lucis::sys::assume(counter == 1);
    lucis::sys::unreachable();

    ret 0;
}
```
