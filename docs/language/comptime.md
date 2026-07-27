# Comptime

Comptime (compile-time) functions are evaluated during compilation via a tree-walking interpreter. Their return values are embedded directly into the final binary as constants.

## Syntax

```lucis
comptime fn max(int32 a, int32 b) int32 {
    if a > b { return a; }
    return b;
}

fn main() int32 {
    int32 result = max(15, 27);  // computed at compile time → 27
    return result;
}
```

## How It Works

1. **Parser** recognizes `comptime` before `fn`
2. **Checker** registers the function in the compile-time registry and fully type-checks the body
3. **IRGen** skips LLVM IR generation for the comptime function body
4. At the call site, the compiler:
   - Converts constant argument values to `ComptimeValue` objects
   - Walks the AST with the interpreter, evaluating every expression
   - Maps the result back to an LLVM constant
5. The runtime binary contains **zero code** for the comptime function — only the computed result

The interpreter runs on the **host machine** regardless of `--target`. The target triple only affects the output binary.

## Supported Operations

| Category | Operations |
|----------|-----------|
| **Arithmetic** | `+`, `-` (Int + Float); `*`, `/`, `%` (Int only) |
| **Unary** | `-` (negation, Int + Float); `!` (logical NOT, Bool); `~` (bitwise NOT, Int) |
| **Relational** | `<`, `>`, `<=`, `>=` (Int only) |
| **Equality** | `==`, `!=` (Int only) |
| **Bitwise** | `&` (AND, Int only) |
| **Control flow** | `if` / `else if` / `else`, `return` |
| **Loops** | `while`, `for-in` (range `0..n` and `0..=n`, list iteration) |
| **Match** | enum variant patterns, wildcard `_`, bindings, default arms |
| **Variables** | declaration with initializer, simple assignment, compound assignment (`+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `|=`) |
| **Calls** | self-recursion, cross-function calls between comptime functions |
| **Literals** | integer, float, bool, string |
| **Indexing** | list indexing `list[i]` |
| **Blocks** | nested block statements `{ ... }` |

## Supported Types

All primitive type families can be used as parameter types, return types, and inside the function body:

| Family | Types |
|--------|-------|
| Signed int | `int8`, `int16`, `int32`, `int64` |
| Unsigned int | `uint8`, `uint16`, `uint32`, `uint64` |
| Float | `float32`, `float64` |
| Bool | `bool` |
| Void | `void` |
| Char | `char` (internally `int8`) |
| String | `string` (from builtins and function returns) |

## Builtins

Seven compile-time builtins are available for type introspection:

| Builtin | Signature | Description |
|---------|-----------|-------------|
| `@typeName(T)` | `→ string` | Returns the type's name |
| `@fields(T)` | `→ string` | Comma-separated field names (Struct/Union) |
| `@variants(T)` | `→ string` | Comma-separated variant names (Enum) |
| `@typeId(T)` | `→ int64` | FNV-1a 64-bit hash of the type name |
| `@isStruct(T)` | `→ bool` | True if type is a struct |
| `@isEnum(T)` | `→ bool` | True if type is an enum |
| `@isUnion(T)` | `→ bool` | True if type is a union |

```lucis
comptime fn debugType(T) string {
    if @isStruct(T) {
        return "struct " + @typeName(T) + " has fields: " + @fields(T);
    }
    if @isEnum(T) {
        return "enum " + @typeName(T) + " has variants: " + @variants(T);
    }
    return @typeName(T);
}
```

## Examples

**While loop:**
```lucis
comptime fn sumTo(int32 n) int32 {
    int32 i = 0;
    int32 sum = 0;
    while i <= n {
        sum += i;
        i += 1;
    }
    return sum;
}
```

**For-in range:**
```lucis
comptime fn rangeSum(int32 n) int32 {
    int32 sum = 0;
    for int32 i in 1..=n {
        sum += i;
    }
    return sum;
}
```

**Cross-function calls:**
```lucis
comptime fn max(int32 a, int32 b) int32 {
    if a > b { return a; }
    return b;
}

comptime fn clamp(int32 n, int32 lo, int32 hi) int32 {
    int32 x = max(n, lo);
    return max(x, hi);  // cross-function call
}
```

**Recursion:**
```lucis
comptime fn factorial(int32 n) int32 {
    if n <= 1 { return 1; }
    return n * factorial(n - 1);
}
```

## Limitations

- No `comptime { }` blocks — only standalone `comptime fn`
- No `loop` / `do-while` — `while` and `for-in` are supported
- No `switch` — `match` is supported
- No ternary (`?:`)
- No logical AND/OR (`&&`, `||`)
- No Bitwise OR/XOR/SHIFT (`|`, `^`, `<<`, `>>`)
- No cast (`as`) between types
- No generic comptime functions
- Float arithmetic limited to `+`, `-`, unary `-`
- No struct, array, tuple, or slice types in comptime function signatures
- No pointer operations (deref, addrOf)
- No field access (`struct.field`)
- No `sizeof` / `typeof` / `alignof` / `offsetof`
- No lambda expressions
- All argument values must be compile-time constants
- String concatenation not directly supported (use `@typeName` / `@fields` for string results)
