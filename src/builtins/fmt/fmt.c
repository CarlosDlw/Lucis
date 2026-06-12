#include "fmt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>

/* ── helper ──────────────────────────────────────────────────────────── */
static lucis_fmt_str_result make_result(const char* src, size_t len) {
    lucis_fmt_str_result res = {NULL, 0};
    if (len == 0) { res.ptr = ""; return res; }
    char* buf = (char*)malloc(len);
    if (!buf) return res;
    memcpy(buf, src, len);
    res.ptr = buf;
    res.len = len;
    return res;
}

/* ── lpad ─────────────────────────────────────────────────────────────── */
lucis_fmt_str_result lucis_lpad(const char* s, size_t s_len,
                                   size_t width, uint8_t fill) {
    if (s_len >= width) return make_result(s, s_len);
    size_t pad = width - s_len;
    char* buf = (char*)malloc(width);
    if (!buf) { lucis_fmt_str_result r = {NULL, 0}; return r; }
    memset(buf, fill, pad);
    memcpy(buf + pad, s, s_len);
    lucis_fmt_str_result res;
    res.ptr = buf;
    res.len = width;
    return res;
}

/* ── rpad ─────────────────────────────────────────────────────────────── */
lucis_fmt_str_result lucis_rpad(const char* s, size_t s_len,
                                   size_t width, uint8_t fill) {
    if (s_len >= width) return make_result(s, s_len);
    size_t pad = width - s_len;
    char* buf = (char*)malloc(width);
    if (!buf) { lucis_fmt_str_result r = {NULL, 0}; return r; }
    memcpy(buf, s, s_len);
    memset(buf + s_len, fill, pad);
    lucis_fmt_str_result res;
    res.ptr = buf;
    res.len = width;
    return res;
}

/* ── center ───────────────────────────────────────────────────────────── */
lucis_fmt_str_result lucis_center(const char* s, size_t s_len,
                                     size_t width, uint8_t fill) {
    if (s_len >= width) return make_result(s, s_len);
    size_t total_pad = width - s_len;
    size_t left_pad = total_pad / 2;
    size_t right_pad = total_pad - left_pad;
    char* buf = (char*)malloc(width);
    if (!buf) { lucis_fmt_str_result r = {NULL, 0}; return r; }
    memset(buf, fill, left_pad);
    memcpy(buf + left_pad, s, s_len);
    memset(buf + left_pad + s_len, fill, right_pad);
    lucis_fmt_str_result res;
    res.ptr = buf;
    res.len = width;
    return res;
}

/* ── hex ──────────────────────────────────────────────────────────────── */
lucis_fmt_str_result lucis_fmtHex(uint64_t val) {
    char tmp[32];
    int n = snprintf(tmp, sizeof(tmp), "0x%" PRIx64, val);
    return make_result(tmp, (size_t)n);
}

/* ── hexUpper ─────────────────────────────────────────────────────────── */
lucis_fmt_str_result lucis_fmtHexUpper(uint64_t val) {
    char tmp[32];
    int n = snprintf(tmp, sizeof(tmp), "0x%" PRIX64, val);
    return make_result(tmp, (size_t)n);
}

/* ── oct ──────────────────────────────────────────────────────────────── */
lucis_fmt_str_result lucis_fmtOct(uint64_t val) {
    char tmp[32];
    int n = snprintf(tmp, sizeof(tmp), "0o%" PRIo64, val);
    return make_result(tmp, (size_t)n);
}

/* ── bin ──────────────────────────────────────────────────────────────── */
lucis_fmt_str_result lucis_fmtBin(uint64_t val) {
    if (val == 0) return make_result("0b0", 3);

    char tmp[67]; /* "0b" + 64 bits */
    int pos = 66;
    tmp[pos] = '\0';
    uint64_t v = val;
    while (v) {
        tmp[--pos] = (v & 1) ? '1' : '0';
        v >>= 1;
    }
    tmp[--pos] = 'b';
    tmp[--pos] = '0';
    size_t len = 66 - (size_t)pos;
    return make_result(tmp + pos, len);
}

/* ── fixed ────────────────────────────────────────────────────────────── */
lucis_fmt_str_result lucis_fixed(double val, uint32_t decimals) {
    char tmp[64];
    int n = snprintf(tmp, sizeof(tmp), "%.*f", (int)decimals, val);
    if (n < 0) n = 0;
    return make_result(tmp, (size_t)n);
}

/* ── scientific ───────────────────────────────────────────────────────── */
lucis_fmt_str_result lucis_scientific(double val) {
    char tmp[64];
    int n = snprintf(tmp, sizeof(tmp), "%e", val);
    if (n < 0) n = 0;
    return make_result(tmp, (size_t)n);
}

/* ── humanBytes ───────────────────────────────────────────────────────── */
lucis_fmt_str_result lucis_humanBytes(uint64_t bytes) {
    static const char* units[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB"};
    char tmp[64];
    int n;

    if (bytes < 1024) {
        n = snprintf(tmp, sizeof(tmp), "%" PRIu64 " B", bytes);
    } else {
        double val = (double)bytes;
        int unit = 0;
        while (val >= 1024.0 && unit < 6) {
            val /= 1024.0;
            unit++;
        }
        /* Use 1 decimal if fractional, 0 if whole */
        if (fabs(val - floor(val)) < 0.05) {
            n = snprintf(tmp, sizeof(tmp), "%.0f %s", val, units[unit]);
        } else {
            n = snprintf(tmp, sizeof(tmp), "%.1f %s", val, units[unit]);
        }
    }
    if (n < 0) n = 0;
    return make_result(tmp, (size_t)n);
}

/* ── commas ───────────────────────────────────────────────────────────── */
lucis_fmt_str_result lucis_commas(int64_t val) {
    char tmp[32];
    int n;
    int negative = 0;
    uint64_t uval;

    if (val < 0) {
        negative = 1;
        /* Handle INT64_MIN safely */
        uval = (uint64_t)(-(val + 1)) + 1;
    } else {
        uval = (uint64_t)val;
    }

    /* Write digits to tmp in reverse */
    int pos = 31;
    tmp[pos] = '\0';
    int digits = 0;

    if (uval == 0) {
        tmp[--pos] = '0';
    } else {
        while (uval > 0) {
            if (digits > 0 && digits % 3 == 0) {
                tmp[--pos] = ',';
            }
            tmp[--pos] = '0' + (int)(uval % 10);
            uval /= 10;
            digits++;
        }
    }

    if (negative) tmp[--pos] = '-';
    n = 31 - pos;
    return make_result(tmp + pos, (size_t)n);
}

/* ── percent ──────────────────────────────────────────────────────────── */
lucis_fmt_str_result lucis_percent(double val) {
    double pct = val * 100.0;
    char tmp[64];
    int n;
    /* Use 1 decimal if fractional, 0 if whole */
    if (fabs(pct - floor(pct)) < 0.05) {
        n = snprintf(tmp, sizeof(tmp), "%.0f%%", pct);
    } else {
        n = snprintf(tmp, sizeof(tmp), "%.1f%%", pct);
    }
    if (n < 0) n = 0;
    return make_result(tmp, (size_t)n);
}
