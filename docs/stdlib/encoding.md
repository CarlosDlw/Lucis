# std::encoding

Encoding and decoding utilities for Base64 and URL formats.

## Import

```
use std::encoding::{ base64EncodeStr, base64DecodeStr, urlEncode, urlDecode };
```

## Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `base64Encode` | `(*uint8, int64) -> string` | Encode raw bytes to Base64 |
| `base64Decode` | `(string) -> vec<uint8>` | Decode Base64 to byte vector |
| `base64EncodeStr` | `(string) -> string` | Encode string to Base64 |
| `base64DecodeStr` | `(string) -> string` | Decode Base64 to string |
| `urlEncode` | `(string) -> string` | Percent-encode string |
| `urlDecode` | `(string) -> string` | Decode percent-encoded string |

## Example

```
use std::encoding::{ base64EncodeStr, base64DecodeStr, urlEncode, urlDecode };
use std::log::println;

string encoded = base64EncodeStr("Hello, World!");
println(encoded);                  // SGVsbG8sIFdvcmxkIQ==

string decoded = base64DecodeStr(encoded);
println(decoded);                  // Hello, World!

string url = urlEncode("hello world & foo=bar");
println(url);                      // hello%20world%20%26%20foo%3Dbar

println(urlDecode(url));           // hello world & foo=bar
```

## See Also

- [std::crypto](crypto.md) — Cryptographic hashing
- [std::string](string.md) — String manipulation
