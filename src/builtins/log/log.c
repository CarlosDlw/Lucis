#include "log.h"
#include "string/string.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// ── println (signed) ─────────────────────────────────────────────────────────

void lux_println_i1(int val)          { printf("%d\n", val & 1); }
void lux_println_i8(signed char val)  { printf("%d\n", (int)val); }
void lux_println_i16(short val)       { printf("%d\n", (int)val); }
void lux_println_i32(int val)         { printf("%d\n", val); }
void lux_println_i64(long long val)   { printf("%lld\n", val); }
void lux_println_f32(float val)       { printf("%.9g\n", (double)val); }
void lux_println_f64(double val)       { printf("%.17g\n", val); }
void lux_println_bool(int val)        { printf("%s\n", (val & 1) ? "true" : "false"); }
void lux_println_char(char val)        { printf("%c\n", val); }
void lux_println_str(const char* data, unsigned long len) { printf("%.*s\n", (int)len, data); }

void lux_println_cstr(const char* str) { printf("%s\n", str ? str : "(null)"); }

static void print_i128_digits(__int128 val) {
    if (val > 9) print_i128_digits(val / 10);
    putchar('0' + (int)(val % 10));
}

void lux_println_i128(__int128 val) {
    if (val < 0) {
        putchar('-');
        val = -val;
    }
    print_i128_digits(val);
    putchar('\n');
}

// ── print (signed, no newline) ───────────────────────────────────────────────

void lux_print_i1(int val)          { printf("%d", val & 1); }
void lux_print_i8(signed char val)  { printf("%d", (int)val); }
void lux_print_i16(short val)       { printf("%d", (int)val); }
void lux_print_i32(int val)         { printf("%d", val); }
void lux_print_i64(long long val)   { printf("%lld", val); }
void lux_print_f32(float val)       { printf("%.9g", (double)val); }
void lux_print_f64(double val)       { printf("%.17g", val); }
void lux_print_bool(int val)        { printf("%s", (val & 1) ? "true" : "false"); }
void lux_print_char(char val)        { printf("%c", val); }
void lux_print_str(const char* data, unsigned long len)   { printf("%.*s", (int)len, data); }

void lux_print_cstr(const char* str) { printf("%s", str ? str : "(null)"); }

void lux_print_i128(__int128 val) {
    if (val < 0) {
        putchar('-');
        val = -val;
    }
    print_i128_digits(val);
}

// ── println (unsigned) ───────────────────────────────────────────────────────

void lux_println_u1(int val)                     { printf("%u\n", (unsigned)(val & 1)); }
void lux_println_u8(unsigned char val)            { printf("%u\n", (unsigned)val); }
void lux_println_u16(unsigned short val)          { printf("%u\n", (unsigned)val); }
void lux_println_u32(unsigned int val)            { printf("%u\n", val); }
void lux_println_u64(unsigned long long val)      { printf("%llu\n", val); }

static void print_u128_digits(unsigned __int128 val) {
    if (val > 9) print_u128_digits(val / 10);
    putchar('0' + (int)(val % 10));
}

void lux_println_u128(unsigned __int128 val) {
    print_u128_digits(val);
    putchar('\n');
}

// ── print (unsigned, no newline) ─────────────────────────────────────────────

void lux_print_u1(int val)                     { printf("%u", (unsigned)(val & 1)); }
void lux_print_u8(unsigned char val)            { printf("%u", (unsigned)val); }
void lux_print_u16(unsigned short val)          { printf("%u", (unsigned)val); }
void lux_print_u32(unsigned int val)            { printf("%u", val); }
void lux_print_u64(unsigned long long val)      { printf("%llu", val); }

void lux_print_u128(unsigned __int128 val) {
    print_u128_digits(val);
}

// ── eprintln (signed, stderr + newline) ──────────────────────────────────────

void lux_eprintln_i1(int val)          { fprintf(stderr, "%d\n", val & 1); }
void lux_eprintln_i8(signed char val)  { fprintf(stderr, "%d\n", (int)val); }
void lux_eprintln_i16(short val)       { fprintf(stderr, "%d\n", (int)val); }
void lux_eprintln_i32(int val)         { fprintf(stderr, "%d\n", val); }
void lux_eprintln_i64(long long val)   { fprintf(stderr, "%lld\n", val); }
void lux_eprintln_f32(float val)       { fprintf(stderr, "%.9g\n", (double)val); }
void lux_eprintln_f64(double val)      { fprintf(stderr, "%.17g\n", val); }
void lux_eprintln_bool(int val)        { fprintf(stderr, "%s\n", (val & 1) ? "true" : "false"); }
void lux_eprintln_char(char val)       { fprintf(stderr, "%c\n", val); }
void lux_eprintln_str(const char* data, unsigned long len) { fprintf(stderr, "%.*s\n", (int)len, data); }

void lux_eprintln_cstr(const char* str) { fprintf(stderr, "%s\n", str ? str : "(null)"); }

void lux_eprintln_i128(__int128 val) {
    if (val < 0) {
        fputc('-', stderr);
        val = -val;
    }
    // Reuse buffer approach for stderr
    char buf[40];
    int pos = 39;
    buf[pos] = '\0';
    unsigned __int128 uv = (unsigned __int128)val;
    do {
        buf[--pos] = '0' + (int)(uv % 10);
        uv /= 10;
    } while (uv > 0);
    fprintf(stderr, "%s\n", &buf[pos]);
}

// ── eprint (signed, stderr, no newline) ──────────────────────────────────────

void lux_eprint_i1(int val)          { fprintf(stderr, "%d", val & 1); }
void lux_eprint_i8(signed char val)  { fprintf(stderr, "%d", (int)val); }
void lux_eprint_i16(short val)       { fprintf(stderr, "%d", (int)val); }
void lux_eprint_i32(int val)         { fprintf(stderr, "%d", val); }
void lux_eprint_i64(long long val)   { fprintf(stderr, "%lld", val); }
void lux_eprint_f32(float val)       { fprintf(stderr, "%.9g", (double)val); }
void lux_eprint_f64(double val)      { fprintf(stderr, "%.17g", val); }
void lux_eprint_bool(int val)        { fprintf(stderr, "%s", (val & 1) ? "true" : "false"); }
void lux_eprint_char(char val)       { fprintf(stderr, "%c", val); }
void lux_eprint_str(const char* data, unsigned long len) { fprintf(stderr, "%.*s", (int)len, data); }

void lux_eprint_cstr(const char* str) { fprintf(stderr, "%s", str ? str : "(null)"); }

void lux_eprint_i128(__int128 val) {
    if (val < 0) {
        fputc('-', stderr);
        val = -val;
    }
    char buf[40];
    int pos = 39;
    buf[pos] = '\0';
    unsigned __int128 uv = (unsigned __int128)val;
    do {
        buf[--pos] = '0' + (int)(uv % 10);
        uv /= 10;
    } while (uv > 0);
    fprintf(stderr, "%s", &buf[pos]);
}

// ── eprintln (unsigned, stderr + newline) ────────────────────────────────────

void lux_eprintln_u1(int val)                     { fprintf(stderr, "%u\n", (unsigned)(val & 1)); }
void lux_eprintln_u8(unsigned char val)            { fprintf(stderr, "%u\n", (unsigned)val); }
void lux_eprintln_u16(unsigned short val)          { fprintf(stderr, "%u\n", (unsigned)val); }
void lux_eprintln_u32(unsigned int val)            { fprintf(stderr, "%u\n", val); }
void lux_eprintln_u64(unsigned long long val)      { fprintf(stderr, "%llu\n", val); }

void lux_eprintln_u128(unsigned __int128 val) {
    char buf[40];
    int pos = 39;
    buf[pos] = '\0';
    do {
        buf[--pos] = '0' + (int)(val % 10);
        val /= 10;
    } while (val > 0);
    fprintf(stderr, "%s\n", &buf[pos]);
}

// ── eprint (unsigned, stderr, no newline) ────────────────────────────────────

void lux_eprint_u1(int val)                     { fprintf(stderr, "%u", (unsigned)(val & 1)); }
void lux_eprint_u8(unsigned char val)            { fprintf(stderr, "%u", (unsigned)val); }
void lux_eprint_u16(unsigned short val)          { fprintf(stderr, "%u", (unsigned)val); }
void lux_eprint_u32(unsigned int val)            { fprintf(stderr, "%u", val); }
void lux_eprint_u64(unsigned long long val)      { fprintf(stderr, "%llu", val); }

void lux_eprint_u128(unsigned __int128 val) {
    char buf[40];
    int pos = 39;
    buf[pos] = '\0';
    do {
        buf[--pos] = '0' + (int)(val % 10);
        val /= 10;
    } while (val > 0);
    fprintf(stderr, "%s", &buf[pos]);
}

// ── dbg (stderr debug output: [file:line] = value) ──────────────────────────

#define DBG_SIGNED(suffix, type, fmt) \
    void lux_dbg_##suffix(const char* file, unsigned long flen, \
                             int line, type val) { \
        fprintf(stderr, "[%.*s:%d] = " fmt "\n", (int)flen, file, line, val); \
    }

#define DBG_UNSIGNED(suffix, type, fmt) \
    void lux_dbg_##suffix(const char* file, unsigned long flen, \
                             int line, type val) { \
        fprintf(stderr, "[%.*s:%d] = " fmt "\n", (int)flen, file, line, val); \
    }

DBG_SIGNED(i1,   int,         "%d")
DBG_SIGNED(i8,   signed char, "%d")
DBG_SIGNED(i16,  short,       "%d")
DBG_SIGNED(i32,  int,         "%d")
DBG_SIGNED(i64,  long long,   "%lld")
DBG_SIGNED(f32,  float,       "%g")
DBG_SIGNED(f64,  double,      "%g")

void lux_dbg_bool(const char* file, unsigned long flen,
                     int line, int val) {
    fprintf(stderr, "[%.*s:%d] = %s\n", (int)flen, file, line,
            (val & 1) ? "true" : "false");
}

void lux_dbg_char(const char* file, unsigned long flen,
                     int line, char val) {
    fprintf(stderr, "[%.*s:%d] = '%c'\n", (int)flen, file, line, val);
}

void lux_dbg_str(const char* file, unsigned long flen, int line,
                    const char* data, unsigned long len) {
    fprintf(stderr, "[%.*s:%d] = \"%.*s\"\n",
            (int)flen, file, line, (int)len, data);
}

void lux_dbg_cstr(const char* file, unsigned long flen, int line,
            const char* str) {
    fprintf(stderr, "[%.*s:%d] = \"%s\"\n",
        (int)flen, file, line, str ? str : "(null)");
}

void lux_dbg_i128(const char* file, unsigned long flen,
                     int line, __int128 val) {
    fprintf(stderr, "[%.*s:%d] = ", (int)flen, file, line);
    if (val < 0) {
        fputc('-', stderr);
        val = -val;
    }
    char buf[40];
    int pos = 39;
    buf[pos] = '\0';
    unsigned __int128 uv = (unsigned __int128)val;
    do {
        buf[--pos] = '0' + (int)(uv % 10);
        uv /= 10;
    } while (uv > 0);
    fprintf(stderr, "%s\n", &buf[pos]);
}

DBG_UNSIGNED(u1,   int,                "%u")
DBG_UNSIGNED(u8,   unsigned char,      "%u")
DBG_UNSIGNED(u16,  unsigned short,     "%u")
DBG_UNSIGNED(u32,  unsigned int,       "%u")
DBG_UNSIGNED(u64,  unsigned long long, "%llu")

void lux_dbg_u128(const char* file, unsigned long flen,
                     int line, unsigned __int128 val) {
    fprintf(stderr, "[%.*s:%d] = ", (int)flen, file, line);
    char buf[40];
    int pos = 39;
    buf[pos] = '\0';
    do {
        buf[--pos] = '0' + (int)(val % 10);
        val /= 10;
    } while (val > 0);
    fprintf(stderr, "%s\n", &buf[pos]);
}

// ═══════════════════════════════════════════════════════════════════════
//  sprintf — format "{}" placeholders with string args
// ═══════════════════════════════════════════════════════════════════════

lux_str_slice lux_sprintf(const char* fmt, unsigned long fmtLen,
                                const lux_str_slice* args, unsigned long argCount) {
    // First pass: compute total output length
    unsigned long outLen = 0;
    unsigned long argIdx = 0;
    for (unsigned long i = 0; i < fmtLen; i++) {
        if (i + 1 < fmtLen && fmt[i] == '{' && fmt[i + 1] == '}') {
            if (argIdx < argCount)
                outLen += args[argIdx].len;
            argIdx++;
            i++; // skip '}'
        } else {
            outLen++;
        }
    }

    // Allocate output buffer
    char* out = (char*)lux_allocString(outLen + 1);
    if (!out) {
        lux_str_slice empty = { "", 0 };
        return empty;
    }

    // Second pass: build the output
    unsigned long pos = 0;
    argIdx = 0;
    for (unsigned long i = 0; i < fmtLen; i++) {
        if (i + 1 < fmtLen && fmt[i] == '{' && fmt[i + 1] == '}') {
            if (argIdx < argCount) {
                memcpy(out + pos, args[argIdx].ptr, args[argIdx].len);
                pos += args[argIdx].len;
            }
            argIdx++;
            i++; // skip '}'
        } else {
            out[pos++] = fmt[i];
        }
    }
    out[pos] = '\0';

    lux_str_slice result = { out, pos };
    return result;
}
