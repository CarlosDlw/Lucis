# Expressions

Expressions in Lux produce values. They can be combined, nested, and used anywhere a value is expected — in assignments, function arguments, return statements, conditions, and more.

This page covers expression forms beyond individual operators (see [Operators](operators.md) for arithmetic, comparison, logical, and bitwise operators).

---

## Ternary Expression

The ternary operator `? :` evaluates one of two expressions based on a condition:

```
int32 a = 10;
int32 b = 20;

int32 max = a > b ? a : b;
println(max);   // 20

int32 min = a < b ? a : b;
println(min);   // 10
```

Ternary expressions can be nested:

```
int32 x = 5;
int32 result = x > 10 ? 1 : (x > 3 ? 2 : 3);
println(result);   // 2
```

And used inside larger expressions:

```
int32 doubled = (a > 5 ? a : b) * 2;
println(doubled);   // 20
```

---

## Null Coalescing

The `??` operator provides a fallback value when a pointer is `null`:

```
int32 val = 42;
*int32 ptr = &val;

int32 result = ptr ?? 0;
println(result);   // 42

*int32 nilPtr = null;
int32 fallback = nilPtr ?? 99;
println(fallback);   // 99
```

Only works with pointer types. If the pointer is non-null, its dereferenced value is returned. If null, the right-hand side is used.

---

## Type Cast (`as`)

The `as` keyword casts a value from one type to another:

```
int32 x = 42;
int64 wide = x as int64;
float64 f = x as float64;

float64 pi = 3.14;
int32 truncated = pi as int32;   // 3
```

Casts are type-checked by the compiler. See [Types](types.md) for the full conversion table.

---

## Type Check (`is`)

The `is` keyword checks whether an expression matches a given type:

```
bool check = x is int32;   // true
```

---

## `sizeof` and `typeof`

`sizeof` returns the byte size of a type at compile time:

```
println(sizeof(int8));     // 1
println(sizeof(int32));    // 4
println(sizeof(int64));    // 8
println(sizeof(float64));  // 8
println(sizeof(bool));     // 1
println(sizeof(char));     // 1
```

`typeof` returns the type name as a string at runtime:

```
string name = typeof(42);   // "int32"
```

---

## Increment and Decrement

Both prefix and postfix forms are supported:

```
int32 a = 5;

// Prefix: increments, then returns new value
int32 b = ++a;   // a = 6, b = 6

// Postfix: returns current value, then increments
int32 x = 10;
int32 y = x++;   // x = 11, y = 10
```

Can be used as standalone statements:

```
int32 n = 0;
n++;
n++;
n++;
println(n);   // 3
```

---

## Struct Literals

Structs are created with named field syntax:

```
Point p = Point { x: 10, y: 20 };
```

See [Structs](structs.md) for details.

---

## Enum Access

Enum variants are accessed with `::` scope resolution:

```
Color c = Color::Red;

Result<int32, string> ok = Result<int32, string>::Ok(10);
Shape s = Shape::Circle { r: 4.0 };
```

Variant checks also work inside expressions via `is`:

```
if ok is Result<int32, string>::Ok(value) {
	println(value);
}
```

See [Enums](enums.md) for details.

---

## Method Calls

Methods are called with dot notation on values or arrow notation on pointers:

```
int32 x = -42;
int32 a = x.abs();        // 42

float64 f = 9.0;
float64 s = f.sqrt();     // 3.0

string name = "hello";
bool empty = name.isEmpty();  // false
```

See [Types](types.md) for all built-in type methods and [Structs](structs.md) for user-defined methods via `extend`.

---

## Array and Index Expressions

Array literals create arrays, and indexing accesses elements:

```
[]int32 nums = [10, 20, 30];
println(nums[0]);   // 10

// Multi-dimensional
[][]int32 matrix = [[1, 2], [3, 4]];
println(matrix[1][0]);   // 3
```

Subscript also works on `vec<T>` and `map<K, V>`:

```
vec<int32> v = [1, 2, 3];
println(v[0]);   // 1

map<string, int64> m;
m.insert("key", 42);
println(m["key"]);   // 42
```

---

## List Comprehensions

List comprehensions generate arrays inline:

```
[4]int32 squares = [x * x | for int32 x in 0..4];
// [0, 1, 4, 9]

[8]int32 evens = [x | for int32 x in 0..8 if x % 2 == 0];
// [0, 2, 4, 6]
```

See [Arrays](arrays.md) for full details.

---

## Pointer Expressions

Address-of (`&`) and dereference (`*`) operate on pointers:

```
int32 x = 42;
*int32 p = &x;
println(*p);   // 42
```

Arrow (`->`) accesses struct fields through pointers:

```
*Point ptr = &p;
println(ptr->x);
```

See [Pointers](pointers.md) for details.

---

## Spawn and Await

Concurrency expressions create and resolve tasks:

```
Task<int32> t = spawn compute(10, 20);
int32 result = await t;
```

See [Concurrency](concurrency.md) for details.

---

## Expression Precedence

See [Operators](operators.md) for the full 16-level precedence table.

---

## See Also

- [Operators](operators.md) — Operator categories and precedence table
- [Variables](variables.md) — Compound assignment operators
- [Types](types.md) — Type casting and conversion
- [Arrays](arrays.md) — Array literals and list comprehensions
