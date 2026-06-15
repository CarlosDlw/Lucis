# Control Flow

This page covers all control flow constructs in T: conditionals, loops, switch, break, continue, and error handling.

## If / Else

The `if` statement executes a block when a condition is true. Parentheses around the condition are optional:

```

use std::log::println;

fn main() int32 {
    int32 x = 10;

    // without parentheses
    if x > 5 {
        println("x is greater than 5");
    }

    // with parentheses (also valid)
    if (x == 10) {
        println("x is 10");
    }

    ret 0;
}
```

### Else and Else If

```

use std::log::println;

fn main() int32 {
    int32 score = 85;

    if score >= 90 {
        println("Excellent");
    } else if score >= 70 {
        println("Good");
    } else if score >= 50 {
        println("Average");
    } else {
        println("Needs improvement");
    }

    ret 0;
}
// Output:
// Good
```

You can mix parenthesized and non-parenthesized conditions in the same chain:

```
if x == 1 {
    println(1);
} else if (x == 5) {
    println(5);
} else if x == 10 {
    println(10);
} else {
    println(0);
}
```

## For Loops

Lucis has two forms of `for` loops: C-style and for-in.

### C-Style For Loop

```

use std::log::println;

fn main() int32 {
    for int32 i = 0; i < 5; i++ {
        println(i);
    }

    ret 0;
}
// Output:
// 0
// 1
// 2
// 3
// 4
```

### For-In with Ranges

Use `..` for exclusive ranges and `..=` for inclusive ranges:

```

use std::log::println;

fn main() int32 {
    // exclusive: 0, 1, 2, 3, 4
    for int32 i in 0..5 {
        println(i);
    }

    // inclusive: 1, 2, 3, 4, 5
    for int32 i in 1..=5 {
        println(i);
    }

    ret 0;
}
```

### For-In with Arrays

```

use std::log::println;

fn main() int32 {
    []int32 numbers = [10, 20, 30, 40, 50];

    for int32 x in numbers {
        println(x);
    }

    ret 0;
}
// Output:
// 10
// 20
// 30
// 40
// 50
```

## While Loop

The `while` loop repeats as long as a condition is true. Parentheses are optional:

```

use std::log::println;

fn main() int32 {
    int32 count = 0;

    while count < 5 {
        println(count);
        count += 1;
    }

    ret 0;
}
// Output:
// 0
// 1
// 2
// 3
// 4
```

With parentheses:

```
int32 j = 10;
while (j > 7) {
    println(j);
    j -= 1;
}
```

## Do-While Loop

The `do-while` loop executes the body at least once, then checks the condition:

```

use std::log::println;

fn main() int32 {
    int32 count = 100;

    do {
        println(count);
        count += 1;
    } while count < 103;

    ret 0;
}
// Output:
// 100
// 101
// 102
```

## Infinite Loop

The `loop` keyword creates an infinite loop. Use `break` to exit:

```

use std::log::println;

fn main() int32 {
    int32 count = 0;

    loop {
        if count == 5 {
            break;
        }
        println(count);
        count += 1;
    }

    ret 0;
}
// Output:
// 0
// 1
// 2
// 3
// 4
```

## Break and Continue

`break` exits the innermost loop. `continue` skips to the next iteration:

```

use std::log::println;

fn main() int32 {
    // break: stop at 3
    for int32 i in 0..100 {
        if i == 3 {
            break;
        }
        println(i);
    }
    // Output: 0 1 2

    // continue: skip 2 and 4
    for int32 i in 0..6 {
        if i == 2 {
            continue;
        }
        if i == 4 {
            continue;
        }
        println(i);
    }
    // Output: 0 1 3 5

    ret 0;
}
```

`break` and `continue` work in all loop types: `for`, `while`, `do-while`, and `loop`.

## Switch

The `switch` statement matches a value against multiple cases. Parentheses around the value are optional. Each case uses a block with braces — there is no fallthrough:

```

use std::log::println;

fn main() int32 {
    int32 day = 3;

    switch day {
        case 1 {
            println("Monday");
        }
        case 2 {
            println("Tuesday");
        }
        case 3 {
            println("Wednesday");
        }
        default {
            println("Other day");
        }
    }

    ret 0;
}
// Output:
// Wednesday
```

### Multi-Value Cases

A single case can match multiple values separated by commas:

```

use std::log::println;

fn main() int32 {
    int32 category = 5;

    switch category {
        case 1, 2, 3 {
            println("Low");
        }
        case 4, 5, 6 {
            println("Medium");
        }
        case 7, 8, 9 {
            println("High");
        }
        default {
            println("Unknown");
        }
    }

    ret 0;
}
// Output:
// Medium
```

### Switch Without Default

The `default` clause is optional. If no case matches and there is no default, execution continues after the switch:

```
int32 v = 3;
switch v {
    case 1 {
        println(1);
    }
    case 3 {
        println(3);
    }
}
// Output: 3
```

## Try / Catch / Finally

T uses `try`/`catch`/`finally` for error handling with a built-in `Error` type:

```

use std::log::println;

fn main() int32 {
    try {
        println("before throw");
        throw Error { message: "something went wrong" };
        println("this does not execute");
    } catch (Error e) {
        println(e.message);
        println(e.file);
        println(e.line);
        println(e.column);
    } finally {
        println("finally block");
    }

    println("after try/catch");

    ret 0;
}
// Output:
// before throw
// something went wrong
// TryCatchDemo.lc
// 7
// 9
// finally block
// after try/catch
```

### The Error Type

The built-in `Error` struct has these fields:

| Field | Type | Description |
|---|---|---|
| `message` | `string` | Error description |
| `file` | `string` | Source file where the error occurred |
| `line` | `int32` | Line number |
| `column` | `int32` | Column number |

`file`, `line`, and `column` are automatically populated by the compiler when you `throw`.

### Throw

Use `throw` to raise an error from anywhere:

```
throw Error { message: "invalid argument" };
```

### Finally

The `finally` block always executes, whether or not an exception was thrown. It is useful for cleanup:

```
try {
    // work that might fail
} catch (Error e) {
    println(e.message);
} finally {
    // always runs — cleanup goes here
}
```

## See Also

- [Ranges](ranges.md) — Range expressions (`..`, `..=`) in detail
- [Functions](functions.md) — Function declarations and calls
- [Error Handling](error-handling.md) — Detailed error handling guide
- [Variables](variables.md) — Variable scope in blocks
