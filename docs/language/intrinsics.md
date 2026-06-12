# Intrinsics

Intrinsics are special functions built directly into the compiler. They provide low-level access to hardware features, LLVM primitives, and compiler-specific functionality that cannot be expressed in plain Lucis code.

Lucis organises intrinsics into the `lucis` namespace, with each sub-namespace covering a specific domain.

## Available Namespaces

### [`lucis::unsafe`](intrinsics/unsafe.md) — Unsafe low-level operations

Variadic argument handling (`va_list`, `va_start`, `va_arg`, `va_end`), raw memory access, and other operations that bypass language safety guarantees.

### [`lucis::sys`](intrinsics/sys.md) — System control

Memory, volatile access, bit manipulation, overflow detection, stack/frame pointers, memory fences, prefetch, CPU control, and raw system calls.

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
