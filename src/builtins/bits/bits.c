#include "bits/bits.h"

// ═══════════════════════════════════════════════════════════════════════════════
// Counting
// ═══════════════════════════════════════════════════════════════════════════════

uint32_t lucis_popcount(uint64_t v) {
    return (uint32_t)__builtin_popcountll(v);
}

uint32_t lucis_ctz(uint64_t v) {
    if (v == 0) return 64;
    return (uint32_t)__builtin_ctzll(v);
}

uint32_t lucis_clz(uint64_t v) {
    if (v == 0) return 64;
    return (uint32_t)__builtin_clzll(v);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Rotation
// ═══════════════════════════════════════════════════════════════════════════════

uint64_t lucis_rotl(uint64_t v, uint32_t n) {
    n &= 63;
    if (n == 0) return v;
    return (v << n) | (v >> (64 - n));
}

uint64_t lucis_rotr(uint64_t v, uint32_t n) {
    n &= 63;
    if (n == 0) return v;
    return (v >> n) | (v << (64 - n));
}

// ═══════════════════════════════════════════════════════════════════════════════
// Byte / bit reordering
// ═══════════════════════════════════════════════════════════════════════════════

uint64_t lucis_bswap(uint64_t v) {
    return __builtin_bswap64(v);
}

uint64_t lucis_bitReverse(uint64_t v) {
    v = ((v & 0x5555555555555555ULL) << 1)  | ((v >> 1)  & 0x5555555555555555ULL);
    v = ((v & 0x3333333333333333ULL) << 2)  | ((v >> 2)  & 0x3333333333333333ULL);
    v = ((v & 0x0F0F0F0F0F0F0F0FULL) << 4) | ((v >> 4)  & 0x0F0F0F0F0F0F0F0FULL);
    return __builtin_bswap64(v);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Power-of-two
// ═══════════════════════════════════════════════════════════════════════════════

int lucis_isPow2(uint64_t v) {
    return v != 0 && (v & (v - 1)) == 0;
}

uint64_t lucis_nextPow2(uint64_t v) {
    if (v == 0) return 1;
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v |= v >> 32;
    return v + 1;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Bit field operations
// ═══════════════════════════════════════════════════════════════════════════════

uint64_t lucis_extractBits(uint64_t v, uint32_t pos, uint32_t width) {
    if (width == 0 || pos >= 64) return 0;
    if (width > 64) width = 64;
    uint64_t mask = (width >= 64) ? ~0ULL : ((1ULL << width) - 1);
    return (v >> pos) & mask;
}

uint64_t lucis_setBit(uint64_t v, uint32_t pos) {
    return v | (1ULL << (pos & 63));
}

uint64_t lucis_clearBit(uint64_t v, uint32_t pos) {
    return v & ~(1ULL << (pos & 63));
}

uint64_t lucis_toggleBit(uint64_t v, uint32_t pos) {
    return v ^ (1ULL << (pos & 63));
}

int lucis_testBit(uint64_t v, uint32_t pos) {
    return (v >> (pos & 63)) & 1;
}
