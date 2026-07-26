# Traits

Traits are Lucis's type-class system — behavioural contracts that types can implement. They replace hardcoded type constraints (like `<T: numeric>`) with a user-extensible mechanism based on compile-time monomorphization.

---

## Syntax

### Declaring a Trait

```
trait Hashable {
    fn hash(&self) uint64;
}
```

Traits can have generic type parameters:

```
trait From<T> {
    fn from(T value) Self;
}
```

### Implementing a Trait

```
struct Point {
    int32 x;
    int32 y;
}

impl Hashable for Point {
    fn hash(&self) uint64 {
        ret (self.x as uint64) ^ ((self.y as uint64) << 32);
    }
}
```

For generic traits, specify the concrete type argument:

```
impl From<int32> for Point {
    fn from(int32 value) Point {
        ret Point { x: value, y: value };
    }
}
```

### Using Traits as Constraints

```
fn dedup<T: Hashable>(vec<T> items) vec<T> {
    // T is guaranteed to implement Hashable
}

fn convert<A, B>(A value) B
    where A: From<B>
{
    ret B::from(value);
}
```

---

## Trait Methods

### Instance Methods (`&self`)

```
trait Greeter {
    fn greet(&self) string;
    fn greetFormal(&self, string title) string;
}

impl Greeter for Person {
    fn greet(&self) string {
        ret "Hello, " + self.name;
    }

    fn greetFormal(&self, string title) string {
        ret title + " " + self.name;
    }
}
```

### Static Methods

```
trait Default {
    fn default() Self;
    fn make(int32 n) Self;
}

impl Default for Config {
    fn default() Config {
        ret Config { port: 8080, host: "localhost" };
    }

    fn make(int32 n) Config {
        ret Config { port: n, host: "localhost" };
    }
}
```

Call static methods via `Type::method()`:

```
Config cfg = Config::default();
Config alt = Config::make(3000);
```

### Void Methods

Traits can declare void methods for side effects:

```
trait Counter {
    fn incr(&self) void;
    fn reset(&self) void;
}
```

---

## Multiple Traits

A single type can implement multiple traits:

```
trait Hashable {
    fn hash(&self) uint64;
}

trait Comparable {
    fn compare(&self, &Self other) int32;
}

struct Point {
    int32 x;
    int32 y;
}

impl Hashable for Point {
    fn hash(&self) uint64 { ret (self.x * 31 + self.y) as uint64; }
}

impl Comparable for Point {
    fn compare(&self, &Point other) int32 {
        ret self.x - other.x;
    }
}
```

---

## Cross-File Usage

Traits can be defined in one file and implemented in another:

**`traits.lc`:**
```
trait Sum {
    fn sum(&self) int32;
}
```

**`main.lc`:**
```
use traits::Sum;

struct Data {
    int32 a;
    int32 b;
}

impl Sum for Data {
    fn sum(&self) int32 {
        ret self.a + self.b;
    }
}
```

---

## Built-in Constraints

Lucis provides built-in trait-like constraints that work out of the box:

| Constraint | Accepts |
|-----------|---------|
| `numeric` | Any integer or float type |
| `integer` | Any integer type |
| `float` | Any float type |
| `signed` | Signed integer types |
| `unsigned` | Unsigned integer types |
| `bool` | The `bool` type |
| `string` | The `string` type |

These can be mixed with user-defined traits as constraints in future versions.

---

## How It Works

Traits are resolved at compile time via **monomorphization** — the same mechanism used for generics. Each concrete `impl` produces a new set of LLVM functions specialized for that type. There is no runtime overhead, no vtables, and no dynamic dispatch.

```
impl Hashable for Point {
    fn hash(&self) uint64 { ... }
}
```

Generates a function with the mangled name `impl_Hashable_Point__hash`. When the compiler encounters `point.hash()`, it emits a direct call to this function.

---

## Limitations (Current Version)

- **No `Self` in trait return types**: `fn default() Self` is not yet supported. Use the concrete type name.
- **No `dyn Trait`**: Runtime polymorphism via trait objects is deferred.
- **No default method implementations**: Every method must be implemented by each type.
- **No trait inheritance**: `trait Ord : Eq` is not yet supported.
- **No `#[derive]`**: Traits must be implemented manually.
- **Single constraint per type param**: `T: A + B` is parsed but not fully enforced.

---

## LSP Support

The Lucis LSP provides full support for traits:

| Feature | Description |
|---------|-------------|
| **Completions** | Trait names in `impl` position, trait methods on dot access, static methods on scope access, constraint names after `T:`, trait symbols in `use` imports |
| **Hover** | Trait declarations show method signatures; constrained parameters show constraints (`(parameter Hashable) T val`); imported traits show source location |
| **Go-to-Definition** | Jump from trait usage to declaration, cross-file supported |
| **Signature Help** | Method signatures for trait impl methods |
| **Semantic Tokens** | `trait` and `impl` keywords highlighted; trait names classified as types |
