# std::str

String utility functions that operate on `string` values.

## Ownership Note

Most transformation functions in `std::str` return a new owned `string` buffer.
That includes APIs such as `toUpper`, `toLower`, `trim`, `reverse`, `replace`,
`substring`, `slice`, `repeat`, and similar functions that produce a modified
string value.

If you keep the result, release it with `freeStr(...)` when you're done:

```lux
use std::str::{ toUpper };
use std::log::{ println };

fn main() int32 {
    string upper = toUpper("hello");
    defer freeStr(upper);

    println(upper);
    ret 0;
}
```

If a string is borrowed from elsewhere, such as a field access like
`err.message`, do not free the borrowed value itself. Only free the new owned
string returned by the transformation:

```lux
string msg = err.message.capitalize();
defer freeStr(msg);
```

## Import

```
use std::str::{ contains, split, parseInt, toUpper };
```

## Functions

### Search and Match

| Function | Signature | Description |
|----------|-----------|-------------|
| `contains` | `(string, string) -> bool` | Whether string contains substring |
| `startsWith` | `(string, string) -> bool` | Whether string starts with prefix |
| `endsWith` | `(string, string) -> bool` | Whether string ends with suffix |
| `indexOf` | `(string, string) -> int64` | Index of first occurrence (-1 if not found) |
| `lastIndexOf` | `(string, string) -> int64` | Index of last occurrence |
| `count` | `(string, string) -> int64` | Number of occurrences |

### Transformation

| Function | Signature | Description |
|----------|-----------|-------------|
| `toUpper` | `(string) -> string` | Convert to uppercase |
| `toLower` | `(string) -> string` | Convert to lowercase |
| `trim` | `(string) -> string` | Remove leading/trailing whitespace |
| `trimLeft` | `(string) -> string` | Remove leading whitespace |
| `trimRight` | `(string) -> string` | Remove trailing whitespace |

### Replace

| Function | Signature | Description |
|----------|-----------|-------------|
| `replace` | `(string, string, string) -> string` | Replace all occurrences |
| `replaceFirst` | `(string, string, string) -> string` | Replace first occurrence |
| `repeat` | `(string, int64) -> string` | Repeat string n times |
| `reverse` | `(string) -> string` | Reverse the string |

### Padding

| Function | Signature | Description |
|----------|-----------|-------------|
| `padLeft` | `(string, int64, char) -> string` | Pad on left to given width |
| `padRight` | `(string, int64, char) -> string` | Pad on right to given width |

### Extraction

| Function | Signature | Description |
|----------|-----------|-------------|
| `substring` | `(string, int64, int64) -> string` | Extract substring (start, length) |
| `charAt` | `(string, int64) -> char` | Character at index |
| `slice` | `(string, int64, int64) -> string` | Slice by index range (supports negative) |

### Parsing

| Function | Signature | Description |
|----------|-----------|-------------|
| `parseInt` | `(string) -> int64` | Parse string to integer |
| `parseIntRadix` | `(string, int32) -> int64` | Parse with given base |
| `parseFloat` | `(string) -> float64` | Parse string to float |
| `fromCharCode` | `(int32) -> char` | Character from ASCII code |

### Splitting and Joining

| Function | Signature | Description |
|----------|-----------|-------------|
| `split` | `(string, string) -> vec<string>` | Split into parts |
| `splitN` | `(string, string, int64) -> vec<string>` | Split with max count |
| `joinVec` | `(vec<string>, string) -> string` | Join Vec with separator |
| `lines` | `(string) -> vec<string>` | Split into lines |
| `chars` | `(string) -> vec<char>` | Split into characters |
| `fromChars` | `(vec<char>) -> string` | Build string from characters |
| `toBytes` | `(string) -> vec<uint8>` | Convert to byte vector |
| `fromBytes` | `(vec<uint8>) -> string` | Build string from bytes |

## Example

```
use std::str::{ contains, toUpper, split, parseInt, slice };
use std::log::println;

println(contains("hello world", "world"));   // true
println(toUpper("hello"));                   // HELLO
println(parseInt("42"));                     // 42
println(slice("Hello, World!", 7, 12));      // World

vec<string> parts = split("a,b,c", ",");
println(parts.len());   // 3
parts.free();
```

## See Also

- [std::conv](conv.md) — Type conversion functions
- [std::fmt](fmt.md) — String formatting
- [std::regex](regex.md) — Pattern-based string operations
