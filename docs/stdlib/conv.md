# std::conv

Type conversion functions between strings, integers, and floats.

## Import

```
use std::conv::{ itoa, atoi, toHex, toBinary, fromHex };
```

## Functions

### Integer to String

| Function | Signature | Description |
|----------|-----------|-------------|
| `itoa` | `(int64) -> string` | Integer to decimal string |
| `itoaRadix` | `(int64, int32) -> string` | Integer to string in given base |
| `utoa` | `(uint64) -> string` | Unsigned integer to string |

### Float to String

| Function | Signature | Description |
|----------|-----------|-------------|
| `ftoa` | `(float64) -> string` | Float to string |
| `ftoaPrecision` | `(float64, uint32) -> string` | Float to string with precision |

### String to Number

| Function | Signature | Description |
|----------|-----------|-------------|
| `atoi` | `(string) -> int64` | String to integer |
| `atof` | `(string) -> float64` | String to float |

### Base Conversion

| Function | Signature | Description |
|----------|-----------|-------------|
| `toHex` | `(int64) -> string` | Integer to hexadecimal |
| `toOctal` | `(int64) -> string` | Integer to octal |
| `toBinary` | `(int64) -> string` | Integer to binary |
| `fromHex` | `(string) -> uint64` | Hexadecimal string to integer |

### Character Conversion

| Function | Signature | Description |
|----------|-----------|-------------|
| `charToInt` | `(char) -> int32` | Character to ASCII code |
| `intToChar` | `(int32) -> char` | ASCII code to character |

## Example

```
use std::conv::{ itoa, atoi, toHex, toBinary, fromHex, ftoaPrecision };
use std::log::println;

println(itoa(42));              // "42"
println(atoi("456"));           // 456
println(toHex(255));            // "ff"
println(toBinary(10));          // "1010"
println(fromHex("ff"));         // 255
println(ftoaPrecision(3.14159, 2));  // "3.14"
```

## See Also

- [std::str](string.md) — `parseInt`, `parseFloat`
- [std::fmt](fmt.md) — Formatted output
