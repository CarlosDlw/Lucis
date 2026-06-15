# Namespaces (deprecated)

> **⚠️ Removed in v0.0.3-beta.** The `namespace` keyword has been removed.
> See [Modules](./modules.md) for the current path-based import system.

The `namespace` keyword was previously used to declare a file's namespace:

```lucis
namespace Main;  // No longer supported
```

In the new path-based system, a file's identity is its path relative to the project root.
Use `use` with file paths (minus the `.lc` extension, with `::` replacing `/`):

```lucis
// Old (namespace-based):
namespace Main;
use std::log::println;

// New (path-based):
use std::log::println;
use lib::math::add;  // imports src/lib/math.lc
```
