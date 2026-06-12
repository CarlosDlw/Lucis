# std::regex

Regular expression matching and replacement functions.

## Import

```
use std::regex::{ match, find, findAll, regexReplace, regexSplit };
```

## Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `match` | `(string, string) -> bool` | Test if string matches pattern |
| `find` | `(string, string) -> string` | Find first match |
| `findIndex` | `(string, string) -> int32` | Index of first match (-1 if none) |
| `findAll` | `(string, string) -> vec<string>` | All matches |
| `regexReplace` | `(string, string, string) -> string` | Replace all matches |
| `regexReplaceFirst` | `(string, string, string) -> string` | Replace first match |
| `isValid` | `(string) -> bool` | Check if pattern is valid |
| `regexSplit` | `(string, string) -> vec<string>` | Split string by pattern |

## Example

```
use std::regex::{ match, find, findAll, regexReplace, regexSplit };
use std::log::println;

println(match("hello123", "[a-z]+[0-9]+"));        // true
println(find("abc 123 def", "[0-9]+"));             // 123

vec<string> matches = findAll("a1 b2 c3", "[a-z][0-9]");
// matches: ["a1", "b2", "c3"]

string result = regexReplace("hello world", "[aeiou]", "*");
println(result);                                     // h*ll* w*rld

vec<string> parts = regexSplit("one::two::three", ":+");
// parts: ["one", "two", "three"]
```

## See Also

- [std::string](string.md) — String manipulation
