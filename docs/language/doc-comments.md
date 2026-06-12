# Doc-Comments

Doc-comments are special comments that document functions, structs, enums, extend methods, and other declarations. They are parsed by the LSP and displayed as rich hover information, signature help, and completion details in the editor.

## Syntax

Doc-comments use the `/** ... */` delimiter — exactly **two** stars after the slash:

```lucis
/** This is a doc-comment. */
```

Multi-line doc-comments follow the conventional star-prefix style:

```lucis
/**
 * This is a multi-line doc-comment.
 * Each line can start with ` * ` which is automatically stripped.
 */
```

### What is NOT a doc-comment

- `// ...` — line comments are never doc-comments
- `/* ... */` — regular block comments (single star)
- `/*** ... */` — three or more stars is treated as a regular block comment

### Placement

A doc-comment must appear **immediately before** the declaration it documents, with at most one blank line between them:

```lucis
/** Computes the absolute value of n. */
fn abs(int32 n) int32 {
    ret n < 0 ? -n : n;
}
```

```lucis
/** Computes the absolute value of n. */

fn abs(int32 n) int32 {   // one blank line — still valid
    ret n < 0 ? -n : n;
}
```

If there are two or more blank lines between the doc-comment and the declaration, the doc-comment is **not** associated.

## Summary

The text before the first `@tag` is the **summary** — a free-form description of the declaration:

```lucis
/**
 * Computes the greatest common divisor of two integers
 * using the Euclidean algorithm.
 */
fn gcd(int32 a, int32 b) int32 { ... }
```

The summary is displayed first in hover information and can span multiple lines.

## Doc-Tags

After the summary, you can use `@tag` annotations to document specific aspects. Tags start with `@` followed by the tag name, and their syntax depends on the tag category.

### Parameter Tags

Document function parameters, struct fields, or properties. Syntax: `@tag name description`

| Tag | Usage | Purpose |
|-----|-------|---------|
| `@param` | `@param name description` | Document a function parameter |
| `@field` | `@field name description` | Document a struct field |
| `@property` | `@property name description` | Document a property |

```lucis
/**
 * Creates a new point at the given coordinates.
 * @param x The horizontal position
 * @param y The vertical position
 * @return A new Point with the given coordinates
 */
fn create(int32 x, int32 y) Point { ... }
```

```lucis
/**
 * A 2D point in Cartesian space.
 * @field x The horizontal coordinate
 * @field y The vertical coordinate
 */
struct Point {
    int32 x;
    int32 y;
}
```

### Description Tags

Provide a single-line or short description. Syntax: `@tag description`

| Tag | Usage | Purpose |
|-----|-------|---------|
| `@return` | `@return description` | Document the return value |
| `@returns` | `@returns description` | Alias for `@return` |
| `@brief` | `@brief description` | Short summary (alternative to free-text summary) |
| `@throws` | `@throws description` | Document an error/exception the function can throw |
| `@deprecated` | `@deprecated reason` | Mark as deprecated with optional reason |
| `@since` | `@since version` | Version when the symbol was introduced |
| `@version` | `@version semver` | Current version of the symbol |
| `@author` | `@author name` | Author of the code |
| `@see` | `@see reference` | Cross-reference to related symbols or docs |
| `@todo` | `@todo description` | Note a pending task |

```lucis
/**
 * Reads the entire file at the given path.
 * @param path The file path to read
 * @return The file contents as a string
 * @throws If the file does not exist or cannot be read
 * @since 0.0.1 beta
 */
fn readFile(string path) string { ... }
```

```lucis
/**
 * Use newFunction() instead.
 * @deprecated This function will be removed in v2.0
 * @see newFunction
 */
fn oldFunction() void { ... }
```

### Block Tags

Capture multi-line content until the next `@tag` or end of comment. Syntax: `@tag` followed by content on the next lines.

| Tag | Usage | Purpose |
|-----|-------|---------|
| `@example` | Code example (rendered as `lucis` code block) | Show usage examples |
| `@remarks` | Additional details | Extended discussion |
| `@note` | Informational note (rendered as blockquote) | Highlight important info |
| `@warning` | Warning message (rendered as blockquote with ⚠) | Highlight dangers or pitfalls |

```lucis
/**
 * Sorts the vector in ascending order.
 * @warning Mutates the vector in place
 * @example
 * Vec<int32> v = [3, 1, 2];
 * v.sort();
 * // v is now [1, 2, 3]
 */
```

```lucis
/**
 * Computes the factorial of n.
 * @param n Must be non-negative
 * @return n! as int64
 * @note Uses iterative algorithm, not recursive
 * @example
 * auto result = factorial(5);
 * // result == 120
 */
fn factorial(int32 n) int64 { ... }
```

### Flag Tags

Markers with no arguments. Syntax: just `@tag` alone.

| Tag | Usage | Purpose |
|-----|-------|---------|
| `@private` | `@private` | Mark as private |
| `@public` | `@public` | Mark as public |
| `@protected` | `@protected` | Mark as protected |
| `@internal` | `@internal` | Mark as internal/implementation detail |
| `@struct` | `@struct` | Mark as struct documentation |
| `@namespace` | `@namespace` | Mark as namespace documentation |

```lucis
/**
 * Internal helper — do not call directly.
 * @internal
 */
fn validateInput(int32 x) bool { ... }
```

## How Doc-Comments Appear in the Editor

### Hover

When you hover over a symbol, the doc-comment is displayed below the type signature, separated by a horizontal line:

```
(function) int32 gcd(int32 a, int32 b)
─────────────────────────────────────────────
Computes the greatest common divisor of two integers
using the Euclidean algorithm.

@param `a` — The first integer
@param `b` — The second integer
@return The GCD of a and b
```

The rendering order in hover is:
1. Summary
2. `@deprecated` (as blockquote)
3. `@param` / `@field` / `@property` (grouped)
4. `@return` / `@returns`
5. `@throws`
6. `@example` (as code block)
7. `@remarks` / `@note` / `@warning`
8. `@brief` / `@todo`
9. Metadata (`@since`, `@version`, `@author`, `@see`)
10. Visibility flags (`@private`, `@public`, etc.)

### Signature Help

When typing a function call, the doc-comment for each parameter is shown in the signature popup. The `@param` tags are matched to the parameter being typed.

### Completion

When typing inside a `/** */` block and pressing `@`, the editor suggests all available doc-tags with snippets:

- `@param` inserts `@param name description` with tab stops
- `@return` inserts `@return description` with a tab stop
- `@example` inserts the tag followed by a new line for code
- Flag tags insert just the tag name

## Complete Example

```lucis
/**
 * A cardinal direction in a 2D grid.
 *
 * Used for movement and orientation in tile-based systems.
 *
 * @since 0.0.1 beta
 * @see Point
 */
enum Direction {
    North,
    South,
    East,
    West,
}

/**
 * A 2D point in Cartesian space.
 *
 * @field x The horizontal coordinate
 * @field y The vertical coordinate
 */
struct Point {
    int32 x;
    int32 y;
}

extend Point {
    /**
     * Creates a new Point from the given coordinates.
     *
     * @param x The horizontal position
     * @param y The vertical position
     * @return A new Point at (x, y)
     *
     * @example
     * auto p = Point::create(10, 20);
     */
    fn create(int32 x, int32 y) Point {
        ret Point{ x: x, y: y };
    }
}

extend Point {
    /**
     * Computes the Manhattan distance from the origin.
     *
     * The Manhattan distance is |x| + |y|, also known as
     * the taxicab distance or L1 norm.
     *
     * @return The sum of absolute values of x and y
     * @note Always returns a non-negative value
     *
     * @example
     * auto p = Point{ x: 3, y: -4 };
     * auto d = p.manhattan();
     * // d == 7
     */
    fn manhattan(&self) int32 {
        auto ax = self.x < 0 ? -self.x : self.x;
        auto ay = self.y < 0 ? -self.y : self.y;
        ret ax + ay;
    }
}
```
