#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ═══════════════════════════════════════════════════════════════════════════════
// std::hash — Hashing utilities
//
// FNV-1a based hashing for strings and integers, plus hash combining.
// ═══════════════════════════════════════════════════════════════════════════════

// FNV-1a hash of a string (ptr + len)
uint64_t lucis_hashString(const char* data, size_t len);

// Hash an integer (splitmix64-style mixing)
uint64_t lucis_hashInt(int64_t value);

// Combine two hash values (boost-style)
uint64_t lucis_hashCombine(uint64_t h1, uint64_t h2);

// Vec-based hashing
typedef struct { void* ptr; size_t len; size_t cap; } lucis_hash_vec_header;

uint64_t lucis_hashBytesVec(const lucis_hash_vec_header* data);
uint32_t lucis_crc32Bytes(const lucis_hash_vec_header* data);

#ifdef __cplusplus
}
#endif
