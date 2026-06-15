# Hello World

This page walks you through writing, compiling, and running your first Lucis program.

## Your First Program

Create a file called `hello.lc`:

```

use std::log::println;

fn main() int32 {
    println("Hello, world!");
    ret 0;
}
```

## Compile and Run

Compile the program to a native binary and execute it:

```bash
./build/lucis hello.lc ./hello
./hello
```

Output:

```
Hello, world!
```

You can also view the generated LLVM IR by omitting the output argument:

```bash
./build/lucis hello.lc
```

## Understanding the Code

Every Lucis program has three essential parts:

### 1. Namespace Declaration

```
```

Every `.lc` file **must** begin with a namespace declaration. This names the module and is used for imports across files.

### 2. Imports

```
use std::log::println;
```

The `use` keyword imports functions from the standard library or other modules. Here, `println` is imported from `std::log`, which provides formatted output to the console.

You can import individual functions or entire modules:

```
use std::log::println;       // import a single function
use std::math;               // import the whole module (access via math::sqrt, etc.)
```

### 3. The `main` Function

```
fn main() int32 {
    println("Hello, world!");
    ret 0;
}
```

- The entry point is `fn main() int32`, or `fn main(vec<string> args) int32` to receive command-line arguments.
- Functions use `fn` keyword syntax: the return type comes after the parameter list.
- `ret 0;` returns an exit code. A zero value signals success.
- `ret` is required — the compiler does not auto-insert a return.

## More Examples

### Variables and Arithmetic

```

use std::log::println;

fn main() int32 {
    int32 width = 10;
    int32 height = 5;
    int32 area = width * height;

    println("Area:");
    println(area);

    ret 0;
}
```

Output:

```
Area:
50
```

### Multiple Types

```

use std::log::println;

fn main() int32 {
    int32 age = 25;
    float64 temperature = 36.6;
    bool is_active = true;
    string name = "Alice";
    char letter = 'A';

    println(name);
    println(age);
    println(temperature);
    println(is_active);
    println(letter);

    ret 0;
}
```

Output:

```
Alice
25
36.600000
1
A
```

### Functions

```

use std::log::println;

fn add(int32 a, int32 b) int32 {
    ret a + b;
}

fn circle_area(float64 radius) float64 {
    ret 3.14159 * radius * radius;
}

fn main() int32 {
    int32 sum = add(3, 7);
    println(sum);

    float64 area = circle_area(5.0);
    println(area);

    ret 0;
}
```

Output:

```
10
78.539750
```

### Control Flow

```

use std::log::println;

fn main() int32 {
    int32 score = 85;

    if score >= 90 {
        println("Excellent");
    } else if score >= 70 {
        println("Good");
    } else {
        println("Needs improvement");
    }

    for int32 i in 1..=5 {
        println(i);
    }

    ret 0;
}
```

Output:

```
Good
1
2
3
4
5
```

## File Structure Summary

```

use std::module::func;   // imports — at the top of the file

// function definitions

fn main() int32 {           // entry point
    // your code
    ret 0;               // required return
}
```

## See Also

- [Installation](installation.md) — Build the compiler from source
- [CLI Usage](cli-usage.md) — Compiler flags of options
- [Types](../language/types.md) — All built-in types in T
- [Variables](../language/variables.md) — Variable declaration and scope
