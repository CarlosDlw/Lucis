# Ranges

This page covers range expressions in Lucis — the `..` and `..=` operators used in loops and list comprehensions.

## Overview

Ranges represent a sequence of integer values between a start and end. Lucis has two range operators:

| Operator | Name | Description |
|---|---|---|
| `..` | Exclusive range | Includes start, excludes end |
| `..=` | Inclusive range | Includes both start and end |

## Exclusive Range (`..`)

`start..end` produces values from `start` up to but **not** including `end`:

```

use std::log::println;

fn main() int32 {
    // 0..5 produces: 0, 1, 2, 3, 4
    for int32 i in 0..5 {
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

## Inclusive Range (`..=`)

`start..=end` produces values from `start` up to **and** including `end`:

```

use std::log::println;

fn main() int32 {
    // 1..=5 produces: 1, 2, 3, 4, 5
    for int32 i in 1..=5 {
        println(i);
    }

    ret 0;
}
// Output:
// 1
// 2
// 3
// 4
// 5
```

## Usage in For-In Loops

Ranges are most commonly used with `for..in` loops to iterate over a sequence of integers:

```

use std::log::println;

fn main() int32 {
    // count from 0 to 9
    for int32 i in 0..10 {
        println(i);
    }

    // count from 1 to 10 (inclusive)
    for int32 i in 1..=10 {
        println(i);
    }

    // countdown is not directly supported — use a while loop instead
    int32 i = 5;
    while i > 0 {
        println(i);
        i -= 1;
    }

    ret 0;
}
```

## Usage in List Comprehensions

Ranges are the primary source for list comprehensions:

```

extern void printf(cstring fmt, ...);

fn main() int32 {
    // basic: collect range into array
    [4]int32 nums = [x | for int32 x in 0..4];
    printf(c"%d %d %d %d\n", nums[0], nums[1], nums[2], nums[3]);
    // Output: 0 1 2 3

    // transform: square each value
    [4]int32 squares = [x * x | for int32 x in 0..4];
    printf(c"%d %d %d %d\n", squares[0], squares[1], squares[2], squares[3]);
    // Output: 0 1 4 9

    // inclusive range in comprehension
    [5]int32 incl = [x | for int32 x in 1..=5];
    printf(c"%d %d %d %d %d\n", incl[0], incl[1], incl[2], incl[3], incl[4]);
    // Output: 1 2 3 4 5

    // with filter
    [8]int32 evens = [x | for int32 x in 0..8 if x % 2 == 0];
    printf(c"%d %d %d %d\n", evens[0], evens[1], evens[2], evens[3]);
    // Output: 0 2 4 6

    ret 0;
}
```

See [Arrays — List Comprehensions](arrays.md) for more examples.

## Operator Precedence

Range operators have very low precedence (level 14 out of 16). They bind looser than arithmetic, comparison, and logical operators. In practice this means you rarely need parentheses:

```
for int32 i in 0..count * 2 {
    // 0..(count * 2), not (0..count) * 2
}
```

## See Also

- [Control Flow](control-flow.md) — For loops and iteration
- [Arrays](arrays.md) — List comprehensions
- [Operators](operators.md) — Full precedence table
