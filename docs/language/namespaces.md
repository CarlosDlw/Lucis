# Namespaces

Every Lucis source file begins with a `namespace` declaration. Namespaces identify the module a file belongs to and enable cross-file symbol resolution.

---

## Declaration

The `namespace` keyword must be the first declaration in every `.lc` file:

```
namespace MyApp;

fn main() int32 {
    ret 0;
}
```

The name is a single identifier — typically PascalCase.

---

## Multi-File Projects

Namespaces enable multi-file compilation. Functions defined in one file can be imported in another using `use`:

### File: `math.lc`

```
namespace Math;

fn add(int32 a, int32 b) int32 {
    ret a + b;
}

fn multiply(int32 a, int32 b) int32 {
    ret a * b;
}
```

### File: `main.lc`

```
namespace Main;

use std::log::println;
use Math::add;
use Math::multiply;

fn main() int32 {
    int32 sum = add(3, 4);
    int32 prod = multiply(5, 6);
    println(sum);    // 7
    println(prod);   // 30
    ret 0;
}
```

Both files are compiled together — the compiler resolves `Math::add` by matching the `Math` namespace across files.

---

## Scope Resolution (`::`)

The `::` operator resolves symbols within a namespace or type:

| Usage | Example |
|-------|---------|
| Stdlib import | `use std::log::println;` |
| User module import | `use Math::add;` |
| Enum variant | `Color::Red` |
| Static method | `Vec2::create(3, 4)` |

---

## Namespace Rules

- Every `.lc` file must have exactly one `namespace` declaration
- It must be the first declaration in the file (before `use`, `#include`, or any other code)
- The standard library uses the `std` namespace hierarchy (`std::log`, `std::math`, etc.)
- User namespaces are flat identifiers — no nesting like `App::Utils::Math`

---

## See Also

- [Modules](modules.md) — The `use` import system
- [Syntax](syntax.md) — General syntax rules
