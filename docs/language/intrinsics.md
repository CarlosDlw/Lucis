# Intrinsics

Intrinsics are special functions that are built directly into the compiler. They provide low-level access to hardware features or compiler-specific functionality that cannot be expressed directly in Lux.

Lux organizes intrinsics into the `lux` namespace.

## Unsafe Intrinsics (`lux::unsafe`)

The `unsafe` namespace provides direct access to low-level memory and calling convention features. These should be used with extreme care as they bypass the language's safety guarantees.

### Variadic Argument Support

Lux provides a set of intrinsics to handle C-style variadic arguments (`...`). These are designed to be compatible with the target platform's ABI by using native LLVM support.

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

#### `va_end(va: va_list)`

```lux
lux::unsafe::va_end(args);
```

Cleans up the variadic argument list state. Every `va_start` should have a corresponding `va_end`.

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