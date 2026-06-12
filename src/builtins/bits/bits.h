#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ═══════════════════════════════════════════════════════════════════════════════
// std::bits — Bit manipulation utilities
// ═══════════════════════════════════════════════════════════════════════════════

// Counting
uint32_t lucis_popcount(uint64_t v);
uint32_t lucis_ctz(uint64_t v);
uint32_t lucis_clz(uint64_t v);

// Rotation
uint64_t lucis_rotl(uint64_t v, uint32_t n);
uint64_t lucis_rotr(uint64_t v, uint32_t n);

// Byte / bit reordering
uint64_t lucis_bswap(uint64_t v);
uint64_t lucis_bitReverse(uint64_t v);

// Power-of-two checks
int      lucis_isPow2(uint64_t v);
uint64_t lucis_nextPow2(uint64_t v);

// Bit field operations
uint64_t lucis_extractBits(uint64_t v, uint32_t pos, uint32_t width);
uint64_t lucis_setBit(uint64_t v, uint32_t pos);
uint64_t lucis_clearBit(uint64_t v, uint32_t pos);
uint64_t lucis_toggleBit(uint64_t v, uint32_t pos);
int      lucis_testBit(uint64_t v, uint32_t pos);

#ifdef __cplusplus
}
#endif
