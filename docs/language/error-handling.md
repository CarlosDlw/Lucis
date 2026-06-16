# Error Handling

Lucis provides structured error handling with `try`/`catch`/`finally`, `throw`, custom enum-based error types, the `?` propagate operator, and the built-in `Error` struct. For immediate program termination, global builtins `panic`, `assert`, `assertMsg`, and `unreachable` are also available.

---

## The Error Struct

When an error is thrown, it is represented as an `Error` struct with four fields:

| Field | Type | Description |
|-------|------|-------------|
| `message` | `string` | Error description |
| `file` | `string` | Source file where the error was thrown |
| `line` | `int32` | Line number |
| `column` | `int32` | Column number |

The `file`, `line`, and `column` fields are automatically filled by the compiler at the `throw` site.

---

## throw

The `throw` statement raises an error. Only the `message` field needs to be provided:

```
throw Error { message: "something went wrong" };
```

The remaining fields (`file`, `line`, `column`) are injected automatically.

---

## try / catch / finally

The `try` block wraps code that may throw. The `catch` block handles the error. The `finally` block runs regardless of whether an error occurred.

```
try {
    println("before throw");
    throw Error { message: "something went wrong" };
    println("should not print");
} catch (Error e) {
    println(e.message);   // "something went wrong"
    println(e.file);      // source file path
    println(e.line);      // line number of the throw
    println(e.column);    // column number
} finally {
    println("finally block");
}

println("after try/catch");
```

**Output:**
```
before throw
something went wrong
<file path>
<line>
<column>
finally block
after try/catch
```

### Block combinations

All three blocks can be combined:

| Form | Valid |
|------|-------|
| `try { } catch (Error e) { }` | Yes |
| `try { } finally { }` | Yes |
| `try { } catch (Error e) { } finally { }` | Yes |

### Execution flow

1. Code in `try` executes normally
2. If `throw` is reached, execution jumps to `catch`
3. `finally` always executes — after `try` completes normally, or after `catch` handles the error
4. Execution continues after the entire `try`/`catch`/`finally` block

---

## Enum Unwrap with catch

`expr catch { ... }` is an expression-level unwrap for enum-based error returns.

Use it when a function returns an enum shaped like success-or-error, and you want:

- the success payload as the value of the expression
- an inline error branch with access to the `Error` payload

### Basic form

```
auto value = divide(10, 0) catch {
    println(it.message);
    ret 1;
};
```

### What happens

1. `divide(10, 0)` is evaluated.
2. If it is the success variant, the success payload becomes the value of the full expression.
3. If it is the error variant, the `catch` block runs.
4. Inside that block, `it` is the implicit error payload.

### Required enum shape

`expr catch { ... }` is accepted only if the expression type is an enum with this exact structure:

1. Exactly 2 variants total.
2. One variant has exactly one payload — this is the error variant.
3. The other variant has exactly one payload of any type (the success payload).
4. The error variant is identified by one of:
   - The `#[error]` attribute placed before the variant
   - The naming convention `Err`, `Error`, `Failure`, `Fail`, or `None`
   - A payload type named `Error` (the built-in struct)
5. Variant names other than the error variant do not matter. Only shape, error-variant identification, and payload types are checked.

### Custom error variant names with `#[error]`

For enums where `Err`/`Error`/etc. don't fit semantically, use `#[error]` to mark the error variant:

```
enum Status<T> {
    #[error]
    Fault(Error),
    Good(T)
}
```

The `#[error]` attribute takes priority over naming convention. Only one variant may be marked. Without `#[error]`, the naming convention applies as before.

### Type inference

The unwrap expression type is the success payload type.

```
auto value = divide(10, 2) catch {
    ret 1;
};
// value is inferred as int32 if success payload is int32
```

### Scope of `it`

- `it` exists only inside the `catch` block of `expr catch { ... }`.
- Using `it` outside this block is a checker error. The type of `it` is the error variant's payload type.

### Importing variants with `use Type::*;`

Instead of qualifying every variant with the enum name (e.g. `Response::Ok(value)`), use `use` to bring all variant names into scope:

```
fn example() {
    use Response::*;
    ret Ok(42);        // instead of Response::Ok(42)
}
```

The `use` declaration works both at the top level and inside function bodies. Variants are scoped like local variables — visible only after the `use` statement and limited to the enclosing block.

```
fn scope_demo() {
    // Response::Ok not available here
    use Response::*;
    // Ok and Err are now in scope
    {
        // still visible inside nested blocks
        ret Ok(a / b);
    }
}
```

### Valid and invalid patterns

Valid (built-in `Error` payload):

```
enum Response {
    Ok(int32),
    Err(Error)
}
```

Valid (custom string payload):

```
enum HttpResult {
    Success(string),
    Err(string)
}
// Inside catch, it is typed as string
```

Valid (custom struct payload):

```
struct HttpError {
    status int32;
    message string;
}

enum HttpResponse {
    Ok(string),
    Err(HttpError)
}
// Inside catch, it is typed as HttpError
```

Also valid (different names, same shape):

```
enum OperationResult {
    Success(string),
    Failure(Error)
}
```

Invalid (more than 2 variants):

```
enum Bad {
    Ok(int32),
    Err(Error),
    Unknown(Error)
}
```

Invalid (error variant name does not follow convention and no `#[error]`):

```
enum Bad {
    Ok(int32),
    /// 'Abc' is not recognized as an error variant
    Abc(string)
}
```

This can be fixed with `#[error]`:

```
enum Good {
    Ok(int32),
    #[error]
    Abc(string)
}
```

### Complete example — built-in Error

```

#include <stdlib.h>
#include <stdio.h>

enum Response {
    Ok(int32),
    Err(Error)
}

fn divide(int32 a, int32 b) Response {
    use Response::*;

    try {
        if b == 0 {
            ret Err(Error { message: "Division by zero!" });
        }

        ret Ok(a / b);
    } catch(Error e) {
        ret Err(e);
    }
}

fn main() int32 {
    auto value = divide(10, 0) catch {
        printf(c"Error: %s\\n", it.message);
        ret 1;
    };

    printf(c"%d\\n", value);
    ret 0;
}
```

### Complete example — custom error payload with `?`

```

struct HttpError {
    status int32;
    message string;
}

enum HttpResponse {
    Ok(string),
    Err(HttpError)
}

/// Simulates an HTTP GET that may fail.
fn httpGet(string url) HttpResponse {
    use HttpResponse::*;

    if url == "" {
        ret Err(HttpError { status: 400, message: "empty url" });
    }
    // On success, return body as string.
    ret Ok("<html>...</html>");
}

/// Fetches a URL and returns the body length.
/// Propagates any HttpError to the caller.
fn fetchAndMeasure(string url) int32 {
    use HttpResponse::*;
    string body = httpGet(url) ?;
    ret body.len();
}

fn main() int32 {
    int32 len = fetchAndMeasure("https://example.com") catch {
        println("HTTP error: " + it.status + " " + it.message);
        ret 0;
    };
    println("body length: " + len);
    ret 0;
}
```

---

## The `?` Propagate Operator

The `?` (propagate) operator is a shorthand for "unwrap the success payload or return the error variant early from the current function". It works with any two-variant enum that follows the same convention as `expr catch`.

### Syntax

Place `?` after a call expression that returns a compatible enum:

```
auto value = fallibleFunction() ?;
```

This is equivalent to:

```
auto tmp = fallibleFunction();
auto value = tmp catch { ret tmp; };
```

### Requirements

1. The expression must have an enum type with exactly 2 variants.
2. One variant follows the error-variant identification rules (same as `expr catch` — `#[error]` attribute or naming convention).
3. The current function's return type must be **compatible** with the source enum type:
   - **Exact match**: Source and return are the same enum type.
   - **Compatible enums**: Both are two-variant enums with the same error payload type (e.g., both carry `Error`). The success payload types can differ — `?` extracts the source's success payload, which becomes the expression type directly.
   - **Void return**: Functions returning `void` can use `?` to propagate the error and discard the success value. Unit success variants (no payload) are supported.

### Void functions

`?` in a `void` function propagates the error variant and discards the success:

```
enum MaybeError {
    Ok,
    Err(Error)
}

fn fallible() MaybeError { ... }

fn main() void {
    fallible() ?;  // propagates Err, discards Ok
    println("success");
}
```

### Compatible enums

`?` works between different enum types as long as they share the same error payload:

```
enum MyResult<T> { Ok(T), Err(Error) }
enum YourResult<T> { Success(T), Failure(Error) }

fn inner() MyResult<int32> { ... }

fn outer() YourResult<int32> {
    auto val = inner() ?;  // MyResult<int32> → YourResult<int32>
    ret YourResult<int32>::Success(val);
}
```

The compatibility check only requires matching error payload types. The success payload becomes the expression type directly — no type coercion needed.

### Context block

`?` accepts an optional block `{ ... }` that executes on the error path before propagation:

```
auto cfg = load_config() ? {
    printf("config load failed, using defaults\n");
};
auto ok = validate(cfg) ? {
    printf("validation failed for input\n");
};
```

The context block runs only when an error occurs, immediately before the error is propagated. Useful for logging, cleanup, or side effects on the error path.

### Custom payloads

The `?` operator works with any error payload type, just like `expr catch`:

```
enum HttpResult {
    Success(string),
    Err(int32)
}

fn fetchData(bool fail) string {
    // htttpGet returns HttpResult;
    // if Err, the int32 code propagates up
    // if Success, body gets the string
    string body = httpGet("https://example.com") ?;
    ret body;
}
```

### Simple propagation example

```
enum Result {
    Ok(int32),
    Err(Error)
}

fn inner() Result {
    // Returns Ok(42) or Err(Error{...})
    ret ...
}

fn outer() int32 {
    int32 val = inner() ?;
    // If inner() returned Err, outer() returns that Err immediately.
    // If inner() returned Ok(42), val = 42.
    ret val;
}
```

### Deep propagation

Multiple `?` operators can chain across functions. At each level the error variant propagates up immediately, while the success payload continues as the normal value:

```
enum FileResult {
    Ok(string),        // file contents
    Err(Error)
}

FileResult readFile(string path) { ... }

enum ProcessResult {
    Ok(int32),
    Err(Error)
}

fn process() ProcessResult {
    use ProcessResult::*;
    string contents = readFile("input.txt") ?;
    // If readFile failed, process() returns the Err variant immediately.
    // Otherwise contents holds the file data.
    int32 lines = countLines(contents);
    ret Ok(lines);
}

fn main() int32 {
    use ProcessResult::*;
    int32 result = process() ?;
    // If process() failed, main() returns the Err variant.
    // Otherwise result holds the line count.
    println("lines: " + result);
    ret 0;
}
```

### Key differences from `expr catch`

| Aspect | `expr catch` | `?` operator |
|--------|-------------|--------------|
| Purpose | Handle the error inline | Propagate the error to the caller |
| Requires a catch block | Yes | No (optional context block `? { ... }`) |
| Allows recovery | Yes | No |
| Return type constraint | None | Must be compatible enum or void |
| Void functions | N/A | Supported |

---

## Try Expression with Fallback

The `try` expression wraps a sub-expression in exception handling. If the expression throws, a `null`/`0` value is returned. With `or`, a custom fallback value can be specified:

```
auto x = try fallible() or -1;
auto cfg = try load_config() or default_config();
```

- If no exception occurs, the expression value is used.
- If an exception is thrown, the fallback expression is evaluated and used instead.
- The fallback type must be assignable to the inner expression type.
- Without `or`, the default is `null` / `0` (zero-initialized).

---

## `defer` with `throw`

`defer` statements now execute before `throw`, ensuring cleanup like resource freeing runs even on exception paths:

```
fn read_file(string path) string {
    auto fd = open(path, 0, 0);
    defer close(fd);       // executes before throw
    auto data = read_all(fd);
    if data.len() == 0 {
        throw Error { message: "empty file" };
    }
    ret data;
}
```

Deferred statements run in LIFO order (last declared, first executed) and fire on:
- Normal scope exit
- Early `return`
- `?` propagation
- `throw` statements

---

## Global Error Builtins

These functions are available without any `use` import and immediately terminate the program:

### `panic(string)`

Aborts with an error message:

```
panic("something went wrong!");
```

Prints the message to stderr and exits with a non-zero code.

### `assert(bool)`

Aborts if the condition is `false`:

```
assert(x > 0);
```

### `assertMsg(bool, string)`

Aborts if the condition is `false`, with a custom message:

```
assertMsg(x > 0, "expected positive value");
```

### `unreachable()`

Marks code that should never be reached. If executed, the program aborts:

```
unreachable();
```

---

## See Also

- [Control Flow](control-flow.md) — `try`/`catch`/`finally` in the context of control flow
- [Modules](modules.md) — Global builtins available without imports
