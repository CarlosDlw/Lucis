#include "global.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ── Process Control ─────────────────────────────────────────────────────────

void lucis_exit(int code) {
    exit(code);
}

void lucis_panic(const char* msg, size_t len) {
    fprintf(stderr, "panic: %.*s\n", (int)len, msg);
    exit(1);
}

void lucis_assert(int cond, const char* file, size_t fileLen, int line) {
    if (!cond) {
        fprintf(stderr, "assertion failed at %.*s:%d\n",
                (int)fileLen, file, line);
        abort();
    }
}

void lucis_assertMsg(int cond, const char* msg, size_t msgLen) {
    if (!cond) {
        fprintf(stderr, "assertion failed: %.*s\n", (int)msgLen, msg);
        abort();
    }
}

void lucis_unreachable(const char* file, size_t fileLen, int line) {
    fprintf(stderr, "unreachable code reached at %.*s:%d\n",
            (int)fileLen, file, line);
    abort();
}

// ── Type Conversions (string → T) ──────────────────────────────────────────

int64_t lucis_toInt(const char* data, size_t len) {
    char buf[64];
    size_t n = len < sizeof(buf) - 1 ? len : sizeof(buf) - 1;
    memcpy(buf, data, n);
    buf[n] = '\0';
    char* end;
    int64_t result = strtoll(buf, &end, 10);
    if (end == buf) {
        fprintf(stderr, "toInt: invalid integer '%.*s'\n", (int)len, data);
        exit(1);
    }
    return result;
}

double lucis_toFloat(const char* data, size_t len) {
    char buf[128];
    size_t n = len < sizeof(buf) - 1 ? len : sizeof(buf) - 1;
    memcpy(buf, data, n);
    buf[n] = '\0';
    char* end;
    double result = strtod(buf, &end);
    if (end == buf) {
        fprintf(stderr, "toFloat: invalid float '%.*s'\n", (int)len, data);
        exit(1);
    }
    return result;
}

int lucis_toBool(const char* data, size_t len) {
    if (len == 4 && memcmp(data, "true", 4) == 0)  return 1;
    if (len == 5 && memcmp(data, "false", 5) == 0) return 0;
    fprintf(stderr, "toBool: invalid bool '%.*s' (expected \"true\" or \"false\")\n",
            (int)len, data);
    exit(1);
    return 0;
}

// ── toString (T → string) ──────────────────────────────────────────────────

static lucis_string_ret make_string(const char* buf, int len) {
    char* heap = (char*)malloc(len);
    memcpy(heap, buf, len);
    return (lucis_string_ret){ heap, (size_t)len };
}

lucis_string_ret lucis_toString_i8(int8_t val) {
    char buf[8];
    int len = snprintf(buf, sizeof(buf), "%d", (int)val);
    return make_string(buf, len);
}

lucis_string_ret lucis_toString_i16(int16_t val) {
    char buf[8];
    int len = snprintf(buf, sizeof(buf), "%d", (int)val);
    return make_string(buf, len);
}

lucis_string_ret lucis_toString_i32(int32_t val) {
    char buf[16];
    int len = snprintf(buf, sizeof(buf), "%d", val);
    return make_string(buf, len);
}

lucis_string_ret lucis_toString_i64(int64_t val) {
    char buf[24];
    int len = snprintf(buf, sizeof(buf), "%lld", (long long)val);
    return make_string(buf, len);
}

lucis_string_ret lucis_toString_u8(uint8_t val) {
    char buf[8];
    int len = snprintf(buf, sizeof(buf), "%u", (unsigned)val);
    return make_string(buf, len);
}

lucis_string_ret lucis_toString_u16(uint16_t val) {
    char buf[8];
    int len = snprintf(buf, sizeof(buf), "%u", (unsigned)val);
    return make_string(buf, len);
}

lucis_string_ret lucis_toString_u32(uint32_t val) {
    char buf[16];
    int len = snprintf(buf, sizeof(buf), "%u", val);
    return make_string(buf, len);
}

lucis_string_ret lucis_toString_u64(uint64_t val) {
    char buf[24];
    int len = snprintf(buf, sizeof(buf), "%llu", (unsigned long long)val);
    return make_string(buf, len);
}

lucis_string_ret lucis_toString_f32(float val) {
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%g", (double)val);
    return make_string(buf, len);
}

lucis_string_ret lucis_toString_f64(double val) {
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%g", val);
    return make_string(buf, len);
}

lucis_string_ret lucis_toString_bool(int val) {
    if (val & 1) return (lucis_string_ret){ "true",  4 };
    return (lucis_string_ret){ "false", 5 };
}

lucis_string_ret lucis_toString_char(char val) {
    char* heap = (char*)malloc(1);
    heap[0] = val;
    return (lucis_string_ret){ heap, 1 };
}
