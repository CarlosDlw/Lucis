# Operators

This page documents all operators in Lucis, organized by category, with examples and a complete precedence table.

## Arithmetic Operators

| Operator | Description | Example |
|---|---|---|
| `+` | Addition | `10 + 5` → `15` |
| `-` | Subtraction | `20 - 8` → `12` |
| `*` | Multiplication | `6 * 7` → `42` |
| `/` | Division | `100 / 4` → `25` |
| `%` | Modulo (remainder) | `17 % 5` → `2` |
| `-` (unary) | Negation | `-42` |

```

use std::log::println;

fn main() int32 {
    int32 a = 10;
    int32 b = 3;

    println(a + b);    // 13
    println(a - b);    // 7
    println(a * b);    // 30
    println(a / b);    // 3 (integer division)
    println(a % b);    // 1

    // precedence: * and / before + and -
    int32 result = 2 + 3 * 4;
    println(result);   // 14

    // parentheses override precedence
    int32 grouped = (2 + 3) * 4;
    println(grouped);  // 20

    ret 0;
}
```

## Comparison Operators

All comparison operators return `bool`:

| Operator | Description | Example |
|---|---|---|
| `==` | Equal to | `10 == 10` → `true` |
| `!=` | Not equal to | `10 != 5` → `true` |
| `<` | Less than | `5 < 10` → `true` |
| `>` | Greater than | `10 > 5` → `true` |
| `<=` | Less than or equal | `5 <= 5` → `true` |
| `>=` | Greater than or equal | `10 >= 10` → `true` |

```

use std::log::println;

fn main() int32 {
    int32 a = 10;
    int32 b = 20;

    println(a == 10);     // 1 (true)
    println(a != b);      // 1
    println(a < b);       // 1
    println(a > b);       // 0 (false)
    println(a <= 10);     // 1
    println(b >= 20);     // 1

    // with expressions
    println(a * 2 == b);  // 1

    ret 0;
}
```

## Logical Operators

| Operator | Description | Example |
|---|---|---|
| `&&` | Logical AND | `true && false` → `false` |
| `\|\|` | Logical OR | `true \|\| false` → `true` |
| `!` | Logical NOT | `!true` → `false` |

```

use std::log::println;

fn main() int32 {
    bool a = true;
    bool b = false;

    println(!a);          // 0 (false)
    println(a && b);      // 0
    println(a || b);      // 1 (true)

    // combined with comparisons
    int32 x = 10;
    println((x > 0) && (x < 100));   // 1
    println((x > 100) || (x == 10)); // 1

    ret 0;
}
```

`&&` and `||` use short-circuit evaluation: the right operand is not evaluated if the left operand determines the result.

## Bitwise Operators

| Operator | Description | Example |
|---|---|---|
| `&` | Bitwise AND | `12 & 10` → `8` |
| `\|` | Bitwise OR | `12 \| 10` → `14` |
| `^` | Bitwise XOR | `12 ^ 10` → `6` |
| `~` | Bitwise NOT | `~0` → `-1` |
| `<<` | Left shift | `1 << 4` → `16` |
| `>>` | Right shift | `64 >> 2` → `16` |

```

use std::log::println;

fn main() int32 {
    int32 a = 0b1100;    // 12 in binary
    int32 b = 0b1010;    // 10 in binary

    println(a & b);   // 8  (1000)
    println(a | b);   // 14 (1110)
    println(a ^ b);   // 6  (0110)
    println(~0);      // -1

    println(1 << 4);  // 16
    println(64 >> 2); // 16

    // hex literals are convenient for bitmask operations
    int32 value = 0xFF;
    println(value & 0x0F);    // 15
    println(value | 0x100);   // 511
    println(value ^ 0xFF);    // 0

    // set and check flags
    int32 flags = 0 | (1 << 3);   // set bit 3
    println(flags & 8);            // 8 (bit 3 is set)

    ret 0;
}
```

## Assignment Operators

| Operator | Description |
|---|---|
| `=` | Simple assignment |
| `+=` | Add and assign |
| `-=` | Subtract and assign |
| `*=` | Multiply and assign |
| `/=` | Divide and assign |
| `%=` | Modulo and assign |
| `&=` | Bitwise AND and assign |
| `\|=` | Bitwise OR and assign |
| `^=` | Bitwise XOR and assign |
| `<<=` | Left shift and assign |
| `>>=` | Right shift and assign |

```

use std::log::println;

fn main() int32 {
    int32 x = 10;
    x += 5;      println(x);  // 15
    x -= 3;      println(x);  // 12
    x *= 2;      println(x);  // 24
    x /= 6;      println(x);  // 4
    x %= 3;      println(x);  // 1

    int32 bits = 1;
    bits <<= 4;  println(bits); // 16
    bits >>= 2;  println(bits); // 4

    ret 0;
}
```

## Increment and Decrement

| Operator | Description |
|---|---|
| `++x` | Pre-increment: increment, then return |
| `x++` | Post-increment: return, then increment |
| `--x` | Pre-decrement: decrement, then return |
| `x--` | Post-decrement: return, then decrement |

See [Variables — Increment and Decrement](variables.md#increment-and-decrement) for examples.

## Ternary Operator

The ternary operator provides inline conditional expressions:

```
int32 max = a > b ? a : b;
```

```

use std::log::println;

fn main() int32 {
    int32 a = 10;
    int32 b = 20;

    int32 max = a > b ? a : b;
    println(max);   // 20

    int32 min = a < b ? a : b;
    println(min);   // 10

    // nested ternary
    int32 x = 5;
    int32 category = x > 10 ? 1 : (x > 3 ? 2 : 3);
    println(category);  // 2

    ret 0;
}
```

## Null Coalescing Operator

The `??` operator provides a fallback value when a pointer is null:

```
int32 result = ptr ?? 0;    // 0 if ptr is null
```

```

use std::log::println;

fn main() int32 {
    *int32 valid = null;
    int32 fallback = valid ?? 42;
    println(fallback);   // 42

    ret 0;
}
```

### Null Coalescing Assignment (`??=`)

The `??=` operator assigns a value only if the target pointer is currently null:

```
ptr ??= default;  // only assigns if ptr == null
```

```

use stdio::printf;

fn main() int32 {
    *int32 ptr = null;
    int32 val = 42;

    // ptr is null, so ??= assigns
    ptr ??= &val;
    printf("*ptr = {d}\n", *ptr);   // 42

    // ptr is not null now, so ??= does nothing
    int32 other = 99;
    ptr ??= &other;
    printf("*ptr = {d}\n", *ptr);   // still 42
    ret 0;
}
```

The `??=` operator works with all compound assignment lvalue forms: simple identifiers, field access (`obj.field ??=`), arrow access (`ptr->field ??=`), and deref (`*ptr ??=` for pointer-to-pointer types).

## Type Operators

| Operator | Description | Example |
|---|---|---|
| `as` | Type cast | `x as float64` |
| `is` | Type check | `x is int32` |
| `sizeof(T)` | Size of type in bytes | `sizeof(int32)` → `4` |
| `typeof(expr)` | Type name as string | `typeof(42)` → `"int32"` |

```

use std::log::println;

fn main() int32 {
    int32 x = 42;

    float64 f = x as float64;
    println(f);              // 42.000000

    println(sizeof(int32));  // 4
    println(sizeof(int64));  // 8
    println(typeof(x));      // int32

    ret 0;
}
```

## Pointer Operators

| Operator | Description | Example |
|---|---|---|
| `&x` | Address-of | `*int32 ptr = &x;` |
| `*ptr` | Dereference | `int32 val = *ptr;` |
| `ptr->field` | Arrow access | `ptr->x` (same as `(*ptr).x`) |
| `.` | Dot access | `point.x` |

See [Pointers](pointers.md) for full documentation.

## Range Operators

| Operator | Description | Example |
|---|---|---|
| `..` | Exclusive range | `0..5` → 0, 1, 2, 3, 4 |
| `..=` | Inclusive range | `0..=5` → 0, 1, 2, 3, 4, 5 |

See [Ranges](ranges.md) for full documentation.

## Scope Resolution Operator

`::` is used for module access, enum variants, and static method calls:

```
use std::log::println;        // module access
Color c = Color::Red;          // enum variant
int32 r = Math::compute(x);    // static method
```

## Spread Operator

`...` is used in variadic function declarations:

```
extern int32 printf(*char fmt, ...);
```

## Operator Precedence

From highest to lowest precedence:

| Precedence | Operators | Associativity |
|---|---|---|
| 1 (highest) | `.method()`, `()`, `.field`, `->field`, `[]`, `as`, `is`, `x++`, `x--` | Left to right |
| 2 | `*` (deref), `&` (addr), `-` (negate), `!`, `~`, `++x`, `--x`, `spawn`, `await`, `sizeof`, `typeof` | Right to left |
| 3 | `*`, `/`, `%` | Left to right |
| 4 | `+`, `-` | Left to right |
| 5 | `<<`, `>>` | Left to right |
| 6 | `<`, `>`, `<=`, `>=` | Left to right |
| 7 | `==`, `!=` | Left to right |
| 8 | `&` (bitwise AND) | Left to right |
| 9 | `^` (bitwise XOR) | Left to right |
| 10 | `\|` (bitwise OR) | Left to right |
| 11 | `&&` (logical AND) | Left to right |
| 12 | `\|\|` (logical OR) | Left to right |
| 13 | `??` (null coalescing) | Left to right |
| 14 | `..`, `..=` (range) | Left to right |
| 15 | `? :` (ternary) | Right to left |
| 16 (lowest) | `try` | Right to left |

## See Also

- [Variables](variables.md) — Variable declaration and compound assignment
- [Types](types.md) — Type casting and conversion
- [Control Flow](control-flow.md) — Comparison operators in conditions
- [Pointers](pointers.md) — Pointer operators in depth
- [Operator Precedence](../reference/operator-precedence.md) — Full reference table
