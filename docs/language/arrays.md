# Arrays

Arrays in Lux are fixed-size, stack-allocated sequences of elements of the same type. They support both explicit and inferred sizing, multi-dimensional layouts, indexing, built-in methods, and list comprehensions.

---

## Declaration

There are two forms of array declaration:

### Fixed-Size `[N]T`

The size is specified explicitly in the type:

```
[5]int32 nums = [10, 20, 30, 40, 50];
[3]float64 coords = [1.0, 2.5, 3.7];
[6]char hello = ['H', 'e', 'l', 'l', 'o', '\0'];
```

### Inferred-Size `[]T`

The size is determined from the initializer:

```
[]int32 nums = [10, 20, 30, 40, 50];
[]string names = ["alice", "bob", "carol"];
```

Both forms produce the same result — a contiguous block of `N` elements on the stack. `[]T` is syntactic sugar that lets the compiler count the elements for you.

---

## Indexing

Elements are accessed with zero-based indexing using `[i]`:

```
[]int32 nums = [10, 20, 30, 40, 50];
println(nums[0]);   // 10
println(nums[2]);   // 30
println(nums[4]);   // 50
```

### Index Assignment

Individual elements can be reassigned:

```
[]int32 nums = [10, 20, 30];
nums[1] = 99;
println(nums[1]);   // 99
```

---

## Multi-Dimensional Arrays

Arrays can be nested to create matrices and higher-dimensional structures:

```
[][]int32 matrix = [[1, 2, 3], [4, 5, 6]];
println(matrix[0][0]);   // 1
println(matrix[0][2]);   // 3
println(matrix[1][1]);   // 5

matrix[1][2] = 42;
println(matrix[1][2]);   // 42
```

---

## Array Methods

All arrays support the following built-in methods:

| Method | Signature | Description |
|--------|-----------|-------------|
| `len` | `() -> usize` | Number of elements |
| `isEmpty` | `() -> bool` | Whether array has zero elements |
| `at` | `(usize) -> T` | Element at index (bounds-checked) |
| `first` | `() -> T` | First element |
| `last` | `() -> T` | Last element |
| `contains` | `(T) -> bool` | Whether array contains a value |
| `indexOf` | `(T) -> int64` | Index of first occurrence, or -1 |
| `lastIndexOf` | `(T) -> int64` | Index of last occurrence, or -1 |
| `count` | `(T) -> usize` | Number of occurrences of a value |
| `fill` | `(T)` | Fill all elements with a value |
| `swap` | `(usize, usize)` | Swap two elements by index |
| `reverse` | `()` | Reverse elements in place |
| `copy` | `() -> [N]T` | Return a copy of the array |
| `slice` | `(usize, usize) -> []T` | Extract a sub-array |
| `sum` | `() -> T` | Sum of all elements (numeric only) |
| `product` | `() -> T` | Product of all elements (numeric only) |
| `min` | `() -> T` | Minimum element (numeric only) |
| `max` | `() -> T` | Maximum element (numeric only) |
| `minIndex` | `() -> usize` | Index of minimum element |
| `maxIndex` | `() -> usize` | Index of maximum element |
| `average` | `() -> float64` | Average value (numeric only) |
| `sort` | `()` | Sort in ascending order (numeric types only) |
| `sortDesc` | `()` | Sort in descending order (numeric types only) |
| `isSorted` | `() -> bool` | Whether elements are in ascending order |
| `equals` | `([N]T) -> bool` | Compare two arrays element-by-element |
| `toString` | `() -> string` | String representation |
| `join` | `(string) -> string` | Join elements with separator |
| `rotate` | `(int64)` | Rotate elements (positive = right, negative = left) |

```
[]int32 nums = [30, 10, 20, 10, 40];

println(nums.len());        // 5
println(nums.isEmpty());    // false
println(nums.first());      // 30
println(nums.last());       // 40
println(nums.at(2));        // 20
println(nums.contains(10)); // true
println(nums.indexOf(10));  // 1
println(nums.count(10));    // 2
```

---

## List Comprehensions

Arrays can be created using list comprehension syntax, which combines generation, transformation, and filtering in a single expression:

```
[expression | for type name in range]
[expression | for type name in range if condition]
```

### Basic Generation

```
[4]int32 seq = [x | for int32 x in 0..4];
// [0, 1, 2, 3]
```

### With Transformation

```
[4]int32 squares = [x * x | for int32 x in 0..4];
// [0, 1, 4, 9]
```

### Inclusive Range

```
[5]int32 incl = [x | for int32 x in 1..=5];
// [1, 2, 3, 4, 5]
```

### With Filter

```
[8]int32 evens = [x | for int32 x in 0..8 if x % 2 == 0];
// [0, 2, 4, 6]
```

### Transformation + Filter

```
[6]int32 oddSquares = [x * x | for int32 x in 0..6 if x % 2 != 0];
// [1, 9, 25]
```

> **Note:** The array size `[N]` in the type must be large enough to hold all generated elements. Filtered comprehensions may produce fewer elements than `N`.

---

## Array Decay

Fixed-size arrays can decay to pointers when passed to C functions, similar to C:

```
[6]char hello = ['H', 'e', 'l', 'l', 'o', '\0'];
puts(&hello as *char);
```

This is useful when interfacing with C APIs that expect `*char` or `*T` arguments.

---

## See Also

- [Types](types.md) — Primitive type reference
- [Ranges](ranges.md) — `..` and `..=` range operators
- [Generics](generics.md) — `vec<T>` dynamic arrays
- [Control Flow](control-flow.md) — `for-in` loops over arrays
