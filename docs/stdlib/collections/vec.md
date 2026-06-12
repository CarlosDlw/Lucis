# vec\<T\>

A dynamically-growable array backed by contiguous heap memory.

## Declaration

`vec`, `map`, and `set` are native keywords — no import required.

```
vec<int32> numbers = [];                  // empty
vec<string> names = ["alice", "bob"];     // with initial values
```

## Methods

### Size & Capacity

| Method | Returns | Description |
|--------|---------|-------------|
| `len()` | `usize` | Number of elements |
| `capacity()` | `usize` | Allocated capacity |
| `isEmpty()` | `bool` | Whether vector is empty |

### Element Access

| Method | Returns | Description |
|--------|---------|-------------|
| `at(usize)` | `T` | Element at index |
| `first()` | `T` | First element |
| `last()` | `T` | Last element |

Elements can also be accessed via indexing: `v[0]`, `v[1]`.

### Mutation

| Method | Returns | Description |
|--------|---------|-------------|
| `push(T)` | `void` | Append element |
| `pop()` | `T` | Remove and return last element |
| `insert(usize, T)` | `void` | Insert at index |
| `removeAt(usize)` | `T` | Remove at index (preserves order) |
| `removeSwap(usize)` | `T` | Remove at index (swap with last, O(1)) |
| `clear()` | `void` | Remove all elements |
| `fill(T)` | `void` | Fill all elements with value |
| `swap(usize, usize)` | `void` | Swap elements at two indices |

### Memory Management

| Method | Returns | Description |
|--------|---------|-------------|
| `reserve(usize)` | `void` | Ensure capacity for N elements |
| `shrink()` | `void` | Shrink capacity to fit length |
| `resize(usize, T)` | `void` | Resize with default value |
| `truncate(usize)` | `void` | Truncate to N elements |
| `free()` | `void` | Free memory (auto-called at scope exit) |

### Search

| Method | Returns | Description |
|--------|---------|-------------|
| `contains(T)` | `bool` | Whether element exists |
| `indexOf(T)` | `int64` | Index of first occurrence (-1 if none) |
| `lastIndexOf(T)` | `int64` | Index of last occurrence (-1 if none) |
| `count(T)` | `usize` | Count of occurrences |

### Reordering

| Method | Returns | Description |
|--------|---------|-------------|
| `reverse()` | `void` | Reverse in-place |
| `sort()` | `void` | Sort ascending (numeric types) |
| `sortDesc()` | `void` | Sort descending (numeric types) |
| `rotate(int32)` | `void` | Rotate elements by offset |

### Aggregation (Numeric Types Only)

| Method | Returns | Description |
|--------|---------|-------------|
| `sum()` | `T` | Sum of all elements |
| `product()` | `T` | Product of all elements |
| `min()` | `T` | Minimum value |
| `max()` | `T` | Maximum value |
| `average()` | `float64` | Arithmetic mean |

### Query & Conversion

| Method | Returns | Description |
|--------|---------|-------------|
| `equals(vec<T>)` | `bool` | Whether two vectors are equal |
| `isSorted()` | `bool` | Whether sorted ascending (numeric) |
| `toString()` | `string` | String representation |
| `join(string)` | `string` | Join elements with separator |
| `clone()` | `vec<T>` | Deep copy |

## Example

```
vec<int32> nums = [];

nums.push(3);
nums.push(1);
nums.push(4);
nums.push(1);
nums.push(5);

println(nums.len());              // 5
println(nums.contains(4));        // true
println(nums.indexOf(1));         // 1

nums.sort();
println(nums.toString());         // [1, 1, 3, 4, 5]
println(nums.sum());              // 14
println(nums.min());              // 1
println(nums.max());              // 5

nums.reverse();
println(nums.join(", "));         // 5, 4, 3, 1, 1
```

## List Comprehensions

Arrays can be built with list comprehensions and assigned to Vecs:

```
[10]int32 squares = [x * x | for int32 x in 0..10];
[8]int32 evens = [x | for int32 x in 0..8 if x % 2 == 0];
```

See [Arrays — List Comprehensions](../../language/arrays.md) for full syntax details.

## Auto-Cleanup

`vec<T>` memory is automatically freed when the containing function returns. You can also call `.free()` manually or use `defer`.

## See Also

- [Map\<K,V\>](map.md) — Key-value hash map
- [Set\<T\>](set.md) — Unique value set
- [Arrays](../../language/arrays.md) — Fixed and dynamic arrays
- [Generics](../../language/generics.md) — Generic types overview
