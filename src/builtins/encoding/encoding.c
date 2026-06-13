#include "encoding.h"
#include "../string/string.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ── Base64 tables ──────────────────────────────────────────────── */

static const char b64_enc_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const unsigned char b64_dec_table[256] = {
    ['A']=0,  ['B']=1,  ['C']=2,  ['D']=3,  ['E']=4,  ['F']=5,
    ['G']=6,  ['H']=7,  ['I']=8,  ['J']=9,  ['K']=10, ['L']=11,
    ['M']=12, ['N']=13, ['O']=14, ['P']=15, ['Q']=16, ['R']=17,
    ['S']=18, ['T']=19, ['U']=20, ['V']=21, ['W']=22, ['X']=23,
    ['Y']=24, ['Z']=25,
    ['a']=26, ['b']=27, ['c']=28, ['d']=29, ['e']=30, ['f']=31,
    ['g']=32, ['h']=33, ['i']=34, ['j']=35, ['k']=36, ['l']=37,
    ['m']=38, ['n']=39, ['o']=40, ['p']=41, ['q']=42, ['r']=43,
    ['s']=44, ['t']=45, ['u']=46, ['v']=47, ['w']=48, ['x']=49,
    ['y']=50, ['z']=51,
    ['0']=52, ['1']=53, ['2']=54, ['3']=55, ['4']=56, ['5']=57,
    ['6']=58, ['7']=59, ['8']=60, ['9']=61,
    ['+']=62, ['/']=63,
};

/* ── Base64 encode (string → string) ────────────────────────────── */

lucis_encoding_str_result lucis_base64EncodeStr(const char* s, size_t s_len) {
    size_t out_len = 4 * ((s_len + 2) / 3);
    char* out = (char*)lucis_allocString(out_len + 1);
    if (!out) { return (lucis_encoding_str_result){ "", 0 }; }

    size_t i = 0, j = 0;
    while (i + 2 < s_len) {
        unsigned int triple = ((unsigned char)s[i] << 16)
                            | ((unsigned char)s[i+1] << 8)
                            | ((unsigned char)s[i+2]);
        out[j++] = b64_enc_table[(triple >> 18) & 0x3F];
        out[j++] = b64_enc_table[(triple >> 12) & 0x3F];
        out[j++] = b64_enc_table[(triple >>  6) & 0x3F];
        out[j++] = b64_enc_table[ triple        & 0x3F];
        i += 3;
    }

    if (i < s_len) {
        unsigned int triple = (unsigned char)s[i] << 16;
        if (i + 1 < s_len) triple |= (unsigned char)s[i+1] << 8;

        out[j++] = b64_enc_table[(triple >> 18) & 0x3F];
        out[j++] = b64_enc_table[(triple >> 12) & 0x3F];
        out[j++] = (i + 1 < s_len) ? b64_enc_table[(triple >> 6) & 0x3F] : '=';
        out[j++] = '=';
    }

    out[j] = '\0';
    return (lucis_encoding_str_result){ out, j };
}

/* ── Base64 decode (string → string) ────────────────────────────── */

lucis_encoding_str_result lucis_base64DecodeStr(const char* s, size_t s_len) {
    /* Strip trailing padding for length calculation */
    size_t pad = 0;
    if (s_len >= 1 && s[s_len - 1] == '=') pad++;
    if (s_len >= 2 && s[s_len - 2] == '=') pad++;

    size_t out_len = (s_len / 4) * 3 - pad;
    char* out = (char*)lucis_allocString(out_len + 1);
    if (!out) { return (lucis_encoding_str_result){ "", 0 }; }

    size_t i = 0, j = 0;
    while (i + 3 < s_len) {
        unsigned int a = b64_dec_table[(unsigned char)s[i]];
        unsigned int b = b64_dec_table[(unsigned char)s[i+1]];
        unsigned int c = b64_dec_table[(unsigned char)s[i+2]];
        unsigned int d = b64_dec_table[(unsigned char)s[i+3]];
        unsigned int triple = (a << 18) | (b << 12) | (c << 6) | d;

        if (j < out_len) out[j++] = (char)((triple >> 16) & 0xFF);
        if (j < out_len) out[j++] = (char)((triple >>  8) & 0xFF);
        if (j < out_len) out[j++] = (char)( triple        & 0xFF);
        i += 4;
    }

    out[j] = '\0';
    return (lucis_encoding_str_result){ out, j };
}

/* ── URL encode ─────────────────────────────────────────────────── */

static int is_unreserved(unsigned char c) {
    return isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~';
}

static const char hex_upper[] = "0123456789ABCDEF";

lucis_encoding_str_result lucis_urlEncode(const char* s, size_t s_len) {
    char* raw = (char*)malloc(s_len * 3 + 1);
    if (!raw) { return (lucis_encoding_str_result){ "", 0 }; }

    size_t j = 0;
    for (size_t i = 0; i < s_len; i++) {
        unsigned char c = (unsigned char)s[i];
        if (is_unreserved(c)) {
            raw[j++] = (char)c;
        } else {
            raw[j++] = '%';
            raw[j++] = hex_upper[c >> 4];
            raw[j++] = hex_upper[c & 0x0F];
        }
    }
    raw[j] = '\0';

    char* out = (char*)lucis_allocString(j + 1);
    if (!out) { free(raw); return (lucis_encoding_str_result){ "", 0 }; }
    memcpy(out, raw, j + 1);
    free(raw);
    return (lucis_encoding_str_result){ out, j };
}

/* ── URL decode ─────────────────────────────────────────────────── */

static int hex_val(unsigned char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

lucis_encoding_str_result lucis_urlDecode(const char* s, size_t s_len) {
    char* out = (char*)lucis_allocString(s_len + 1);
    if (!out) { return (lucis_encoding_str_result){ "", 0 }; }

    size_t j = 0;
    for (size_t i = 0; i < s_len; i++) {
        if (s[i] == '%' && i + 2 < s_len) {
            int hi = hex_val((unsigned char)s[i+1]);
            int lo = hex_val((unsigned char)s[i+2]);
            if (hi >= 0 && lo >= 0) {
                out[j++] = (char)((hi << 4) | lo);
                i += 2;
                continue;
            }
        }
        if (s[i] == '+') {
            out[j++] = ' ';
        } else {
            out[j++] = s[i];
        }
    }

    out[j] = '\0';
    return (lucis_encoding_str_result){ out, j };
}

/* ── Base64 encode from Vec<uint8> ────────────────────────────── */

lucis_encoding_str_result lucis_base64EncodeVec(const lucis_enc_vec_header* data) {
    return lucis_base64EncodeStr((const char*)data->ptr, data->len);
}

/* ── Base64 decode to Vec<uint8> ──────────────────────────────── */

void lucis_base64DecodeVec(lucis_enc_vec_header* out, const char* s, size_t s_len) {
    lucis_encoding_str_result decoded = lucis_base64DecodeStr(s, s_len);

    uint8_t* buf = (uint8_t*)malloc(decoded.len);
    if (buf && decoded.ptr) {
        memcpy(buf, decoded.ptr, decoded.len);
    }
    free((void*)decoded.ptr);

    out->ptr = buf;
    out->len = decoded.len;
    out->cap = decoded.len;
}
