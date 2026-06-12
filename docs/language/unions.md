# Unions

Unions are composite types where all fields share the same memory location. Writing to one field overwrites the data of all other fields. This is the same behavior as C unions.

---

## Declaration

A union is defined with the `union` keyword followed by a name and a block of typed fields. Unions can also declare type parameters:

```
union IntOrFloat {
    int32 i;
    float32 f;
}

union Result<T, E> {
    T ok;
    E err;
}
```

The size of the union is the size of its largest field. In this example, both `int32` and `float32` are 4 bytes, so `IntOrFloat` is 4 bytes.

---

## Union Literals

Unions are instantiated with the same literal syntax as structs, specifying the initial field:

```
IntOrFloat u = IntOrFloat { i: 42 };
printf(c"i = %d\n", u.i);   // i = 42

Result<int32, string> ok = Result<int32, string> { ok: 42 };
Result<int32, string> err = Result<int32, string> { err: "boom" };
```

Union literals must initialize exactly one field, including for generic unions.

---

## Field Access and Mutation

Fields are accessed with dot notation. Since all fields share memory, writing to one field changes the raw bits seen by all others:

```
IntOrFloat u = IntOrFloat { i: 42 };
println(u.i);   // 42

u.f = 3.14 as float32;
println(u.f);   // 3.14

// u.i is now the raw bit pattern of 3.14 as float32 — NOT 42
int32 raw = u.i;
assert(raw != 42);
```

This is the fundamental property of unions: **fields share memory**. Reading a field that wasn't the last one written gives you the raw bit reinterpretation of the stored value.

---

## When to Use Unions

Unions are useful for:

- **Type punning** — Reinterpreting the bits of one type as another
- **Memory-efficient variants** — Storing one of several types in the same space
- **C interop** — Matching C union layouts for FFI

---

## See Also

- [Structs](structs.md) — Composite types where each field has its own memory
- [Enums](enums.md) — Fixed set of named variants
- [Types](types.md) — Primitive type reference
