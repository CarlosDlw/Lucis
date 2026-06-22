# Inline Assembly

Inline assembly allows you to embed raw machine instructions directly in your Lucis code using the `asm` keyword. This is an **unstable** feature intended for low-level systems programming, OS development, and cases where you need precise control over CPU instructions.

> **Warning:** Inline assembly is inherently unsafe. The compiler trusts your assembly strings, operand constraints, and clobber lists. Incorrect usage can silently produce wrong code or crash.

## Syntax

```
asm volatile? "(" template_string ( "," template_string )*
    ( ":" output_list )?
    ( ":" input_list  )?
    ( ":" clobber_list)?
")" ";"
```

The template consists of one or more string literals separated by commas. Multiple strings are concatenated with newlines, allowing each instruction on its own line:

```
asm volatile(
    "movq $2, $0",
    "addq $3, $1"
    : "=r"(out1), "=r"(out2)
    : "r"(a), "r"(b)
    : "cc"
);
```

A single string literal (the old syntax) continues to work:

### Components

| Part | Description |
|---|---|
| `asm` | Keyword that begins an inline assembly statement |
| `volatile` | Optional. Prevents the optimizer from removing or reordering the asm block |
| `template_string` | Assembly instruction template using LLVM `$N` operand references |
| `output_list` | Comma-separated list of output operands: `"constraint"(variable)` |
| `input_list` | Comma-separated list of input operands: `"constraint"(expression)` |
| `clobber_list` | Comma-separated list of clobbered register names as string literals |

### Operand Format

Each operand in the output or input list follows this pattern:

```
"constraint"(operand)
```

- **constraint** — A string literal specifying the register/memory constraint (e.g. `"=r"`, `"r"`, `"m"`, `"{rax}"`)
- **operand** — For outputs: a variable name (must be declared before the asm statement). For inputs: any expression.

## Operand Numbering

Operands are referenced in the template string by `$0`, `$1`, `$2`, etc., in LLVM style:

### Simple case (`=` output + inputs)

| Slot | Kind | Description |
|---|---|---|
| `$0` | Output | Return value from `=r` constraint |
| `$1` | Input | First input operand |
| `$2` | Input | Second input operand |

### `+r` read-write output

The `+r` constraint is internally expanded into a write-only `=r` output plus a matching `"0"` input. This pushes all subsequent input operand slots by one:

| Slot | Kind | Description |
|---|---|---|
| `$0` | Output | Return value from `+r` (write) |
| `$1` | Matching | Same register as `$0` (holds the initial value) |
| `$2` | Input | First user-specified input operand |

Example — `asm("add $2, $0" : "+r"(val) : "r"(y))` adds `y` (`$2`) to `val` (`$0`/`$1`).

### Matching constraint

When an explicit matching constraint like `"0"` is used, the matched input also occupies a slot:

| Slot | Kind | Description |
|---|---|---|
| `$0` | Output | Return value from `=r` |
| `$1` | Matching | Input with `"0"` — shares register with `$0` |
| `$2` | Input | First user input operand |

This is identical to the `+r` layout because both expand to the same internal representation.

## Constraints

Common constraints for x86-64:

| Constraint | Description |
|---|---|
| `"=r"` | Output: any general-purpose register |
| `"+r"` | Read-write: same register for input and output |
| `"r"` | Input: any general-purpose register |
| `"m"` | Memory operand |
| `"0"`, `"1"`, ... | Matching constraint: use same location as operand N |
| `"=rm"` | Output: register or memory |
| `"i"` | Immediate integer operand |
| `"{reg}"` | Physical register constraint (e.g. `"{rax}"`, `"{rdi}"`). **Must use curly braces `{}`** so LLVM targets the specific physical register instead of interpreting individual letters as single-character constraints (such as `r`, `a`, `x`). |

## Clobbers

Clobbers tell the compiler which registers or state the assembly modifies:

| Clobber | Description |
|---|---|
| `"cc"` | Condition codes (flags) |
| `"memory"` | Memory (prevents reordering around the asm) |
| `"rax"`, `"rcx"`, `"rdx"`, ... | Specific register clobbers |

## Examples

### No operands (e.g., NOP)

```
asm volatile("nop");
```

### Intel dialect example

```
asm volatile intel("mov $0, 42" : "=r"(x));
```

### Single output

```
int64 result = 0;
asm("mov $$42, $0" : "=r"(result) : : "cc");
// result == 42
```

### Input and output

```
int64 a = 10;
int64 result = 0;
asm("mov $1, $0" : "=r"(result) : "r"(a) : "cc");
// result == 10
```

### Addition with matching constraint

The matching constraint `"0"` tells the compiler that this input shares the same register as output `$0`. This is equivalent to a read-write operand:

```
int64 x = 5;
int64 y = 10;
asm volatile("add $2, $0" : "=r"(x) : "0"(x), "r"(y) : "cc");
// x == 15
```

Operand numbering: `$0` = output (x), `$1` = input matching `"0"` (same register as x), `$2` = input y.

### Read-write operand (`"+r"`)

The `+r` constraint marks a variable that is both read and written in one output slot — no separate matching input needed:

```
int64 val = 7;
asm("shl $$2, $0" : "+r"(val) : : "cc");
// val == 28
```

With additional inputs, remember that `+r` consumes slot `$0` and its matching input takes `$1`. The first user input is at `$2`:

```
int64 val = 7;
int64 y = 10;
asm("add $2, $0" : "+r"(val) : "r"(y) : "cc");
// val == 17
```

### Multiple clobbers

```
int64 result = 0;
asm("mov $$1, $0" : "=r"(result) : : "rcx", "r11", "cc", "memory");
```

### As expression

Inline asm can be used as an expression that returns a value. The output variable is both stored and returned:

```
int64 x = 0;
int64 result = asm("movq $$42, $0" : "=r"(x) : : "cc");
// x == 42, result == 42
```

When the output has no variable binding, asm returns the value directly. Use with `auto` for type inference (defaults to `int64`):

```
auto x = asm("movq $$42, $0" : "=r");
// x is int64, x == 42
// (the output register value is returned directly, no variable binding needed)
```

Or with an explicit type:

```
int32 y = asm("movq $$100, $0" : "=r");
// y == 100 (coerced from int64 to int32)
```

The asm expression can be composed with other operators:

```
int64 y = 5;
int64 sixtyFour = 64;
int64 z = asm("addq $2, $0" : "+r"(y) : "r"(sixtyFour) : "cc") + 100;
// y == 69, z == 169
```

And used directly as a function argument:

```
int64 a = 0;
printf("direct = {d}\n", asm("movq $$99, $0" : "=r"(a) : : "cc"));
```

### Multiple outputs

Inline asm supports multiple output operands. LLVM returns a struct value which is then split into individual variables:

```
int64 out1 = 0;
int64 out2 = 0;

asm volatile(
    "movq $2, $0",
    "addq $3, $1"
    : "=r"(out1), "=r"(out2)
    : "r"(10), "r"(20)
    : "cc"
);
// out1 == 10, out2 == 20
```

The template string can be split across multiple string literals for readability. Each string becomes a separate line in the assembly output.

When using multiple outputs, each output variable is stored independently. For expression form, the first output determines the return type (multi-output expressions are intended for statement form).

Multiple outputs work with the `+r` read-write constraint and matching constraints too:

### System Calls (Linux x86-64)

When invoking system calls, you must place arguments in specific physical registers. You do this by enclosing register names in curly braces `{}`.

#### Using `cstring` (C-compatible null-terminated strings)

```lucis
int64 fd = 1;          // stdout
cstring msg = c"Hello, world!\n";
int64 len = 14;

int64 bytes_written = asm volatile(
    "syscall"
    : "={rax}"
    : "{rax}"(1), "{rdi}"(fd), "{rsi}"(msg), "{rdx}"(len)
    : "rcx", "r11", "memory"
);
// bytes_written == 14
```

#### Using native `string` (Lucis string)

Lucis native `string` values are structures containing `{ ptr, len }`. You can use their `.ptr()` and `.len()` methods to obtain the correct values for physical registers:

```lucis
int64 fd = 1;          // stdout
string msg = "Hello, from lucis!\n";

int64 bytes_written = asm volatile(
    "syscall"
    : "={rax}"
    : "{rax}"(1), "{rdi}"(fd), "{rsi}"(msg.ptr()), "{rdx}"(msg.len())
    : "rcx", "r11", "memory"
);
// bytes_written == 19
```

## Volatile

Use `volatile` when the asm block has side effects that the optimizer must not remove:

```
asm volatile("wbinvd");
asm volatile("outl %0, $0xcf9" : : "a"(0x04));
```

Without `volatile`, the optimizer may eliminate the asm block if it determines the output values are unused.

## Dialect

Inline asm supports both **AT&T** (default) and **Intel** syntax. Use the `intel` keyword to switch dialects per-block:

```
asm intel("mov $0, 42" : "=r"(x));
asm volatile intel("add $0, $2" : "+r"(val) : "r"(inc) : "cc");
```

### AT&T (default)

- `mov $42, %rax` → `movq $$42, $0` (use `q`/`l`/`w`/`b` size suffixes)
- Register references are implicit via operand numbering
- Size suffixes (`movq`, `addl`, `cmpb`) are required when the assembler cannot infer operand sizes
- Immediate values prefixed with `$`, registers prefixed with `%`

### Intel (with `intel` keyword)

- `mov rax, 42` → `mov $0, 42` (destination first)
- `add rax, rbx` → `add $0, $1`
- No size suffixes needed (assembler infers from operands)
- No `$` or `%` prefixes on registers/immediates

### Comparison

| AT&T | Intel |
|---|---|
| `movq $$42, $0` | `mov $0, 42` |
| `addq $1, $0` | `add $0, 1` |
| `shlq $$2, $0` | `shl $0, 2` |

## Unique Labels (`%=`)

Use `%=` in the template string to generate a unique number per asm block. This is useful for defining local labels that won't collide when the same asm is inlined or instantiated multiple times:

```
asm volatile(
    "test $1, $1",
    "jz label_%= ",
    "movq $2, $0",
    "label_%=:"
    : "=r"(x)
    : "r"(val), "r"(42)
    : "cc"
);
```

The `%=` is translated internally to LLVM's `${:uid}` syntax and replaced with an incrementing counter during codegen. Each asm block gets its own unique number.

## `asm goto` With Labels

`asm goto` allows the inline assembly to branch to a label in the current function. Labels are defined as `name:` followed by a statement. The `goto` keyword and label list come after clobbers (the 4th colon section):

```
fn main() int32 {
  int64 result = 0i64;

  asm volatile goto(
    "test $2, $2",
    "jz ${3:l}",
    "movq $1, $0"
    : "=r"(result)
    : "r"(42i64), "r"(value)
    : "cc"
    : skip_label
  );

  // Fallthrough: ran when condition was false

skip_label:
  printf("result = {d}\n", result);
  return 0;
}
```

Key points:
- The `goto` keyword is placed after `volatile` (if present) and before `(`.
- Labels are listed after the 4th colon (`: label1, label2`), one colon section after clobbers.
- In the template, labels are referenced as `${N:l}` where `N` is the operand slot (slots count outputs, then inputs, then labels). For example, with one output (`$0`) and one input (`$1`), the first label is `$2`.
- Labels can be forward-referenced (defined after the `asm goto` that uses them).
- Output values are only valid on the fallthrough path; using them on an indirect branch is undefined behavior.
- `asm goto` implicitly implies `volatile`.

Label definition syntax:

```
label_name:
    statement
```

Labels must be unique within a function.

## Comparison with GCC Syntax

| GCC | Lucis (LLVM) |
|---|---|
| `%0`, `%1` | `$0`, `$1` |
| `$42` | `$$42` |
| `"rax"` output | `"={rax}"` (requires curly braces `{}`) |
| `"rax"` input | `"{rax}"` (requires curly braces `{}`) |
| `%lN` | `${N:l}` |
| `%=` | `%=` (translated to `${:uid}` internally) |
| `"cc"` clobber | `"cc"` clobber (wrapped as `~{cc}` internally) |
| `"memory"` clobber | `"memory"` clobber |

## Limitations

- Physical register multi-output constraints (`"=a"` + `"=d"` with struct return) may have codegen issues on some LLVM versions; use `"=r"` outputs instead

## When to Use Inline Assembly

- Implementing CPU-specific features (CPUID, RDTSC, etc.)
- System call invocations without the standard library
- Interrupt and exception handling
- Performance-critical hot paths where the compiler's codegen is suboptimal
- Accessing special-purpose registers (CR0, MSRs, etc.)

For most use cases, prefer the standard library or `lucis::sys` intrinsics over raw assembly.
