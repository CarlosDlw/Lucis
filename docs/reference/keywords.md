# Keywords

Lucis has 39 reserved keywords and 20 type keywords. All keywords are lowercase.

---

## Reserved Keywords

### Modules and Imports

| Keyword | Description | Example |
|---------|-------------|---------|
| `use` | Imports symbols from a module | `use std::log::println;` |

### Declarations

| Keyword | Description | Example |
|---------|-------------|---------|
| `fn` | Declares a function type | `type F = fn(int32) -> int32;` |
| `struct` | Declares a struct type | `struct Point { float64 x; float64 y; }` |
| `union` | Declares a union type | `union Value { int32 i; float32 f; }` |
| `enum` | Declares an enum type | `enum Color { Red, Green, Blue }` |
| `type` | Declares a type alias | `type Byte = uint8;` |
| `extend` | Adds methods to an existing type | `extend Point { ... }` |
| `extern` | Declares an external C function | `extern int32 printf(*char fmt, ...);` |

### Control Flow

| Keyword | Description | Example |
|---------|-------------|---------|
| `if` | Conditional branch | `if x > 0 { ... }` |
| `else` | Alternative branch | `else { ... }` |
| `for` | For loop (range or classic) | `for int32 i in 0..10 { ... }` |
| `in` | Part of for-in syntax | `for int32 x in arr { ... }` |
| `while` | While loop | `while x > 0 { ... }` |
| `do` | Do-while loop | `do { ... } while x < 10;` |
| `loop` | Infinite loop | `loop { ... }` |
| `switch` | Multi-way branch | `switch x { case 1 { ... } }` |
| `case` | Switch case label | `case 1, 2 { ... }` |
| `default` | Switch default label | `default { ... }` |
| `break` | Exit a loop | `break;` |
| `continue` | Skip to next iteration | `continue;` |
| `ret` | Return from function | `ret 0;` |

### Error Handling

| Keyword | Description | Example |
|---------|-------------|---------|
| `try` | Begin try block or try expression | `try { ... } catch (Error e) { ... }` |
| `catch` | Handle an exception | `catch (Error e) { ... }` |
| `finally` | Always-execute block | `finally { cleanup(); }` |
| `throw` | Throw an exception | `throw Error { message: "fail" };` |

### Memory and Safety

| Keyword | Description | Example |
|---------|-------------|---------|
| `defer` | Schedule cleanup at scope exit | `defer close(fd);` |
| `null` | Null literal | `*int32 ptr = null;` |

### Concurrency

| Keyword | Description | Example |
|---------|-------------|---------|
| `spawn` | Spawn an async task | `Task<int32> t = spawn compute();` |
| `await` | Wait for a task result | `int32 result = await t;` |
| `lock` | Lock a mutex for a block | `lock(mtx) { ... }` |

### Type Operations

| Keyword | Description | Example |
|---------|-------------|---------|
| `auto` | Infer variable type from initializer | `auto x = 42;` |
| `as` | Type cast | `float64 f = x as float64;` |
| `is` | Type check | `if val is int32 { ... }` |
| `sizeof` | Size of a type in bytes | `usize n = sizeof(int32);` |
| `typeof` | Name of expression's type | `string t = typeof(x);` |

---

## Type Keywords

Type keywords are reserved identifiers for primitive types.

### Signed Integers

| Keyword | Size | Range |
|---------|------|-------|
| `int1` | 1 bit | 0 or -1 |
| `int8` | 1 byte | -128 to 127 |
| `int16` | 2 bytes | -32,768 to 32,767 |
| `int32` | 4 bytes | -2,147,483,648 to 2,147,483,647 |
| `int64` | 8 bytes | -9.2 × 10¹⁸ to 9.2 × 10¹⁸ |
| `int128` | 16 bytes | -1.7 × 10³⁸ to 1.7 × 10³⁸ |
| `intinf` | Variable | Arbitrary precision |
| `isize` | Pointer-sized | Platform dependent (32 or 64 bits) |

### Unsigned Integers

| Keyword | Size | Range |
|---------|------|-------|
| `uint1` | 1 bit | 0 or 1 |
| `uint8` | 1 byte | 0 to 255 |
| `uint16` | 2 bytes | 0 to 65,535 |
| `uint32` | 4 bytes | 0 to 4,294,967,295 |
| `uint64` | 8 bytes | 0 to 1.8 × 10¹⁹ |
| `uint128` | 16 bytes | 0 to 3.4 × 10³⁸ |
| `usize` | Pointer-sized | Platform dependent |

### Floating Point

| Keyword | Size | Precision |
|---------|------|-----------|
| `float32` | 4 bytes | ~7 digits |
| `float64` | 8 bytes | ~15 digits |
| `float80` | target ABI dependent (typically 16 bytes on x86-64) | ~18 digits (80-bit precision) |
| `float128` | 16 bytes | ~33 digits |
| `double` | 8 bytes | Alias for `float64` |

### Other Types

| Keyword | Description |
|---------|-------------|
| `bool` | Boolean (`true` or `false`) |
| `char` | Single byte character |
| `void` | No value (used for function return types) |
| `string` | Immutable UTF-8 string (fat pointer: ptr + len) |
| `cstring` | C-compatible string (alias for `*char`) |

### Collection Types

| Keyword | Description |
|---------|-------------|
| `vec` | Dynamic growable array (`vec<T>`) |
| `map` | Open-addressing hash map (`map<K, V>`) |
| `set` | Open-addressing hash set (`set<T>`) |

---

## Contextual Identifiers

These are not reserved keywords but have special meaning in certain contexts:

| Identifier | Context | Meaning |
|------------|---------|---------|
| `true` | Expression | Boolean true literal |
| `false` | Expression | Boolean false literal |
| `self` | Extend method | Reference to the receiver |
| `Error` | Catch clause | Exception type |

---

## Operator Tokens

These symbols act as operators and are reserved:

```
+  -  *  /  %  =  ==  !=  <  >  <=  >=
&&  ||  !  &  |  ^  ~  <<  >>
++  --  ->  .  ::  ??  ..  ..=  ...
+=  -=  *=  /=  %=  &=  |=  ^=  <<=  >>=
?  :  ;  ,
(  )  [  ]  {  }
```
