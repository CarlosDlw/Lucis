# `lux::sys` — System Control Intrinsics

The `sys` namespace provides **low-level system control** intrinsics — direct LLVM access for memory operations, CPU intrinsics, barriers, and system calls.

> **Always available** — no `use lux::sys` declaration needed.

---

## Memory Operations

Raw memory block operations that lower to LLVM `@llvm.memcpy`, `@llvm.memmove`, and `@llvm.memset`.

### `lux::sys::memcpy(dst, src, n)`

```lux
lux::sys::memcpy(&dst, &src, size);
```

Copies `n` bytes from `src` to `dst`. The source and destination must not overlap (use `memmove` for overlapping regions).

| Param | Type | Description |
|-------|------|-------------|
| `dst` | `*_any` | Destination pointer |
| `src` | `*_any` | Source pointer |
| `n` | `int32` | Number of bytes to copy |

### `lux::sys::memmove(dst, src, n)`

```lux
lux::sys::memmove(&dst, &src, size);
```

Copies `n` bytes from `src` to `dst`, handling overlapping regions correctly.

### `lux::sys::memset(dst, val, n)`

```lux
lux::sys::memset(&buf, 0, 64);
```

Fills `n` bytes starting at `dst` with byte value `val`.

---

## Volatile Memory Access

Generic intrinsics for hardware-level volatile loads and stores. The compiler will not optimize, reorder, or eliminate these accesses.

### `lux::sys::volatile_load\<T\>(ptr) -> T`

```lux
int32 status = lux::sys::volatile_load<int32>(&hw_register);
```

Reads a value of type `T` through `ptr` using a volatile load instruction.

| Param | Type | Description |
|-------|------|-------------|
| `<T>` | type | Value type to load |
| `ptr` | `*_any` | Pointer to read from |

### `lux::sys::volatile_store\<T\>(ptr, val)`

```lux
lux::sys::volatile_store<int32>(&ctrl_register, 0xFF);
```

Writes `val` of type `T` through `ptr` using a volatile store instruction.

---

## Bit Manipulation

Generic intrinsics for bit-level operations that lower to LLVM bit intrinsics.

### `lux::sys::bitreverse\<T\>(x) -> T`

```lux
int32 rev = lux::sys::bitreverse<int32>(0x12345678);
// rev == 0x1E6A2C48
```

Reverses the bit order of `x`.

### `lux::sys::bswap\<T\>(x) -> T`

```lux
int32 bs = lux::sys::bswap<int32>(0x12345678);
// bs == 0x78563412
```

Reverses the byte order of `x`.

### `lux::sys::ctpop\<T\>(x) -> T`

```lux
int32 ones = lux::sys::ctpop<int32>(0x12345678);
// ones == 13 (bits set)
```

Counts the number of 1-bits in `x`.

### `lux::sys::ctlz\<T\>(x) -> T`

```lux
int32 lz = lux::sys::ctlz<int32>(0x12345678);
// lz == 3 (leading zeros)
int32 lz0 = lux::sys::ctlz<int32>(0);
// lz0 == 32 (bit width — is_zero_undef = false)
```

Counts the number of leading zero bits in `x`. When `x` is zero, returns the full bit width (safe / defined-at-zero behaviour).

### `lux::sys::cttz\<T\>(x) -> T`

```lux
int32 tz = lux::sys::cttz<int32>(0x12345678);
// tz == 3 (trailing zeros)
int32 tz0 = lux::sys::cttz<int32>(0);
// tz0 == 32 (bit width — is_zero_undef = false)
```

Counts the number of trailing zero bits in `x`. When `x` is zero, returns the full bit width.

---

## Arithmetic Overflow Detection

Generic intrinsics that perform arithmetic while detecting overflow. Returns `bool` (`true` if overflow occurred) and writes the result through a pointer.

> Overflow intrinsics take the result **through a pointer** to avoid returning a struct type.

### `lux::sys::sadd_with_overflow\<T\>(a, b, &result) -> bool`

```lux
int32 res = 0;
bool overflowed = lux::sys::sadd_with_overflow<int32>(INT32_MAX, 1, &res);
// overflowed == true, res == -2147483648
```

Signed addition with overflow detection.

### `lux::sys::uadd_with_overflow\<T\>(a, b, &result) -> bool`

```lux
uint32 res = 0;
bool overflowed = lux::sys::uadd_with_overflow<uint32>(4294967295, 1, &res);
// overflowed == true, res == 0
```

Unsigned addition with overflow detection.

### `lux::sys::ssub_with_overflow\<T\>(a, b, &result) -> bool`

```lux
int32 res = 0;
bool overflowed = lux::sys::ssub_with_overflow<int32>(INT32_MIN, 1, &res);
// overflowed == true, res == 2147483647
```

Signed subtraction with overflow detection.

### `lux::sys::usub_with_overflow\<T\>(a, b, &result) -> bool`

```lux
uint32 res = 0;
bool overflowed = lux::sys::usub_with_overflow<uint32>(0, 1, &res);
// overflowed == true, res == 4294967295
```

Unsigned subtraction with overflow detection.

### `lux::sys::smul_with_overflow\<T\>(a, b, &result) -> bool`

```lux
int32 res = 0;
bool overflowed = lux::sys::smul_with_overflow<int32>(46341, 46341, &res);
// overflowed == true
```

Signed multiplication with overflow detection.

### `lux::sys::umul_with_overflow\<T\>(a, b, &result) -> bool`

```lux
uint32 res = 0;
bool overflowed = lux::sys::umul_with_overflow<uint32>(65536, 65536, &res);
// overflowed == true
```

Unsigned multiplication with overflow detection.

---

## Stack & Frame Pointer

Access to the current frame pointer, return address, and stack pointer.

### `lux::sys::frame_address(level) -> usize`

```lux
usize fp = lux::sys::frame_address(0);
```

Returns the frame pointer at the given call depth. `level = 0` returns the current function's frame pointer.

### `lux::sys::return_address(level) -> usize`

```lux
usize ra = lux::sys::return_address(0);
```

Returns the return address at the given call depth. `level = 0` returns the address the current function will return to.

> **Note**: On x86-64, `return_address` is **not** declared with an overloaded pointer type — doing so causes a linker error (`undefined reference to llvm.returnaddress.p0`).

### `lux::sys::stack_save() -> usize`

```lux
usize sp = lux::sys::stack_save();
```

Saves the current stack pointer.

### `lux::sys::stack_restore(sp)`

```lux
lux::sys::stack_restore(sp);
```

Restores the stack pointer to a previously saved value. Typically balanced with a prior `stack_save()`.

---

## Memory Fences

Memory ordering barriers for multi-threaded synchronization. Each function emits a `fence` instruction with the corresponding `AtomicOrdering`.

### `lux::sys::fence_acquire()`

```lux
lux::sys::fence_acquire();
```

Acquire fence — prevents memory operations from being reordered after the fence. Used for **read** (consume) synchronisation.

### `lux::sys::fence_release()`

```lux
lux::sys::fence_release();
```

Release fence — prevents memory operations from being reordered before the fence. Used for **write** (publish) synchronisation.

### `lux::sys::fence_acq_rel()`

```lux
lux::sys::fence_acq_rel();
```

Combined acquire-release fence.

### `lux::sys::fence_seq_cst()`

```lux
lux::sys::fence_seq_cst();
```

Sequentially-consistent fence — the strongest ordering. All threads observe a total order of sequentially-consistent operations.

---

## Prefetch

Cache prefetch hints, lowering to `@llvm.prefetch`.

### `lux::sys::prefetch_read(addr, locality)`

```lux
lux::sys::prefetch_read(&data, 3);
```

Prefetches memory for reading.

| Param | Type | Description |
|-------|------|-------------|
| `addr` | `*_any` | Address to prefetch |
| `locality` | `int32` | 0 (no temporal reuse) — 3 (high persistence) |

### `lux::sys::prefetch_write(addr, locality)`

```lux
lux::sys::prefetch_write(&data, 0);
```

Prefetches memory for writing. Same locality semantics as `prefetch_read`.

---

## CPU Control

Architecture-specific CPU hints and introspection, using inline assembly with target-triple detection.

### `lux::sys::cpu_relax()`

```lux
lux::sys::cpu_relax();
```

Hints the CPU that the current thread is in a spin-wait loop:

| Architecture | Instruction |
|-------------|-------------|
| x86 / x86-64 | `pause` |
| AArch64 | `yield` |
| RISC-V 64 | `fence iorw, iorw` |
| Others | `nop` |

### `lux::sys::breakpoint()`

```lux
lux::sys::breakpoint();
```

Triggers a debugger breakpoint (`@llvm.debugtrap`). Execution **continues** after the breakpoint — unlike `lux::core::trap()` which aborts. If no debugger is attached, the CPU may ignore or raise `SIGTRAP`.

### `lux::sys::read_cycle_counter() -> int64`

```lux
int64 start = lux::sys::read_cycle_counter();
// ... work ...
int64 end = lux::sys::read_cycle_counter();
```

Reads the current CPU cycle counter (`@llvm.readcyclecounter`):

| Architecture | Instruction |
|-------------|-------------|
| x86 / x86-64 | `rdtsc` |
| AArch64 | `mrs cntvct_el0` |

The counter is monotonically increasing (though not synchronised across cores).

---

## System Calls

Raw syscall interface using inline assembly. Supports 0–6 argument variants for all three major architectures.

> ⚠ Raw syscalls bypass the operating system's libc wrappers. Incorrect arguments can crash the process.

### x86-64 ABI

| Register | Role |
|----------|------|
| `rax` | Syscall number (also receives result) |
| `rdi` | arg1 |
| `rsi` | arg2 |
| `rdx` | arg3 |
| `r10` | arg4 |
| `r8`  | arg5 |
| `r9`  | arg6 |
| `rcx, r11` | Clobbered by `syscall` instruction |

### AArch64 ABI

| Register | Role |
|----------|------|
| `x8`  | Syscall number |
| `x0`  | arg1 (also receives result) |
| `x1`  | arg2 |
| `x2`  | arg3 |
| `x3`  | arg4 |
| `x4`  | arg5 |
| `x5`  | arg6 |

### RISC-V 64 ABI

| Register | Role |
|----------|------|
| `x17` (a7) | Syscall number |
| `x10` (a0) | arg1 (also receives result) |
| `x11` (a1) | arg2 |
| `x12` (a2) | arg3 |
| `x13` (a3) | arg4 |
| `x14` (a4) | arg5 |
| `x15` (a5) | arg6 |

### Functions

All variants take a syscall number as the first `int64` argument and return `int64`.

#### `lux::sys::syscall0(number) -> int64`

```lux
int64 pid = lux::sys::syscall0(39);  // getpid on x86-64
```

#### `lux::sys::syscall1(number, a1) -> int64`

```lux
int64 ret = lux::sys::syscall1(60, 0);  // exit(0)
```

#### `lux::sys::syscall2(number, a1, a2) -> int64`

#### `lux::sys::syscall3(number, a1, a2, a3) -> int64`

#### `lux::sys::syscall4(number, a1, a2, a3, a4) -> int64`

#### `lux::sys::syscall5(number, a1, a2, a3, a4, a5) -> int64`

#### `lux::sys::syscall6(number, a1, a2, a3, a4, a5, a6) -> int64`

```lux
int64 result = lux::sys::syscall6(0, 0, 0, 0, 0, 0, 0);
```

Unused arguments should be passed as `0i64` (zero of type `int64`).
