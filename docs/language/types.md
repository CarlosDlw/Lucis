# Types

This page documents all built-in types in Lucis, including their sizes, ranges, methods, and conversion rules.

## Overview

Lucis has a rich set of primitive types with explicit sizes. There is no implicit type coercion — all conversions must use the `as` keyword.

## Integer Types

### Signed Integers

| Type | Size | Range |
|---|---|---|
| `int1` | 1 bit | 0 or 1 |
| `int8` | 1 byte | -128 to 127 |
| `int16` | 2 bytes | -32,768 to 32,767 |
| `int32` | 4 bytes | -2,147,483,648 to 2,147,483,647 |
| `int64` | 8 bytes | -9.2 × 10¹⁸ to 9.2 × 10¹⁸ |
| `int128` | 16 bytes | ±1.7 × 10³⁸ |
| `intinf` | arbitrary | Arbitrary precision (no overflow) |
| `isize` | pointer-sized | 32 or 64 bits, matching the target architecture |

```

use std::log::println;

fn main() int32 {
    int8 small = 127;
    int32 normal = 1000;
    int64 big = 10000000000;
    int128 huge = 100000000000000000;
    isize pointer_sized = 42;

    println(small);
    println(normal);
    println(big);
    println(huge);
    println(pointer_sized);

    ret 0;
}
```

### Unsigned Integers

| Type | Size | Range |
|---|---|---|
| `uint1` | 1 bit | 0 or 1 |
| `uint8` | 1 byte | 0 to 255 |
| `uint16` | 2 bytes | 0 to 65,535 |
| `uint32` | 4 bytes | 0 to 4,294,967,295 |
| `uint64` | 8 bytes | 0 to 1.8 × 10¹⁹ |
| `uint128` | 16 bytes | 0 to 3.4 × 10³⁸ |
| `usize` | pointer-sized | 0 to max pointer value |

```

use std::log::println;

fn main() int32 {
    uint8 byte_val = 200;
    uint32 counter = 3000000;
    uint64 address = 10000000000;
    usize length = 42;

    println(byte_val);
    println(counter);
    println(address);
    println(length);

    ret 0;
}
```

### Integer Literal Formats

Lucis supports decimal, hexadecimal, octal, and binary integer literals:

```
int32 dec = 255;          // decimal
int32 hex = 0xFF;         // hexadecimal (0x or 0X prefix)
int32 oct = 0o377;        // octal (0o or 0O prefix)
int32 bin = 0b11111111;   // binary (0b or 0B prefix)

// all four represent the same value: 255
```

| Format | Prefix | Digits |
|---|---|---|
| Decimal | *(none)* | `0`–`9` |
| Hexadecimal | `0x` / `0X` | `0`–`9`, `a`–`f`, `A`–`F` |
| Octal | `0o` / `0O` | `0`–`7` |
| Binary | `0b` / `0B` | `0`, `1` |

Integer literals are untyped until assigned — the compiler coerces them to the target type automatically:

```
uint8 mask = 0xFF;        // coerced to uint8
int64 flags = 0b1010;     // coerced to int64
int32 perms = 0o755;      // coerced to int32
```

### Suffixed Literals (Rust-style)

You can also specify the exact type of a literal with a suffix:

```lucis
uint8  a = 255u8;         // unsigned 8-bit
int16  b = -1i16;         // signed 16-bit
uint32 c = 0xFF00FF00u32; // unsigned 32-bit
int64  d = 42i64;         // signed 64-bit
float32 e = 3.14f32;      // 32-bit float

uint64 hex = 0xDEADBEEFu64;
uint16 oct = 0o755u16;
uint8  bin = 0b1010u8;
```

Supported suffixes:

| Suffix | Type | Suffix | Type | Suffix | Type |
|--------|------|--------|------|--------|------|
| `i8` | `int8` | `u8` | `uint8` | `f32` | `float32` |
| `i16` | `int16` | `u16` | `uint16` | `f64` | `float64` |
| `i32` | `int32` | `u32` | `uint32` | `f80` | `float80` |
| `i64` | `int64` | `u64` | `uint64` | `f128` | `float128` |
| `i128` | `int128` | `u128` | `uint128` | | |
| `iinf` | `intinf` | `usize` | `usize` | | |
| `isize` | `isize` | | | | |

Suffixed literals work with all literal forms (decimal, hex `0x`, octal `0o`, binary `0b`, and scientific float).

### Integer Methods

All integer types (`int*` and `uint*`) support these methods via dot notation:

| Method | Signature | Description |
|---|---|---|
| `abs()` | `-> T` | Absolute value |
| `clamp(min, max)` | `-> T` | Clamp to range |
| `pow(exp)` | `-> T` | Exponentiation |
| `min(other)` | `-> T` | Smaller of two values |
| `max(other)` | `-> T` | Larger of two values |
| `wrappingAdd(other)` | `-> T` | Addition with wrapping overflow |
| `wrappingSub(other)` | `-> T` | Subtraction with wrapping overflow |
| `wrappingMul(other)` | `-> T` | Multiplication with wrapping overflow |
| `saturatingAdd(other)` | `-> T` | Addition saturating at bounds |
| `saturatingSub(other)` | `-> T` | Subtraction saturating at bounds |
| `leadingZeros()` | `-> T` | Count of leading zero bits |
| `trailingZeros()` | `-> T` | Count of trailing zero bits |
| `countOnes()` | `-> T` | Count of set bits (popcount) |
| `rotateLeft(n)` | `-> T` | Bitwise left rotation |
| `rotateRight(n)` | `-> T` | Bitwise right rotation |
| `toBigEndian()` | `-> T` | Convert to big-endian byte order |
| `toLittleEndian()` | `-> T` | Convert to little-endian byte order |
| `byteSwap()` | `-> T` | Reverse byte order |
| `isPowerOfTwo()` | `-> bool` | Check if value is a power of two |
| `nextPowerOfTwo()` | `-> T` | Smallest power of two ≥ value |
| `log2()` | `-> T` | Integer base-2 logarithm |
| `sign()` | `-> T` | Sign: -1, 0, or 1 |
| `toString()` | `-> string` | Convert to string |
| `toStringRadix(radix)` | `-> string` | Convert to string with given base |
| `toFloat()` | `-> float64` | Convert to float |
| `toChar()` | `-> char` | Convert to character |

```

use std::log::println;

fn main() int32 {
    int32 x = -42;
    println(x.abs());           // 42
    println(x.sign());          // -1

    int32 y = 15;
    println(y.clamp(0, 10));    // 10
    println(y.pow(2));          // 225
    println(y.min(20));         // 15
    println(y.max(20));         // 20

    uint32 bits = 255;
    println(bits.countOnes());  // 8
    println(bits.isPowerOfTwo()); // 0 (false)

    int32 n = 42;
    println(n.toString());      // "42"

    ret 0;
}
```

## Floating-Point Types

| Type | Size | Precision |
|---|---|---|
| `float32` | 4 bytes | ~7 decimal digits (IEEE 754 single) |
| `float64` | 8 bytes | ~15 decimal digits (IEEE 754 double) |
| `float80` | target ABI dependent (typically 16 bytes on x86-64) | ~18 decimal digits (80-bit x87 extended precision) |
| `float128` | 16 bytes | ~33 decimal digits (IEEE 754 quad) |
| `double` | 8 bytes | Alias for `float64` |

```

use std::log::println;

fn main() int32 {
    float32 pi = 3.14;
    float64 precise_pi = 3.141592653589793;
    double e = 2.718281828459045;
    float64 scientific = 1.0e10;

    println(pi);
    println(precise_pi);
    println(e);
    println(scientific);

    ret 0;
}
```

### Float Literal Formats

```
float64 a = 3.14;       // standard decimal
float64 b = .5;          // leading dot
float64 c = 1e10;        // scientific notation
float64 d = 1.2e-5;      // scientific with negative exponent
```

### Float Methods

All float types support these methods via dot notation:

| Method | Signature | Description |
|---|---|---|
| `abs()` | `-> T` | Absolute value |
| `ceil()` | `-> T` | Round up to nearest integer |
| `floor()` | `-> T` | Round down to nearest integer |
| `round()` | `-> T` | Round to nearest integer |
| `trunc()` | `-> T` | Truncate fractional part |
| `fract()` | `-> T` | Fractional part only |
| `sqrt()` | `-> T` | Square root |
| `cbrt()` | `-> T` | Cube root |
| `pow(exp)` | `-> T` | Exponentiation |
| `exp()` | `-> T` | e raised to the power |
| `exp2()` | `-> T` | 2 raised to the power |
| `ln()` | `-> T` | Natural logarithm |
| `log2()` | `-> T` | Base-2 logarithm |
| `log10()` | `-> T` | Base-10 logarithm |
| `sin()`, `cos()`, `tan()` | `-> T` | Trigonometric functions |
| `asin()`, `acos()`, `atan()` | `-> T` | Inverse trigonometric |
| `atan2(other)` | `-> T` | Two-argument arctangent |
| `sinh()`, `cosh()`, `tanh()` | `-> T` | Hyperbolic functions |
| `hypot(other)` | `-> T` | Hypotenuse (sqrt(a² + b²)) |
| `min(other)` | `-> T` | Smaller of two values |
| `max(other)` | `-> T` | Larger of two values |
| `clamp(min, max)` | `-> T` | Clamp to range |
| `lerp(other, t)` | `-> T` | Linear interpolation |
| `sign()` | `-> T` | Sign: -1.0, 0.0, or 1.0 |
| `copySign(other)` | `-> T` | Copy sign from other |
| `isNaN()` | `-> bool` | Check for NaN |
| `isInf()` | `-> bool` | Check for infinity |
| `isFinite()` | `-> bool` | Check if finite |
| `isNormal()` | `-> bool` | Check if normal (not zero, NaN, inf, subnormal) |
| `isNegative()` | `-> bool` | Check if negative |
| `toRadians()` | `-> T` | Convert degrees to radians |
| `toDegrees()` | `-> T` | Convert radians to degrees |
| `toString()` | `-> string` | Convert to string |
| `toStringPrecision(n)` | `-> string` | Convert with n decimal places |
| `toInt()` | `-> int64` | Truncate to integer |
| `toBits()` | `-> uint64` | Bit representation |

```

use std::log::println;

fn main() int32 {
    float64 x = -3.7;
    println(x.abs());       // 3.700000
    println(x.ceil());      // -3.000000
    println(x.floor());     // -4.000000
    println(x.round());     // -4.000000

    float64 y = 2.0;
    println(y.sqrt());      // 1.414214
    println(y.pow(10.0));   // 1024.000000

    float64 angle = 3.141592653589793;
    println(angle.sin());   // 0.000000
    println(angle.cos());   // -1.000000

    println(x.isNaN());     // 0 (false)
    println(x.isNegative()); // 1 (true)

    ret 0;
}
```

## Boolean Type

| Type | Size | Values |
|---|---|---|
| `bool` | 1 bit (`i1` in LLVM) | `true` or `false` |

```

use std::log::println;

fn main() int32 {
    bool is_active = true;
    bool is_done = false;

    println(is_active);  // 1
    println(is_done);    // 0

    ret 0;
}
```

### Bool Methods

| Method | Signature | Description |
|---|---|---|
| `toString()` | `-> string` | `"true"` or `"false"` |
| `toInt()` | `-> int32` | `1` or `0` |
| `toggle()` | `-> bool` | Inverts the value |

## Character Type

| Type | Size | Values |
|---|---|---|
| `char` | 1 byte | ASCII character (0–255) |

```

use std::log::println;

fn main() int32 {
    char letter = 'A';
    char digit = '9';
    char newline = '\n';
    char tab = '\t';
    char hex = '\x41';       // 'A' in hex

    println(letter);
    println(digit);
    println(hex);

    ret 0;
}
```

### Char Methods

| Method | Signature | Description |
|---|---|---|
| `isAlpha()` | `-> bool` | Is alphabetic |
| `isDigit()` | `-> bool` | Is decimal digit |
| `isHexDigit()` | `-> bool` | Is hexadecimal digit |
| `isAlphaNum()` | `-> bool` | Is alphanumeric |
| `isUpper()` | `-> bool` | Is uppercase |
| `isLower()` | `-> bool` | Is lowercase |
| `isSpace()` | `-> bool` | Is whitespace |
| `isPrintable()` | `-> bool` | Is printable character |
| `isControl()` | `-> bool` | Is control character |
| `isPunct()` | `-> bool` | Is punctuation |
| `isAscii()` | `-> bool` | Is valid ASCII |
| `toUpper()` | `-> char` | Convert to uppercase |
| `toLower()` | `-> char` | Convert to lowercase |
| `toInt()` | `-> int32` | ASCII value |
| `toString()` | `-> string` | Single-character string |
| `repeat(n)` | `-> string` | Repeat n times |
| `digitToInt()` | `-> int32` | Digit character to integer value |

```

use std::log::println;

fn main() int32 {
    char c = 'a';
    println(c.isAlpha());    // 1 (true)
    println(c.isDigit());    // 0 (false)
    println(c.toUpper());    // A
    println(c.toInt());      // 97

    char d = '7';
    println(d.digitToInt()); // 7

    ret 0;
}
```

## String Type

| Type | Size | Description |
|---|---|---|
| `string` | pointer + length | Length-tracked byte string (`[]uint8` internally) |

Strings in Lucis are **not** null-terminated. They carry their length and are backed by a `uint8` array. For null-terminated C strings, use [`cstring`](#c-string-type) or `c"..."` literals.

```

use std::log::println;

fn main() int32 {
    string greeting = "Hello, world!";
    string empty = "";

    println(greeting);
    println(empty);
    println("inline literal");

    ret 0;
}
```

### String Methods

| Method | Signature | Description |
|---|---|---|
| `len()` | `-> usize` | Length in bytes |
| `isEmpty()` | `-> bool` | Check if length is 0 |
| `at(index)` | `-> char` | Character at position |
| `front()` | `-> char` | First character |
| `back()` | `-> char` | Last character |
| `contains(sub)` | `-> bool` | Check if substring exists |
| `startsWith(prefix)` | `-> bool` | Check prefix |
| `endsWith(suffix)` | `-> bool` | Check suffix |
| `indexOf(sub)` | `-> int64` | First occurrence (-1 if not found) |
| `lastIndexOf(sub)` | `-> int64` | Last occurrence |
| `count(sub)` | `-> usize` | Count occurrences |
| `substring(start, len)` | `-> string` | Extract substring |
| `slice(start, end)` | `-> string` | Extract range (supports negative indices) |
| `trim()` | `-> string` | Remove leading/trailing whitespace |
| `trimLeft()` | `-> string` | Remove leading whitespace |
| `trimRight()` | `-> string` | Remove trailing whitespace |
| `trimChar(c)` | `-> string` | Trim specific character |
| `toUpper()` | `-> string` | Convert to uppercase |
| `toLower()` | `-> string` | Convert to lowercase |
| `capitalize()` | `-> string` | Capitalize first character |
| `reverse()` | `-> string` | Reverse the string |
| `repeat(n)` | `-> string` | Repeat n times |
| `concat(other)` | `-> string` | Concatenate two strings |
| `padLeft(width, ch)` | `-> string` | Left-pad to width |
| `padRight(width, ch)` | `-> string` | Right-pad to width |
| `replace(old, new)` | `-> string` | Replace all occurrences |
| `replaceFirst(old, new)` | `-> string` | Replace first occurrence |
| `removePrefix(prefix)` | `-> string` | Remove prefix if present |
| `removeSuffix(suffix)` | `-> string` | Remove suffix if present |
| `remove(start, len)` | `-> string` | Remove substring |
| `insert(pos, sub)` | `-> string` | Insert at position |
| `split(delim)` | `-> vec<string>` | Split by delimiter |
| `join(sep)` | `-> string` | Join (only on Vec\<string\>) |
| `chars()` | `-> vec<char>` | Convert to character array |
| `bytes()` | `-> vec<uint8>` | Convert to byte array |
| `lines()` | `-> vec<string>` | Split by newlines |
| `words()` | `-> vec<string>` | Split by whitespace |
| `compareTo(other)` | `-> int32` | Lexicographic comparison |
| `equalsIgnoreCase(other)` | `-> bool` | Case-insensitive equality |
| `isNumeric()` | `-> bool` | All characters are digits |
| `isAlpha()` | `-> bool` | All characters are letters |
| `isAlphaNum()` | `-> bool` | All characters are alphanumeric |
| `isUpper()` | `-> bool` | All letters are uppercase |
| `isLower()` | `-> bool` | All letters are lowercase |
| `isBlank()` | `-> bool` | Empty or only whitespace |
| `toInt()` | `-> int64` | Parse as integer |
| `toFloat()` | `-> float64` | Parse as float |
| `toBool()` | `-> bool` | `"true"` -> true, else false |
| `hash()` | `-> uint64` | Hash value |

```

use std::log::println;

fn main() int32 {
    string text = "Hello, World!";

    println(text.len());              // 13
    println(text.contains("World"));  // 1 (true)
    println(text.startsWith("Hello")); // 1 (true)
    println(text.indexOf("World"));   // 7
    println(text.toUpper());          // HELLO, WORLD!
    println(text.reverse());          // !dlroW ,olleH

    string padded = "42".padLeft(6, '0');
    println(padded);                  // 000042

    string sub = text.substring(7, 5);
    println(sub);                     // World

    ret 0;
}
```

## Void Type

| Type | Size | Description |
|---|---|---|
| `void` | 0 bytes | No value — used as function return type |

```
fn greet(string name) void {
    println(name);
}
```

`void` cannot be used as a variable type. It is only valid as a function return type or pointer target (`*void`).

## C String Type

| Type | Size | Description |
|---|---|---|
| `cstring` | pointer-sized | Alias for `*char` — null-terminated C string |

Lucis provides two ways to create C strings:

```

use std::log::println;

fn main() int32 {
    // C string literal — produces *char directly
    cstring greeting = c"Hello from C";

    // Convert Lucis string to C string
    string t_str = "Hello from Lucis";
    cstring c_str = cstr(t_str);

    // Convert C string back to Lucis string
    string back = fromCStr(c_str);
    println(back);

    ret 0;
}
```

## Pointer Types

| Type | Size | Description |
|---|---|---|
| `*T` | pointer-sized | Pointer to any type T |
| `**T` | pointer-sized | Pointer to pointer |
| `*void` | pointer-sized | Generic/opaque pointer |

See [Pointers](pointers.md) for full documentation.

```
int32 x = 42;
*int32 ptr = &x;
int32 val = *ptr;    // dereference: 42
```

## Array Types

| Type | Description |
|---|---|
| `[N]T` | Fixed-size array of N elements of type T |
| `[]T` | Array with inferred size (from initializer) |
| `[][]T` | Multi-dimensional array |

See [Arrays](arrays.md) for full documentation.

```
[5]int32 fixed = [1, 2, 3, 4, 5];
[]int32 inferred = [10, 20, 30];
[][]int32 matrix = [[1, 2], [3, 4]];
```

## Function Types

Function pointer types use the `fn` keyword:

```
type BinOp = fn(int32, int32) -> int32;

fn add(int32 a, int32 b) int32 { ret a + b; }

fn main() int32 {
    fn(int32, int32) -> int32 op = &add;
    int32 result = op(3, 4);    // 7
    ret 0;
}
```

## Null

`null` is a literal representing a null pointer. It is compatible with any pointer type:

```
*int32 ptr = null;
*void generic = null;
```

## Type Casting

Use the `as` keyword to convert between types:

```
int32 x = 42;
float64 f = x as float64;
int64 big = x as int64;
*void generic = &x as *void;
```

The compiler validates cast compatibility — not all casts are allowed.

## Type Checking

Use `is` to check a value's type at runtime:

```
bool check = x is int32;    // true
```

## Type Queries

```
int64 size = sizeof(int32);     // 4 (bytes)
string name = typeof(42);       // "int32"
```

## Type Conversion Summary

| From | To | Method |
|---|---|---|
| Any integer | Larger/smaller integer | `as` cast |
| Integer | Float | `as` cast or `.toFloat()` |
| Float | Integer | `as` cast or `.toInt()` |
| Any primitive | String | `.toString()` or `toString(expr)` |
| String | Integer | `toInt(str)` or `std::str::parseInt` |
| String | Float | `toFloat(str)` or `std::str::parseFloat` |
| String | Bool | `toBool(str)` |
| String | C string | `cstr(str)` |
| C string | String | `fromCStr(ptr)` or `fromCStrLen(ptr, len)` |
| Char | Integer | `.toInt()` |
| Integer | Char | `.toChar()` |

## See Also

- [Variables](variables.md) — Variable declaration and scope
- [Operators](operators.md) — All operators and precedence
- [Pointers](pointers.md) — Pointer types in depth
- [Arrays](arrays.md) — Array types in depth
- [Type Aliases](type-aliases.md) — The `type` keyword
- [Type Methods](../reference/type-methods.md) — Complete method reference
