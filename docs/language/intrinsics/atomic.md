# `lucis::atomic` — Atomic Operations

The `atomic` namespace provides **atomic memory operations** with configurable ordering. These are essential for lock-free concurrent programming.

> **Always available** — no `use lucis::atomic` declaration needed.

## Ordering Variants

All operations have ordering variants. The default variants use **seq_cst** (sequential consistency, the strongest ordering). Suffixed variants provide weaker orderings for better performance:

| Suffix | Ordering | Use Case |
|--------|----------|----------|
| _(none) | `seq_cst` | Safe default, total order across threads |
| `_acq` | `acquire` | Load before accessing protected data |
| `_rel` | `release` | Store after updating protected data |
| `_acqrel` | `acq_rel` | RMW operations (e.g. reference counting) |
| `_rlx` | `relaxed` | Simple counters where only atomicity matters |

**Available suffix combinations:**

| Base | `_acq` | `_rel` | `_acqrel` | `_rlx` |
|------|--------|--------|-----------|--------|
| `load` | `load_acq` | — | — | `load_rlx` |
| `store` | — | `store_rel` | — | `store_rlx` |
| `add` | — | — | `add_acqrel` | `add_rlx` |
| `sub` | — | — | `sub_acqrel` | `sub_rlx` |
| `exchange` | — | — | `xchg_acqrel` | `xchg_rlx` |
| `cas` | — | — | `cas_acqrel` | `cas_rlx` |

---

## Load & Store

**Default (seq_cst):**
- `lucis::atomic::load\<T\>(ptr) -> T`
- `lucis::atomic::store\<T\>(ptr, val)`

**Acquire / Release variants:**
- `lucis::atomic::load_acq\<T\>(ptr) -> T` — acquire ordering for loads
- `lucis::atomic::store_rel\<T\>(ptr, val)` — release ordering for stores

**Relaxed variants:**
- `lucis::atomic::load_rlx\<T\>(ptr) -> T`
- `lucis::atomic::store_rlx\<T\>(ptr, val)`

```lucis
int32 val = lucis::atomic::load_acq<int32>(&counter);
lucis::atomic::store_rel<int32>(&counter, 42);
```

---

## Fetch-and-Op (Read-Modify-Write)

These atomically modify a value and return the **old** value. They lower to LLVM `atomicrmw` instructions.

**Default (seq_cst):** `add`, `sub`, `bit_and`, `bit_or`, `bit_xor`

**Acq_rel variants:** `add_acqrel`, `sub_acqrel`

**Relaxed variants:** `add_rlx`, `sub_rlx`

```lucis
int32 old = lucis::atomic::add_acqrel<int32>(&refcount, -1);
int32 val = lucis::atomic::add_rlx<int32>(&stats_counter, 1);
```

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

**Default (seq_cst):** `exchange`, `cas`

**Acq_rel variants:** `xchg_acqrel`, `cas_acqrel`

**Relaxed variants:** `xchg_rlx`, `cas_rlx`

```lucis
int32 old = lucis::atomic::xchg_acqrel<int32>(&lock, 1);
bool ok = lucis::atomic::cas_rlx<int32>(&data, expected, desired);
```

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

Memory fences are available in `lucis::sys`:

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
