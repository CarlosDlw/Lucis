# Pointers

Pointers in Lucis hold the memory address of a value. They support address-of, dereference, arrow notation for struct fields, null, and pointer casting.

---

## Pointer Types

A pointer type is written as `*T`, where `T` is the pointed-to type:

```
*int32       // pointer to int32
*float64     // pointer to float64
*Point       // pointer to a struct
*void        // generic pointer (like C's void*)
*char        // C string pointer (also aliased as cstring)
```

---

## Address-of (`&`)

The `&` operator takes the address of a variable:

```
int32 x = 42;
*int32 p = &x;
```

---

## Dereference (`*`)

The unary `*` operator reads or writes the value at the pointed-to address:

### Read

```
int32 x = 42;
*int32 p = &x;
println(*p);   // 42
```

### Write

```
*p = 99;
println(x);   // 99
```

Writing through a dereferenced pointer modifies the original variable.

---

## Null Pointers

Pointers can be initialized to `null`:

```
*int32 p = null;
```

Use the `??` (null coalescing) operator to provide a fallback:

```
*int32 p = null;
int32 val = p ?? 0;    // 0 (p is null)
```

---

## Arrow Notation (`->`)

The `->` operator accesses struct fields through a pointer, combining dereference and field access:

```
struct Point {
    int32 x;
    int32 y;
}

Point p = Point { x: 10, y: 20 };
*Point ptr = &p;

println(ptr->x);   // 10
println(ptr->y);   // 20
```

Arrow also works for assignment:

```
ptr->x = 99;
println(p.x);   // 99
```

### Chained Arrow Access

For nested pointer structures like linked lists:

```
struct Node {
    int32 value;
    *Node next;
}

Node c = Node { value: 30, next: null };
Node b = Node { value: 20, next: &c };
Node a = Node { value: 10, next: &b };

println(a.next->value);        // 20
println(a.next->next->value);  // 30
```

---

## Pointer Casting

Pointers can be cast between types using `as`:

```
*int32 p = &x;
*void generic = p as *void;
*int32 back = generic as *int32;
```

This is commonly used for FFI with `malloc`/`free`:

```
*void buf = malloc(64);
*char cbuf = buf as *char;
free(buf);
```

---

## Cross-Struct Pointers

Structs can hold pointers to other struct types:

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

---

## Self-Referencing Structs

Structs can have pointer fields to their own type, enabling linked data structures:

```
struct Node {
    int32 value;
    *Node next;
}

Node c = Node { value: 30, next: null };
Node b = Node { value: 20, next: &c };
Node a = Node { value: 10, next: &b };
```

---

## Comparison: `.` vs `->`

| Syntax | Used on | Example |
|--------|---------|---------|
| `.` | Direct struct values | `p.x` |
| `->` | Pointers to structs | `ptr->x` |
| `->` | `&self` in methods | `self->x` |

> **Precedence note:** postfix operators (`.`, `[]`, `()`) bind tighter than the unary dereference `*`. This means `*ptr.field` is parsed as `*(ptr.field)`, **not** `(*ptr).field`. When accessing a field through a pointer, always use `ptr->field` or parentheses: `(*ptr).field`.

---

## See Also

- [Memory Management](memory-management.md) — `defer`, auto-cleanup, `std::mem`
- [Structs](structs.md) — Struct declarations and `extend` methods
- [Expressions](expressions.md) — Null coalescing (`??`)
- [Types](types.md) — `*void`, `cstring` pointer types
