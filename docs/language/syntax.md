# Syntax

This page covers the general syntax rules of T: how statements are structured, how blocks work, and how the language handles comments, identifiers, and whitespace.

## Statements and Semicolons

Every statement in Lucis ends with a semicolon:

```
int32 x = 10;
println(x);
ret 0;
```

Block statements (`if`, `for`, `while`, `switch`, `lock`, `try`) do **not** end with a semicolon — the closing brace ends the statement:

```
if x > 0 {
    println(x);
}
```

## Blocks

Blocks use curly braces `{}`. Braces are always required — there are no single-statement shortcuts:

```
// correct
if x > 0 {
    println(x);
}

// this is NOT valid
// if x > 0 println(x);
```

## Comments

T supports two comment styles:

```
// Line comment — everything after // to end of line

/* Block comment — can span
   multiple lines */
```

Block comments do not nest.

Lucis also supports **doc-comments** (`/** ... */`) for documenting functions, structs, enums, and other declarations. Doc-comments are parsed by the LSP and displayed as hover information in the editor. See [Doc-Comments](doc-comments.md) for full details.

## Identifiers

Identifiers follow standard rules: they start with a letter or underscore, followed by letters, digits, or underscores:

```
int32 count = 0;
string user_name = "Alice";
float64 _temp = 98.6;
bool isReady = true;
```

Lucis is case-sensitive: `count`, `Count`, and `COUNT` are three different identifiers.

## Modules & Imports

Every `.lc` file must begin with a namespace declaration:

```
```

The namespace name is used for module resolution when importing symbols across files. It must be the first non-comment line in the file.

## Imports

At the top of the file, you can import symbols from the standard library or other modules:

```
use std::log::println;                    // import a single function
use std::log::{ println, print, dbg };    // import multiple functions
use std::math;                            // import an entire module
```

Imports must appear after the namespace declaration and before any other declarations.

## Include Directives

C headers can be included for FFI:

```
#include <stdio.h>        // system header
#include "mylib.h"        // local header
```

Include directives are processed before compilation and make C functions, structs, enums, and constants available in the T file.

## Type-First Declarations

All declarations in Lucis place the type before the name (except functions, which use `fn`):

```
int32 age = 25;                               // variable
fn calculate_area(float64 radius) float64 { }    // function
struct Point { int32 x; int32 y; }            // struct fields
```

This applies consistently to variables, function parameters, and struct fields.

## Function Syntax

Functions are declared with `fn`, followed by the name, parameters, and return type:

```
fn add(int32 a, int32 b) int32 {
    ret a + b;
}

fn greet(string name) void {
    println(name);
}
```

The `ret` keyword is used to return values. There is no implicit return.

## Optional Parentheses

Control flow statements (`if`, `while`, `switch`) accept conditions with or without parentheses:

```
// both are valid
if x > 0 { println(x); }
if (x > 0) { println(x); }

while x > 0 { x -= 1; }
while (x > 0) { x -= 1; }

switch x { case 1 { } }
switch (x) { case 1 { } }
```

## Entry Point

The program entry point is the `main` function, which must return `int32`:

```
fn main() int32 {
    // program starts here
    ret 0;
}
```

## Escape Sequences

String and character literals support these escape sequences:

| Sequence | Meaning |
|---|---|
| `\n` | Newline (line feed, 0x0A) |
| `\r` | Carriage return (0x0D) |
| `\t` | Horizontal tab (0x09) |
| `\\` | Backslash (0x5C) |
| `\"` | Double quote (0x22) |
| `\'` | Single quote (0x27) |
| `\0` .. `\377` | Octal byte value (1–3 octal digits, max `\377` = 255) |
| `\a` | Bell/alert (0x07) |
| `\b` | Backspace (0x08) |
| `\f` | Form feed (0x0C) |
| `\v` | Vertical tab (0x0B) |
| `\e` / `\E` | Escape character (ESC, 0x1B) |
| `\?` | Question mark (0x3F) |
| `\xNN` | Hex byte (e.g., `\x41` = `'A'`) |
| `\uNNNN` | Unicode 16-bit code point (UTF-8 encoded in strings; must fit in byte for chars) |
| `\UNNNNNNNN` | Unicode 32-bit code point (UTF-8 encoded in strings; must fit in byte for chars) |

## See Also

- [Overview](overview.md) — Language philosophy and goals
- [Types](types.md) — All built-in types
- [Variables](variables.md) — Variable declaration and scope
