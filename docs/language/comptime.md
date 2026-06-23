# Comptime

Comptime (compile-time) functions are evaluated during compilation rather than at runtime. They are compiled to native code via LLVM JIT, executed, and their return values are embedded directly into the final binary as constants.

## Syntax

```lucis
comptime fn add(int32 a, int32 b) int32 {
    return a + b;
}

fn main() int32 {
    int32 result = add(10, 20);  // computed at compile time → 30
    return result;
}
```

## How It Works

1. **Parser** recognizes `comptime` before `fn`
2. **Checker** registers the function in the compile-time registry and skips normal body validation
3. **IRGen** skips code generation for the comptime function body
4. At the call site, the compiler:
   - Extracts constant argument values
   - Compiles the comptime function to LLVM IR
   - Executes it via LLVM's MCJIT engine
   - Replaces the call with the resulting constant

The runtime binary contains **zero code** for the comptime function — only the computed result.

## Supported Operations

| Category | Supported |
|----------|-----------|
| Arithmetic | `+`, `-`, `*`, `/` |
| Variables | Local vars and parameters |
| Literals | Integer (`42`), float, bool |
| Control flow | `return` |
| Function calls | Other comptime functions |

## Limitations (current)

- No `if`, `for`, `while` yet (future)
- No `match` / `switch`
- No `string` or complex types in comptime functions
- No `comptime` blocks — only functions
- Only `int32` return type fully supported
- All arguments must be compile-time constants

## Use Cases

**Compile-time assertions:**
```lucis
comptime fn assert_size(usize actual, usize expected) bool {
    return actual == expected;
}
```

**Constant folding:**
```lucis
comptime fn factorial(int32 n) int32 {
    if n <= 1 { return 1; }
    return n * factorial(n - 1);
}
```

## Flags

```
lucis build --target x86_64-unknown-none
```

The comptime JIT always runs on the **host machine** regardless of `--target`. The target triple only affects the output binary.
