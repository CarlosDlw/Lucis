# Variables

This page covers variable declaration, initialization, scope, and reassignment in Lucis.

## Declaration

Variables are declared with type-first syntax — the type comes before the variable name:

```
int32 age = 25;
float64 temperature = 36.6;
string name = "Alice";
bool is_active = true;
```

## Type Inference with `auto`

Instead of writing the type explicitly, you can use `auto` to let the compiler infer the type from the initializer expression:

```

use std::log::println;

#include <stdio.h>

fn main() int32 {
    auto x = 42;              // inferred as int32
    auto pi = 3.14;           // inferred as float64
    auto name = "Alice";      // inferred as string
    auto flag = true;         // inferred as bool
    auto ch = 'A';            // inferred as char

    println(typeof(x));       // int32
    println(typeof(pi));      // float64
    println(typeof(name));    // string

    // also works with function call return types
    auto n = printf(c"hello\n");
    println(typeof(n));       // int32

    ret 0;
}
```

`auto` requires an initializer — the compiler needs a value to infer the type from. Declaring `auto x;` without an assignment is a compile-time error.

The inferred type follows the same rules as explicit types:

| Expression | Inferred Type |
|---|---|
| `42` | `int32` |
| `3.14` | `float64` |
| `true` / `false` | `bool` |
| `'A'` | `char` |
| `"hello"` | `string` |
| `c"hello"` | `*char` |
| `null` | `*void` |
| Function call | Return type of the function |

## Uninitialized Variables

You can declare a variable without assigning a value. The compiler zero-initializes it:

```

use std::log::println;

fn main() int32 {
    int32 count;        // initialized to 0
    float64 total;      // initialized to 0.0
    bool flag;          // initialized to false
    string text;        // initialized to ""

    println(count);     // 0
    println(total);     // 0.000000
    println(flag);      // 0
    println(text);      // (empty)

    // assign later
    count = 42;
    println(count);     // 42

    ret 0;
}
```

Zero-initialization values by type:

| Type | Zero Value |
|---|---|
| Integer types | `0` |
| Float types | `0.0` |
| `bool` | `false` |
| `string` | `""` (empty string) |
| `char` | `'\0'` |
| Pointer types | `null` |

## Reassignment

Variables can be reassigned after declaration. There is no `const` or `let` — all variables are mutable:

```

use std::log::println;

fn main() int32 {
    int32 x = 10;
    println(x);      // 10

    x = 42;
    println(x);      // 42

    x = x + 8;
    println(x);      // 50

    ret 0;
}
```

## Ownership and Moves

For heap-backed values (`string`, `vec`, `map`, `set`), reassignment and initialization may move ownership.

```
string a = fromCStrCopy(c"hello");
string b = a;      // move
// println(a);     // compile-time error: use-after-move
println(b);
```

Use clone/copy-style APIs when you need to keep both values alive.

## Compound Assignment

T supports compound assignment operators that combine an operation with assignment:

```

use std::log::println;

fn main() int32 {
    int32 x = 10;

    x += 5;       // x = x + 5    → 15
    println(x);

    x -= 3;       // x = x - 3    → 12
    println(x);

    x *= 2;       // x = x * 2    → 24
    println(x);

    x /= 6;       // x = x / 6    → 4
    println(x);

    x %= 3;       // x = x % 3    → 1
    println(x);

    ret 0;
}
```

The full list of compound assignment operators: `+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `|=`, `^=`, `<<=`, `>>=`.

## Increment and Decrement

T supports both prefix and postfix `++` and `--`:

```

use std::log::println;

fn main() int32 {
    int32 a = 5;

    // prefix: increments, then returns new value
    int32 b = ++a;
    println(a);    // 6
    println(b);    // 6

    // postfix: returns current value, then increments
    int32 c = a++;
    println(a);    // 7
    println(c);    // 6

    // as standalone statements
    int32 n = 0;
    n++;
    n++;
    n++;
    println(n);    // 3

    n--;
    println(n);    // 2

    ret 0;
}
```

## Scope

Variables are scoped to the block `{}` in which they are declared. A variable declared inside a block is not accessible outside that block:

```

use std::log::println;

fn main() int32 {
    int32 x = 10;

    if x > 5 {
        int32 y = 20;
        println(y);     // 20 — y is accessible here
    }

    // y is NOT accessible here — it was declared inside the if block

    for int32 i in 0..3 {
        println(i);     // i is scoped to the for body
    }

    // i is NOT accessible here

    ret 0;
}
```

## Shadowing

A variable in an inner scope can have the same name as a variable in an outer scope. The inner variable shadows the outer one for the duration of the block:

```

use std::log::println;

fn main() int32 {
    int32 x = 10;
    println(x);        // 10

    if true {
        int32 x = 20;  // shadows outer x
        println(x);    // 20
    }

    println(x);        // 10 — outer x is unchanged

    ret 0;
}
```

## Multiple Declarations

Each variable declaration is a separate statement. Lucis does not support declaring multiple variables on one line:

```
// each on its own line
int32 width = 10;
int32 height = 5;
int32 area = width * height;
```

## Global Variables

Variables declared outside of any function are global and accessible throughout the file:

```

use std::log::println;

int32 counter = 0;

fn increment() void {
    counter += 1;
}

fn main() int32 {
    increment();
    increment();
    increment();
    println(counter);   // 3

    ret 0;
}
```

## See Also

- [Types](types.md) — All built-in types
- [Operators](operators.md) — Arithmetic, comparison, logical, bitwise operators
- [Control Flow](control-flow.md) — If, for, while, switch
- [Functions](functions.md) — Function parameters and return values
- [Keywords](../reference/keywords.md) — Complete list of reserved keywords including `auto`
