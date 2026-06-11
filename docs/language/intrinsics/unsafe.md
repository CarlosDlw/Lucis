# `lux::unsafe` — Unsafe Intrinsics

The `unsafe` namespace provides direct access to low-level memory and calling convention features. These bypass the language's safety guarantees.

---

## Variadic Argument Support

Lux provides a set of intrinsics to handle C-style variadic arguments (`...`). They are compatible with the target platform's ABI using native LLVM support.

Primarily used inside **untyped variadic functions** — functions whose last parameter is a bare `...` without a type or name:

```lux
void log_values(int32 count, ...) {
    va_list va = lux::unsafe::va_list();
    lux::unsafe::va_start(va);

    for int32 i in 0..count {
        int32 val = lux::unsafe::va_arg_int32(va);
        print(val);
        if i < count - 1 { print(", "); }
    }

    lux::unsafe::va_end(va);
}
```

### Lifecycle

1. **Allocate** a `va_list` with `va_list()`.
2. **Initialise** with `va_start(va)` to point at the first variadic argument.
3. **Read** each argument in order with a `va_arg_*` helper.
4. **Clean up** with `va_end(va)`.

---

### `type va_list`

An opaque type representing the state of a variadic argument list cursor. Layout is platform-dependent (struct on x86-64 Linux, simple pointer on other systems).

### `va_list()`

```lux
va_list args = lux::unsafe::va_list();
```

Allocates a new variadic argument list state on the stack. Must be initialised with `va_start` before use.

### `va_start(va: va_list)`

```lux
lux::unsafe::va_start(args);
```

Initialises the `va_list` state to point at the first variadic argument.

> Unlike C, `va_start` in Lux does **not** require the last fixed parameter as an argument.

### `va_arg<T>(va: va_list) -> T`

```lux
int32 value = lux::unsafe::va_arg<int32>(args);
```

Reads the next argument of type `T` from the variadic list and advances the cursor.

> **Warning**: `va_arg<T>` may crash the LLVM X86 backend for types like `bool`, `string`, or structs. Prefer the typed helpers below.

### Typed `va_arg` Helpers

| Helper | Reads as | Returns |
|--------|----------|---------|
| `va_arg_int32(va)` | `int32` | `int32` |
| `va_arg_int64(va)` | `int64` | `int64` |
| `va_arg_float32(va)` | `float32` | `float32` |
| `va_arg_float64(va)` | `float64` | `float64` |
| `va_arg_ptr(va)` | pointer | `*void` |
| `va_arg_bool(va)` | `int32` (promoted) | `bool` |
| `va_arg_string(va)` | pointer → `strlen` | `string` |

```lux
int32   i = lux::unsafe::va_arg_int32(va);
int64   l = lux::unsafe::va_arg_int64(va);
float32 f = lux::unsafe::va_arg_float32(va);
float64 d = lux::unsafe::va_arg_float64(va);
void*   p = lux::unsafe::va_arg_ptr(va);
bool    b = lux::unsafe::va_arg_bool(va);
string  s = lux::unsafe::va_arg_string(va);
```

`va_arg_bool` reads a promoted `int32` and truncates to 1-bit `bool`, avoiding an LLVM crash on `i1`.

`va_arg_string` reads a `char*`, calls `lux_fromCStr` / `strlen` to reconstruct the Lux `{ptr, len}` fat pointer.

### `va_end(va: va_list)`

```lux
lux::unsafe::va_end(args);
```

Cleans up the variadic argument list state. Every `va_start` must have a matching `va_end`.

### Mixed-Type Example

```lux
void print_mixed(int32 count, ...) {
    va_list va = lux::unsafe::va_list();
    lux::unsafe::va_start(va);

    int32   i = lux::unsafe::va_arg_int32(va);
    float64 f = lux::unsafe::va_arg_float64(va);
    bool    b = lux::unsafe::va_arg_bool(va);
    string  s = lux::unsafe::va_arg_string(va);

    println(i);
    println(f);
    println(b);
    println(s);

    lux::unsafe::va_end(va);
}

int32 main() {
    print_mixed(4, 42, 3.14, true, c"hello");
    ret 0;
}
```

### ABI Details

The `va_*` intrinsics follow the target platform's C ABI (e.g., System V AMD64 on Linux). On x86-64:

- First **6 integer/pointer** args passed in registers (`rdi`, `rsi`, `rdx`, `rcx`, `r8`, `r9`).
- First **8 floating-point** args in XMM registers.
- Additional args on the stack.
- `va_start` saves all register arguments to the register save area.

**Default argument promotion** (matches C):

| Actual type | Promoted to |
|-------------|-------------|
| `int1` – `int8` | `int32` |
| `uint8` – `uint16` | `int32` |
| `float32` | `float64` |
| `bool` | `int32` |
