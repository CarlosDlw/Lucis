# C Strings

Lux strings and C strings have different internal representations. This page explains the difference, how to work with C string literals, and how to convert between the two formats.

---

## The Problem

In C, strings are null-terminated arrays of `char`:

```c
"hello"  →  ['h', 'e', 'l', 'l', 'o', '\0']
```

In T, strings are length-prefixed — they carry a pointer and a length, and are **not** null-terminated:

```
"hello"  →  { ptr: → 'h','e','l','l','o', len: 5 }
```

This means you **cannot** pass a T `string` directly to a C function that expects `char*`. You need to convert it. Lux provides three tools for this.

---

## C String Literals (`c"..."`)

The `c"..."` syntax creates a null-terminated string constant at compile time. Its type is `*char` — exactly what C functions expect:

```
extern int32 puts(*char s);

fn main() int32 {
    puts(c"Hello, World!");
    ret 0;
}
// Output:
// Hello, World!
```

### Escape Sequences

C string literals support the same escape sequences as regular strings:

| Escape | Meaning |
|--------|---------|
| `\n` | Newline |
| `\t` | Tab |
| `\r` | Carriage return |
| `\\` | Backslash |
| `\"` | Double quote |
| `\0` | Null byte |
| `\xHH` | Hex byte (e.g., `\x41` = 'A') |

```
extern int32 printf(*char fmt, ...);

fn main() int32 {
    printf(c"Line 1\nLine 2\n");
    printf(c"Tab\there\n");
    printf(c"Quote: \"\n");
    printf(c"Hex A: \x41\n");
    ret 0;
}
// Output:
// Line 1
// Line 2
// Tab	here
// Quote: "
// Hex A: A
```

### When to Use `c"..."`

Use C string literals when:
- The string is a **constant** known at compile time
- You're passing it directly to a C function
- You don't need to manipulate it as a Lux string first

```
puts(c"This is a constant");                 // direct use
printf(c"Name: %s, Age: %d\n", c"Alice", 30);  // format string + string arg
```

---

## The `cstring` Type Alias

`cstring` is a built-in type alias for `*char`. It makes C string declarations more readable:

```
cstring greeting = c"Hello!";
puts(greeting);

cstring name = c"Alice";
printf(c"Name: %s\n", name);
```

`cstring` and `*char` are fully interchangeable — they are the exact same type:

```
*char a = c"hello";
cstring b = a;        // same type, no conversion
```

---

## Converting T Strings to C Strings: `cstr()`

When you have a T `string` and need to pass it to a C function, use the `cstr()` builtin. It allocates a new null-terminated copy:

```
extern int32 puts(*char s);
extern void free(*void ptr);

fn main() int32 {
    string name = "Lux";
    *char c_name = cstr(name);   // allocates: "Lux\0"
    puts(c_name);                // passes to C
    free(c_name as *void);       // you own the memory — free it!
    ret 0;
}
```

### How `cstr()` Works

1. Allocates `len + 1` bytes via `malloc`
2. Copies the string's bytes into the buffer
3. Appends a null terminator (`\0`)
4. Returns a `*char` pointer to the new buffer

**Important:** The returned pointer is heap-allocated. You are responsible for freeing it when done. Use `defer` for automatic cleanup:

```
string message = "Hello from Lux";
*char c_msg = cstr(message);
defer free(c_msg as *void);    // cleaned up at function exit

puts(c_msg);
printf(c"Message: %s\n", c_msg);
// no manual free needed — defer handles it
```

### Signature

```
cstr(string) -> *char
```

- **Parameter:** A T `string` value
- **Returns:** A newly-allocated, null-terminated `*char`
- **Cost:** O(n) — copies all bytes
- **Ownership:** Caller owns the returned pointer and must free it

---

## Converting C Strings to T Strings: `fromCStr()`

When you receive a `*char` from C and want to use it as a T `string`, use `fromCStr()`:

```
extern *char getenv(*char name);

fn main() int32 {
    *char home = getenv(c"HOME");
    string home_str = fromCStr(home);
    println(home_str);    // /home/user
    ret 0;
}
```

### How `fromCStr()` Works

1. Calls `strlen()` on the pointer to determine the length
2. Wraps the pointer and length into a Lux string struct
3. **Does NOT copy** — the Lux string points to the original C memory

**Important:** Since `fromCStr()` does not copy, the returned string is only valid as long as the original `*char` pointer is valid. If the C function uses a static buffer or the pointer is freed, the Lux string becomes dangling.

### Signature

```
fromCStr(*char) -> string
```

- **Parameter:** A null-terminated `*char` pointer
- **Returns:** A T `string` wrapping the pointer
- **Cost:** O(n) for the `strlen()` call, but zero-copy
- **Ownership:** The Lux string does NOT own the memory — it borrows it

### Null Safety

If you pass `null`, `fromCStr()` returns an empty string `""`:

```
*char ptr = null;
string s = fromCStr(ptr);
println(s);    // (empty string)
```

---

## Converting with Known Length: `fromCStrLen()`

If you already know the length of the C string, use `fromCStrLen()` to skip the `strlen()` call:

```
*char data = c"Hello World!!!!!";
string greeting = fromCStrLen(data, 5);    // takes first 5 bytes: "Hello"
println(greeting);    // Hello
```

### Signature

```
fromCStrLen(*char, usize) -> string
```

- **Parameter 1:** A `*char` pointer (does not need to be null-terminated)
- **Parameter 2:** The number of bytes to use
- **Returns:** A T `string` wrapping the pointer with the given length
- **Cost:** O(1) — zero-cost, just wraps the pointer and length
- **Ownership:** The Lux string does NOT own the memory

This is useful when:
- You know the length from another source (e.g., a `read()` return value)
- You want a substring of a C buffer
- You're working with binary data that isn't null-terminated

---

## Round-Trip Conversion

You can safely convert back and forth between Lux strings and C strings:

```
extern int32 strcmp(*char s1, *char s2);
extern void free(*void ptr);

fn main() int32 {
    // Start with a Lux string
    string original = "Round Trip Test";

    // Convert to C string
    *char c1 = cstr(original);
    defer free(c1 as *void);

    // Convert back to Lux string
    string back = fromCStr(c1);

    // Convert again to C string
    *char c2 = cstr(back);
    defer free(c2 as *void);

    // Compare — they're identical
    int32 result = strcmp(c1, c2);
    assert(result == 0);

    ret 0;
}
```

---

## Summary Table

| Function | Direction | Copies? | Cost | Ownership |
|----------|-----------|---------|------|-----------|
| `c"..."` | Compile-time C string | N/A (static) | Zero | Static (program lifetime) |
| `cstr(s)` | Lux string → `*char` | Yes (malloc) | O(n) | Caller must `free()` |
| `fromCStr(p)` | `*char` → Lux string | No (wraps) | O(n) strlen | Borrows original |
| `fromCStrLen(p, n)` | `*char` → Lux string | No (wraps) | O(1) | Borrows original |

---

## Common Mistakes

### Forgetting to Free `cstr()` Results

```
// BAD — memory leak!
fn log_name(string name) void {
    puts(cstr(name));    // allocated but never freed
}

// GOOD — free after use
fn log_name(string name) void {
    *char c = cstr(name);
    defer free(c as *void);
    puts(c);
}
```

### Using a T String Where C Expects `*char`

```
extern int32 puts(*char s);

string msg = "Hello";
// puts(msg);          // ERROR: type mismatch (string vs *char)
puts(cstr(msg));       // CORRECT: convert first
```

### Using `fromCStr()` After the Source is Freed

```
// BAD — dangling pointer!
fn get_message() string {
    *char buf = malloc(100 as usize) as *char;
    // ... fill buf ...
    string s = fromCStr(buf);
    free(buf as *void);
    ret s;    // s points to freed memory!
}

// GOOD — copy before freeing
fn get_message() string {
    *char buf = malloc(100 as usize) as *char;
    // ... fill buf ...
    *char copy = cstr(fromCStr(buf));    // make an owned copy
    free(buf as *void);
    string s = fromCStr(copy);
    ret s;    // safe — copy is still alive
}
```

---

## See Also

- [FFI Overview](overview.md) — Type correspondence table
- [Calling C Functions](calling-c.md) — `extern` and `#include` usage
- [Pointers](../language/pointers.md) — Pointer types and operations
- [Types](../language/types.md) — String type details
