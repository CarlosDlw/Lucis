#ifndef LUCIS_CRYPTO_H
#define LUCIS_CRYPTO_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char* ptr;
    size_t      len;
} lucis_crypto_str_result;

lucis_crypto_str_result lucis_md5String(const char* s, size_t slen);
lucis_crypto_str_result lucis_sha1String(const char* s, size_t slen);
lucis_crypto_str_result lucis_sha256String(const char* s, size_t slen);
lucis_crypto_str_result lucis_sha512String(const char* s, size_t slen);

/* Vec<uint8> variants */
typedef struct { void* ptr; size_t len; size_t cap; } lucis_crypto_vec_header;

lucis_crypto_str_result lucis_md5Bytes(const lucis_crypto_vec_header* data);
lucis_crypto_str_result lucis_sha1Bytes(const lucis_crypto_vec_header* data);
lucis_crypto_str_result lucis_sha256Bytes(const lucis_crypto_vec_header* data);
lucis_crypto_str_result lucis_sha512Bytes(const lucis_crypto_vec_header* data);
void lucis_hmacSha256(lucis_crypto_vec_header* out,
                       const lucis_crypto_vec_header* key,
                       const lucis_crypto_vec_header* data);
void lucis_randomBytes(lucis_crypto_vec_header* out, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* LUCIS_CRYPTO_H */
