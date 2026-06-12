# Lucis — Builtins & Standard Library Plan

## 1. Global Builtins (always available, no `use` required) ✅

These are fundamental operations that every program needs access to without imports.

### Process Control
| Function | Signature | Description | Status |
|----------|-----------|-------------|--------|
| `exit` | `exit(int32)` | Terminate program with exit code | ✅ Done |
| `panic` | `panic(string)` | Print error to stderr and abort (exit 1) | ✅ Done |
| `assert` | `assert(bool)` | Abort with file/line info if condition is false | ✅ Done |
| `assertMsg` | `assertMsg(bool, string)` | Assert with custom error message | ✅ Done |
| `unreachable` | `unreachable()` | Mark code path as unreachable, abort if reached | ✅ Done |

### Type Conversions
| Function | Signature | Description | Status |
|----------|-----------|-------------|--------|
| `toInt` | `toInt(string) -> int64` | Parse string to integer | ✅ Done |
| `toFloat` | `toFloat(string) -> float64` | Parse string to float | ✅ Done |
| `toBool` | `toBool(string) -> bool` | Parse string to bool ("true"/"false") | ✅ Done |
| `toString` | `toString(T) -> string` | Convert any primitive to string | ✅ Done |

### Memory
| Function | Signature | Description |
|----------|-----------|-------------|
| `sizeof` | `sizeof(T) -> usize` | ✅ Already exists (expression) |
| `typeof` | `typeof(expr) -> string` | ✅ Already exists (expression) |

---

## 2. Standard Library Modules (via `use`)

### ✅ `std::log` — Console Output
| Symbol | Signature | Status | Description |
|--------|-----------|--------|-------------|
| `println` | `println(T)` | ✅ Done | Print value + newline to stdout |
| `print` | `print(T)` | ✅ Done | Print value to stdout |
| `eprintln` | `eprintln(T)` | ✅ Done | Print value + newline to stderr |
| `eprint` | `eprint(T)` | ✅ Done | Print value to stderr |
| `dbg` | `dbg(T) -> T` | ✅ Done | Print debug info (file:line = value) and return value |

---

### ✅ `std::io` — Input / Streams
| Symbol | Signature | Status | Description |
|--------|-----------|--------|-------------|
| **Stdin — Text** | | | |
| `readLine` | `readLine() -> string` | ✅ Done | Read a line from stdin |
| `readChar` | `readChar() -> char` | ✅ Done | Read a single character from stdin |
| `readInt` | `readInt() -> int64` | ✅ Done | Read and parse an integer from stdin |
| `readFloat` | `readFloat() -> float64` | ✅ Done | Read and parse a float from stdin |
| `readBool` | `readBool() -> bool` | ✅ Done | Read and parse a bool from stdin |
| `readAll` | `readAll() -> string` | ✅ Done | Read all of stdin until EOF |
| `readLines` | `readLines() -> []string` | ⬜ Pending | Read all lines from stdin until EOF |
| `prompt` | `prompt(string) -> string` | ✅ Done | Print prompt to stdout, then read a line |
| `promptInt` | `promptInt(string) -> int64` | ✅ Done | Print prompt, read and parse integer |
| `promptFloat` | `promptFloat(string) -> float64` | ✅ Done | Print prompt, read and parse float |
| `promptBool` | `promptBool(string) -> bool` | ✅ Done | Print prompt, read and parse bool |
| **Stdin — Binary** | | | |
| `readByte` | `readByte() -> uint8` | ✅ Done | Read a single byte from stdin |
| `readNBytes` | `readNBytes(usize) -> []uint8` | ⬜ Pending | Read exactly N bytes from stdin |
| **Stdin — Status** | | | |
| `isEOF` | `isEOF() -> bool` | ✅ Done | Check if stdin has reached EOF |
| **Secure Input** | | | |
| `readPassword` | `readPassword() -> string` | ✅ Done | Read a line from stdin with echo disabled |
| `promptPassword` | `promptPassword(string) -> string` | ✅ Done | Print prompt, read line with echo disabled |
| **Stream Control** | | | |
| `flush` | `flush()` | ✅ Done | Flush stdout buffer |
| `flushErr` | `flushErr()` | ✅ Done | Flush stderr buffer |
| **Terminal Detection** | | | |
| `isTTY` | `isTTY() -> bool` | ✅ Done | Check if stdin is a terminal (not piped) |
| `isStdoutTTY` | `isStdoutTTY() -> bool` | ✅ Done | Check if stdout is a terminal |
| `isStderrTTY` | `isStderrTTY() -> bool` | ✅ Done | Check if stderr is a terminal |

---

### ✅ `std::math` — Mathematical Functions & Constants
| Symbol | Signature | Status | Description |
|--------|-----------|--------|-------------|
| **Constants** | | | |
| `PI` | `float64` | ✅ Done | π = 3.14159265358979... |
| `E` | `float64` | ✅ Done | Euler's number = 2.71828182845904... |
| `TAU` | `float64` | ✅ Done | τ = 2π |
| `INF` | `float64` | ✅ Done | +Infinity |
| `NAN` | `float64` | ✅ Done | Not a Number |
| `INT32_MAX` | `int32` | ✅ Done | 2147483647 |
| `INT32_MIN` | `int32` | ✅ Done | -2147483648 |
| `INT64_MAX` | `int64` | ✅ Done | 9223372036854775807 |
| `INT64_MIN` | `int64` | ✅ Done | -9223372036854775808 |
| `UINT32_MAX` | `uint32` | ✅ Done | 4294967295 |
| `UINT64_MAX` | `uint64` | ✅ Done | 18446744073709551615 |
| **Basic Math** | | | |
| `abs` | `abs(T) -> T` | ✅ Done | Absolute value (int or float) |
| `min` | `min(T, T) -> T` | ✅ Done | Minimum of two values |
| `max` | `max(T, T) -> T` | ✅ Done | Maximum of two values |
| `clamp` | `clamp(T, T, T) -> T` | ✅ Done | Clamp value between min and max |
| **Power & Roots** | | | |
| `sqrt` | `sqrt(float64) -> float64` | ✅ Done | Square root |
| `cbrt` | `cbrt(float64) -> float64` | ✅ Done | Cube root |
| `pow` | `pow(float64, float64) -> float64` | ✅ Done | Power |
| `hypot` | `hypot(float64, float64) -> float64` | ✅ Done | Hypotenuse √(a²+b²) |
| **Exponential & Log** | | | |
| `exp` | `exp(float64) -> float64` | ✅ Done | e^x |
| `exp2` | `exp2(float64) -> float64` | ✅ Done | 2^x |
| `ln` | `ln(float64) -> float64` | ✅ Done | Natural logarithm |
| `log2` | `log2(float64) -> float64` | ✅ Done | Base-2 logarithm |
| `log10` | `log10(float64) -> float64` | ✅ Done | Base-10 logarithm |
| **Trigonometry** | | | |
| `sin` | `sin(float64) -> float64` | ✅ Done | Sine |
| `cos` | `cos(float64) -> float64` | ✅ Done | Cosine |
| `tan` | `tan(float64) -> float64` | ✅ Done | Tangent |
| `asin` | `asin(float64) -> float64` | ✅ Done | Arc sine |
| `acos` | `acos(float64) -> float64` | ✅ Done | Arc cosine |
| `atan` | `atan(float64) -> float64` | ✅ Done | Arc tangent |
| `atan2` | `atan2(float64, float64) -> float64` | ✅ Done | Two-argument arc tangent |
| **Hyperbolic** | | | |
| `sinh` | `sinh(float64) -> float64` | ✅ Done | Hyperbolic sine |
| `cosh` | `cosh(float64) -> float64` | ✅ Done | Hyperbolic cosine |
| `tanh` | `tanh(float64) -> float64` | ✅ Done | Hyperbolic tangent |
| **Rounding** | | | |
| `ceil` | `ceil(float64) -> float64` | ✅ Done | Round up |
| `floor` | `floor(float64) -> float64` | ✅ Done | Round down |
| `round` | `round(float64) -> float64` | ✅ Done | Round to nearest |
| `trunc` | `trunc(float64) -> float64` | ✅ Done | Truncate fractional part |
| **Interpolation** | | | |
| `lerp` | `lerp(float64, float64, float64) -> float64` | ✅ Done | Linear interpolation |
| `map` | `map(float64, float64, float64, float64, float64) -> float64` | ✅ Done | Map value from one range to another |
| **Conversion** | | | |
| `toRadians` | `toRadians(float64) -> float64` | ✅ Done | Degrees → radians |
| `toDegrees` | `toDegrees(float64) -> float64` | ✅ Done | Radians → degrees |
| **Checks** | | | |
| `isNaN` | `isNaN(float64) -> bool` | ✅ Done | Check if value is NaN |
| `isInf` | `isInf(float64) -> bool` | ✅ Done | Check if value is ±Infinity |
| `isFinite` | `isFinite(float64) -> bool` | ✅ Done | Check if value is finite |

---

### ✅ `std::str` — String Utilities & Formatting
| Symbol | Signature | Status | Description |
|--------|-----------|--------|-------------|
| **Formatting** | | | |
| `padLeft` | `padLeft(string, usize, char) -> string` | ✅ Done | Pad string on the left to target length |
| `padRight` | `padRight(string, usize, char) -> string` | ✅ Done | Pad string on the right to target length |
| **Search & Match** | | | |
| `contains` | `contains(string, string) -> bool` | ✅ Done | Check if string contains substring |
| `startsWith` | `startsWith(string, string) -> bool` | ✅ Done | Check if string starts with prefix |
| `endsWith` | `endsWith(string, string) -> bool` | ✅ Done | Check if string ends with suffix |
| `indexOf` | `indexOf(string, string) -> int64` | ✅ Done | First index of substring (-1 if not found) |
| `lastIndexOf` | `lastIndexOf(string, string) -> int64` | ✅ Done | Last index of substring (-1 if not found) |
| `count` | `count(string, string) -> usize` | ✅ Done | Count occurrences of substring |
| **Transformation** | | | |
| `toUpper` | `toUpper(string) -> string` | ✅ Done | Convert to uppercase |
| `toLower` | `toLower(string) -> string` | ✅ Done | Convert to lowercase |
| `trim` | `trim(string) -> string` | ✅ Done | Remove leading/trailing whitespace |
| `trimLeft` | `trimLeft(string) -> string` | ✅ Done | Remove leading whitespace |
| `trimRight` | `trimRight(string) -> string` | ✅ Done | Remove trailing whitespace |
| `replace` | `replace(string, string, string) -> string` | ✅ Done | Replace all occurrences of substring |
| `replaceFirst` | `replaceFirst(string, string, string) -> string` | ✅ Done | Replace first occurrence only |
| `repeat` | `repeat(string, usize) -> string` | ✅ Done | Repeat string N times |
| `reverse` | `reverse(string) -> string` | ✅ Done | Reverse the string |
| **Splitting & Joining** | | | |
| `split` | `split(string, string) -> []string` | ⬜ Pending | Split string by delimiter |
| `splitN` | `splitN(string, string, usize) -> []string` | ⬜ Pending | Split into at most N parts |
| `join` | `join([]string, string) -> string` | ⬜ Pending | Join string array with separator |
| `lines` | `lines(string) -> []string` | ⬜ Pending | Split string into lines |
| `chars` | `chars(string) -> []char` | ⬜ Pending | Convert string to char array |
| **Extraction** | | | |
| `substring` | `substring(string, usize, usize) -> string` | ✅ Done | Extract substring by start and length |
| `charAt` | `charAt(string, usize) -> char` | ✅ Done | Get character at index |
| `slice` | `slice(string, int64, int64) -> string` | ✅ Done | Slice with start:end (negative = from end) |
| **Parsing** | | | |
| `parseInt` | `parseInt(string) -> int64` | ✅ Done | Parse string to integer |
| `parseIntRadix` | `parseIntRadix(string, uint32) -> int64` | ✅ Done | Parse with radix (2, 8, 10, 16) |
| `parseFloat` | `parseFloat(string) -> float64` | ✅ Done | Parse string to float |
| **Conversion** | | | |
| `fromCharCode` | `fromCharCode(int32) -> char` | ✅ Done | Integer to char |
| `fromChars` | `fromChars([]char) -> string` | ⬜ Pending | Char array to string |
| `toBytes` | `toBytes(string) -> []uint8` | ⬜ Pending | String to byte array |
| `fromBytes` | `fromBytes([]uint8) -> string` | ⬜ Pending | Byte array to string |

---

### ✅ `std::mem` — Memory Management
| Symbol | Signature | Description |
|--------|-----------|-------------|
| `alloc` | `alloc(usize) -> *void` | Allocate N bytes (malloc) |
| `allocZeroed` | `allocZeroed(usize) -> *void` | Allocate N zeroed bytes (calloc) |
| `realloc` | `realloc(*void, usize) -> *void` | Resize allocation |
| `free` | `free(*void)` | Free allocated memory |
| `copy` | `copy(*void, *void, usize)` | Copy N bytes (memcpy) |
| `move` | `move(*void, *void, usize)` | Move N bytes (memmove, overlap-safe) |
| `set` | `set(*void, uint8, usize)` | Fill N bytes with value (memset) |
| `zero` | `zero(*void, usize)` | Zero out N bytes |
| `compare` | `compare(*void, *void, usize) -> int32` | Compare N bytes (memcmp) |

---

### ✅ `std::random` — Random Number Generation
| Symbol | Signature | Status | Description |
|--------|-----------|--------|-------------|
| `seed` | `seed(uint64)` | ✅ Done | Seed the PRNG |
| `seedTime` | `seedTime()` | ✅ Done | Seed from current time |
| `randInt` | `randInt() -> int64` | ✅ Done | Random int64 |
| `randIntRange` | `randIntRange(int64, int64) -> int64` | ✅ Done | Random int64 in [min, max] |
| `randUint` | `randUint() -> uint64` | ✅ Done | Random uint64 |
| `randFloat` | `randFloat() -> float64` | ✅ Done | Random float64 in [0.0, 1.0) |
| `randFloatRange` | `randFloatRange(float64, float64) -> float64` | ✅ Done | Random float64 in [min, max) |
| `randBool` | `randBool() -> bool` | ✅ Done | Random boolean |
| `randChar` | `randChar() -> char` | ✅ Done | Random printable ASCII char |
| `shuffle` | `shuffle([]T)` | ⬜ Pending | Shuffle array in-place (needs generic array) |
| `choice` | `choice([]T) -> T` | ⬜ Pending | Random element from array (needs generic array) |

---

### ✅ `std::time` — Time & Duration
| Symbol | Signature | Description |
|--------|-----------|-------------|
| `now` | `now() -> uint64` | Current timestamp in milliseconds (epoch) |
| `nowNanos` | `nowNanos() -> uint64` | Current timestamp in nanoseconds |
| `nowMicros` | `nowMicros() -> uint64` | Current timestamp in microseconds |
| `sleep` | `sleep(uint64)` | Sleep for N milliseconds |
| `sleepMicros` | `sleepMicros(uint64)` | Sleep for N microseconds |
| `clock` | `clock() -> uint64` | CPU clock ticks (for benchmarking) |
| `year` | `year() -> int32` | Current year |
| `month` | `month() -> int32` | Current month (1-12) |
| `day` | `day() -> int32` | Current day of month (1-31) |
| `hour` | `hour() -> int32` | Current hour (0-23) |
| `minute` | `minute() -> int32` | Current minute (0-59) |
| `second` | `second() -> int32` | Current second (0-59) |
| `weekday` | `weekday() -> int32` | Day of week (0=Sunday, 6=Saturday) |
| `timestamp` | `timestamp() -> string` | ISO 8601 formatted timestamp |
| `elapsed` | `elapsed(uint64) -> uint64` | Milliseconds elapsed since given timestamp |
| `formatTime` | `formatTime(uint64, string) -> string` | Format timestamp with pattern |
| `parseTime` | `parseTime(string, string) -> uint64` | Parse formatted date string to timestamp |

---

### ✅ `std::fs` — File System
| Symbol | Signature | Status | Description |
|--------|-----------|--------|-------------|
| `readFile` | `readFile(string) -> string` | ✅ Done | Read entire file contents |
| `writeFile` | `writeFile(string, string)` | ✅ Done | Write string to file (overwrite) |
| `appendFile` | `appendFile(string, string)` | ✅ Done | Append string to file |
| `readBytes` | `readBytes(string) -> []uint8` | ⬜ Pending | Read file as byte array |
| `writeBytes` | `writeBytes(string, []uint8)` | ⬜ Pending | Write byte array to file |
| `exists` | `exists(string) -> bool` | ✅ Done | Check if path exists |
| `isFile` | `isFile(string) -> bool` | ✅ Done | Check if path is a file |
| `isDir` | `isDir(string) -> bool` | ✅ Done | Check if path is a directory |
| `remove` | `remove(string) -> bool` | ✅ Done | Delete file |
| `removeDir` | `removeDir(string) -> bool` | ✅ Done | Delete directory (must be empty) |
| `rename` | `rename(string, string) -> bool` | ✅ Done | Rename/move file |
| `mkdir` | `mkdir(string) -> bool` | ✅ Done | Create directory |
| `mkdirAll` | `mkdirAll(string) -> bool` | ✅ Done | Create directory tree (mkdir -p) |
| `listDir` | `listDir(string) -> []string` | ⬜ Pending | List directory entries |
| `fileSize` | `fileSize(string) -> int64` | ✅ Done | Get file size in bytes |
| `cwd` | `cwd() -> string` | ✅ Done | Current working directory |
| `setCwd` | `setCwd(string) -> bool` | ✅ Done | Change working directory |
| `tempDir` | `tempDir() -> string` | ✅ Done | System temp directory path |

---

### ✅ `std::process` — Process & Environment
| Symbol | Signature | Status | Description |
|--------|-----------|--------|-------------|
| `exit` | `exit(int32)` | ✅ Done | Exit with code (also global builtin) |
| `abort` | `abort()` | ✅ Done | Abort immediately (SIGABRT) |
| `env` | `env(string) -> string` | ✅ Done | Get environment variable |
| `setEnv` | `setEnv(string, string)` | ✅ Done | Set environment variable |
| `hasEnv` | `hasEnv(string) -> bool` | ✅ Done | Check if env variable exists |
| `exec` | `exec(string) -> int32` | ✅ Done | Execute shell command, return exit code |
| `execOutput` | `execOutput(string) -> string` | ✅ Done | Execute command, capture stdout |
| `pid` | `pid() -> int32` | ✅ Done | Current process ID |
| `platform` | `platform() -> string` | ✅ Done | OS name ("linux", "macos", "windows") |
| `arch` | `arch() -> string` | ✅ Done | CPU arch ("x86_64", "aarch64") |
| `homeDir` | `homeDir() -> string` | ✅ Done | User home directory path |
| `executablePath` | `executablePath() -> string` | ✅ Done | Path to current executable |

---

### ✅ `std::conv` — Type Conversions & Encoding
| Symbol | Signature | Description | Status |
|--------|-----------|-------------|--------|
| `itoa` | `itoa(int64) -> string` | Integer to string | Done |
| `itoaRadix` | `itoaRadix(int64, uint32) -> string` | Integer to string with radix | Done |
| `utoa` | `utoa(uint64) -> string` | Unsigned to string | Done |
| `ftoa` | `ftoa(float64) -> string` | Float to string | Done |
| `ftoaPrecision` | `ftoaPrecision(float64, uint32) -> string` | Float to string with precision | Done |
| `atoi` | `atoi(string) -> int64` | String to integer | Done |
| `atof` | `atof(string) -> float64` | String to float | Done |
| `toHex` | `toHex(uint64) -> string` | Integer to hex string | Done |
| `toOctal` | `toOctal(uint64) -> string` | Integer to octal string | Done |
| `toBinary` | `toBinary(uint64) -> string` | Integer to binary string | Done |
| `fromHex` | `fromHex(string) -> uint64` | Hex string to integer | Done |
| `charToInt` | `charToInt(char) -> int32` | Char to ASCII code | Done |
| `intToChar` | `intToChar(int32) -> char` | ASCII code to char | Done |

---

### ✅ `std::hash` — Hashing
| Symbol | Signature | Description | Status |
|--------|-----------|-------------|--------|
| `hashString` | `hashString(string) -> uint64` | FNV-1a hash of string | Done |
| `hashBytes` | `hashBytes([]uint8) -> uint64` | FNV-1a hash of byte array | Pending (needs []uint8) |
| `hashInt` | `hashInt(int64) -> uint64` | Hash an integer | Done |
| `hashCombine` | `hashCombine(uint64, uint64) -> uint64` | Combine two hashes | Done |
| `crc32` | `crc32([]uint8) -> uint32` | CRC-32 checksum | Pending (needs []uint8) |

---

### ✅ `std::bits` — Bit Manipulation Utilities
| Symbol | Signature | Description | Status |
|--------|-----------|-------------|--------|
| `popcount` | `popcount(uint64) -> uint32` | Count set bits | Done |
| `ctz` | `ctz(uint64) -> uint32` | Count trailing zeros | Done |
| `clz` | `clz(uint64) -> uint32` | Count leading zeros | Done |
| `rotl` | `rotl(uint64, uint32) -> uint64` | Rotate left | Done |
| `rotr` | `rotr(uint64, uint32) -> uint64` | Rotate right | Done |
| `bswap` | `bswap(uint64) -> uint64` | Byte swap | Done |
| `isPow2` | `isPow2(uint64) -> bool` | Check if power of 2 | Done |
| `nextPow2` | `nextPow2(uint64) -> uint64` | Next power of 2 | Done |
| `bitReverse` | `bitReverse(uint64) -> uint64` | Reverse bit order | Done |
| `extractBits` | `extractBits(uint64, uint32, uint32) -> uint64` | Extract bit field | Done |
| `setBit` | `setBit(uint64, uint32) -> uint64` | Set specific bit | Done |
| `clearBit` | `clearBit(uint64, uint32) -> uint64` | Clear specific bit | Done |
| `toggleBit` | `toggleBit(uint64, uint32) -> uint64` | Toggle specific bit | Done |
| `testBit` | `testBit(uint64, uint32) -> bool` | Test specific bit | Done |

---

### ⬜ `collections` — Data Structures (native keywords)

> `vec<T>` is already implemented. The types below follow the same Extended Type pattern.

#### `map<K,V>` — Hash Map ✅ 
| Method | Signature | Description | Status |
|--------|-----------|-------------|--------|
| `len` | `len() -> usize` | Number of key-value pairs | ✅ |
| `isEmpty` | `isEmpty() -> bool` | Check if map is empty | ✅ |
| `get` | `get(K) -> V` | Get value by key (abort if missing) | ✅ |
| `getOrDefault` | `getOrDefault(K, V) -> V` | Get value or return default | ✅ |
| `insert` | `insert(K, V)` | Insert or update key-value pair | ✅ |
| `has` | `has(K) -> bool` | Check if key exists | ✅ |
| `remove` | `remove(K) -> bool` | Remove key, return true if existed | ✅ |
| `clear` | `clear()` | Remove all entries | ✅ |
| `free` | `free()` | Deallocate map memory | ✅ |

#### `set<T>` — Hash Set ✅ 
| Method | Signature | Description | Status |
|--------|-----------|-------------|--------|
| `len` | `len() -> usize` | Number of elements | ✅ |
| `isEmpty` | `isEmpty() -> bool` | Check if set is empty | ✅ |
| `add` | `add(T) -> bool` | Add element, return true if new | ✅ |
| `has` | `has(T) -> bool` | Check if element exists | ✅ |
| `remove` | `remove(T) -> bool` | Remove element, return true if existed | ✅ |
| `clear` | `clear()` | Remove all elements | ✅ |
| `free` | `free()` | Deallocate set memory | ✅ |

---

### ✅ `std::ascii` — Character Classification & Conversion
| Symbol | Signature | Description | Status |
|--------|-----------|-------------|--------|
| `isAlpha` | `isAlpha(char) -> bool` | Is alphabetic (a-z, A-Z) | ✅ |
| `isDigit` | `isDigit(char) -> bool` | Is numeric digit (0-9) | ✅ |
| `isAlphaNum` | `isAlphaNum(char) -> bool` | Is alphanumeric | ✅ |
| `isUpper` | `isUpper(char) -> bool` | Is uppercase letter | ✅ |
| `isLower` | `isLower(char) -> bool` | Is lowercase letter | ✅ |
| `isWhitespace` | `isWhitespace(char) -> bool` | Is whitespace (space, tab, newline) | ✅ |
| `isPrintable` | `isPrintable(char) -> bool` | Is printable ASCII | ✅ |
| `isControl` | `isControl(char) -> bool` | Is control character | ✅ |
| `isHexDigit` | `isHexDigit(char) -> bool` | Is hex digit (0-9, a-f, A-F) | ✅ |
| `isAscii` | `isAscii(char) -> bool` | Is valid ASCII (0-127) | ✅ |
| `toUpper` | `toUpper(char) -> char` | Convert to uppercase | ✅ |
| `toLower` | `toLower(char) -> char` | Convert to lowercase | ✅ |
| `toDigit` | `toDigit(char) -> int32` | Char '0'-'9' to integer 0-9 | ✅ |
| `fromDigit` | `fromDigit(int32) -> char` | Integer 0-9 to char '0'-'9' | ✅ |

---

### ✅ `std::path` — Path Manipulation (12/13 — joinAll pending, needs []string)
| Symbol | Signature | Description | Status |
|--------|-----------|-------------|--------|
| `join` | `join(string, string) -> string` | Join two path components | ✅ |
| `parent` | `parent(string) -> string` | Parent directory ("a/b/c" → "a/b") | ✅ |
| `fileName` | `fileName(string) -> string` | Final component ("a/b/c.txt" → "c.txt") | ✅ |
| `stem` | `stem(string) -> string` | File name without extension ("c.txt" → "c") | ✅ |
| `extension` | `extension(string) -> string` | File extension ("c.txt" → "txt") | ✅ |
| `isAbsolute` | `isAbsolute(string) -> bool` | Check if path is absolute | ✅ |
| `isRelative` | `isRelative(string) -> bool` | Check if path is relative | ✅ |
| `normalize` | `normalize(string) -> string` | Normalize path (resolve `.` and `..`) | ✅ |
| `toAbsolute` | `toAbsolute(string) -> string` | Make path absolute (relative to cwd) | ✅ |
| `separator` | `separator() -> char` | OS path separator ('/' or '\\') | ✅ |
| `withExtension` | `withExtension(string, string) -> string` | Change file extension | ✅ |
| `withFileName` | `withFileName(string, string) -> string` | Change final component | ✅ |

---

### ✅ `std::fmt` — Advanced Formatting
| Symbol | Signature | Description | Status |
|--------|-----------|-------------|--------|
| `lpad` | `lpad(string, usize, char) -> string` | Left-pad to width | ✅ |
| `rpad` | `rpad(string, usize, char) -> string` | Right-pad to width | ✅ |
| `center` | `center(string, usize, char) -> string` | Center-pad to width | ✅ |
| `hex` | `hex(uint64) -> string` | Format as hex (0xff) | ✅ |
| `hexUpper` | `hexUpper(uint64) -> string` | Format as uppercase hex (0xFF) | ✅ |
| `oct` | `oct(uint64) -> string` | Format as octal (0o77) | ✅ |
| `bin` | `bin(uint64) -> string` | Format as binary (0b1010) | ✅ |
| `fixed` | `fixed(float64, uint32) -> string` | Float with fixed decimal places | ✅ |
| `scientific` | `scientific(float64) -> string` | Float in scientific notation | ✅ |
| `humanBytes` | `humanBytes(uint64) -> string` | Bytes to human readable ("1.5 MB") | ✅ |
| `commas` | `commas(int64) -> string` | Integer with comma separators ("1,000,000") | ✅ |
| `percent` | `percent(float64) -> string` | Float as percentage ("85.5%") | ✅ |

---

### ✅ `std::regex` — Regular Expressions (6/8 — findAll/split pending, need []string)
| Symbol | Signature | Description | Status |
|--------|-----------|-------------|--------|
| `match` | `match(string, string) -> bool` | Check if string matches pattern | ✅ |
| `find` | `find(string, string) -> string` | Find first match | ✅ |
| `findIndex` | `findIndex(string, string) -> int64` | Index of first match (-1 if none) | ✅ |
| `regexReplace` | `regexReplace(string, string, string) -> string` | Replace all matches | ✅ |
| `regexReplaceFirst` | `regexReplaceFirst(string, string, string) -> string` | Replace first match | ✅ |
| `isValid` | `isValid(string) -> bool` | Check if pattern is valid regex | ✅ |

---

### ✅ `std::encoding` — Data Encoding & Decoding (4/8 — byte funcs + JSON pending)
| Symbol | Signature | Description | Status |
|--------|-----------|-------------|--------|
| **Base64** | | | |
| `base64EncodeStr` | `base64EncodeStr(string) -> string` | Encode string to base64 | ✅ |
| `base64DecodeStr` | `base64DecodeStr(string) -> string` | Decode base64 to string | ✅ |
| **URL** | | | |
| `urlEncode` | `urlEncode(string) -> string` | URL-encode string | ✅ |
| `urlDecode` | `urlDecode(string) -> string` | URL-decode string | ✅ |

---

### ✅ `std::os` — OS-Level Operations & Signals 
| Symbol | Signature | Description | Status |
|--------|-----------|-------------|--------|
| `getpid` | `getpid() -> int32` | Current process ID | ✅ |
| `getppid` | `getppid() -> int32` | Parent process ID | ✅ |
| `getuid` | `getuid() -> uint32` | Current user ID | ✅ |
| `getgid` | `getgid() -> uint32` | Current group ID | ✅ |
| `hostname` | `hostname() -> string` | System hostname | ✅ |
| `pageSize` | `pageSize() -> usize` | OS memory page size | ✅ |
| `errno` | `errno() -> int32` | Last OS error code | ✅ |
| `strerror` | `strerror(int32) -> string` | Error code to message | ✅ |
| `kill` | `kill(int32, int32) -> int32` | Send signal to process | ✅ |
| `dup` | `dup(int32) -> int32` | Duplicate file descriptor | ✅ |
| `dup2` | `dup2(int32, int32) -> int32` | Duplicate fd to specific number | ✅ |
| `closeFd` | `closeFd(int32) -> int32` | Close file descriptor | ✅ |

---

### ✅ `std::test` — Testing Framework (13/13)
| Symbol | Signature | Description |
|--------|-----------|-------------|
| `assertEqual` | `assertEqual(T, T)` | Assert two values are equal |
| `assertNotEqual` | `assertNotEqual(T, T)` | Assert two values are not equal |
| `assertTrue` | `assertTrue(bool)` | Assert condition is true |
| `assertFalse` | `assertFalse(bool)` | Assert condition is false |
| `assertGreater` | `assertGreater(T, T)` | Assert first > second |
| `assertLess` | `assertLess(T, T)` | Assert first < second |
| `assertGreaterEq` | `assertGreaterEq(T, T)` | Assert first >= second |
| `assertLessEq` | `assertLessEq(T, T)` | Assert first <= second |
| `assertStringContains` | `assertStringContains(string, string)` | Assert string contains substring |
| `assertNear` | `assertNear(float64, float64, float64)` | Assert floats are within epsilon |
| `fail` | `fail(string)` | Unconditional test failure with message |
| `skip` | `skip(string)` | Skip current test with reason |
| `log` | `log(string)` | Log message in test output |

---

### ✅ `std::crypto` — Cryptographic Hashing (4/10)
| Symbol | Signature | Description |
|--------|-----------|-------------|
| `md5` | `md5([]uint8) -> string` | ⬜ MD5 hash (hex string) — needs `[]uint8` |
| `md5String` | `md5String(string) -> string` | ✅ MD5 hash of string |
| `sha1` | `sha1([]uint8) -> string` | ⬜ SHA-1 hash (hex string) — needs `[]uint8` |
| `sha1String` | `sha1String(string) -> string` | ✅ SHA-1 hash of string |
| `sha256` | `sha256([]uint8) -> string` | ⬜ SHA-256 hash (hex string) — needs `[]uint8` |
| `sha256String` | `sha256String(string) -> string` | ✅ SHA-256 hash of string |
| `sha512` | `sha512([]uint8) -> string` | ⬜ SHA-512 hash (hex string) — needs `[]uint8` |
| `sha512String` | `sha512String(string) -> string` | ✅ SHA-512 hash of string |
| `hmacSha256` | `hmacSha256([]uint8, []uint8) -> []uint8` | ⬜ HMAC-SHA256 — needs `[]uint8` |
| `randomBytes` | `randomBytes(usize) -> []uint8` | ⬜ Cryptographically secure random bytes — needs `[]uint8` |

---

### ✅ `std::compress` — Compression & Decompression (5/5)
| Symbol | Signature | Description |
|--------|-----------|-------------|
| `gzipCompress` | `gzipCompress(string) -> string` | ✅ Gzip compress data |
| `gzipDecompress` | `gzipDecompress(string) -> string` | ✅ Gzip decompress data |
| `deflate` | `deflate(string) -> string` | ✅ Raw deflate compress |
| `inflate` | `inflate(string) -> string` | ✅ Raw deflate decompress |
| `compressLevel` | `compressLevel(string, int32) -> string` | ✅ Gzip compress with level (1-9) |

---

### ✅ `std::net` — Networking (TCP/UDP basics) (11/11)
| Symbol | Signature | Description |
|--------|-----------|-------------|
| `tcpConnect` | `tcpConnect(string, uint16) -> int32` | Connect to host:port, returns socket fd |
| `tcpListen` | `tcpListen(string, uint16) -> int32` | Listen on host:port, returns socket fd |
| `tcpAccept` | `tcpAccept(int32) -> int32` | Accept connection, returns client fd |
| `tcpSend` | `tcpSend(int32, string) -> int64` | Send data on socket |
| `tcpRecv` | `tcpRecv(int32, usize) -> string` | Receive data from socket |
| `udpBind` | `udpBind(string, uint16) -> int32` | Bind UDP socket |
| `udpSendTo` | `udpSendTo(int32, string, string, uint16) -> int64` | Send UDP packet |
| `udpRecvFrom` | `udpRecvFrom(int32, usize) -> string` | Receive UDP packet |
| `close` | `close(int32)` | Close socket |
| `setTimeout` | `setTimeout(int32, uint64)` | Set socket timeout (ms) |
| `resolve` | `resolve(string) -> string` | DNS hostname → IP address |

---
