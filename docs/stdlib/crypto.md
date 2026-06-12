# std::crypto

Cryptographic hashing and utility functions.

## Import

```
use std::crypto::{ sha256String, md5String, hmacSha256, randomBytes };
```

## Functions

### String Hashing

| Function | Signature | Description |
|----------|-----------|-------------|
| `md5String` | `(string) -> string` | MD5 hash of string (hex) |
| `sha1String` | `(string) -> string` | SHA-1 hash of string (hex) |
| `sha256String` | `(string) -> string` | SHA-256 hash of string (hex) |
| `sha512String` | `(string) -> string` | SHA-512 hash of string (hex) |

### Buffer Hashing

| Function | Signature | Description |
|----------|-----------|-------------|
| `md5` | `(*uint8, int64) -> string` | MD5 hash of byte buffer (hex) |
| `sha1` | `(*uint8, int64) -> string` | SHA-1 hash of byte buffer (hex) |
| `sha256` | `(*uint8, int64) -> string` | SHA-256 hash of byte buffer (hex) |
| `sha512` | `(*uint8, int64) -> string` | SHA-512 hash of byte buffer (hex) |

### Utilities

| Function | Signature | Description |
|----------|-----------|-------------|
| `hmacSha256` | `(string, string) -> string` | HMAC-SHA256 of message with key |
| `randomBytes` | `(int32) -> vec<uint8>` | Cryptographically secure random bytes |

## Example

```
use std::crypto::{ md5String, sha256String, hmacSha256, randomBytes };
use std::log::println;

println(md5String("hello"));
// 5d41402abc4b2a76b9719d911017c592

println(sha256String("hello"));
// 2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824

string mac = hmacSha256("message", "secret-key");
println(mac);

vec<uint8> bytes = randomBytes(16);
// 16 cryptographically secure random bytes
```

> **Warning:** `md5` and `sha1` are considered weak for cryptographic purposes. Prefer `sha256` or `sha512` for security-critical applications.

## See Also

- [std::hash](hash.md) — General-purpose hashing
- [std::encoding](encoding.md) — Base64 and URL encoding
