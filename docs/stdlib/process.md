# std::process

Functions for interacting with the current process and executing external commands.

## Import

```
use std::process::{ exit, exec, execOutput, env, pid, platform };
```

## Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `exit` | `(int32) -> void` | Terminate with exit code |
| `abort` | `() -> void` | Abort immediately |
| `env` | `(string) -> string` | Get environment variable |
| `setEnv` | `(string, string) -> void` | Set environment variable |
| `hasEnv` | `(string) -> bool` | Check if env var exists |
| `exec` | `(string) -> int32` | Execute command, return exit code |
| `execOutput` | `(string) -> string` | Execute command, return output |
| `pid` | `() -> int32` | Current process ID |
| `platform` | `() -> string` | OS name (`linux`, `darwin`, `windows`) |
| `arch` | `() -> string` | CPU architecture (`x86_64`, `aarch64`) |
| `homeDir` | `() -> string` | User home directory |
| `executablePath` | `() -> string` | Path to current executable |

## Example

```
use std::process::{ exit, exec, execOutput, env, pid, platform };
use std::log::println;

println(pid());                    // 12345
println(platform());              // linux
println(env("HOME"));             // /home/user

int32 code = exec("ls -la");
println(code);                     // 0

string output = execOutput("uname -r");
println(output);                   // 6.1.0-generic

exit(0);
```

## See Also

- [std::os](os.md) — Low-level OS primitives
- [std::fs](fs.md) — File system operations
