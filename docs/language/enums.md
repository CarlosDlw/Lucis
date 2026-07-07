# Enums

Enums define tagged variant types. A variant can be unit (no payload), tuple payload, or named payload. Enums can also be generic.

---

## Declaration

An enum is defined with the `enum` keyword, a name, optional type parameters, and a comma-separated list of variants.

### Unit variants

```
enum Color {
    Red,
    Green,
    Blue,
}
```

```
enum Direction {
    North,
    South,
    East,
    West,
}
```

### Tuple payload variants

```
enum Token {
    Eof,
    Number(float64),
    Ident(string),
}
```

### Named payload variants

```
enum Shape {
    Unit,
    Circle { r: float64 },
    Rect { w: float64, h: float64 },
}
```

### Generic enums

```
enum Result<V, E> {
    Ok(V),
    Err(E),
}
```

---

## Explicit Discriminants

Each enum variant is assigned an integer discriminant (tag value) that identifies
the variant at runtime. By default, discriminants are assigned sequentially starting
from `0`. You can override this with `= expression`:

```
enum Color {
    Red   = 0,
    Green = 1,
    Blue  = 2,
}

enum HttpStatus {
    Ok       = 200,
    NotFound = 404,
    Error    = 500,
}
```

### Mixed explicit and implicit

When a variant has an explicit discriminant, subsequent implicit variants continue
sequentially from that value:

```
enum Mixed {
    First,         // 0
    Second = 10,   // 10
    Third,         // 11
    Fourth = 7,    // 7
    Fifth,         // 8
}
```

### Discriminant expressions

The expression after `=` must be a compile-time constant unsigned integer.
Simple literals, suffixed integer literals (`0u32`, `42i64`), and basic arithmetic
are supported.

Duplicate discriminant values are rejected at compile time:

```
enum Foo {
    A = 1,
    B = 1,  // compile error: duplicate discriminant 1
}
```

Discriminants can be specified on any variant kind — unit, tuple payload, or named
payload:

```
enum Shape {
    Unit,
    Circle { r: float64 } = 5,
    Rect(float64, float64) = 10,
}
```

---

## Using Enum Values

Enum variants are accessed with the `::` (scope resolution) operator:

```
Color c = Color::Red;
println(c);   // 0

Color g = Color::Green;
println(g);   // 1

Color b = Color::Blue;
println(b);   // 2
```

For payload variants, construct values by passing payload data:

```
Token t1 = Token::Eof;
Token t2 = Token::Number(42.0);
Token t3 = Token::Ident("lucis");

Shape s1 = Shape::Circle { r: 4.0 };
Shape s2 = Shape::Rect { w: 10.0, h: 6.0 };

Result<int32, string> ok = Result<int32, string>::Ok(123);
Result<int32, string> err = Result<int32, string>::Err("division by zero");
```

The generic enum access form is:

```
EnumName<T1, T2>::Variant
```

And with payload constructor:

```
EnumName<T1, T2>::Variant(...)
```

Named payload variants use braces:

```
EnumName::Variant { field: value }
```

or

```
EnumName<T>::Variant { field: value }
```

---

## Enums in Switch

Enums work naturally with `switch` statements:

```
Color c = Color::Green;

switch c {
    case Color::Red {
        println("red");
    }
    case Color::Green {
        println("green");
    }
    case Color::Blue {
        println("blue");
    }
}
```

You can also switch on payload variants by matching against concrete constructor expressions when needed.

---

## Variant Checks with is

`is` supports both type checks and enum variant identity checks.

```
Result<int32, string> res = Result<int32, string>::Err("boom");

if res is Result<int32, string>::Err {
    println("error variant");
}
```

You can optionally bind payload in the condition:

```
if res is Result<int32, string>::Err(message) {
    println(message);
}
```

Binder variables are scoped to the branch block where the condition is true.

---

## Enums in Structs

Enums can be used as struct fields:

```
enum Color {
    Red,
    Green,
    Blue,
}

struct Particle {
    Vec2 pos;
    int32 speed;
    Color color;
    bool active;
}

Particle p = Particle { pos: pos, speed: 1, color: Color::Red, active: true };
```

---

## Extend Support

Enums do not support `extend` blocks.

`extend` is reserved for structs and unions, while enums are constructed and checked through their variants:

```
Result<int32, string> value = Result<int32, string>::Ok(42);

if value is Result<int32, string>::Ok(number) {
    println(number);
}
```

---

## C Enum Interop

C enums from `#include` headers are automatically imported and mapped to integer constants:

```c
// colors.h
typedef enum {
    COLOR_RED   = 0,
    COLOR_GREEN = 1,
    COLOR_BLUE  = 2
} Color;

const char* color_name(Color c);
Color next_color(Color c);
```

```
#include "colors.h"

int32 r = COLOR_RED;     // 0
int32 g = COLOR_GREEN;   // 1
int32 b = COLOR_BLUE;    // 2

*char name = color_name(COLOR_GREEN);
int32 next = next_color(COLOR_BLUE);   // 0 (wraps to RED)
```

C enum variants are imported as `int32` constants with their original names.

---

## See Also

- [Structs](structs.md) — Composite types with named fields
- [Unions](unions.md) — Types with shared memory layout
- [Control Flow](control-flow.md) — `switch` statements
