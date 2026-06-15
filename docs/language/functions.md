# Functions

This page covers function declaration, parameters, return values, variadic functions, and function pointers in Lucis.

## Declaration

Functions are declared with the `fn` keyword, followed by name, parameters, and return type:

```
fn add(int32 a, int32 b) int32 {
    ret a + b;
}

fn greet(string name) void {
    println(name);
}
```

- The return type comes after the parameter list.
- Parameters are declared with `type name` syntax.
- Use `ret` to return a value. There is no implicit return.
- Use `void` as the return type when the function returns nothing.

## Calling Functions

```

use std::log::println;

fn square(int32 x) int32 {
    ret x * x;
}

fn main() int32 {
    int32 result = square(5);
    println(result);    // 25

    ret 0;
}
```

## Return Values

Every non-void function must return a value with `ret`:

```
fn factorial(int32 n) int32 {
    if n <= 1 {
        ret 1;
    }
    ret n * factorial(n - 1);
}
```

`void` functions can use `ret;` without a value, or simply let execution reach the end of the function:

```
fn log_if_positive(int32 x) void {
    if x <= 0 {
        ret;    // early return, no value
    }
    println(x);
}
```

Returning a heap-backed local transfers ownership to the caller:

```
fn buildMsg() string {
    string msg = fromCStrCopy(c"ok");
    ret msg; // move to caller
}
```

## Parameters

Parameters use the same type-first syntax as variable declarations:

```
fn circle_area(float64 radius) float64 {
    ret 3.14159 * radius * radius;
}

fn clamp_value(int32 value, int32 min_val, int32 max_val) int32 {
    if value < min_val { ret min_val; }
    if value > max_val { ret max_val; }
    ret value;
}
```

### Ownership in Calls

Some APIs consume ownership (move), while others only borrow.

- Borrow example: read-only string operations.
- Consume example: `freeStr(x)` consumes `x`.

Signature help shows ownership hints as `[borrow]` and `[consumes]` for builtins where metadata is available.
Ownership diagnostics exposed through LSP include stable codes (for example `OWN001`, `OWN002`).

## Variadic Functions

T supports variadic functions — functions that accept a variable number of arguments. The variadic parameter uses `...` before the parameter name:

```

use std::log::println;

fn sum(int32 ...values) int32 {
    int32 total = 0;
    for int32 x in values {
        total = total + x;
    }
    ret total;
}

fn print_all(int32 ...values) void {
    for int32 x in values {
        println(x);
    }
}

fn main() int32 {
    println(sum(1, 2, 3, 4, 5));    // 15
    println(sum(10, 20));            // 30
    println(sum());                   // 0

    print_all(10, 20, 30);
    // Output: 10 20 30

    ret 0;
}
```

### Variadic Parameter Rules

- The variadic parameter must be the last parameter (or the only one).
- You can have regular parameters before the variadic one:

```
fn sum_with_base(int32 base, int32 ...values) int32 {
    int32 total = base;
    for int32 x in values {
        total = total + x;
    }
    ret total;
}

// sum_with_base(100, 1, 2, 3) → 106
```

- Inside the function, the variadic parameter behaves like an array. You can iterate it with `for..in`, access elements by index, and check its length:

```
fn first_value(int32 ...values) int32 {
    ret values[0];
}

fn count_values(int32 ...values) int32 {
    int64 n = values.len;
    ret n;
}
```

## C Variadic Functions

C-style variadic functions (like `printf`) use `...` without a parameter name in `extern` declarations:

```
extern int32 printf(*char fmt, ...);

fn main() int32 {
    printf(c"Hello %s, you are %d years old\n", c"Alice", 30);
    ret 0;
}
```

This is different from Lucis variadic functions — C variadic arguments are not type-checked and must match the format string.

## C-Style Untyped Variadic Functions

Lucis also supports untyped variadic functions — functions declared with a bare `...` as the last parameter, without a type or parameter name:

```

use std::log::println;

fn sum(int32 count, ...) void {
    int32 total = 0;

    va_list va = lucis::unsafe::va_list();
    lucis::unsafe::va_start(va);

    for int32 _ in 0..count {
        total += lucis::unsafe::va_arg_int32(va);
    }

    lucis::unsafe::va_end(va);

    println(total);
}

fn main() int32 {
    sum(3, 10, 20, 30);   // 60
    sum(2, 42, 100);       // 142
    ret 0;
}
```

When arguments of different types are needed, use the corresponding typed helper for each:

```
fn print_mixed(int32 count, ...) void {
    va_list va = lucis::unsafe::va_list();
    lucis::unsafe::va_start(va);

    int32   i = lucis::unsafe::va_arg_int32(va);
    float64 f = lucis::unsafe::va_arg_float64(va);
    bool    b = lucis::unsafe::va_arg_bool(va);
    string  s = lucis::unsafe::va_arg_string(va);

    println(i);
    println(f);
    println(b);
    println(s);

    lucis::unsafe::va_end(va);
}

fn main() int32 {
    print_mixed(4, 42, 3.14, true, c"hello");
    ret 0;
}
```

### Key Differences from Typed Variadics

| Aspect | Typed `int32 ...values` | Untyped `...` |
|--------|-------------------------|---------------|
| Argument type | Homogeneous (all `int32`) | Any type per call |
| Access method | Array-like (`values[0]`, `for..in`) | Must use `lucis::unsafe::va_*` intrinsics |
| Type safety | Fully type-checked at compile time | Unchecked — `va_arg<T>` read type must match what was passed |
| Zero-argument calls | Allowed (`sum()`) | Not possible (need at least one fixed param for counting) |

### When to Use

- **Prefer typed variadics** (`int32 ...values`) when all variadic arguments share the same type — they are safer, easier to use, and perform better.
- **Use untyped variadics** (`...`) when you need to pass arguments of different types (mimicking C varargs like `printf`), or when interfacing with C code that expects this pattern.

### Rules

- The untyped `...` must be the last parameter.
- At least one fixed parameter is required before `...` (unlike typed variadics which can stand alone).
- The function body must use `lucis::unsafe::va_list`, `va_start`, `va_arg_*`, and `va_end` to access the arguments (see [Intrinsics](intrinsics.md#variadic-argument-support)).
- The caller is responsible for passing arguments that match the types read by the `va_arg_*` helpers — there is no compile-time type checking for individual variadic arguments.

## Function Pointers

Functions can be referenced by their address and stored in variables:

```

use std::log::println;

fn add(int32 a, int32 b) int32 {
    ret a + b;
}

fn multiply(int32 a, int32 b) int32 {
    ret a * b;
}

fn main() int32 {
    fn(int32, int32) -> int32 op = add;
    println(op(3, 4));      // 7

    op = multiply;
    println(op(3, 4));      // 12

    ret 0;
}
```

### Function Type Syntax

The type of a function pointer uses the `fn` keyword:

```
fn(ParamType1, ParamType2) -> ReturnType
```

Examples:

```
fn(int32, int32) -> int32       // takes two int32, returns int32
fn(string) -> void              // takes string, returns nothing
fn() -> bool                    // takes nothing, returns bool
```

### Type Aliases for Function Types

Use `type` to create named aliases for function types:

```
type BinOp = fn(int32, int32) -> int32;
type Predicate = fn(int32) -> bool;
type Action = fn() -> void;
```

### Higher-Order Functions

Functions can accept function pointers as parameters and return them:

```

use std::log::println;

type BinOp = fn(int32, int32) -> int32;

fn add(int32 a, int32 b) int32 { ret a + b; }
fn mul(int32 a, int32 b) int32 { ret a * b; }

fn apply(BinOp op, int32 x, int32 y) int32 {
    ret op(x, y);
}

fn main() int32 {
    println(apply(add, 3, 4));    // 7
    println(apply(mul, 5, 6));    // 30

    ret 0;
}
```

## Forward Declarations

Functions can be called before they are defined in the same file. The compiler resolves all function declarations before generating code:

```

use std::log::println;

fn main() int32 {
    println(double_value(21));    // 42 — works even though defined below
    ret 0;
}

fn double_value(int32 x) int32 {
    ret x * 2;
}
```

## The `main` Function

Every Lucis program must have a `main` function that:

- Returns `int32` (the exit code)
- Takes no parameters, or a single `vec<string>` parameter for command-line arguments
- Ends with `ret 0;` (or another integer exit code)

```
fn main() int32 {
    // program logic
    ret 0;
}
```

### Command-Line Arguments

To receive command-line arguments, declare `main` with a `vec<string>` parameter:

```
use std::log::println;

fn main(vec<string> args) int32 {
    for string arg in args {
        println(arg);
    }

    ret 0;
}
```

The vector contains all arguments passed to the compiled binary, including the program name as the first element.

## See Also

- [Variables](variables.md) — Variable scope in function bodies
- [Types](types.md) — Function type syntax
- [Type Aliases](type-aliases.md) — Named function type aliases
- [Control Flow](control-flow.md) — Control flow within functions
- [Pointers](pointers.md) — Function pointers and address-of
