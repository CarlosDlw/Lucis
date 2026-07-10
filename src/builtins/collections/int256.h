#ifndef LUCIS_INT256_H
#define LUCIS_INT256_H

#include <stdint.h>
#include <string.h>

// ── 256-bit integer types ───────────────────────────────────────────────
// Used by the C runtime for Lucis's `intinf` (arbitrary precision integer).
// LLVM IR treats it as i256 (32 bytes). Since there's no native __int256_t
// in GCC/Clang, we use a struct of two 128-bit halves.

// Check compiler support for 128-bit integers
#ifndef __SIZEOF_INT128__
#error "Compiler must support __int128 for intinf support"
#endif

typedef struct {
    __int128         lo;  // lower 128 bits (least significant)
    __int128         hi;  // upper 128 bits (most significant, determines sign)
} lucis_int256_t;

typedef struct {
    unsigned __int128 lo;
    unsigned __int128 hi;
} lucis_uint256_t;

// ── Equality (bitwise) ─────────────────────────────────────────────────
static inline int lucis_int256_eq(const lucis_int256_t* a, const lucis_int256_t* b) {
    return a->lo == b->lo && a->hi == b->hi;
}

// ── Comparison (signed) ────────────────────────────────────────────────
static inline int lucis_int256_cmp(const lucis_int256_t* a, const lucis_int256_t* b) {
    if (a->hi < b->hi) return -1;
    if (a->hi > b->hi) return 1;
    // hi equal: compare lo as unsigned (since it's the lower bits regardless of sign)
    if ((unsigned __int128)a->lo < (unsigned __int128)b->lo) return -1;
    if ((unsigned __int128)a->lo > (unsigned __int128)b->lo) return 1;
    return 0;
}

// ── Zero-initialized value ─────────────────────────────────────────────
#define LUCIS_INT256_ZERO  ((lucis_int256_t){0, 0})

#endif // LUCIS_INT256_H
