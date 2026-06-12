# set\<T\>

An open-addressing hash set for storing unique values.

## Declaration

`vec`, `map`, and `set` are native keywords — no import required.

```
set<int32> ids = [];
set<string> tags = [\"alpha\", \"beta\"];
```

## Methods

### Size & Query

| Method | Returns | Description |
|--------|---------|-------------|
| `len()` | `usize` | Number of elements |
| `isEmpty()` | `bool` | Whether set is empty |

### Mutation

| Method | Returns | Description |
|--------|---------|-------------|
| `add(T)` | `bool` | Add element, returns false if already present |
| `has(T)` | `bool` | Whether element exists |
| `remove(T)` | `bool` | Remove element, returns whether it existed |
| `clear()` | `void` | Remove all elements |

### Iteration

| Method | Returns | Description |
|--------|---------|-------------|
| `values()` | `set<T>` | Copy of the set |

### Cleanup

| Method | Returns | Description |
|--------|---------|-------------|
| `free()` | `void` | Free memory (auto-called at scope exit) |

## Example

```
set<string> tags = [];

tags.add("rust");
tags.add("cpp");
tags.add("rust");                  // duplicate, returns false

println(tags.len());               // 2
println(tags.has("rust"));         // true
println(tags.has("go"));           // false

tags.remove("cpp");
println(tags.len());               // 1
```

## Auto-Cleanup

`set<T>` memory is automatically freed when the containing function returns. You can also call `.free()` manually or use `defer`.

## See Also

- [vec\<T\>](vec.md) — Dynamic array
- [map\<K,V\>](map.md) — Key-value hash map
- [Generics](../../language/generics.md) — Generic types overview
