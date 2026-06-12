#include "conv.h"
#include "string/string.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── helper: make heap-allocated result from buffer ──────────────────── */
static lucis_conv_str_result make_result(const char* buf, size_t len) {
    lucis_conv_str_result res;
    char* out = (char*)lucis_allocString(len + 1);
    if (out) {
        memcpy(out, buf, len);
        out[len] = '\0';
        res.ptr = out;
        res.len = len;
    } else {
        res.ptr = NULL;
        res.len = 0;
    }
    return res;
}

static char* make_cstr(const char* s, size_t len) {
    char* buf = (char*)malloc(len + 1);
    if (!buf) return NULL;
    memcpy(buf, s, len);
    buf[len] = '\0';
    return buf;
}

/* ── itoa ────────────────────────────────────────────────────────────── */
lucis_conv_str_result lucis_itoa(int64_t value) {
    char buf[32];
    int n = snprintf(buf, sizeof(buf), "%ld", (long)value);
    return make_result(buf, (size_t)n);
}

/* ── itoaRadix ───────────────────────────────────────────────────────── */
lucis_conv_str_result lucis_itoaRadix(int64_t value, uint32_t radix) {
    if (radix < 2 || radix > 36) {
        return make_result("0", 1);
    }

    char buf[66]; /* 64 bits + sign + null */
    int negative = 0;
    uint64_t uval;

    if (value < 0) {
        negative = 1;
        uval = (uint64_t)(-(value + 1)) + 1;
    } else {
        uval = (uint64_t)value;
    }

    static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    char* p = buf + sizeof(buf) - 1;
    *p = '\0';

    if (uval == 0) {
        *(--p) = '0';
    } else {
        while (uval > 0) {
            *(--p) = digits[uval % radix];
            uval /= radix;
        }
    }
    if (negative) *(--p) = '-';

    size_t len = (size_t)(buf + sizeof(buf) - 1 - p);
    return make_result(p, len);
}

/* ── utoa ────────────────────────────────────────────────────────────── */
lucis_conv_str_result lucis_utoa(uint64_t value) {
    char buf[32];
    int n = snprintf(buf, sizeof(buf), "%lu", (unsigned long)value);
    return make_result(buf, (size_t)n);
}

/* ── ftoa ────────────────────────────────────────────────────────────── */
lucis_conv_str_result lucis_ftoa(double value) {
    char buf[64];
    int n = snprintf(buf, sizeof(buf), "%.17g", value);
    return make_result(buf, (size_t)n);
}

/* ── ftoaPrecision ───────────────────────────────────────────────────── */
lucis_conv_str_result lucis_ftoaPrecision(double value, uint32_t precision) {
    char fmt[16];
    snprintf(fmt, sizeof(fmt), "%%.%uf", precision);
    char buf[128];
    int n = snprintf(buf, sizeof(buf), fmt, value);
    return make_result(buf, (size_t)n);
}

/* ── atoi ────────────────────────────────────────────────────────────── */
int64_t lucis_atoi(const char* data, size_t len) {
    char* cstr = make_cstr(data, len);
    if (!cstr) return 0;
    int64_t result = strtoll(cstr, NULL, 10);
    free(cstr);
    return result;
}

/* ── atof ────────────────────────────────────────────────────────────── */
double lucis_atof(const char* data, size_t len) {
    char* cstr = make_cstr(data, len);
    if (!cstr) return 0.0;
    double result = strtod(cstr, NULL);
    free(cstr);
    return result;
}

/* ── toHex ───────────────────────────────────────────────────────────── */
lucis_conv_str_result lucis_toHex(uint64_t value) {
    char buf[32];
    int n = snprintf(buf, sizeof(buf), "%lx", (unsigned long)value);
    return make_result(buf, (size_t)n);
}

/* ── toOctal ─────────────────────────────────────────────────────────── */
lucis_conv_str_result lucis_toOctal(uint64_t value) {
    char buf[32];
    int n = snprintf(buf, sizeof(buf), "%lo", (unsigned long)value);
    return make_result(buf, (size_t)n);
}

/* ── toBinary ────────────────────────────────────────────────────────── */
lucis_conv_str_result lucis_toBinary(uint64_t value) {
    char buf[65];
    if (value == 0) {
        return make_result("0", 1);
    }
    char* p = buf + 64;
    *p = '\0';
    uint64_t v = value;
    while (v > 0) {
        *(--p) = (v & 1) ? '1' : '0';
        v >>= 1;
    }
    size_t len = (size_t)(buf + 64 - p);
    return make_result(p, len);
}

/* ── fromHex ─────────────────────────────────────────────────────────── */
uint64_t lucis_fromHex(const char* data, size_t len) {
    char* cstr = make_cstr(data, len);
    if (!cstr) return 0;
    uint64_t result = strtoull(cstr, NULL, 16);
    free(cstr);
    return result;
}

/* ── charToInt ───────────────────────────────────────────────────────── */
int32_t lucis_charToInt(int8_t c) {
    return (int32_t)(uint8_t)c;
}

/* ── intToChar ───────────────────────────────────────────────────────── */
int8_t lucis_intToChar(int32_t code) {
    return (int8_t)(code & 0xFF);
}
