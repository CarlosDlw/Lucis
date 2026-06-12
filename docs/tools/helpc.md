# helpc — C Library Reference

`lucis helpc` is a built-in command that inspects C library headers and displays
detailed information about functions, structs, enums, macros, typedefs, and globals.
It uses libclang to parse headers accurately, and maps every C type to its Lucis equivalent.

## Synopsis

```
lucis helpc <lib> [symbol] [flags]
```

## Quick Examples

```bash
# Look up a function
lucis helpc raylib InitWindow

# Look up a struct (with known constants)
lucis helpc raylib Color

# List everything in a library
lucis helpc raylib --list

# Search for symbols by name
lucis helpc raylib -s Draw

# Search with regex
lucis helpc raylib -s "^Is" --regex
```

---

## Arguments

### Library Name (Required)

The first argument after `helpc` is the library name. It can be:

- A **short name** that maps to a known header: `raylib`, `stdio`, `math`, `SDL2`, `curl`, etc.
- A **full header path**: `raylib.h`, `SDL2/SDL.h`, `curl/curl.h`

```bash
lucis helpc raylib          # resolves to <raylib.h>
lucis helpc stdio           # resolves to <stdio.h>
lucis helpc math            # resolves to <math.h>
lucis helpc string          # resolves to <string.h>
lucis helpc SDL2            # resolves to <SDL2/SDL.h>
lucis helpc curl            # resolves to <curl/curl.h>
lucis helpc openssl         # resolves to <openssl/ssl.h>
lucis helpc sqlite3         # resolves to <sqlite3.h>
```

Any installed system header works — helpc uses the same discovery mechanism as the
compiler (`clang -E -x c -v /dev/null`) to find system include paths.

### Symbol Name (Optional)

The second positional argument looks up a specific symbol:

```bash
lucis helpc raylib InitWindow   # function
lucis helpc raylib Color        # struct
lucis helpc raylib KeyboardKey  # enum
lucis helpc raylib RED          # macro (struct literal)
lucis helpc raylib Texture2D    # typedef
lucis helpc stdio printf        # function from another lib
lucis helpc math sin            # function from math.h
```

If no symbol is given and no flags are specified, helpc prints a summary of the library.

---

## Flags

### Listing

```
--list          Print a summary: symbol counts by category, link flag
-l functions    List all functions (grouped by prefix)
-l structs      List all structs (with size and field preview)
-l enums        List all enums (with value count and range)
-l macros       List all macros (integer constants and struct literals)
-l typedefs     List all typedefs (with resolved target)
-l globals      List all global variables
```

### Search

```
-s <query>       Substring search across all symbol types
-s <pattern> --regex   Regex search (uses C++ std::regex)
```

### Display Options

```
--all, -a        Show all values without truncation
--verbose, -v    Show field offsets, sizes, alignment, raw C details
--related, -r    Show functions and macros that reference this type
--json           Output as JSON instead of formatted text
--no-color       Disable ANSI color codes
```

### Include Paths

```
-I<path>         Add an extra include search path
```

---

## Output Modes

### Symbol Lookup

When looking up a specific symbol, helpc displays a formatted box with all relevant
information. The exact content depends on the symbol kind.

#### Functions

```bash
$ lucis helpc raylib InitWindow
```

```
╭─ function ──────────────────────────────────────────────────╮
│ void InitWindow(int32 width, int32 height, *char title)     │
│                                                             │
│ Parameters:                                                 │
│   width   int   (int32)                                     │
│   height  int   (int32)                                     │
│   title   const char *  (*char)                             │
│                                                             │
│ Returns: void                                               │
│ Header:  <raylib.h>                                         │
│ Link:    -lraylib                                           │
│                                                             │
│ T usage:                                                    │
│   InitWindow(800, 600, c"Hello");                           │
╰─────────────────────────────────────────────────────────────╯
```

Each parameter shows its C type and the corresponding Lucis type in parentheses.
Variadic functions display `...` as an extra parameter.

#### Structs

```bash
$ lucis helpc raylib Color
```

```
╭─ struct ────────────────────────────────────────────────────╮
│ struct Color (4 bytes)                                      │
│                                                             │
│ Fields:                                                     │
│   r     unsigned char  (uint8)                              │
│   g     unsigned char  (uint8)                              │
│   b     unsigned char  (uint8)                              │
│   a     unsigned char  (uint8)                              │
│                                                             │
│ Known constants (26 total):                                 │
│   RED       { 230, 41, 55, 255 }                            │
│   GREEN     { 0, 228, 48, 255 }                             │
│   BLUE      { 0, 121, 241, 255 }                            │
│   ...                                                       │
│                                                             │
│ T usage:                                                    │
│   Color x = Color { 0, 0, 0, 0 };                          │
╰─────────────────────────────────────────────────────────────╯
```

Struct macros (like `RED`, `GREEN`, `BLUE`) are automatically detected from `#define`
directives that use the `CLITERAL(Type){ ... }` pattern and shown as known constants.

#### Enums

```bash
$ lucis helpc raylib KeyboardKey
```

Shows all enum values with their integer constants. Use `--all` to see every value
when the list is long.

#### Macros

```bash
$ lucis helpc raylib RED
```

For struct literal macros (like color constants), helpc shows the expanded fields
with their individual values.

For integer macros, it shows the name and numeric value.

#### Typedefs

```bash
$ lucis helpc raylib Texture2D
```

Shows the alias target. If it resolves to a known struct, the struct fields are
shown inline.

### Verbose Mode

Add `--verbose` or `-v` to see low-level details:

```bash
$ lucis helpc raylib Color -v
```

```
╭─ struct ────────────────────────────────────────────────────╮
│ struct Color (4 bytes, align 1)                             │
│                                                             │
│ Fields:                                                     │
│   r     unsigned char  (uint8)  offset 0  size 1            │
│   g     unsigned char  (uint8)  offset 1  size 1            │
│   b     unsigned char  (uint8)  offset 2  size 1            │
│   a     unsigned char  (uint8)  offset 3  size 1            │
│   ...                                                       │
╰─────────────────────────────────────────────────────────────╯
```

This is useful for understanding ABI layout when doing manual memory manipulation.

### Related Symbols

Add `--related` or `-r` to see functions and macros that reference a given type:

```bash
$ lucis helpc raylib Color -r
```

Shows the struct info followed by all functions that take or return `Color`, plus
all macros that produce `Color` values.

### JSON Output

Add `--json` for machine-readable output:

```bash
$ lucis helpc raylib InitWindow --json
```

```json
{
  "kind": "function",
  "name": "InitWindow",
  "header": "raylib.h",
  "library": "-lraylib",
  "returnType": { "c": "void", "t": "void" },
  "parameters": [
    { "name": "width", "cType": "int", "tType": "int32" },
    { "name": "height", "cType": "int", "tType": "int32" },
    { "name": "title", "cType": "const char *", "tType": "*char" }
  ],
  "isVariadic": false
}
```

JSON output is available for functions, structs, enums, and full header dumps.

### Summary

When you specify a library without a symbol or listing flag, helpc prints a summary:

```bash
$ lucis helpc raylib
```

```
<raylib.h> — 737 symbols

  Functions:      581
  Structs:        34
  Enums:          21
  Typedefs:       69
  Macros:         32

  Link: -lraylib
```

### Fuzzy Matching

If a symbol name doesn't match exactly, helpc suggests similar symbols based on
edit distance:

```bash
$ lucis helpc raylib initwindow
```

```
Symbol 'initwindow' not found in <raylib.h>.

Did you mean:
  InitWindow      function  void InitWindow(int, int, const char *)
  CloseWindow     function  void CloseWindow()
  MinimizeWindow  function  void MinimizeWindow()
```

---

## Supported Libraries

helpc works with **any installed C header**. These short names are pre-configured
for convenience:

| Short Name | Header | Link Flag |
|---|---|---|
| `raylib` | `raylib.h` | `-lraylib` |
| `SDL2` | `SDL2/SDL.h` | `-lSDL2` |
| `stdio` | `stdio.h` | — |
| `stdlib` | `stdlib.h` | — |
| `string` | `string.h` | — |
| `math` | `math.h` | `-lm` |
| `pthread` | `pthread.h` | `-lpthread` |
| `curl` | `curl/curl.h` | `-lcurl` |
| `openssl` | `openssl/ssl.h` | `-lssl` |
| `sqlite3` | `sqlite3.h` | `-lsqlite3` |
| `zlib` | `zlib.h` | `-lz` |
| `json` | `json-c/json.h` | `-ljson-c` |
| `png` | `png.h` | `-lpng` |
| `glfw` | `GLFW/glfw3.h` | `-lglfw` |
| `gtk` | `gtk/gtk.h` | — (use pkg-config) |

For headers not in this table, use the full header name:

```bash
lucis helpc mylib.h MyFunction -I/path/to/headers
```

---

## See Also

- [Calling C Functions](../ffi/calling-c.md) — How to use C functions in Lucis code
- [Linking](../ffi/linking.md) — Linking with external C libraries
- [CLI Usage](../getting-started/cli-usage.md) — All compiler options
