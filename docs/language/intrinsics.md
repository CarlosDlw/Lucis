# Intrinsics

Intrinsics are special functions that are built directly into the compiler. They provide low-level access to hardware features or compiler-specific functionality that cannot be expressed directly in Lux.

Lux organizes intrinsics into the `lux` namespace.

## Unsafe Intrinsics (`lux::unsafe`)

The `unsafe` namespace provides direct access to low-level memory and calling convention features. These should be used with extreme care as they bypass the language's safety guarantees.

### Variadic Argument Support

Lux provides a set of intrinsics to handle C-style variadic arguments (`...`). These are designed to be compatible with the target platform's ABI by using native LLVM support.

They are primarily used inside **untyped variadic functions** — functions whose last parameter is a bare `...` without a type or name:

```t
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

The typical lifecycle is:

1. **Allocate** a `va_list` with `va_list()`.
2. **Initialize** it with `va_start(va)` to point at the first variadic argument.
3. **Read** each argument in order with a `va_arg_*` helper, selecting the one that matches the expected type.
4. **Clean up** with `va_end(va)`.

---

#### `type va_list`

An opaque type that represents the state of a variadic argument list cursor. The internal layout of this type is platform-dependent (e.g., it may be a struct on x86-64 Linux or a simple pointer on other systems).

#### `va_list()`

```lux
va_list args = lux::unsafe::va_list();
```

Allocates a new variadic argument list state on the stack and returns it. This state must be initialized using `va_start` before use.

#### `va_start(va: va_list)`

```lux
lux::unsafe::va_start(args);
```

Initializes the `va_list` state. When called within a variadic function, it points the cursor to the first variadic argument.

> **Note**: Unlike C, `va_start` in Lux does not require the last fixed parameter as an argument; the compiler handles this automatically.

#### `va_arg<T>(va: va_list) -> T`

```lux
int32 value = lux::unsafe::va_arg<int32>(args);
```

Reads the next argument of type `T` from the variadic list and advances the cursor. This intrinsic uses the native LLVM `va_arg` instruction, ensuring correct handling of the platform's ABI (including promotion and register-to-stack transitions).

> **Warning**: The generic `va_arg<T>` may crash the compiler backend (LLVM X86) for types like `bool`, `string`, or user-defined structs. Prefer the typed helpers below for those cases.

#### Typed `va_arg` Helpers

For common types, Lux provides dedicated helpers that avoid LLVM backend limitations. These use only types that the X86 backend can safely lower for `va_arg`:

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

`va_arg_bool` reads a promoted `int32` from the argument list (C variadic promotes `bool` to `int`), then truncates the result to a 1-bit `bool`. This avoids LLVM crashing on a raw `va_arg` with `i1` type.

`va_arg_string` reads a `char*` from the argument list (the caller passes only the pointer, not the full `{ptr, len}` fat pointer), then calls `lux_fromCStr` which uses `strlen` to recover the length and reconstructs the Lux `string` type. This matches the C ABI convention where strings in variadic contexts are passed as null-terminated `char*`. Strings containing null bytes are truncated by `strlen`; for binary data, use `va_arg_ptr` + `va_arg_int64` manually.

> **Tip**: When writing variadic helpers that handle multiple types (like a custom `printf`), always use the typed helpers. They are guaranteed to work across all supported targets.

#### `va_end(va: va_list)`

```lux
lux::unsafe::va_end(args);
```

Cleans up the variadic argument list state. Every `va_start` should have a corresponding `va_end`.

### Mixed-Type Example

Because untyped variadics accept any argument, you can pass values of different types by reading them with the appropriate typed helper:

```t
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

### Calling Conventions & ABI

The `va_*` intrinsics follow the target platform's C ABI (e.g., System V AMD64 on Linux). On x86-64:

- The **first 6 integer/pointer** arguments are passed in registers (`rdi`, `rsi`, `rdx`, `rcx`, `r8`, `r9`).
- The **first 8 floating-point** arguments are passed in XMM registers.
- Additional arguments are passed on the stack.
- `va_start` saves all register arguments to the register save area before the function body runs, so `va_arg` can read from a uniform memory layout.

The compiler automatically applies **default argument promotion** for variadic arguments:
| Actual type | Promoted to |
|-------------|-------------|
| `int1` – `int8` | `int32` |
| `uint8` – `uint16` | `int32` |
| `float32` | `float64` |
| `bool` | `int32` |

This matches C behaviour and ensures ABI compatibility when calling or being called from C.

## Core Intrinsics (`lux::core`)

Core intrinsics provide basic language control features.

### `trap()`

```lux
lux::core::trap();
```

Aborts program execution immediately by raising a hardware trap (lowers to `@llvm.trap`). This is a "no-return" function used for diagnosing unreachable states.

## Debug Intrinsics (`lux::debug`)

Intrinsics used for debugging and program introspection.

> not yet implemented