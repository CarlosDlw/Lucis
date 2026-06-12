# Standard Library Overview

Lux's standard library provides 23 modules covering I/O, math, strings, file system, networking, cryptography, and more. All modules are imported with the `use` keyword.

---

## Importing

```
use std::log::println;
use std::math::{ sqrt, PI };
```

Individual symbols must be imported explicitly — there is no wildcard import.

> **Note:** `vec`, `map`, and `set` are native keywords — no import required.

---

## Module Index

### Core

| Module | Description |
|--------|-------------|
| [std::log](log.md) | Output: `println`, `print`, `dbg`, `sprintf` |
| [std::io](io.md) | Input: `readLine`, `prompt`, `readInt` |
| [std::fmt](fmt.md) | Formatting: `lpad`, `hex`, `bin`, `commas`, `humanBytes` |
| [std::str](string.md) | String utilities: `split`, `parseInt`, `parseFloat` |
| [std::conv](conv.md) | Type conversions: `itoa`, `atoi`, `toHex`, `toBinary` |

### Collections

`vec<T>`, `map<K,V>`, and `set<T>` are **native keywords** — no import needed. See:

| Reference | Description |
|-----------|-------------|
| [vec<T>](collections/vec.md) | Dynamic array |
| [map<K,V>](collections/map.md) | Hash map |
| [set<T>](collections/set.md) | Hash set |

### System

| Module | Description |
|--------|-------------|
| [std::fs](fs.md) | File system: `readFile`, `writeFile`, `mkdir`, `listDir` |
| [std::path](path.md) | Path manipulation: `join`, `parent`, `extension` |
| [std::process](process.md) | Process control: `exec`, `env`, `pid`, `platform` |
| [std::os](os.md) | OS-level: `getpid`, `hostname`, `errno`, `strerror` |

### Math and Science

| Module | Description |
|--------|-------------|
| [std::math](math.md) | Math functions: `PI`, `sqrt`, `sin`, `pow`, `clamp` |
| [std::random](random.md) | Random numbers: `randInt`, `randFloat`, `randBool` |
| [std::bits](bits.md) | Bit manipulation: `popcount`, `rotl`, `setBit` |

### Time

| Module | Description |
|--------|-------------|
| [std::time](time.md) | Date/time: `now`, `sleep`, `year`, `formatTime` |

### Networking

| Module | Description |
|--------|-------------|
| [std::net](net.md) | TCP/UDP: `tcpConnect`, `tcpSend`, `udpBind` |

### Security

| Module | Description |
|--------|-------------|
| [std::crypto](crypto.md) | Hashing: `sha256String`, `hmacSha256`, `randomBytes` |
| [std::encoding](encoding.md) | Base64/URL: `base64EncodeStr`, `urlEncode` |
| [std::compress](compress.md) | Compression: `gzipCompress`, `deflate` |

### Text Processing

| Module | Description |
|--------|-------------|
| [std::regex](regex.md) | Regex: `match`, `find`, `regexReplace` |
| [std::ascii](ascii.md) | ASCII: `isAlpha`, `isDigit`, `toUpper` |
| [std::hash](hash.md) | Hash functions: `hashString`, `hashInt`, `crc32` |

### Memory and Threading

| Module | Description |
|--------|-------------|
| [std::mem](mem.md) | Memory: `alloc`, `free`, `copy`, `zero` |
| [std::thread](thread.md) | Threading: `cpuCount`, `threadId`, `Task`, `Mutex` |

### Testing

| Module | Description |
|--------|-------------|
| [std::test](test.md) | Testing: `assertEqual`, `assertTrue`, `assertNear` |

---

## See Also

- [Modules](../language/modules.md) — How the `use` import system works
- [Generics](../language/generics.md) — `vec<T>`, `map<K,V>`, `set<T>`
