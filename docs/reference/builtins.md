# Builtins

Complete list of all builtin functions available through `use std::*` imports. Functions marked **(P)** are polymorphic (work on multiple types). Functions marked **(V)** are variadic (accept extra arguments).

---

## std::log

```
use std::log::{println, print, eprintln, eprint, dbg, sprintf};
```

| Function | Signature | Description |
|----------|-----------|-------------|
| `println` **(P)** | `(_any) → void` | Print value with newline to stdout |
| `print` **(P)** | `(_any) → void` | Print value without newline to stdout |
| `eprintln` **(P)** | `(_any) → void` | Print value with newline to stderr |
| `eprint` **(P)** | `(_any) → void` | Print value without newline to stderr |
| `dbg` **(P)** | `(_any) → _any` | Print value to stderr and return it |
| `sprintf` **(P)(V)** | `(string, ...) → string` | Format string with arguments |

`println`, `print`, `eprintln`, `eprint`, and `dbg` also accept C strings (`*char`) directly.

---

## std::io

```
use std::io::{readLine, readInt, prompt, flush};
```

| Function | Signature | Description |
|----------|-----------|-------------|
| `readLine` | `() → string` | Read a line from stdin |
| `readChar` | `() → char` | Read a single character |
| `readInt` | `() → int64` | Read and parse an integer |
| `readFloat` | `() → float64` | Read and parse a float |
| `readBool` | `() → bool` | Read and parse a boolean |
| `readAll` | `() → string` | Read all of stdin |
| `readByte` | `() → uint8` | Read a single byte |
| `readLines` | `() → vec<string>` | Read all lines into a vector |
| `readNBytes` | `(usize) → vec<uint8>` | Read N bytes |
| `readPassword` | `() → string` | Read without echo |
| `prompt` | `(string) → string` | Display prompt, read line |
| `promptInt` | `(string) → int64` | Display prompt, read integer |
| `promptFloat` | `(string) → float64` | Display prompt, read float |
| `promptBool` | `(string) → bool` | Display prompt, read boolean |
| `promptPassword` | `(string) → string` | Display prompt, read without echo |
| `isEOF` | `() → bool` | Check if stdin is at EOF |
| `flush` | `() → void` | Flush stdout |
| `flushErr` | `() → void` | Flush stderr |
| `isTTY` | `() → bool` | Check if stdin is a terminal |
| `isStdoutTTY` | `() → bool` | Check if stdout is a terminal |
| `isStderrTTY` | `() → bool` | Check if stderr is a terminal |

---

## std::math

```
use std::math::{sqrt, sin, cos, PI, abs, min, max};
```

### Constants

| Name | Type | Value |
|------|------|-------|
| `PI` | `float64` | 3.14159265358979... |
| `E` | `float64` | 2.71828182845904... |
| `TAU` | `float64` | 6.28318530717958... |
| `INF` | `float64` | Positive infinity |
| `NAN` | `float64` | Not a number |
| `INT32_MAX` | `int32` | 2,147,483,647 |
| `INT32_MIN` | `int32` | -2,147,483,648 |
| `INT64_MAX` | `int64` | 9,223,372,036,854,775,807 |
| `INT64_MIN` | `int64` | -9,223,372,036,854,775,808 |
| `UINT32_MAX` | `uint32` | 4,294,967,295 |
| `UINT64_MAX` | `uint64` | 18,446,744,073,709,551,615 |

### Trigonometry

| Function | Signature |
|----------|-----------|
| `sin` | `(float64) → float64` |
| `cos` | `(float64) → float64` |
| `tan` | `(float64) → float64` |
| `asin` | `(float64) → float64` |
| `acos` | `(float64) → float64` |
| `atan` | `(float64) → float64` |
| `atan2` | `(float64, float64) → float64` |
| `sinh` | `(float64) → float64` |
| `cosh` | `(float64) → float64` |
| `tanh` | `(float64) → float64` |

### Exponential and Logarithm

| Function | Signature |
|----------|-----------|
| `sqrt` | `(float64) → float64` |
| `cbrt` | `(float64) → float64` |
| `exp` | `(float64) → float64` |
| `exp2` | `(float64) → float64` |
| `ln` | `(float64) → float64` |
| `log2` | `(float64) → float64` |
| `log10` | `(float64) → float64` |
| `pow` | `(float64, float64) → float64` |
| `hypot` | `(float64, float64) → float64` |

### Rounding

| Function | Signature |
|----------|-----------|
| `ceil` | `(float64) → float64` |
| `floor` | `(float64) → float64` |
| `round` | `(float64) → float64` |
| `trunc` | `(float64) → float64` |

### Polymorphic Numeric

| Function | Signature | Description |
|----------|-----------|-------------|
| `abs` **(P)** | `(_numeric) → _numeric` | Absolute value |
| `min` **(P)** | `(_numeric, _numeric) → _numeric` | Minimum of two values |
| `max` **(P)** | `(_numeric, _numeric) → _numeric` | Maximum of two values |
| `clamp` **(P)** | `(_numeric, _numeric, _numeric) → _numeric` | Clamp to range |

### Interpolation and Mapping

| Function | Signature |
|----------|-----------|
| `lerp` | `(float64, float64, float64) → float64` |
| `map` | `(float64, float64, float64, float64, float64) → float64` |
| `toRadians` | `(float64) → float64` |
| `toDegrees` | `(float64) → float64` |

### Float Checks

| Function | Signature |
|----------|-----------|
| `isNaN` | `(float64) → bool` |
| `isInf` | `(float64) → bool` |
| `isFinite` | `(float64) → bool` |

---

## std::str

```
use std::str::{contains, split, replace, trim, parseInt};
```

### Search

| Function | Signature |
|----------|-----------|
| `contains` | `(string, string) → bool` |
| `startsWith` | `(string, string) → bool` |
| `endsWith` | `(string, string) → bool` |
| `indexOf` | `(string, string) → int64` |
| `lastIndexOf` | `(string, string) → int64` |
| `count` | `(string, string) → usize` |

### Transform

Methods and functions in this section that return `string` produce a new owned buffer.
Store the result and release it with `freeStr(...)` when you are done using it.

| Function | Signature |
|----------|-----------|
| `toUpper` **(P)** | `(_any) → _any` |
| `toLower` **(P)** | `(_any) → _any` |
| `trim` | `(string) → string` |
| `trimLeft` | `(string) → string` |
| `trimRight` | `(string) → string` |
| `reverse` | `(string) → string` |
| `repeat` | `(string, usize) → string` |

### Replace

| Function | Signature |
|----------|-----------|
| `replace` | `(string, string, string) → string` |
| `replaceFirst` | `(string, string, string) → string` |

### Padding

| Function | Signature |
|----------|-----------|
| `padLeft` | `(string, usize, char) → string` |
| `padRight` | `(string, usize, char) → string` |

### Extraction

| Function | Signature |
|----------|-----------|
| `substring` | `(string, usize, usize) → string` |
| `charAt` | `(string, usize) → char` |
| `slice` | `(string, int64, int64) → string` |

### Parsing

| Function | Signature |
|----------|-----------|
| `parseInt` | `(string) → int64` |
| `parseIntRadix` | `(string, uint32) → int64` |
| `parseFloat` | `(string) → float64` |

### Split and Join

| Function | Signature |
|----------|-----------|
| `split` | `(string, string) → vec<string>` |
| `splitN` | `(string, string, usize) → vec<string>` |
| `joinVec` | `(vec<string>, string) → string` |
| `lines` | `(string) → vec<string>` |
| `chars` | `(string) → vec<char>` |
| `fromChars` | `(vec<char>) → string` |
| `toBytes` | `(string) → vec<uint8>` |
| `fromBytes` | `(vec<uint8>) → string` |

### Conversion

| Function | Signature |
|----------|-----------|
| `fromCharCode` | `(int32) → char` |

---

## std::conv

```
use std::conv::{itoa, atoi, toHex, toBinary};
```

| Function | Signature | Description |
|----------|-----------|-------------|
| `itoa` | `(int64) → string` | Integer to string |
| `itoaRadix` | `(int64, uint32) → string` | Integer to string in given base |
| `utoa` | `(uint64) → string` | Unsigned to string |
| `ftoa` | `(float64) → string` | Float to string |
| `ftoaPrecision` | `(float64, uint32) → string` | Float to string with precision |
| `atoi` | `(string) → int64` | String to integer |
| `atof` | `(string) → float64` | String to float |
| `toHex` | `(uint64) → string` | To hexadecimal string |
| `toOctal` | `(uint64) → string` | To octal string |
| `toBinary` | `(uint64) → string` | To binary string |
| `fromHex` | `(string) → uint64` | Parse hex string |
| `charToInt` | `(char) → int32` | Character to integer |
| `intToChar` | `(int32) → char` | Integer to character |

---

## std::fmt

```
use std::fmt::{hex, fixed, commas, humanBytes};
```

| Function | Signature | Description |
|----------|-----------|-------------|
| `lpad` | `(string, usize, char) → string` | Left-pad string |
| `rpad` | `(string, usize, char) → string` | Right-pad string |
| `center` | `(string, usize, char) → string` | Center string |
| `hex` | `(uint64) → string` | Format as hex |
| `hexUpper` | `(uint64) → string` | Format as uppercase hex |
| `oct` | `(uint64) → string` | Format as octal |
| `bin` | `(uint64) → string` | Format as binary |
| `fixed` | `(float64, uint32) → string` | Format float with fixed precision |
| `scientific` | `(float64) → string` | Format in scientific notation |
| `humanBytes` | `(uint64) → string` | Format bytes (e.g., "1.5 MB") |
| `commas` | `(int64) → string` | Format with comma separators |
| `percent` | `(float64) → string` | Format as percentage |

---

## std::fs

```
use std::fs::{readFile, writeFile, exists, mkdir, listDir};
```

| Function | Signature | Description |
|----------|-----------|-------------|
| `readFile` | `(string) → string` | Read entire file as string |
| `writeFile` | `(string, string) → void` | Write string to file |
| `appendFile` | `(string, string) → void` | Append string to file |
| `exists` | `(string) → bool` | Check if path exists |
| `isFile` | `(string) → bool` | Check if path is a file |
| `isDir` | `(string) → bool` | Check if path is a directory |
| `remove` | `(string) → bool` | Delete a file |
| `removeDir` | `(string) → bool` | Delete a directory |
| `rename` | `(string, string) → bool` | Rename/move a file |
| `mkdir` | `(string) → bool` | Create a directory |
| `mkdirAll` | `(string) → bool` | Create directories recursively |
| `listDir` | `(string) → vec<string>` | List directory contents |
| `readBytes` | `(string) → vec<uint8>` | Read file as bytes |
| `writeBytes` | `(string, vec<uint8>) → void` | Write bytes to file |
| `fileSize` | `(string) → int64` | Get file size in bytes |
| `cwd` | `() → string` | Current working directory |
| `setCwd` | `(string) → bool` | Change working directory |
| `tempDir` | `() → string` | System temp directory |

---

## std::path

```
use std::path::{join, parent, extension, isAbsolute};
```

| Function | Signature | Description |
|----------|-----------|-------------|
| `join` | `(string, string) → string` | Join two path segments |
| `joinAll` | `(vec<string>) → string` | Join multiple segments |
| `parent` | `(string) → string` | Parent directory |
| `fileName` | `(string) → string` | File name with extension |
| `stem` | `(string) → string` | File name without extension |
| `extension` | `(string) → string` | File extension |
| `isAbsolute` | `(string) → bool` | Starts with `/` |
| `isRelative` | `(string) → bool` | Does not start with `/` |
| `normalize` | `(string) → string` | Resolve `.` and `..` |
| `toAbsolute` | `(string) → string` | Convert to absolute path |
| `separator` | `() → char` | Platform path separator |
| `withExtension` | `(string, string) → string` | Replace extension |
| `withFileName` | `(string, string) → string` | Replace file name |

---

## std::process

```
use std::process::{exit, env, exec, platform};
```

| Function | Signature | Description |
|----------|-----------|-------------|
| `exit` | `(int32) → void` | Exit with code |
| `abort` | `() → void` | Abort immediately |
| `env` | `(string) → string` | Get environment variable |
| `setEnv` | `(string, string) → void` | Set environment variable |
| `hasEnv` | `(string) → bool` | Check if env var exists |
| `exec` | `(string) → int32` | Execute command, return exit code |
| `execOutput` | `(string) → string` | Execute command, return output |
| `pid` | `() → int32` | Current process ID |
| `platform` | `() → string` | Platform name |
| `arch` | `() → string` | CPU architecture |
| `homeDir` | `() → string` | User home directory |
| `executablePath` | `() → string` | Path to current executable |

---

## std::os

```
use std::os::{getpid, hostname, errno, strerror};
```

| Function | Signature | Description |
|----------|-----------|-------------|
| `getpid` | `() → int32` | Process ID |
| `getppid` | `() → int32` | Parent process ID |
| `getuid` | `() → uint32` | User ID |
| `getgid` | `() → uint32` | Group ID |
| `hostname` | `() → string` | System hostname |
| `pageSize` | `() → usize` | Memory page size |
| `errno` | `() → int32` | Last error number |
| `strerror` | `(int32) → string` | Error number to message |
| `kill` | `(int32, int32) → int32` | Send signal to process |
| `dup` | `(int32) → int32` | Duplicate file descriptor |
| `dup2` | `(int32, int32) → int32` | Duplicate to specific fd |
| `closeFd` | `(int32) → int32` | Close file descriptor |

---

## std::time

```
use std::time::{now, sleep, timestamp, elapsed};
```

| Function | Signature | Description |
|----------|-----------|-------------|
| `now` | `() → uint64` | Current time in milliseconds |
| `nowNanos` | `() → uint64` | Current time in nanoseconds |
| `nowMicros` | `() → uint64` | Current time in microseconds |
| `sleep` | `(uint64) → void` | Sleep for milliseconds |
| `sleepMicros` | `(uint64) → void` | Sleep for microseconds |
| `clock` | `() → uint64` | CPU clock ticks |
| `year` | `() → int32` | Current year |
| `month` | `() → int32` | Current month (1-12) |
| `day` | `() → int32` | Current day (1-31) |
| `hour` | `() → int32` | Current hour (0-23) |
| `minute` | `() → int32` | Current minute (0-59) |
| `second` | `() → int32` | Current second (0-59) |
| `weekday` | `() → int32` | Day of week (0=Sunday) |
| `timestamp` | `() → string` | ISO 8601 timestamp |
| `elapsed` | `(uint64) → uint64` | Milliseconds since given time |
| `formatTime` | `(uint64, string) → string` | Format timestamp |
| `parseTime` | `(string, string) → uint64` | Parse timestamp string |

---

## std::random

```
use std::random::{seed, seedTime, randInt, randIntRange, randFloat};
```

| Function | Signature | Description |
|----------|-----------|-------------|
| `seed` | `(uint64) → void` | Seed RNG with value |
| `seedTime` | `() → void` | Seed RNG with current time |
| `randInt` | `() → int64` | Random integer |
| `randIntRange` | `(int64, int64) → int64` | Random integer in [min, max] |
| `randUint` | `() → uint64` | Random unsigned integer |
| `randFloat` | `() → float64` | Random float in [0, 1) |
| `randFloatRange` | `(float64, float64) → float64` | Random float in [min, max) |
| `randBool` | `() → bool` | Random boolean |
| `randChar` | `() → char` | Random printable character |
| `uuid_v4` | `() → string` | Random UUID v4 string |

---

## std::mem

```
use std::mem::{alloc, free, copy, zero};
```

| Function | Signature | Description |
|----------|-----------|-------------|
| `alloc` | `(usize) → *void` | Allocate bytes |
| `allocZeroed` | `(usize) → *void` | Allocate zeroed bytes |
| `realloc` | `(*void, usize) → *void` | Resize allocation |
| `free` | `(*void) → void` | Free allocation |
| `copy` | `(*void, *void, usize) → void` | Copy bytes (src, dst, len) |
| `move` | `(*void, *void, usize) → void` | Move bytes (handles overlap) |
| `set` | `(*void, uint8, usize) → void` | Fill memory with byte |
| `zero` | `(*void, usize) → void` | Zero out memory |
| `compare` | `(*void, *void, usize) → int32` | Compare memory regions |

---

## std::hash

```
use std::hash::{hashString, hashInt, crc32};
```

| Function | Signature | Description |
|----------|-----------|-------------|
| `hashString` | `(string) → uint64` | Hash a string |
| `hashInt` | `(int64) → uint64` | Hash an integer |
| `hashCombine` | `(uint64, uint64) → uint64` | Combine two hashes |
| `hashBytes` | `(vec<uint8>) → uint64` | Hash a byte vector |
| `crc32` | `(vec<uint8>) → uint32` | CRC32 checksum |

---

## std::bits

```
use std::bits::{popcount, ctz, clz, isPow2, setBit, testBit};
```

| Function | Signature | Description |
|----------|-----------|-------------|
| `popcount` | `(uint64) → uint32` | Count set bits |
| `ctz` | `(uint64) → uint32` | Count trailing zeros |
| `clz` | `(uint64) → uint32` | Count leading zeros |
| `rotl` | `(uint64, uint32) → uint64` | Rotate left |
| `rotr` | `(uint64, uint32) → uint64` | Rotate right |
| `bswap` | `(uint64) → uint64` | Byte swap |
| `bitReverse` | `(uint64) → uint64` | Reverse all bits |
| `isPow2` | `(uint64) → bool` | Is power of two |
| `nextPow2` | `(uint64) → uint64` | Next power of two |
| `extractBits` | `(uint64, uint32, uint32) → uint64` | Extract bit range |
| `setBit` | `(uint64, uint32) → uint64` | Set a bit |
| `clearBit` | `(uint64, uint32) → uint64` | Clear a bit |
| `toggleBit` | `(uint64, uint32) → uint64` | Toggle a bit |
| `testBit` | `(uint64, uint32) → bool` | Test a bit |

---

## std::ascii

```
use std::ascii::{isAlpha, isDigit, isUpper, isLower};
```

| Function | Signature | Description |
|----------|-----------|-------------|
| `isAlpha` | `(char) → bool` | Is alphabetic |
| `isDigit` | `(char) → bool` | Is digit (0-9) |
| `isAlphaNum` | `(char) → bool` | Is alphanumeric |
| `isUpper` | `(char) → bool` | Is uppercase |
| `isLower` | `(char) → bool` | Is lowercase |
| `isWhitespace` | `(char) → bool` | Is whitespace |
| `isPrintable` | `(char) → bool` | Is printable |
| `isControl` | `(char) → bool` | Is control character |
| `isHexDigit` | `(char) → bool` | Is hex digit (0-9, a-f) |
| `isAscii` | `(char) → bool` | Is ASCII (0-127) |
| `toDigit` | `(char) → int32` | Character to digit value |
| `fromDigit` | `(int32) → char` | Digit value to character |

---

## std::regex

```
use std::regex::{match, find, findAll, regexReplace};
```

| Function | Signature | Description |
|----------|-----------|-------------|
| `match` | `(string, string) → bool` | Test if pattern matches |
| `find` | `(string, string) → string` | Find first match |
| `findIndex` | `(string, string) → int64` | Index of first match |
| `findAll` | `(string, string) → vec<string>` | Find all matches |
| `regexReplace` | `(string, string, string) → string` | Replace all matches |
| `regexReplaceFirst` | `(string, string, string) → string` | Replace first match |
| `regexSplit` | `(string, string) → vec<string>` | Split by pattern |
| `isValid` | `(string) → bool` | Check if pattern is valid |

---

## std::encoding

```
use std::encoding::{base64EncodeStr, base64DecodeStr, urlEncode, urlDecode};
```

| Function | Signature | Description |
|----------|-----------|-------------|
| `base64EncodeStr` | `(string) → string` | Encode string to Base64 |
| `base64DecodeStr` | `(string) → string` | Decode Base64 to string |
| `base64Encode` | `(vec<uint8>) → string` | Encode bytes to Base64 |
| `base64Decode` | `(string) → vec<uint8>` | Decode Base64 to bytes |
| `urlEncode` | `(string) → string` | URL-encode string |
| `urlDecode` | `(string) → string` | URL-decode string |

---

## std::crypto

```
use std::crypto::{sha256String, md5String, randomBytes};
```

| Function | Signature | Description |
|----------|-----------|-------------|
| `md5String` | `(string) → string` | MD5 hash of string |
| `sha1String` | `(string) → string` | SHA-1 hash of string |
| `sha256String` | `(string) → string` | SHA-256 hash of string |
| `sha512String` | `(string) → string` | SHA-512 hash of string |
| `md5` | `(vec<uint8>) → string` | MD5 hash of bytes |
| `sha1` | `(vec<uint8>) → string` | SHA-1 hash of bytes |
| `sha256` | `(vec<uint8>) → string` | SHA-256 hash of bytes |
| `sha512` | `(vec<uint8>) → string` | SHA-512 hash of bytes |
| `hmacSha256` | `(vec<uint8>, vec<uint8>) → vec<uint8>` | HMAC-SHA256 |
| `randomBytes` | `(usize) → vec<uint8>` | Cryptographic random bytes |

---

## std::compress

```
use std::compress::{gzipCompress, gzipDecompress, deflate, inflate};
```

| Function | Signature | Description |
|----------|-----------|-------------|
| `gzipCompress` | `(string) → string` | Gzip compress |
| `gzipDecompress` | `(string) → string` | Gzip decompress |
| `deflate` | `(string) → string` | Deflate compress |
| `inflate` | `(string) → string` | Inflate decompress |
| `compressLevel` | `(string, int32) → string` | Compress with level (1-9) |

---

## std::net

```
use std::net::{tcpConnect, tcpSend, tcpRecv, close};
```

| Function | Signature | Description |
|----------|-----------|-------------|
| `tcpConnect` | `(string, uint16) → int32` | Connect to host:port |
| `tcpListen` | `(string, uint16) → int32` | Listen on host:port |
| `tcpAccept` | `(int32) → int32` | Accept connection |
| `tcpSend` | `(int32, string) → int64` | Send data |
| `tcpRecv` | `(int32, usize) → string` | Receive data |
| `udpBind` | `(string, uint16) → int32` | Bind UDP socket |
| `udpSendTo` | `(int32, string, string, uint16) → int64` | Send UDP data |
| `udpRecvFrom` | `(int32, usize) → string` | Receive UDP data |
| `close` | `(int32) → void` | Close socket |
| `setTimeout` | `(int32, uint64) → void` | Set timeout (ms) |
| `resolve` | `(string) → string` | DNS resolve |

---

## std::thread

```
use std::thread::{cpuCount, threadId, yield};
```

| Function | Signature | Description |
|----------|-----------|-------------|
| `cpuCount` | `() → uint32` | Number of CPU cores |
| `threadId` | `() → uint64` | Current thread ID |
| `yield` | `() → void` | Yield to scheduler |

---

## std::test

```
use std::test::{assertEqual, assertTrue, assertNear, fail, skip};
```

| Function | Signature | Description |
|----------|-----------|-------------|
| `assertEqual` **(P)** | `(_any, _any) → void` | Assert two values are equal |
| `assertNotEqual` **(P)** | `(_any, _any) → void` | Assert two values differ |
| `assertTrue` | `(bool) → void` | Assert condition is true |
| `assertFalse` | `(bool) → void` | Assert condition is false |
| `assertGreater` **(P)** | `(_numeric, _numeric) → void` | Assert a > b |
| `assertLess` **(P)** | `(_numeric, _numeric) → void` | Assert a < b |
| `assertGreaterEq` **(P)** | `(_numeric, _numeric) → void` | Assert a >= b |
| `assertLessEq` **(P)** | `(_numeric, _numeric) → void` | Assert a <= b |
| `assertStringContains` | `(string, string) → void` | Assert string contains substring |
| `assertNear` | `(float64, float64, float64) → void` | Assert floats within epsilon |
| `fail` | `(string) → void` | Force test failure |
| `skip` | `(string) → void` | Skip test with reason |
| `log` | `(string) → void` | Log message in test output |
