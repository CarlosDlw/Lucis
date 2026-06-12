# Structs

Structs are user-defined composite types that group named fields together. Lux structs support field access, mutation, nested composition, self-referencing pointers, and methods via `extend` blocks.

---

## Declaration

A struct is defined with the `struct` keyword followed by a name and a block of typed fields:

```
struct Point {
    int32 x;
    int32 y;
}
```

```
struct Particle {
    Vec2 pos;
    int32 speed;
    Color color;
    bool active;
}
```

Fields follow the standard type-first declaration syntax.

---

## Struct Literals

Structs are instantiated using a literal syntax with named fields:

```
Point p = Point { x: 10, y: 20 };
println(p.x);   // 10
println(p.y);   // 20
```

All fields must be specified in the literal.

---

## Field Access and Mutation

Fields are accessed with dot notation and can be reassigned:

```
Point p = Point { x: 10, y: 20 };
println(p.x);   // 10

p.x = 99;
println(p.x);   // 99
```

---

## Nested Structs

Structs can contain other structs as fields:

```
struct Vec2 {
    int32 x;
    int32 y;
}

struct Rect {
    Vec2 origin;
    Vec2 size;
}

Rect r = Rect {
    origin: Vec2 { x: 0, y: 0 },
    size: Vec2 { x: 100, y: 50 }
};

println(r.size.x);   // 100
```

---

## Pointer Fields and Self-Referencing Structs

Structs can contain pointer fields, including pointers to their own type (useful for linked data structures):

```
struct Node {
    int32 value;
    *Node next;
}

Node c = Node { value: 30, next: null };
Node b = Node { value: 20, next: &c };
Node a = Node { value: 10, next: &b };

println(a.value);              // 10
println(a.next->value);        // 20
println(a.next->next->value);  // 30
```

### Cross-Struct Pointers

```
struct Inner {
    int32 x;
    int32 y;
}

struct Outer {
    int32 id;
    *Inner data;
}

Inner point = Inner { x: 100, y: 200 };
Outer wrapper = Outer { id: 1, data: &point };

println(wrapper.data->x);   // 100
println(wrapper.data->y);   // 200
```

Use `->` (arrow) to access fields through pointers, and `.` (dot) for direct field access.

---

## Methods with `extend`

The `extend` block adds methods to a struct without modifying its declaration. Methods can be **static** (no receiver) or **instance** (with `&self`).

### Static Methods

Static methods are called with `Struct::method()` syntax. They don't have access to an instance:

```
struct Vec2 {
    int32 x;
    int32 y;
}

extend Vec2 {
    fn create(int32 x, int32 y) Vec2 {
        Vec2 v = Vec2 { x: x, y: y };
        ret v;
    }

    fn zero() Vec2 {
        Vec2 v = Vec2 { x: 0, y: 0 };
        ret v;
    }
}

Vec2 a = Vec2::create(3, 4);
Vec2 z = Vec2::zero();
```

### Instance Methods

Instance methods receive `&self` — a pointer to the struct instance. They are called with dot notation:

```
extend Vec2 {
    fn manhattanLength(&self) int32 {
        int32 ax = self->x;
        int32 ay = self->y;
        if (ax < 0) { ax = 0 - ax; }
        if (ay < 0) { ay = 0 - ay; }
        ret ax + ay;
    }

    fn isZero(&self) bool {
        ret self->x == 0 && self->y == 0;
    }

    fn add(&self, Vec2 other) Vec2 {
        Vec2 result = Vec2 { x: self->x + other.x, y: self->y + other.y };
        ret result;
    }

    fn translate(&self, int32 dx, int32 dy) void {
        self->x += dx;
        self->y += dy;
    }
}

Vec2 a = Vec2::create(3, 4);
println(a.manhattanLength());   // 7
println(a.isZero());            // false

Vec2 b = Vec2::create(10, 20);
Vec2 c = a.add(b);
println(c.x);   // 13
println(c.y);   // 24

a.translate(100, 200);
println(a.x);   // 103
println(a.y);   // 204
```

Inside instance methods, `self` is a pointer (`*StructType`), so fields are accessed with `self->field`.

### Extending with Enums and Other Structs

Methods can return any type, including enums and other structs:

```
struct Particle {
    Vec2 pos;
    int32 speed;
    Color color;
    bool active;
}

extend Particle {
    fn create(int32 x, int32 y) Particle {
        Vec2 p = Vec2 { x: x, y: y };
        Particle part = Particle { pos: p, speed: 1, color: Color::Red, active: true };
        ret part;
    }

    fn getColor(&self) Color {
        ret self->color;
    }

    fn getPosition(&self) Vec2 {
        ret self->pos;
    }

    fn deactivate(&self) void {
        self->active = false;
    }
}

Particle p = Particle::create(7, 8);
println(p.getSpeed());     // 1
println(p.isActive());     // true

p.deactivate();
println(p.isActive());     // false
```

---

## C Struct Interop

Structs from C headers are automatically imported when using `#include`. They follow the x86-64 System V ABI for by-value passing:

| Size | Passing Convention |
|------|-------------------|
| ≤ 8 bytes | Single register |
| 9-16 bytes | Two registers |
| > 16 bytes | Indirect (sret/byval) |

```
#include "structs.h"

Point p = make_point(10, 20);
int32 sum = point_sum(p);
println(sum);   // 30
```

---

## See Also

- [Enums](enums.md) — Enum types
- [Unions](unions.md) — Union types (shared memory)
- [Pointers](pointers.md) — Pointer operations and `->` access
- [Generics](generics.md) — Generic types like `vec<T>`
