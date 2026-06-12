#include "hash/hash.h"

// ═══════════════════════════════════════════════════════════════════════════════
// FNV-1a constants (64-bit)
// ═══════════════════════════════════════════════════════════════════════════════

#define FNV_OFFSET_BASIS 14695981039346656037ULL
#define FNV_PRIME        1099511628211ULL

// ═══════════════════════════════════════════════════════════════════════════════
// hashString — FNV-1a hash of a string
// ═══════════════════════════════════════════════════════════════════════════════

uint64_t lucis_hashString(const char* data, size_t len) {
    uint64_t hash = FNV_OFFSET_BASIS;
    for (size_t i = 0; i < len; i++) {
        hash ^= (uint8_t)data[i];
        hash *= FNV_PRIME;
    }
    return hash;
}

// ═══════════════════════════════════════════════════════════════════════════════
// hashInt — splitmix64-style integer hash
// ═══════════════════════════════════════════════════════════════════════════════

uint64_t lucis_hashInt(int64_t value) {
    uint64_t x = (uint64_t)value;
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return x;
}

// ═══════════════════════════════════════════════════════════════════════════════
// hashCombine — boost-style hash combining
// ═══════════════════════════════════════════════════════════════════════════════

uint64_t lucis_hashCombine(uint64_t h1, uint64_t h2) {
    h1 ^= h2 + 0x9e3779b97f4a7c15ULL + (h1 << 12) + (h1 >> 4);
    return h1;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Vec-based hashing
// ═══════════════════════════════════════════════════════════════════════════════

uint64_t lucis_hashBytesVec(const lucis_hash_vec_header* data) {
    return lucis_hashString((const char*)data->ptr, data->len);
}

uint32_t lucis_crc32Bytes(const lucis_hash_vec_header* data) {
    static uint32_t table[256];
    static int inited = 0;
    if (!inited) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        inited = 1;
    }

    const uint8_t* buf = (const uint8_t*)data->ptr;
    size_t len = data->len;
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc = table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}
