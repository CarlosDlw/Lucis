# std::ascii

ASCII character classification and conversion functions.

## Import

```
use std::ascii::{ isAlpha, isDigit, isUpper, toLower, toUpper };
```

## Functions

### Classification

| Function | Signature | Description |
|----------|-----------|-------------|
| `isAlpha` | `(char) -> bool` | Is alphabetic (a-z, A-Z) |
| `isDigit` | `(char) -> bool` | Is decimal digit (0-9) |
| `isAlphaNum` | `(char) -> bool` | Is alphanumeric |
| `isUpper` | `(char) -> bool` | Is uppercase letter |
| `isLower` | `(char) -> bool` | Is lowercase letter |
| `isWhitespace` | `(char) -> bool` | Is whitespace (space, tab, newline) |
| `isPrintable` | `(char) -> bool` | Is printable character |
| `isControl` | `(char) -> bool` | Is control character |
| `isHexDigit` | `(char) -> bool` | Is hex digit (0-9, a-f, A-F) |
| `isAscii` | `(char) -> bool` | Is valid ASCII (0-127) |

### Conversion

| Function | Signature | Description |
|----------|-----------|-------------|
| `toUpper` | `(char) -> char` | Convert to uppercase |
| `toLower` | `(char) -> char` | Convert to lowercase |
| `toDigit` | `(char) -> int32` | Character '0'-'9' to integer 0-9 |
| `fromDigit` | `(int32) -> char` | Integer 0-9 to character '0'-'9' |

## Example

```
use std::ascii::{ isAlpha, isDigit, isUpper, toLower, toUpper, toDigit };
use std::log::println;

println(isAlpha('A'));             // true
println(isDigit('5'));             // true
println(isUpper('Z'));             // true
println(toLower('A'));             // a
println(toUpper('z'));             // Z
println(toDigit('7'));             // 7
```

## See Also

- [std::string](string.md) — String manipulation
- [std::conv](conv.md) — Type conversions
