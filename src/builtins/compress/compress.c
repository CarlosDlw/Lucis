#include "compress.h"
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

/* ── helpers ────────────────────────────────────────────────────────────── */

static lucis_compress_str_result make_result(const char* p, size_t n) {
    return (lucis_compress_str_result){ p, n };
}

static lucis_compress_str_result empty_result(void) {
    char* p = (char*)malloc(1);
    p[0] = '\0';
    return make_result(p, 0);
}

/* ── gzip compress ──────────────────────────────────────────────────────── */

lucis_compress_str_result lucis_gzipCompress(const char* s, size_t slen) {
    if (slen == 0) return empty_result();

    z_stream zs;
    memset(&zs, 0, sizeof(zs));

    /* windowBits=15+16 enables gzip header */
    if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                     15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        return empty_result();

    size_t cap = deflateBound(&zs, (uLong)slen);
    char* out  = (char*)malloc(cap);

    zs.next_in   = (Bytef*)s;
    zs.avail_in  = (uInt)slen;
    zs.next_out  = (Bytef*)out;
    zs.avail_out = (uInt)cap;

    int rc = deflate(&zs, Z_FINISH);
    size_t written = zs.total_out;
    deflateEnd(&zs);

    if (rc != Z_STREAM_END) {
        free(out);
        return empty_result();
    }

    return make_result(out, written);
}

/* ── gzip decompress ────────────────────────────────────────────────────── */

lucis_compress_str_result lucis_gzipDecompress(const char* s, size_t slen) {
    if (slen == 0) return empty_result();

    z_stream zs;
    memset(&zs, 0, sizeof(zs));

    /* windowBits=15+16 auto-detects gzip header */
    if (inflateInit2(&zs, 15 + 16) != Z_OK)
        return empty_result();

    size_t cap = slen * 4;
    if (cap < 256) cap = 256;
    char* out = (char*)malloc(cap);

    zs.next_in   = (Bytef*)s;
    zs.avail_in  = (uInt)slen;
    zs.next_out  = (Bytef*)out;
    zs.avail_out = (uInt)cap;

    int rc;
    while ((rc = inflate(&zs, Z_NO_FLUSH)) == Z_OK) {
        if (zs.avail_out == 0) {
            size_t old = cap;
            cap *= 2;
            out = (char*)realloc(out, cap);
            zs.next_out  = (Bytef*)(out + old);
            zs.avail_out = (uInt)(cap - old);
        }
    }

    size_t written = zs.total_out;
    inflateEnd(&zs);

    if (rc != Z_STREAM_END) {
        free(out);
        return empty_result();
    }

    return make_result(out, written);
}

/* ── raw deflate ────────────────────────────────────────────────────────── */

lucis_compress_str_result lucis_deflate(const char* s, size_t slen) {
    if (slen == 0) return empty_result();

    z_stream zs;
    memset(&zs, 0, sizeof(zs));

    /* windowBits=-15 for raw deflate (no header) */
    if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                     -15, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        return empty_result();

    size_t cap = deflateBound(&zs, (uLong)slen);
    char* out  = (char*)malloc(cap);

    zs.next_in   = (Bytef*)s;
    zs.avail_in  = (uInt)slen;
    zs.next_out  = (Bytef*)out;
    zs.avail_out = (uInt)cap;

    int rc = deflate(&zs, Z_FINISH);
    size_t written = zs.total_out;
    deflateEnd(&zs);

    if (rc != Z_STREAM_END) {
        free(out);
        return empty_result();
    }

    return make_result(out, written);
}

/* ── raw inflate ────────────────────────────────────────────────────────── */

lucis_compress_str_result lucis_inflate(const char* s, size_t slen) {
    if (slen == 0) return empty_result();

    z_stream zs;
    memset(&zs, 0, sizeof(zs));

    /* windowBits=-15 for raw inflate (no header) */
    if (inflateInit2(&zs, -15) != Z_OK)
        return empty_result();

    size_t cap = slen * 4;
    if (cap < 256) cap = 256;
    char* out = (char*)malloc(cap);

    zs.next_in   = (Bytef*)s;
    zs.avail_in  = (uInt)slen;
    zs.next_out  = (Bytef*)out;
    zs.avail_out = (uInt)cap;

    int rc;
    while ((rc = inflate(&zs, Z_NO_FLUSH)) == Z_OK) {
        if (zs.avail_out == 0) {
            size_t old = cap;
            cap *= 2;
            out = (char*)realloc(out, cap);
            zs.next_out  = (Bytef*)(out + old);
            zs.avail_out = (uInt)(cap - old);
        }
    }

    size_t written = zs.total_out;
    inflateEnd(&zs);

    if (rc != Z_STREAM_END) {
        free(out);
        return empty_result();
    }

    return make_result(out, written);
}

/* ── compress with level ────────────────────────────────────────────────── */

lucis_compress_str_result lucis_compressLevel(const char* s, size_t slen, int32_t level) {
    if (slen == 0) return empty_result();

    if (level < 1) level = 1;
    if (level > 9) level = 9;

    z_stream zs;
    memset(&zs, 0, sizeof(zs));

    /* windowBits=15+16 for gzip with custom level */
    if (deflateInit2(&zs, level, Z_DEFLATED,
                     15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        return empty_result();

    size_t cap = deflateBound(&zs, (uLong)slen);
    char* out  = (char*)malloc(cap);

    zs.next_in   = (Bytef*)s;
    zs.avail_in  = (uInt)slen;
    zs.next_out  = (Bytef*)out;
    zs.avail_out = (uInt)cap;

    int rc = deflate(&zs, Z_FINISH);
    size_t written = zs.total_out;
    deflateEnd(&zs);

    if (rc != Z_STREAM_END) {
        free(out);
        return empty_result();
    }

    return make_result(out, written);
}
