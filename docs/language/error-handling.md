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
4. The error payload type can be `Error` (the built-in struct) or any other type. The variant must follow the naming convention `Err`, `Error`, `Failure`, `Fail`, or `None`.
5. Variant names other than the error variant do not matter. Only shape, error-variant naming convention, and payload types are checked.

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

Invalid (error variant name does not follow convention):

```
enum Bad {
    Ok(int32),
    /// 'Abc' is not recognized as an error variant
    Abc(string)
}
```

### Complete example — built-in Error

```
namespace Main;

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
namespace Main;

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
2. One variant follows the error-variant naming convention (`Err`, `Error`, `Failure`, `Fail`, `None`).
3. The success payload type must be assignable to the inferred result variable.
4. The current function's return type must match the source enum type (because the error variant is returned directly).

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
| Requires a catch block | Yes | No |
| Allows recovery | Yes | No |
| Return type constraint | None | Must match enclosing function return type |

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
