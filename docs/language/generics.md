# Generics

T supports generic type parameters for its built-in collection types and concurrency primitives. Generics allow a single type to work with any element type while maintaining type safety.

---

## Syntax

Generic types are parameterized with angle brackets `<T>`:

```
vec<int32> numbers = [1, 2, 3];
map<string, int64> ages;
set<uint64> ids = [];
Task<int32> t = spawn compute(10);
```

---

## Generic Types

T provides four built-in generic types:

| Type | Parameters | Description |
|------|-----------|-------------|
| `vec<T>` | 1 | Dynamic growable array |
| `map<K, V>` | 2 | Open-addressing hash map |
| `set<T>` | 1 | Open-addressing hash set |
| `Task<T>` | 1 | Async task handle |

---

## Generic Type Inference

Lux can infer type arguments for user-defined generic functions and generic static methods when every type parameter appears in at least one formal parameter.

```
fn identity<T>(T value) T {
    ret value;
}

struct Box<T> {
    T value;
}

extend Box<T> {
    fn make(T value) Box<T> {
        ret Box<T>{ value: value };
    }
}

fn main() int32 {
    int32 a = identity(10);        // T => int32
    float64 b = identity(3.2);     // T => float64
    Box<int32> box = Box::make(42); // T => int32
    ret 0;
}
```

Inference is intentionally strict:

- Type arguments are inferred only from formal parameters.
- Every generic parameter must appear in at least one argument position.
- If the same type parameter is inferred as different concrete types, compilation fails.
- Lux does not guess from the return type alone.

These calls are rejected:

```
fn pick<T>(T a, T b) T {
    ret a;
}

fn make<T>() T {
    ret 0 as T;
}

fn main() int32 {
    auto x = pick(10, 10.0); // error: T inferred as int32 and float64
    auto y = make();         // error: T does not appear in parameters
    ret 0;
}
```

---

## vec\<T\> — Dynamic Array

A growable, heap-allocated array. Initialized from an array literal or empty `[]`:

```
vec<int32> v = [10, 20, 30];
println(v.len());      // 3
println(v.first());    // 10
println(v.last());     // 30

v.push(40);
println(v.len());      // 4

int32 popped = v.pop();
println(popped);       // 40
```

### Index Access

```
vec<int32> v = [100, 200, 300];
println(v[0]);    // 100
println(v[2]);    // 300

v[1] = 999;
println(v[1]);    // 999
```

### Vec Methods

| Category | Methods |
|----------|---------|
| **Size** | `len()`, `capacity()`, `isEmpty()` |
| **Access** | `at(usize)`, `first()`, `last()`, `[index]` |
| **Mutation** | `push(T)`, `pop()`, `insert(usize, T)`, `removeAt(usize)`, `removeSwap(usize)`, `clear()`, `fill(T)`, `swap(usize, usize)` |
| **Memory** | `reserve(usize)`, `shrink()`, `resize(usize, T)`, `truncate(usize)`, `free()` |
| **Search** | `contains(T)`, `indexOf(T)`, `lastIndexOf(T)`, `count(T)` |
| **Order** | `reverse()`, `sort()`, `sortDesc()`, `rotate(int32)` |
| **Aggregation** | `sum()`, `product()`, `min()`, `max()`, `average()` |
| **Query** | `equals(vec<T>)`, `isSorted()` |
| **Conversion** | `toString()`, `join(string)`, `clone()` |

```
vec<int32> v = [30, 10, 20, 10, 40];

println(v.contains(10));    // true
println(v.indexOf(10));     // 1
println(v.count(10));       // 2

v.sort();
// v is now [10, 10, 20, 30, 40]

println(v.sum());           // 110
println(v.min());           // 10
println(v.max());           // 40

v.free();
```

### Type Flexibility

`vec<T>` works with any type:

```
vec<float64> fv = [1.5, 2.5, 3.5];
fv.push(4.5);
println(fv.last());   // 4.5
fv.free();

vec<string> names = ["alice", "bob"];
println(names.first());   // alice
names.free();
```

> **Important:** `vec<T>` is heap-allocated. Call `.free()` when done, or use `defer` for automatic cleanup.

---

## map\<K, V\> — Hash Map

A key-value store using open-addressing hashing:

```
map<string, int64> ages;

ages.insert("alice", 30);
ages.insert("bob", 25);
ages.insert("carol", 35);

println(ages.len());          // 3
println(ages.get("alice"));   // 30
println(ages.has("bob"));     // true
```

### Subscript Access

```
map<string, int64> ages;
ages["dave"] = 40;
println(ages["dave"]);   // 40
```

### Map Methods

| Method | Signature | Description |
|--------|-----------|-------------|
| `len` | `() -> usize` | Number of entries |
| `isEmpty` | `() -> bool` | Whether map has zero entries |
| `get` | `(K) -> V` | Value for key |
| `getOrDefault` | `(K, V) -> V` | Value for key, or default if missing |
| `has` | `(K) -> bool` | Whether key exists |
| `insert` | `(K, V)` | Insert or update entry |
| `remove` | `(K) -> bool` | Remove entry, returns success |
| `clear` | `()` | Remove all entries |
| `keys` | `() -> vec<K>` | All keys as a Vec |
| `values` | `() -> vec<V>` | All values as a Vec |
| `free` | `()` | Release memory |

```
map<int32, int64> scores;
scores.insert(1, 100);
scores.insert(2, 200);

println(scores.get(1));              // 100
println(scores.has(3));              // false
println(scores.getOrDefault(4, -1)); // -1

scores.free();
```

---

## set\<T\> — Hash Set

A collection of unique elements using open-addressing hashing:

```
set<int32> s = [];

s.add(10);
s.add(20);
s.add(30);
println(s.len());      // 3

bool dup = s.add(20);
println(dup);           // false (already exists)
println(s.len());       // 3 (unchanged)
```

### Set Methods

| Method | Signature | Description |
|--------|-----------|-------------|
| `len` | `() -> usize` | Number of elements |
| `isEmpty` | `() -> bool` | Whether set is empty |
| `add` | `(T) -> bool` | Add element, returns `true` if new |
| `has` | `(T) -> bool` | Whether element exists |
| `remove` | `(T) -> bool` | Remove element, returns success |
| `clear` | `()` | Remove all elements |
| `values` | `() -> set<T>` | Copy of the set |
| `free` | `()` | Release memory |

```
set<string> ss = [];
ss.add("hello");
ss.add("world");
ss.add("hello");       // returns false — duplicate

println(ss.len());     // 2
println(ss.has("hello")); // true
println(ss.has("foo"));   // false

ss.remove("world");
println(ss.len());     // 1

ss.free();
```

---

## Task\<T\> — Async Task

Created by the `spawn` operator and resolved with `await`. See [Concurrency](concurrency.md) for full details.

```
use std::thread::Task;

Task<int32> t = spawn compute(10, 20);
int32 result = await t;
```

---

## User-Defined Generics

Beyond the built-in collection types, Lux supports user-defined generic structs, unions, methods, and functions. Generics are resolved at compile time via **monomorphization** — each concrete instantiation (`Node<int32>`, `Node<string>`, `Result<int32, string>`) becomes a fully independent type with no runtime overhead.

---

### Generic Structs

Declare a struct with one or more type parameters in angle brackets:

```lux
struct Node<T> {
    T value;
}

struct Pair<A, B> {
    A first;
    B second;
}
```

Type parameters can be any identifier. They act as placeholders for concrete types supplied at the call site.

---

### Generic Unions

Generic unions use the same type parameter syntax:

```lux
union Result<T, E> {
    T ok;
    E err;
}
```

Concrete instantiations can be used anywhere a normal type can be used:

```lux
Result<int32, string> value = Result<int32, string> { ok: 42 };
Result<int32, string> failure = Result<int32, string> { err: "boom" };
```

Union semantics do not change with generics: all fields still share the same memory location, so generic unions are still raw unions rather than tagged variants.

---

### Generic Methods

Use `extend` with the same type parameter list to add methods to a generic struct:

```lux
extend Node<T> {
    // Static factory method
    fn create(T val) Node<T> {
        ret Node<T> { value: val };
    }

    // Instance method (&self receiver)
    fn getValue(&self) T {
        ret self.value;
    }
}
```

- **Static methods** are called with `Node<int32>::create(42)` — the struct name first, then the concrete type args, then `::method()`.
- **Instance methods** receive `&self` as the first parameter (pointer to the struct). Access fields directly with `self.field`.

---

### Generic Functions

A standalone function can be parameterized with `<T>` after the function name:

```lux
fn max<T>(T a, T b) T {
    ret a > b ? a : b;
}

fn identity<T>(T x) T {
    ret x;
}
```

The return type and parameter types use `T` as a placeholder. The concrete type is specified at the call site.

---

### Instantiation Syntax

| Use case | Syntax |
|---|---|
| Declare a variable | `Node<int32> n = ...;` |
| Static method call | `Node<int32>::create(42)` |
| Struct literal | `Node<int32> { value: 42 }` |
| Generic function call | `max<int32>(3, 7)` |
| Instance method call | `n.getValue()` *(type inferred from `n`)* |

```lux
use std::log::println;

struct Node<T> {
    T value;
}

extend Node<T> {
    fn create(T val) Node<T> {
        ret Node<T> { value: val };
    }

    fn getValue(&self) T {
        ret self.value;
    }
}

fn max<T>(T a, T b) T {
    ret a > b ? a : b;
}

fn main() int32 {
    Node<int32> n = Node<int32>::create(42);
    int32 val = n.getValue();   // 42

    int32 m = max<int32>(3, 7); // 7

    println(val);
    println(m);
    ret 0;
}
```

---

### Type Constraints

Type parameters can be annotated with a constraint using the `:` syntax:

```lux
fn clamp<T: numeric>(T val, T lo, T hi) T {
    ret val < lo ? lo : val > hi ? hi : val;
}
```

The constraint name follows `:` and must be a known constraint identifier. The compiler verifies that the concrete type satisfies the constraint at instantiation time.

Currently supported constraints:

| Constraint | Accepted types |
|---|---|
| `numeric` | `int32`, `int64`, `uint32`, `uint64`, `usize`, `isize`, `float32`, `float64` |

Unconstrained type parameters (`<T>`) accept any type.

---

### How Monomorphization Works

Every concrete instantiation generates a distinct type and function set. `Node<int32>` and `Node<string>` become two completely separate structs in the compiled output — no boxing, no vtables, no runtime dispatch. The compiler mangles names as `Base__Type` (e.g. `Node__int32`, `max__int32`) and emits each specialization exactly once.

---

## Memory Management

`vec<T>`, `map<K, V>`, and `set<T>` are heap-allocated. They must be freed when no longer needed:

```
vec<int32> v = [1, 2, 3];
defer v.free();

// use v normally...
// v.free() is called automatically at function exit
```

The compiler also performs automatic cleanup for these types when they go out of scope at function exit, but using `defer` makes the intent explicit.

---

## See Also

- [Arrays](arrays.md) — Fixed-size stack-allocated arrays
- [Type Aliases](type-aliases.md) — `type` keyword for type shortcuts
- [Concurrency](concurrency.md) — `spawn`, `await`, `Task<T>`
