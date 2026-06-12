#ifndef LUCIS_COMPRESS_H
#define LUCIS_COMPRESS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char* ptr;
    size_t      len;
} lucis_compress_str_result;

lucis_compress_str_result lucis_gzipCompress(const char* s, size_t slen);
lucis_compress_str_result lucis_gzipDecompress(const char* s, size_t slen);
lucis_compress_str_result lucis_deflate(const char* s, size_t slen);
lucis_compress_str_result lucis_inflate(const char* s, size_t slen);
lucis_compress_str_result lucis_compressLevel(const char* s, size_t slen, int32_t level);

#ifdef __cplusplus
}
#endif

#endif /* LUCIS_COMPRESS_H */
