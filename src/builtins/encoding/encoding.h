#ifndef LUCIS_ENCODING_H
#define LUCIS_ENCODING_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char* ptr;
    size_t      len;
} lucis_encoding_str_result;

/* Base64 (string ↔ string) */
lucis_encoding_str_result lucis_base64EncodeStr(const char* s, size_t s_len);
lucis_encoding_str_result lucis_base64DecodeStr(const char* s, size_t s_len);

/* URL encoding */
lucis_encoding_str_result lucis_urlEncode(const char* s, size_t s_len);
lucis_encoding_str_result lucis_urlDecode(const char* s, size_t s_len);

/* Base64 with Vec<uint8> */
typedef struct { void* ptr; size_t len; size_t cap; } lucis_enc_vec_header;

lucis_encoding_str_result lucis_base64EncodeVec(const lucis_enc_vec_header* data);
void lucis_base64DecodeVec(lucis_enc_vec_header* out, const char* s, size_t s_len);

#ifdef __cplusplus
}
#endif

#endif /* LUCIS_ENCODING_H */
