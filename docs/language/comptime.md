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
2. **Checker** registers the function in the compile-time registry and type-checks the body (parameter types, return types, expression types, return-path completeness)
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
| Arithmetic | `+`, `-`, `*`, `/`, unary `-` |
| Relational | `>`, `<`, `>=`, `<=`, `==`, `!=` |
| Logical | `!` (not) |
| Cast | `as` (between primitive types) |
| Variables | Parameters only |
| Literals | Integer (`42`, `0xFF`, `0o77`), float (`3.14`, `1.5f32`), bool (`true` / `false`), char (`'A'`) |
| Control flow | `return`, `if` / `else` / `else if` |
| Self-call recursion | Supported |
| Other comptime fns | Not yet implemented |

### Supported Types

All six primitive type families can be used as parameter types, return types, and inside the function body:

| Family | Types |
|--------|-------|
| Signed int | `int8`, `int16`, `int32`, `int64` |
| Unsigned int | `uint8`, `uint16`, `uint32`, `uint64` |
| Float | `float32`, `float64` |
| Bool | `bool` |
| Void | `void` |
| Char | `char` (internally `int8`) |

## Limitations (current)

- No `for`, `while`, `loop` — no iteration yet
- No `match` / `switch`
- No `string` or complex types (structs, arrays, slices) in comptime functions
- No `comptime` blocks — only standalone functions
- No variable declarations (`let` / `var`) inside comptime functions — only parameters
- No bitwise operators (`&`, `|`, `^`, `<<`, `>>`, `~`)
- No ternary expressions (`cond ? a : b`)
- No calls to other comptime functions — only self-recursion is supported
- No generic comptime functions
- No mutable assignment — parameters are read-only
- All argument values must be compile-time constants

## Use Cases

**Compile-time assertions:**
```lucis
comptime fn assert_size(usize actual, usize expected) bool {
    return actual == expected;
}
```

**Compile-time computation (self-recursion):**
```lucis
comptime fn factorial(int32 n) int32 {
    if n <= 1 { return 1; }
    return n * factorial(n - 1);
}
```
Note: cross-function calls (one comptime fn calling another) are not yet supported.

## Flags

```
lucis build --target x86_64-unknown-none
```

The comptime JIT always runs on the **host machine** regardless of `--target`. The target triple only affects the output binary.
