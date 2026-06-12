# std::compress

Data compression and decompression using gzip/deflate.

## Import

```
use std::compress::{ gzipCompress, gzipDecompress, deflate, inflate };
```

## Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `gzipCompress` | `(*uint8, int64) -> vec<uint8>` | Compress data with gzip |
| `gzipDecompress` | `(*uint8, int64) -> vec<uint8>` | Decompress gzip data |
| `deflate` | `(*uint8, int64) -> vec<uint8>` | Compress with raw deflate |
| `inflate` | `(*uint8, int64) -> vec<uint8>` | Decompress raw deflate |
| `compressLevel` | `(*uint8, int64, int32) -> vec<uint8>` | Compress with specific level (1-9) |

## Example

```
use std::compress::{ gzipCompress, gzipDecompress };
use std::log::println;

string data = "Hello, World! Hello, World! Hello, World!";
*uint8 ptr = data as *uint8;
int64 len = data.length() as int64;

vec<uint8> compressed = gzipCompress(ptr, len);
println(compressed.length());      // smaller than original

vec<uint8> decompressed = gzipDecompress(compressed.data(), compressed.length() as int64);
// decompressed contains original bytes
```

## See Also

- [std::encoding](encoding.md) — Base64 encoding
- [std::fs](fs.md) — File system operations
