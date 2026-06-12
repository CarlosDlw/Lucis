# std::math

Mathematical functions and constants.

## Import

```
use std::math::{ PI, E, sqrt, sin, cos, pow, clamp };
```

## Constants

| Constant | Type | Value |
|----------|------|-------|
| `PI` | `float64` | 3.14159265358979... |
| `E` | `float64` | 2.71828182845904... |
| `TAU` | `float64` | 6.28318530717958... (2π) |
| `INF` | `float64` | Positive infinity |
| `NAN` | `float64` | Not a Number |
| `INT32_MAX` | `int32` | 2147483647 |
| `INT32_MIN` | `int32` | -2147483648 |
| `INT64_MAX` | `int64` | 9223372036854775807 |
| `INT64_MIN` | `int64` | -9223372036854775808 |
| `UINT32_MAX` | `uint32` | 4294967295 |
| `UINT64_MAX` | `uint64` | 18446744073709551615 |

## Functions

### Basic

| Function | Signature | Description |
|----------|-----------|-------------|
| `abs` | `(T) -> T` | Absolute value (int or float) |
| `min` | `(T, T) -> T` | Minimum of two values |
| `max` | `(T, T) -> T` | Maximum of two values |
| `clamp` | `(T, T, T) -> T` | Clamp value to range |

### Powers and Roots

| Function | Signature | Description |
|----------|-----------|-------------|
| `sqrt` | `(float64) -> float64` | Square root |
| `cbrt` | `(float64) -> float64` | Cube root |
| `pow` | `(float64, float64) -> float64` | Power |
| `hypot` | `(float64, float64) -> float64` | Hypotenuse |

### Exponential and Logarithmic

| Function | Signature | Description |
|----------|-----------|-------------|
| `exp` | `(float64) -> float64` | e^x |
| `exp2` | `(float64) -> float64` | 2^x |
| `ln` | `(float64) -> float64` | Natural logarithm |
| `log2` | `(float64) -> float64` | Base-2 logarithm |
| `log10` | `(float64) -> float64` | Base-10 logarithm |

### Trigonometric

| Function | Signature | Description |
|----------|-----------|-------------|
| `sin` | `(float64) -> float64` | Sine |
| `cos` | `(float64) -> float64` | Cosine |
| `tan` | `(float64) -> float64` | Tangent |
| `asin` | `(float64) -> float64` | Arc sine |
| `acos` | `(float64) -> float64` | Arc cosine |
| `atan` | `(float64) -> float64` | Arc tangent |
| `atan2` | `(float64, float64) -> float64` | Two-argument arc tangent |

### Hyperbolic

| Function | Signature | Description |
|----------|-----------|-------------|
| `sinh` | `(float64) -> float64` | Hyperbolic sine |
| `cosh` | `(float64) -> float64` | Hyperbolic cosine |
| `tanh` | `(float64) -> float64` | Hyperbolic tangent |

### Rounding

| Function | Signature | Description |
|----------|-----------|-------------|
| `ceil` | `(float64) -> float64` | Round up |
| `floor` | `(float64) -> float64` | Round down |
| `round` | `(float64) -> float64` | Round to nearest |
| `trunc` | `(float64) -> float64` | Truncate toward zero |

### Interpolation and Conversion

| Function | Signature | Description |
|----------|-----------|-------------|
| `lerp` | `(float64, float64, float64) -> float64` | Linear interpolation |
| `map` | `(float64, float64, float64, float64, float64) -> float64` | Map value from one range to another |
| `toRadians` | `(float64) -> float64` | Degrees to radians |
| `toDegrees` | `(float64) -> float64` | Radians to degrees |

### Queries

| Function | Signature | Description |
|----------|-----------|-------------|
| `isNaN` | `(float64) -> bool` | Is Not a Number |
| `isInf` | `(float64) -> bool` | Is infinity |
| `isFinite` | `(float64) -> bool` | Is finite number |

## Example

```
use std::math::{ PI, sqrt, sin, cos, clamp, toRadians, isNaN, NAN };
use std::log::println;

println(sqrt(144.0));              // 12.0
println(sin(toRadians(90.0)));     // 1.0
println(cos(toRadians(0.0)));      // 1.0
println(clamp(150, 0, 100));       // 100
println(isNaN(NAN));               // true
```

## See Also

- [std::random](random.md) — Random number generation
- [std::bits](bits.md) — Bitwise utilities
