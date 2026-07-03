# C Macros in Lucis

Lucis provides two ways to use C `#define` macros:

1. **From headers** — macros defined in C headers included via `#include` are
   automatically parsed by `CHeaderResolver` (batch-eval or function-like).
2. **Inline with `c_macro { ... }`** — write C `#define` directives directly in
   your `.lc` source file without a separate header.

---

## `c_macro { ... }` blocks

A `c_macro` block lets you embed C preprocessor definitions inside Lucis source:

```lucis
c_macro {
    #define FOO 42
    #define BAR(x) ((x) + 1)
}

fn main() int32 {
    printf("FOO={d}, BAR(FOO)={d}\n", FOO, BAR(FOO));
    return 0;
}
```

### Placement

`c_macro` blocks are valid at:

- **Top level** — anywhere a top-level declaration is accepted.
- **Statement level** — inside function bodies.

### Content

The content between `{ }` is raw C preprocessor text.  It is **not** tokenised
by the Lucis lexer — the entire block is a single opaque token.  The lexer
matches balanced braces recursively, so nested `{ }` inside a macro body work.

### Supported directives

| Directive | Example | Supported? |
|---|---|---|
| `#define NAME value` | `#define MAX_SIZE 4096` | Yes — evaluated via `gcc -E -dM` on full builds, `strtoll` in LSP mode. |
| `#define NAME(body)` | `#define ADD(a,b) ((a)+(b))` | Yes — tokenised body is evaluated at call sites (see evaluator below). |
| `#define NAME` | `#define DEBUG` | Yes — flag macro (value 0). |
| `#ifdef NAME` / `#ifndef NAME` | | Yes — the preprocessor directives are processed by `gcc -E -dM`. |
| `#if expr` / `#elif expr` | | Yes — conditionals evaluated by `gcc`. |
| `#else` / `#endif` | | Yes. |
| `#undef NAME` | | Yes — handled by `gcc -E -dM`. |
| `#include <...>` / `#include "..."` | | Yes — resolved by `gcc` during evaluation. |
| `#pragma`, `#error`, `#warning` | | Yes — parsed and highlighted, ignored in value eval. |

### Function-like macro evaluator (`evalMacroAdd`)

Function-like macros are expanded at every call site by a lightweight
recursive-descent parser.  Supported operators and their precedence (highest
to lowest):

| Level | Operators | Associativity |
|---|---|---|
| Primary | `( expr )`, `-` (unary), `~` (unary), integer literals | right-to-left for unary |
| Multiplicative | `*` `/` `%` | left-to-right |
| Additive | `+` `-` | left-to-right |
| Shift | `<<` `>>` | left-to-right |

This matches standard C operator precedence.

For object-like macros whose value is not a simple integer, **full build**
mode runs `gcc -E -dM` + `gcc` + `printf` to obtain the true value — so
`sizeof`, `?:`, casts, and other C constructs are correctly evaluated at
compile time.

### Checker pass ordering

The semantic checker processes `c_macro` blocks **before** top-level
`const` and `var` declarations.  This means function-like macros are
already registered when constant initialisers are resolved:

```lucis
c_macro { #define SQUARE(x) ((x)*(x)) }
const X := SQUARE(3);  // ✓ resolves to 9
```

### LSP support

The Language Server sees `c_macro` results without running C compilation:

| Feature | What's shown |
|---|---|
| **Completions** | Macro names with `(C) #define NAME` detail. |
| **Hover** | `#define NAME value` or `#define NAME(params) body...`. |
| **Signature help** | `NAME(p1, p2, …)` for function-like macros. |
| **Go to definition** | Jumps to the macro definition inside the `c_macro` block. |
| **Semantic tokens** | Highlights `c_macro`, `{`/`}`, `#define`, `#ifdef`, `#if`, `#else`, `#endif`, `#undef`, `#include`, `#pragma`, `#error`, `#warning`, macro names (with `Declaration`), parameters (with `Declaration`), numbers, operators, string/char literals, identifiers, and comments inside the block. |

Macros from `c_macro` blocks are merged into the project-level `cBindings`
during `ProjectContext::build()` and served to all LSP providers.

### Limitations

| Limitation | Details |
|---|---|
| **Braces inside string literals** | The ANTLR lexer fragment uses `~[{}]` for content atoms, so a C string literal containing `{` or `}` (e.g. `"hello { world"`) will break brace balancing. |
| **`#include` export** | Macros from headers `#include`d inside a `c_macro` block are resolved by `gcc` but are **not** automatically exported to Lucis. To export a macro from an included header, add an explicit `#define` in the block. |
| **Multi-line `#define`** | Backslash-continued lines (`#define FOO 42 \` + newline + `...`) are not supported — each `#define` must be on a single line. |
| **Unsupported operators in `evalMacroAdd`** | `&`, `\|`, `^`, `&&`, `\|\|`, `==`, `!=`, `<`, `>`, `<=`, `>=`, `?:` (ternary) are **not** implemented.  The tokeniser captures them, but if a function-like macro body uses them the evaluator fails and returns an undefined value. Object-like macros that use these operators are correctly evaluated by `gcc` on full builds. |
| **LSP value fallback** | When `strtoll` parsing fails (non-numeric value), the macro is registered with value `0` so it appears in completions and hover. The true value is computed on full `gcc` builds only. |
| **Function-like macros at top-level statements** | A bare `SQUARE(3)` as a top-level statement (outside a function body) is **not** valid Lucis syntax.  Use `const x := SQUARE(3);` or call it inside a function instead. |
