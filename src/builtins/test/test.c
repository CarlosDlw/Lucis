#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>

/* ── Internal helpers ───────────────────────────────────────────── */

static void fail_abort(const char* type, const char* detail,
                       const char* file, size_t fileLen, int32_t line) {
    fprintf(stderr, "\033[31mASSERTION FAILED\033[0m [%s]", type);
    if (file && fileLen > 0)
        fprintf(stderr, " at %.*s:%d", (int)fileLen, file, line);
    if (detail) fprintf(stderr, ": %s", detail);
    fprintf(stderr, "\n");
    abort();
}

/* ── assertEqual ────────────────────────────────────────────────── */

void lux_assertEqualI64(int64_t a, int64_t b,
                        const char* file, size_t fileLen, int32_t line) {
    if (a != b) {
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "expected %" PRId64 ", got %" PRId64, b, a);
        fail_abort("assertEqual", buf, file, fileLen, line);
    }
}

void lux_assertEqualF64(double a, double b,
                        const char* file, size_t fileLen, int32_t line) {
    if (a != b) {
        char buf[128];
        snprintf(buf, sizeof(buf), "expected %g, got %g", b, a);
        fail_abort("assertEqual", buf, file, fileLen, line);
    }
}

void lux_assertEqualStr(const char* a, size_t alen,
                        const char* b, size_t blen,
                        const char* file, size_t fileLen, int32_t line) {
    if (alen != blen || memcmp(a, b, alen) != 0) {
        char buf[512];
        snprintf(buf, sizeof(buf), "expected \"%.*s\", got \"%.*s\"",
                 (int)(blen > 200 ? 200 : blen), b,
                 (int)(alen > 200 ? 200 : alen), a);
        fail_abort("assertEqual", buf, file, fileLen, line);
    }
}

void lux_assertEqualBool(int32_t a, int32_t b,
                         const char* file, size_t fileLen, int32_t line) {
    if ((!a) != (!b)) {
        char buf[64];
        snprintf(buf, sizeof(buf), "expected %s, got %s",
                 b ? "true" : "false", a ? "true" : "false");
        fail_abort("assertEqual", buf, file, fileLen, line);
    }
}

void lux_assertEqualChar(int8_t a, int8_t b,
                         const char* file, size_t fileLen, int32_t line) {
    if (a != b) {
        char buf[64];
        snprintf(buf, sizeof(buf), "expected '%c' (%d), got '%c' (%d)",
                 (char)b, (int)b, (char)a, (int)a);
        fail_abort("assertEqual", buf, file, fileLen, line);
    }
}

/* ── assertNotEqual ─────────────────────────────────────────────── */

void lux_assertNotEqualI64(int64_t a, int64_t b,
                           const char* file, size_t fileLen, int32_t line) {
    if (a == b) {
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "values should differ, both are %" PRId64, a);
        fail_abort("assertNotEqual", buf, file, fileLen, line);
    }
}

void lux_assertNotEqualF64(double a, double b,
                           const char* file, size_t fileLen, int32_t line) {
    if (a == b) {
        char buf[128];
        snprintf(buf, sizeof(buf), "values should differ, both are %g", a);
        fail_abort("assertNotEqual", buf, file, fileLen, line);
    }
}

void lux_assertNotEqualStr(const char* a, size_t alen,
                           const char* b, size_t blen,
                           const char* file, size_t fileLen, int32_t line) {
    if (alen == blen && memcmp(a, b, alen) == 0) {
        char buf[512];
        snprintf(buf, sizeof(buf), "values should differ, both are \"%.*s\"",
                 (int)(alen > 200 ? 200 : alen), a);
        fail_abort("assertNotEqual", buf, file, fileLen, line);
    }
}

void lux_assertNotEqualBool(int32_t a, int32_t b,
                            const char* file, size_t fileLen, int32_t line) {
    if ((!a) == (!b)) {
        char buf[64];
        snprintf(buf, sizeof(buf), "values should differ, both are %s",
                 a ? "true" : "false");
        fail_abort("assertNotEqual", buf, file, fileLen, line);
    }
}

void lux_assertNotEqualChar(int8_t a, int8_t b,
                            const char* file, size_t fileLen, int32_t line) {
    if (a == b) {
        char buf[64];
        snprintf(buf, sizeof(buf),
                 "values should differ, both are '%c' (%d)",
                 (char)a, (int)a);
        fail_abort("assertNotEqual", buf, file, fileLen, line);
    }
}

/* ── assertTrue / assertFalse ───────────────────────────────────── */

void lux_assertTrue(int32_t cond,
                    const char* file, size_t fileLen, int32_t line) {
    if (!cond)
        fail_abort("assertTrue", "expected true, got false", file, fileLen, line);
}

void lux_assertFalse(int32_t cond,
                     const char* file, size_t fileLen, int32_t line) {
    if (cond)
        fail_abort("assertFalse", "expected false, got true", file, fileLen, line);
}

/* ── assertGreater ──────────────────────────────────────────────── */

void lux_assertGreaterI64(int64_t a, int64_t b,
                          const char* file, size_t fileLen, int32_t line) {
    if (!(a > b)) {
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "expected %" PRId64 " > %" PRId64, a, b);
        fail_abort("assertGreater", buf, file, fileLen, line);
    }
}

void lux_assertGreaterF64(double a, double b,
                          const char* file, size_t fileLen, int32_t line) {
    if (!(a > b)) {
        char buf[128];
        snprintf(buf, sizeof(buf), "expected %g > %g", a, b);
        fail_abort("assertGreater", buf, file, fileLen, line);
    }
}

/* ── assertLess ─────────────────────────────────────────────────── */

void lux_assertLessI64(int64_t a, int64_t b,
                       const char* file, size_t fileLen, int32_t line) {
    if (!(a < b)) {
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "expected %" PRId64 " < %" PRId64, a, b);
        fail_abort("assertLess", buf, file, fileLen, line);
    }
}

void lux_assertLessF64(double a, double b,
                       const char* file, size_t fileLen, int32_t line) {
    if (!(a < b)) {
        char buf[128];
        snprintf(buf, sizeof(buf), "expected %g < %g", a, b);
        fail_abort("assertLess", buf, file, fileLen, line);
    }
}

/* ── assertGreaterEq ────────────────────────────────────────────── */

void lux_assertGreaterEqI64(int64_t a, int64_t b,
                            const char* file, size_t fileLen, int32_t line) {
    if (!(a >= b)) {
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "expected %" PRId64 " >= %" PRId64, a, b);
        fail_abort("assertGreaterEq", buf, file, fileLen, line);
    }
}

void lux_assertGreaterEqF64(double a, double b,
                            const char* file, size_t fileLen, int32_t line) {
    if (!(a >= b)) {
        char buf[128];
        snprintf(buf, sizeof(buf), "expected %g >= %g", a, b);
        fail_abort("assertGreaterEq", buf, file, fileLen, line);
    }
}

/* ── assertLessEq ───────────────────────────────────────────────── */

void lux_assertLessEqI64(int64_t a, int64_t b,
                         const char* file, size_t fileLen, int32_t line) {
    if (!(a <= b)) {
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "expected %" PRId64 " <= %" PRId64, a, b);
        fail_abort("assertLessEq", buf, file, fileLen, line);
    }
}

void lux_assertLessEqF64(double a, double b,
                         const char* file, size_t fileLen, int32_t line) {
    if (!(a <= b)) {
        char buf[128];
        snprintf(buf, sizeof(buf), "expected %g <= %g", a, b);
        fail_abort("assertLessEq", buf, file, fileLen, line);
    }
}

/* ── assertStringContains ───────────────────────────────────────── */

void lux_assertStringContains(const char* s, size_t slen,
                              const char* sub, size_t sublen,
                              const char* file, size_t fileLen, int32_t line) {
    if (sublen == 0) return; /* empty substring always contained */
    if (sublen > slen) {
        char buf[512];
        snprintf(buf, sizeof(buf),
                 "\"%.*s\" does not contain \"%.*s\"",
                 (int)(slen > 200 ? 200 : slen), s,
                 (int)(sublen > 200 ? 200 : sublen), sub);
        fail_abort("assertStringContains", buf, file, fileLen, line);
    }
    for (size_t i = 0; i <= slen - sublen; i++) {
        if (memcmp(s + i, sub, sublen) == 0) return;
    }
    char buf[512];
    snprintf(buf, sizeof(buf),
             "\"%.*s\" does not contain \"%.*s\"",
             (int)(slen > 200 ? 200 : slen), s,
             (int)(sublen > 200 ? 200 : sublen), sub);
    fail_abort("assertStringContains", buf, file, fileLen, line);
}

/* ── assertNear ─────────────────────────────────────────────────── */

void lux_assertNear(double a, double b, double epsilon,
                    const char* file, size_t fileLen, int32_t line) {
    if (fabs(a - b) > epsilon) {
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "|%g - %g| = %g > epsilon %g",
                 a, b, fabs(a - b), epsilon);
        fail_abort("assertNear", buf, file, fileLen, line);
    }
}

/* ── Utilities ──────────────────────────────────────────────────── */

void lux_testFail(const char* msg, size_t msglen,
                  const char* file, size_t fileLen, int32_t line) {
    char buf[512];
    snprintf(buf, sizeof(buf), "%.*s", (int)(msglen > 500 ? 500 : msglen), msg);
    fail_abort("fail", buf, file, fileLen, line);
}

void lux_testSkip(const char* msg, size_t msglen,
                  const char* file, size_t fileLen, int32_t line) {
    fprintf(stderr, "\033[33mSKIP\033[0m");
    if (file && fileLen > 0)
        fprintf(stderr, " at %.*s:%d", (int)fileLen, file, line);
    fprintf(stderr, ": %.*s\n", (int)(msglen > 500 ? 500 : msglen), msg);
    exit(0);
}

void lux_testLog(const char* msg, size_t msglen,
                 const char* file, size_t fileLen, int32_t line) {
    fprintf(stderr, "\033[36mLOG\033[0m");
    if (file && fileLen > 0)
        fprintf(stderr, " at %.*s:%d", (int)fileLen, file, line);
    fprintf(stderr, ": %.*s\n", (int)(msglen > 500 ? 500 : msglen), msg);
}
