# std::io

Input functions for reading from stdin.

## Import

```
use std::io::readLine;
use std::io::{ prompt, promptInt, promptFloat, promptBool };
```

## Functions

### Reading

| Function | Signature | Description |
|----------|-----------|-------------|
| `readLine` | `() -> string` | Read a line from stdin |
| `readChar` | `() -> char` | Read a single character |
| `readInt` | `() -> int64` | Read and parse an integer |
| `readFloat` | `() -> float64` | Read and parse a float |
| `readBool` | `() -> bool` | Read and parse a boolean |
| `readAll` | `() -> string` | Read all remaining stdin |
| `readByte` | `() -> int32` | Read a single byte |
| `readLines` | `() -> string` | Read all lines |
| `readNBytes` | `(usize) -> string` | Read n bytes |
| `isEOF` | `() -> bool` | Whether stdin has reached end-of-file |

### Prompting

| Function | Signature | Description |
|----------|-----------|-------------|
| `prompt` | `(string) -> string` | Print message, read string |
| `promptInt` | `(string) -> int64` | Print message, read integer |
| `promptFloat` | `(string) -> float64` | Print message, read float |
| `promptBool` | `(string) -> bool` | Print message, read boolean |
| `readPassword` | `() -> string` | Read without echo |
| `promptPassword` | `(string) -> string` | Print message, read without echo |

### Terminal Detection

| Function | Signature | Description |
|----------|-----------|-------------|
| `isTTY` | `() -> bool` | Whether stdin is a terminal |
| `isStdoutTTY` | `() -> bool` | Whether stdout is a terminal |
| `isStderrTTY` | `() -> bool` | Whether stderr is a terminal |

### Flushing

| Function | Signature | Description |
|----------|-----------|-------------|
| `flush` | `() -> void` | Flush stdout |
| `flushErr` | `() -> void` | Flush stderr |

## Example

```
use std::io::{ prompt, promptInt, isTTY, flush };
use std::log::println;

if isTTY() {
    string name = prompt("Name: ");
    int64 age = promptInt("Age: ");
    println(name);
    println(age);
    flush();
}
```

## See Also

- [std::log](log.md) — Output functions
- [std::fs](fs.md) — File I/O
