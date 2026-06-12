# Tuples

Tuples are fixed-size, heterogeneous collections of values. Unlike structs, tuple elements are accessed by numeric index rather than by name, making them ideal for quick groupings, multiple return values, and lightweight data passing.

---

## Declaration

A tuple type is written as `tuple<T1, T2, ...>` with at least two type parameters:

```
tuple<int32, string> pair = (42, "hello");
tuple<int32, float64, bool> triple = (10, 3.14, true);
```

### Type Inference

The `auto` keyword infers the tuple type from the literal:

```
auto pair = (42, "hello");
auto triple = (10, 3.14, true);
```

---

## Tuple Literals

Tuple values are created with parenthesized, comma-separated expressions:

```
auto coords = (3, 7);
auto record = ("alice", 25, true);
auto nested = (1, (2, 3));
```

A tuple literal must have at least two elements. Single-element parenthesized expressions are treated as grouping: `(42)` is just `42`.

---

## Element Access

Elements are accessed with zero-based numeric indices using dot notation:

```
auto t = (42, "hello", true);
println(t.0);   // 42
println(t.1);   // hello
println(t.2);   // true
```

### Nested Access

For nested tuples, indices can be chained:

```
tuple<int32, tuple<string, bool>> nested = (99, ("inner", true));
println(nested.0);     // 99
println(nested.1.0);   // inner
println(nested.1.1);   // true
```

---

## Destructuring

Tuples can be destructured into individual variables using `auto`:

```
auto (x, y) = (100, 200);
println(x);   // 100
println(y);   // 200
```

Destructuring also works with function return values:

```
fn makeRecord(string name, int32 age) tuple<string, int32, bool> {
    ret (name, age, age >= 18);
}

auto (name, age, adult) = makeRecord("alice", 25);
println(name);    // alice
println(age);     // 25
println(adult);   // true
```

---

## Function Parameters and Return Types

Tuples work as both parameters and return values:

```
fn swap(tuple<int32, int32> t) tuple<int32, int32> {
    ret (t.1, t.0);
}

auto result = swap((3, 7));
println(result.0);   // 7
println(result.1);   // 3
```

Integer and float literals in tuple arguments are automatically coerced to match the parameter type. In the example above, `(3, 7)` is coerced from the default integer size to `tuple<int32, int32>`.

---

## Pointer Access

When working with pointers to tuples, use the arrow operator `->` with numeric indices:

```
auto t = (10, 20);
*tuple<int32, int32> ptr = &t;
println(ptr->0);   // 10
println(ptr->1);   // 20
```

Arrow access also supports chained indices for nested tuples:

```
auto nested = (1, (2, 3));
*tuple<int32, tuple<int32, int32>> ptr = &nested;
println(ptr->0);     // 1
println(ptr->1.0);   // 2
println(ptr->1.1);   // 3
```

---

## Nested Tuples

Tuples can contain other tuples (and any other type) as elements:

```
tuple<int32, tuple<string, bool>> tagged = (1, ("active", true));
println(tagged.0);     // 1
println(tagged.1.0);   // active
println(tagged.1.1);   // true
```

Nested generic types with `>>` are handled correctly — no space required between closing angle brackets:

```
tuple<int32, tuple<string, bool>> works = (1, ("ok", true));
tuple<int32, vec<int32>> also_works = (1, my_vec);
```

---

## Tuples with Expressions

Tuple elements can be arbitrary expressions:

```
int32 a = 5;
int32 b = 10;
auto computed = (a + b, a * b);
println(computed.0);   // 15
println(computed.1);   // 50
```

---

## Tuples in `sprintf`

Tuple elements can be used directly in format strings:

```
auto coords = (3, 7);
string msg = sprintf("x={} y={}", coords.0, coords.1);
println(msg);   // x=3 y=7
```

---

## Large Tuples

There is no hard limit on the number of elements:

```
auto big = (1, 2, 3, 4, 5);
println(big.0);   // 1
println(big.4);   // 5
```

---

## Summary

| Feature | Syntax | Example |
|---|---|---|
| Type declaration | `tuple<T1, T2, ...>` | `tuple<int32, string>` |
| Literal | `(expr, expr, ...)` | `(42, "hello")` |
| Element access | `.N` | `t.0`, `t.1` |
| Nested access | `.N.M` | `nested.1.0` |
| Pointer access | `->N` | `ptr->0` |
| Destructuring | `auto (a, b) = expr` | `auto (x, y) = (1, 2)` |
| Function return | `tuple<T1, T2>` | `ret (a, b);` |
| Type inference | `auto` | `auto t = (1, 2)` |
