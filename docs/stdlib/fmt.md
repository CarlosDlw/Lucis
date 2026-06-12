# std::fmt

String formatting utilities for padding, number formatting, and display.

## Import

```
use std::fmt::{ lpad, rpad, hex, bin, fixed, humanBytes, commas };
```

## Functions

### Padding and Alignment

| Function | Signature | Description |
|----------|-----------|-------------|
| `lpad` | `(string, int64, char) -> string` | Left-pad to width with character |
| `rpad` | `(string, int64, char) -> string` | Right-pad to width with character |
| `center` | `(string, int64, char) -> string` | Center-pad to width with character |

### Integer Formatting

| Function | Signature | Description |
|----------|-----------|-------------|
| `hex` | `(int64) -> string` | Format as lowercase hex |
| `hexUpper` | `(int64) -> string` | Format as uppercase hex |
| `oct` | `(int64) -> string` | Format as octal |
| `bin` | `(int64) -> string` | Format as binary |

### Float Formatting

| Function | Signature | Description |
|----------|-----------|-------------|
| `fixed` | `(float64, int32) -> string` | Fixed-point with precision |
| `scientific` | `(float64) -> string` | Scientific notation |

### Display Formatting

| Function | Signature | Description |
|----------|-----------|-------------|
| `humanBytes` | `(uint64) -> string` | Human-readable byte size |
| `commas` | `(int64) -> string` | Number with comma separators |
| `percent` | `(float64) -> string` | Format as percentage |

## Example

```
use std::fmt::{ lpad, hex, hexUpper, bin, fixed, humanBytes };
use std::log::println;

println(lpad("42", 5, '0'));       // "00042"
println(hex(255));                 // "ff"
println(hexUpper(255));            // "FF"
println(bin(10));                  // "1010"
println(fixed(3.14159, 2));        // "3.14"
println(humanBytes(1024));         // "1.0 KB"
println(humanBytes(1536));         // "1.5 KB"
```

## See Also

- [std::conv](conv.md) — Type conversion functions
- [std::str](string.md) — String utilities
