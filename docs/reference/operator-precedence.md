# Operator Precedence

Complete table of operator precedence from **highest** (tightest binding) to **lowest**. Operators in the same level have equal precedence and are evaluated left-to-right unless noted.

---

## Precedence Table

| Level | Operator(s) | Description | Associativity |
|-------|-------------|-------------|---------------|
| 17 | `!` `-` (unary) `~` `*` `&` | Unary: not, negate, bitwise not, dereference, address-of | Right |
| 16 | `.` `->` `()` `[]` | Member access, arrow access, function call, index | Left |
| 15 | `as` | Type cast | Left |
| 14 | `**` | Exponentiation | Right |
| 13 | `*` `/` `%` | Multiplication, division, modulo | Left |
| 12 | `+` `-` | Addition, subtraction | Left |
| 11 | `<<` `>>` | Bitwise shift left, shift right | Left |
| 10 | `&` | Bitwise AND | Left |
| 9 | `^` | Bitwise XOR | Left |
| 8 | `\|` | Bitwise OR | Left |
| 7 | `..` `..=` | Range (exclusive), range (inclusive) | Left |
| 6 | `<` `<=` `>` `>=` | Relational comparison | Left |
| 5 | `==` `!=` | Equality, inequality | Left |
| 4 | `&&` | Logical AND (short-circuit) | Left |
| 3 | `\|\|` | Logical OR (short-circuit) | Left |
| 2 | `??` | Null coalescing | Left |
| 1 | `? :` | Ternary conditional | Right |

---

## Unary Operators (Level 17)

| Operator | Example | Description |
|----------|---------|-------------|
| `!` | `!flag` | Logical NOT |
| `-` | `-x` | Arithmetic negation |
| `~` | `~bits` | Bitwise complement |
| `*` | `*ptr` | Pointer dereference |
| `&` | `&value` | Address-of |

```
int32 x = -5
bool positive = !false
uint8 mask = ~0u8        // 0xFF
*int32 ptr = &x
int32 val = *ptr
```

---

## Postfix Operators (Level 16)

| Operator | Example | Description |
|----------|---------|-------------|
| `.` | `obj.field` | Member access |
| `->` | `ptr->field` | Arrow (pointer member access) |
| `()` | `func(args)` | Function call |
| `[]` | `arr[i]` | Index |

```
string name = person.name
int32 val = ptr->value
int32 result = add(2, 3)
int32 first = numbers[0]
```

---

## Type Cast (Level 15)

```
int32 x = 42
float64 f = x as float64       // 42.0
uint8 byte = 256 as uint8      // truncation: 0
```

---

## Arithmetic (Levels 14–12)

| Level | Operators | Example |
|-------|-----------|---------|
| 14 | `**` | `2 ** 10` → 1024 |
| 13 | `* / %` | `10 / 3` → 3, `10 % 3` → 1 |
| 12 | `+ -` | `a + b - c` |

```
int32 result = 2 + 3 * 4      // 14, not 20
int32 exp = 2 ** 3 ** 2        // 512 (right-assoc: 2^(3^2))
```

---

## Bitwise (Levels 11–8)

| Level | Operator | Name |
|-------|----------|------|
| 11 | `<< >>` | Shift |
| 10 | `&` | AND |
| 9 | `^` | XOR |
| 8 | `\|` | OR |

```
uint8 flags = 0b1010
uint8 masked = flags & 0x0F
uint8 shifted = 1u8 << 4       // 0b10000
uint8 combined = 0x0F | 0xF0   // 0xFF
```

---

## Range (Level 7)

```
for i in 0..10 {       // 0 to 9
    // ...
}

for i in 1..=100 {     // 1 to 100 inclusive
    // ...
}
```

---

## Comparison (Levels 6–5)

```
bool less = a < b
bool equal = x == y
bool inRange = x >= 0 && x < 100
```

---

## Logical (Levels 4–3)

Both `&&` and `||` use **short-circuit evaluation**: the right operand is only evaluated if needed.

```
bool valid = x > 0 && y > 0     // y > 0 skipped if x <= 0
bool fallback = a || b           // b skipped if a is true
```

---

## Null Coalescing (Level 2)

```
int32 port = config.port ?? 8080
```

---

## Ternary Conditional (Level 1)

Right-associative. Lowest precedence among operators.

```
string label = count == 1 ? "item" : "items"
int32 clamped = x < 0 ? 0 : x > 100 ? 100 : x
```

---

## Compound Assignment Operators

Compound assignments are **statements**, not expressions. They combine an operator with assignment.

| Operator | Equivalent |
|----------|-----------|
| `+=` | `x = x + rhs` |
| `-=` | `x = x - rhs` |
| `*=` | `x = x * rhs` |
| `/=` | `x = x / rhs` |
| `%=` | `x = x % rhs` |
| `&=` | `x = x & rhs` |
| `\|=` | `x = x \| rhs` |
| `^=` | `x = x ^ rhs` |
| `<<=` | `x = x << rhs` |
| `>>=` | `x = x >> rhs` |

```
var count: int32 = 0
count += 1
count *= 2
count >>= 1
```

---

## Operator Overloading

T does **not** support operator overloading. All operators work only on built-in types.
