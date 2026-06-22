# Intrinsics

Intrinsics are special functions built directly into the compiler. They provide low-level access to hardware features, LLVM primitives, and compiler-specific functionality that cannot be expressed in plain Lucis code.

Lucis organises intrinsics into the `lucis` namespace, with each sub-namespace covering a specific domain.

## Available Namespaces

### [`lucis::unsafe`](intrinsics/unsafe.md) — Unsafe low-level operations

Variadic argument handling (`va_list`, `va_start`, `va_arg`, `va_end`), raw memory access, and other operations that bypass language safety guarantees.

### [`lucis::sys`](intrinsics/sys.md) — System control

Typed memory access (read/write), pointer arithmetic (offset), bit reinterpretation (bitcast), compiler hints (assume/unreachable/expect), volatile access, bit manipulation (bitreverse/bswap/ctpop/ctlz/cttz), float math (sqrt/fma/ceil/floor/trunc/round/fabs/minimum/maximum/copysign), integer abs/rotate, overflow detection, saturating arithmetic, endianness conversion (to_be/to_le/from_be/from_le), stack/frame pointers, memory fences, prefetch, CPU control, hardware RNG (rdrand), cache control, TLS base access, lifetime hints, and raw system calls.

### [`lucis::io`](intrinsics/io.md) — Low-level file I/O

Unbuffered file descriptor operations: open, read, write, close, lseek. Cross-platform via C wrappers.

### [`lucis::atomic`](intrinsics/atomic.md) — Atomic operations

Atomic load, store, fetch-and-{add,sub,and,or,xor}, exchange, and compare-and-swap with configurable ordering: seq_cst (default), acquire, release, acq_rel, and relaxed.

### `lucis::core` — Core language primitives

- `lucis::core::trap()` — Aborts execution via hardware trap (`@llvm.trap`).

### `lucis::debug` — Debugging helpers (not yet implemented)

---

## How Intrinsics Work

Intrinsics are registered in the compiler's **IntrinsicRegistry** and wired directly to LLVM IR generation. When the checker encounters a call like `lucis::sys::memcpy(...)`, it:

1. **Validates** argument count and types against the intrinsic's declaration.
2. **Resolves** the return type.
3. **Emits** LLVM IR via a C++ lambda (or direct LLVM intrinsic call) at compile time.

No runtime dispatch, no function call overhead — intrinsics are as close to the metal as the language gets.

## For Compiler Developers

See [`docs/advanced/intrinsics.md`](../advanced/intrinsics.md) for a complete guide on creating new intrinsic namespaces, functions, and types in the Lucis compiler.
