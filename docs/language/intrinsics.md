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
        int32 val = lux::unsafe::va_arg<int32>(va);
        print(val);
        if i < count - 1 { print(", "); }
    }

    lux::unsafe::va_end(va);
}
```

The typical lifecycle is:

1. **Allocate** a `va_list` with `va_list()`.
2. **Initialize** it with `va_start(va)` to point at the first variadic argument.
3. **Read** each argument in order with `va_arg<T>(va)`, specifying the expected type.
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
let value = lux::unsafe::va_arg<int32>(args);
```

Reads the next argument of type `T` from the variadic list and advances the cursor. This intrinsic uses the native LLVM `va_arg` instruction, ensuring correct handling of the platform's ABI (including promotion and register-to-stack transitions).

> **Warning**: The type `T` passed to `va_arg<T>` must match the actual type of the argument at the call site. Passing the wrong type results in undefined behaviour (misaligned reads, wrong register selection, or data corruption).

#### `va_end(va: va_list)`

```lux
lux::unsafe::va_end(args);
```

Cleans up the variadic argument list state. Every `va_start` should have a corresponding `va_end`.

### Mixed-Type Example

Because untyped variadics accept any argument, you can pass values of different types by reading them with the correct `va_arg<T>` instantiation:

```t
void print_mixed(int32 count, ...) {
    va_list va = lux::unsafe::va_list();
    lux::unsafe::va_start(va);

    // Read a string, an integer, and a float
    string s = lux::unsafe::va_arg<string>(va);
    int32  i = lux::unsafe::va_arg<int32>(va);
    float64 f = lux::unsafe::va_arg<float64>(va);

    println(s);
    println(i);
    println(f);

    lux::unsafe::va_end(va);
}

int32 main() {
    // All three arguments have different types
    print_mixed(3, c"hello", 42, 3.14);
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