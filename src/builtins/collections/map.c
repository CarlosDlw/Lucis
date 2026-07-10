#include "collections/map.h"
#include "../string/string.h"
#include "int256.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ═══════════════════════════════════════════════════════════════════════════
// Generic hash map core — open addressing with linear probing
// ═══════════════════════════════════════════════════════════════════════════

#define MAP_STATE_EMPTY     0
#define MAP_STATE_OCCUPIED  1
#define MAP_STATE_TOMBSTONE 2

#define MAP_INITIAL_CAP     16
#define MAP_LOAD_FACTOR_NUM 3
#define MAP_LOAD_FACTOR_DEN 4  // 75%

typedef uint64_t (*map_hash_fn)(const void* key);
typedef int      (*map_eq_fn)(const void* a, const void* b);

// ── Hash functions ───────────────────────────────────────────────────────

static uint64_t splitmix64(uint64_t x) {
    x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27; x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return x;
}

static uint64_t hash_key_i32(const void* key) {
    int32_t k = *(const int32_t*)key;
    return splitmix64((uint64_t)(uint32_t)k);
}
static int eq_key_i32(const void* a, const void* b) {
    return *(const int32_t*)a == *(const int32_t*)b;
}

static uint64_t hash_key_i64(const void* key) {
    int64_t k = *(const int64_t*)key;
    return splitmix64((uint64_t)k);
}
static int eq_key_i64(const void* a, const void* b) {
    return *(const int64_t*)a == *(const int64_t*)b;
}

static uint64_t hash_key_u64(const void* key) {
    return splitmix64(*(const uint64_t*)key);
}
static int eq_key_u64(const void* a, const void* b) {
    return *(const uint64_t*)a == *(const uint64_t*)b;
}

static uint64_t hash_key_u128(const void* key) {
    const __uint128_t* k = (const __uint128_t*)key;
    uint64_t low = (uint64_t)*k;
    uint64_t high = (uint64_t)(*k >> 64);
    return splitmix64(low ^ splitmix64(high));
}
static int eq_key_u128(const void* a, const void* b) {
    return *(const __uint128_t*)a == *(const __uint128_t*)b;
}

static uint64_t hash_key_i128(const void* key) {
    const __int128_t* k = (const __int128_t*)key;
    uint64_t low = (uint64_t)(__uint128_t)*k;
    uint64_t high = (uint64_t)((__uint128_t)*k >> 64);
    return splitmix64(low ^ splitmix64(high));
}
static int eq_key_i128(const void* a, const void* b) {
    return *(const __int128_t*)a == *(const __int128_t*)b;
}

// 1-byte hash/eq — for uint1/int1 as map key
static uint64_t hash_key_u1(const void* key) {
    uint8_t k = *(const uint8_t*)key;
    return splitmix64((uint64_t)k);
}
static int eq_key_u1(const void* a, const void* b) {
    return *(const uint8_t*)a == *(const uint8_t*)b;
}

static uint64_t hash_key_i1(const void* key) {
    uint8_t k = *(const uint8_t*)key;
    return splitmix64((uint64_t)k);
}
static int eq_key_i1(const void* a, const void* b) {
    return *(const uint8_t*)a == *(const uint8_t*)b;
}

// 32-byte (256-bit) hash/eq — for intinf as map key
static uint64_t hash_key_iinf(const void* key) {
    const uint64_t* k = (const uint64_t*)key;
    uint64_t h = splitmix64(k[0]);
    h ^= splitmix64(k[1]);
    h ^= splitmix64(k[2]);
    h ^= splitmix64(k[3]);
    return h;
}
static int eq_key_iinf(const void* a, const void* b) {
    return memcmp(a, b, sizeof(lucis_int256_t)) == 0;
}

static uint64_t hash_key_str(const void* key) {
    const lucis_map_string* s = (const lucis_map_string*)key;
    uint64_t h = 14695981039346656037ULL;
    for (size_t i = 0; i < s->len; i++) {
        h ^= (uint8_t)s->ptr[i];
        h *= 1099511628211ULL;
    }
    return h;
}
static int eq_key_str(const void* a, const void* b) {
    const lucis_map_string* sa = (const lucis_map_string*)a;
    const lucis_map_string* sb = (const lucis_map_string*)b;
    return sa->len == sb->len &&
           (sa->len == 0 || memcmp(sa->ptr, sb->ptr, sa->len) == 0);
}

// ── Core operations ──────────────────────────────────────────────────────

static void map_core_init(lucis_map_header* m, size_t key_size, size_t val_size) {
    m->states   = (uint8_t*)calloc(MAP_INITIAL_CAP, 1);
    m->keys     = calloc(MAP_INITIAL_CAP, key_size);
    m->values   = calloc(MAP_INITIAL_CAP, val_size);
    m->hashes   = (uint64_t*)calloc(MAP_INITIAL_CAP, sizeof(uint64_t));
    m->len      = 0;
    m->cap      = MAP_INITIAL_CAP;
    m->key_size = key_size;
    m->val_size = val_size;
}

static void map_core_free(lucis_map_header* m) {
    free(m->states);
    free(m->keys);
    free(m->values);
    free(m->hashes);
    m->states = NULL;
    m->keys   = NULL;
    m->values = NULL;
    m->hashes = NULL;
    m->len    = 0;
    m->cap    = 0;
}

static void map_core_clear(lucis_map_header* m) {
    memset(m->states, 0, m->cap);
    m->len = 0;
}

static inline void* key_at(lucis_map_header* m, size_t idx) {
    return (uint8_t*)m->keys + idx * m->key_size;
}

static inline void* val_at(lucis_map_header* m, size_t idx) {
    return (uint8_t*)m->values + idx * m->val_size;
}

// Find the slot for a key. Returns the slot index.
// If the key is found, *found is set to 1 and the slot has the key.
// If not found, *found is set to 0 and the slot is the best insertion point.
static size_t map_core_find(lucis_map_header* m, const void* key,
                            uint64_t hash, map_eq_fn eq, int* found) {
    size_t mask = m->cap - 1;
    size_t idx  = (size_t)(hash & mask);
    size_t first_tombstone = SIZE_MAX;

    for (size_t i = 0; i < m->cap; i++) {
        size_t slot = (idx + i) & mask;
        uint8_t state = m->states[slot];

        if (state == MAP_STATE_EMPTY) {
            *found = 0;
            return (first_tombstone != SIZE_MAX) ? first_tombstone : slot;
        }
        if (state == MAP_STATE_TOMBSTONE) {
            if (first_tombstone == SIZE_MAX)
                first_tombstone = slot;
            continue;
        }
        // state == OCCUPIED
        if (m->hashes[slot] == hash && eq(key_at(m, slot), key)) {
            *found = 1;
            return slot;
        }
    }

    // Table is full of tombstones — shouldn't happen with proper load factor
    *found = 0;
    return (first_tombstone != SIZE_MAX) ? first_tombstone : 0;
}

static void map_core_grow(lucis_map_header* m, map_hash_fn hash_fn, map_eq_fn eq) {
    size_t old_cap = m->cap;
    uint8_t* old_states = m->states;
    void*    old_keys   = m->keys;
    void*    old_values = m->values;
    uint64_t* old_hashes = m->hashes;

    size_t new_cap = old_cap * 2;
    m->states = (uint8_t*)calloc(new_cap, 1);
    m->keys   = calloc(new_cap, m->key_size);
    m->values = calloc(new_cap, m->val_size);
    m->hashes = (uint64_t*)calloc(new_cap, sizeof(uint64_t));
    m->cap    = new_cap;
    m->len    = 0;

    for (size_t i = 0; i < old_cap; i++) {
        if (old_states[i] == MAP_STATE_OCCUPIED) {
            void* ok = (uint8_t*)old_keys   + i * m->key_size;
            void* ov = (uint8_t*)old_values + i * m->val_size;
            uint64_t h = old_hashes[i];

            int found;
            size_t slot = map_core_find(m, ok, h, eq, &found);
            m->states[slot] = MAP_STATE_OCCUPIED;
            m->hashes[slot] = h;
            memcpy(key_at(m, slot), ok, m->key_size);
            memcpy(val_at(m, slot), ov, m->val_size);
            m->len++;
        }
    }

    free(old_states);
    free(old_keys);
    free(old_values);
    free(old_hashes);
}

static void map_core_set(lucis_map_header* m, const void* key, const void* val,
                         map_hash_fn hash_fn, map_eq_fn eq) {
    // Grow if load > 75%
    if ((m->len + 1) * MAP_LOAD_FACTOR_DEN > m->cap * MAP_LOAD_FACTOR_NUM)
        map_core_grow(m, hash_fn, eq);

    uint64_t h = hash_fn(key);
    int found;
    size_t slot = map_core_find(m, key, h, eq, &found);

    if (found) {
        // Update existing value
        memcpy(val_at(m, slot), val, m->val_size);
    } else {
        // Insert new entry
        m->states[slot] = MAP_STATE_OCCUPIED;
        m->hashes[slot] = h;
        memcpy(key_at(m, slot), key, m->key_size);
        memcpy(val_at(m, slot), val, m->val_size);
        m->len++;
    }
}

// Returns 1 if found and copies value, 0 if not found.
static int map_core_get(lucis_map_header* m, const void* key, void* val_out,
                        map_hash_fn hash_fn, map_eq_fn eq) {
    if (m->len == 0) return 0;
    uint64_t h = hash_fn(key);
    int found;
    size_t slot = map_core_find(m, key, h, eq, &found);
    if (found) {
        memcpy(val_out, val_at(m, slot), m->val_size);
        return 1;
    }
    return 0;
}

static int map_core_has(lucis_map_header* m, const void* key,
                        map_hash_fn hash_fn, map_eq_fn eq) {
    if (m->len == 0) return 0;
    uint64_t h = hash_fn(key);
    int found;
    map_core_find(m, key, h, eq, &found);
    return found;
}

static int map_core_remove(lucis_map_header* m, const void* key,
                           map_hash_fn hash_fn, map_eq_fn eq) {
    if (m->len == 0) return 0;
    uint64_t h = hash_fn(key);
    int found;
    size_t slot = map_core_find(m, key, h, eq, &found);
    if (found) {
        m->states[slot] = MAP_STATE_TOMBSTONE;
        m->len--;
        return 1;
    }
    return 0;
}

// ═══════════════════════════════════════════════════════════════════════════
// Macro-generated typed wrappers
// ═══════════════════════════════════════════════════════════════════════════

// ── Integer key × numeric value ──────────────────────────────────────────

#define LUCIS_MAP_IMPL_INT(KT, KS, VT, VS, HASH_FN, EQ_FN)                \
void lucis_map_init_##KS##_##VS(lucis_map_header* m) {                     \
    map_core_init(m, sizeof(KT), sizeof(VT));                                \
}                                                                            \
void lucis_map_free_##KS##_##VS(lucis_map_header* m) {                     \
    map_core_free(m);                                                        \
}                                                                            \
size_t lucis_map_len_##KS##_##VS(const lucis_map_header* m) {              \
    return m->len;                                                           \
}                                                                            \
int lucis_map_isEmpty_##KS##_##VS(const lucis_map_header* m) {             \
    return m->len == 0;                                                      \
}                                                                            \
void lucis_map_set_##KS##_##VS(lucis_map_header* m, KT key, VT val) {     \
    map_core_set(m, &key, &val, HASH_FN, EQ_FN);                            \
}                                                                            \
VT lucis_map_get_##KS##_##VS(lucis_map_header* m, KT key) {               \
    VT result;                                                               \
    if (!map_core_get(m, &key, &result, HASH_FN, EQ_FN)) {                  \
        fprintf(stderr, "lucis panic: Map::get key not found\n");           \
        abort();                                                             \
    }                                                                        \
    return result;                                                           \
}                                                                            \
VT lucis_map_getOrDefault_##KS##_##VS(lucis_map_header* m,                \
                                       KT key, VT def) {                    \
    VT result;                                                               \
    if (map_core_get(m, &key, &result, HASH_FN, EQ_FN)) return result;      \
    return def;                                                              \
}                                                                            \
int lucis_map_has_##KS##_##VS(lucis_map_header* m, KT key) {              \
    return map_core_has(m, &key, HASH_FN, EQ_FN);                           \
}                                                                            \
int lucis_map_remove_##KS##_##VS(lucis_map_header* m, KT key) {           \
    return map_core_remove(m, &key, HASH_FN, EQ_FN);                        \
}                                                                            \
void lucis_map_clear_##KS##_##VS(lucis_map_header* m) {                   \
    map_core_clear(m);                                                       \
}                                                                            \
void lucis_map_keys_##KS##_##VS(lucis_map_header* m,                      \
                                  lucis_map_vec_out* out) {                \
    KT* arr = (KT*)malloc(m->len * sizeof(KT));                             \
    size_t n = 0;                                                            \
    for (size_t i = 0; i < m->cap; i++) {                                    \
        if (m->states[i] == MAP_STATE_OCCUPIED)                              \
            arr[n++] = *(KT*)((uint8_t*)m->keys + i * m->key_size);         \
    }                                                                        \
    out->ptr = arr; out->len = n; out->cap = n;                              \
}                                                                            \
void lucis_map_values_##KS##_##VS(lucis_map_header* m,                    \
                                    lucis_map_vec_out* out) {              \
    VT* arr = (VT*)malloc(m->len * sizeof(VT));                             \
    size_t n = 0;                                                            \
    for (size_t i = 0; i < m->cap; i++) {                                    \
        if (m->states[i] == MAP_STATE_OCCUPIED)                              \
            arr[n++] = *(VT*)((uint8_t*)m->values + i * m->val_size);       \
    }                                                                        \
    out->ptr = arr; out->len = n; out->cap = n;                              \
}

// ── String key × numeric value ───────────────────────────────────────────

#define LUCIS_MAP_IMPL_STR(VT, VS)                                          \
void lucis_map_init_str_##VS(lucis_map_header* m) {                        \
    map_core_init(m, sizeof(lucis_map_string), sizeof(VT));                 \
}                                                                            \
void lucis_map_free_str_##VS(lucis_map_header* m) {                        \
    for (size_t i = 0; i < m->cap; i++) {                              \
        if (m->states[i] == MAP_STATE_OCCUPIED) {                      \
            lucis_map_string* key =                                       \
                (lucis_map_string*)((uint8_t*)m->keys + i * m->key_size); \
            lucis_freeStr(key->ptr, key->len);                           \
        }                                                                \
    }                                                                    \
    map_core_free(m);                                                        \
}                                                                            \
size_t lucis_map_len_str_##VS(const lucis_map_header* m) {                 \
    return m->len;                                                           \
}                                                                            \
int lucis_map_isEmpty_str_##VS(const lucis_map_header* m) {                \
    return m->len == 0;                                                      \
}                                                                            \
void lucis_map_set_str_##VS(lucis_map_header* m,                           \
                             lucis_map_string key, VT val) {               \
    uint64_t h = hash_key_str(&key);                                          \
    int found;                                                                 \
    size_t slot = map_core_find(m, &key, h, eq_key_str, &found);              \
    if (found) {                                                               \
        lucis_map_string* stored_key = key_at(m, slot);                        \
        if (stored_key->ptr != key.ptr)                                        \
            lucis_freeStr(stored_key->ptr, stored_key->len);                   \
        memcpy(stored_key, &key, m->key_size);                                 \
        memcpy(val_at(m, slot), &val, m->val_size);                            \
        return;                                                                 \
    }                                                                           \
    map_core_set(m, &key, &val, hash_key_str, eq_key_str);                     \
}                                                                            \
VT lucis_map_get_str_##VS(lucis_map_header* m,                             \
                           lucis_map_string key) {                         \
    VT result;                                                               \
    if (!map_core_get(m, &key, &result, hash_key_str, eq_key_str)) {         \
        fprintf(stderr, "lucis panic: Map::get key not found\n");           \
        abort();                                                             \
    }                                                                        \
    return result;                                                           \
}                                                                            \
VT lucis_map_getOrDefault_str_##VS(lucis_map_header* m,                    \
                                    lucis_map_string key, VT def) {        \
    VT result;                                                               \
    if (map_core_get(m, &key, &result, hash_key_str, eq_key_str))            \
        return result;                                                       \
    return def;                                                              \
}                                                                            \
int lucis_map_has_str_##VS(lucis_map_header* m,                            \
                            lucis_map_string key) {                        \
    return map_core_has(m, &key, hash_key_str, eq_key_str);                  \
}                                                                            \
int lucis_map_remove_str_##VS(lucis_map_header* m,                         \
                               lucis_map_string key) {                     \
    if (m->len == 0) return 0;                                                 \
    uint64_t h = hash_key_str(&key);                                           \
    int found;                                                                  \
    size_t slot = map_core_find(m, &key, h, eq_key_str, &found);               \
    if (!found) return 0;                                                       \
    lucis_map_string* stored_key =                                                \
        (lucis_map_string*)((uint8_t*)m->keys + slot * m->key_size);             \
    lucis_freeStr(stored_key->ptr, stored_key->len);                              \
    m->states[slot] = MAP_STATE_TOMBSTONE;                                      \
    m->len--;                                                                    \
    return 1;                                                                    \
}                                                                            \
void lucis_map_clear_str_##VS(lucis_map_header* m) {                       \
    for (size_t i = 0; i < m->cap; i++) {                                       \
        if (m->states[i] != MAP_STATE_OCCUPIED) continue;                       \
        lucis_map_string* key_ptr =                                                \
            (lucis_map_string*)((uint8_t*)m->keys + i * m->key_size);             \
        lucis_freeStr(key_ptr->ptr, key_ptr->len);                                 \
    }                                                                            \
    map_core_clear(m);                                                           \
}                                                                            \
void lucis_map_keys_str_##VS(lucis_map_header* m,                          \
                               lucis_map_vec_out* out) {                   \
    lucis_map_string* arr =                                                 \
        (lucis_map_string*)malloc(m->len * sizeof(lucis_map_string));      \
    size_t n = 0;                                                            \
    for (size_t i = 0; i < m->cap; i++) {                                    \
        if (m->states[i] == MAP_STATE_OCCUPIED)                              \
            arr[n++] = *(lucis_map_string*)                                 \
                ((uint8_t*)m->keys + i * m->key_size);                      \
    }                                                                        \
    out->ptr = arr; out->len = n; out->cap = n;                              \
}                                                                            \
void lucis_map_values_str_##VS(lucis_map_header* m,                        \
                                 lucis_map_vec_out* out) {                 \
    VT* arr = (VT*)malloc(m->len * sizeof(VT));                             \
    size_t n = 0;                                                            \
    for (size_t i = 0; i < m->cap; i++) {                                    \
        if (m->states[i] == MAP_STATE_OCCUPIED)                              \
            arr[n++] = *(VT*)((uint8_t*)m->values + i * m->val_size);       \
    }                                                                        \
    out->ptr = arr; out->len = n; out->cap = n;                              \
}

// ── Integer key × string value ───────────────────────────────────────────

#define LUCIS_MAP_IMPL_INT_STR(KT, KS, HASH_FN, EQ_FN)                     \
void lucis_map_init_##KS##_str(lucis_map_header* m) {                      \
    map_core_init(m, sizeof(KT), sizeof(lucis_map_string));                 \
}                                                                            \
void lucis_map_free_##KS##_str(lucis_map_header* m) {                      \
    for (size_t i = 0; i < m->cap; i++) {                              \
        if (m->states[i] == MAP_STATE_OCCUPIED) {                      \
            lucis_map_string* val =                                       \
                (lucis_map_string*)((uint8_t*)m->values + i * m->val_size); \
            lucis_freeStr(val->ptr, val->len);                           \
        }                                                                \
    }                                                                    \
    map_core_free(m);                                                        \
}                                                                            \
size_t lucis_map_len_##KS##_str(const lucis_map_header* m) {               \
    return m->len;                                                           \
}                                                                            \
int lucis_map_isEmpty_##KS##_str(const lucis_map_header* m) {              \
    return m->len == 0;                                                      \
}                                                                            \
void lucis_map_set_##KS##_str(lucis_map_header* m, KT key,                \
                               lucis_map_string val) {                     \
    uint64_t h = HASH_FN(&key);                                                \
    int found;                                                                  \
    size_t slot = map_core_find(m, &key, h, EQ_FN, &found);                    \
    if (found) {                                                                \
        lucis_map_string* old_val =                                               \
            (lucis_map_string*)((uint8_t*)m->values + slot * m->val_size);        \
        if (old_val->ptr != val.ptr || old_val->len != val.len)                 \
            lucis_freeStr(old_val->ptr, old_val->len);                            \
        memcpy(val_at(m, slot), &val, m->val_size);                             \
        return;                                                                  \
    }                                                                            \
    map_core_set(m, &key, &val, HASH_FN, EQ_FN);                                \
}                                                                            \
lucis_map_string lucis_map_get_##KS##_str(lucis_map_header* m, KT key) {  \
    lucis_map_string result;                                                \
    if (!map_core_get(m, &key, &result, HASH_FN, EQ_FN)) {                  \
        fprintf(stderr, "lucis panic: Map::get key not found\n");           \
        abort();                                                             \
    }                                                                        \
    return result;                                                           \
}                                                                            \
lucis_map_string lucis_map_getOrDefault_##KS##_str(                        \
        lucis_map_header* m, KT key, lucis_map_string def) {              \
    lucis_map_string result;                                                \
    if (map_core_get(m, &key, &result, HASH_FN, EQ_FN)) return result;      \
    return def;                                                              \
}                                                                            \
int lucis_map_has_##KS##_str(lucis_map_header* m, KT key) {               \
    return map_core_has(m, &key, HASH_FN, EQ_FN);                           \
}                                                                            \
int lucis_map_remove_##KS##_str(lucis_map_header* m, KT key) {            \
    if (m->len == 0) return 0;                                                 \
    uint64_t h = HASH_FN(&key);                                                \
    int found;                                                                  \
    size_t slot = map_core_find(m, &key, h, EQ_FN, &found);                    \
    if (!found) return 0;                                                       \
    lucis_map_string* stored_val =                                                \
        (lucis_map_string*)((uint8_t*)m->values + slot * m->val_size);           \
    lucis_freeStr(stored_val->ptr, stored_val->len);                              \
    m->states[slot] = MAP_STATE_TOMBSTONE;                                      \
    m->len--;                                                                    \
    return 1;                                                                    \
}                                                                            \
void lucis_map_clear_##KS##_str(lucis_map_header* m) {                    \
    for (size_t i = 0; i < m->cap; i++) {                                       \
        if (m->states[i] != MAP_STATE_OCCUPIED) continue;                       \
        lucis_map_string* val_ptr =                                                \
            (lucis_map_string*)((uint8_t*)m->values + i * m->val_size);           \
        lucis_freeStr(val_ptr->ptr, val_ptr->len);                                 \
    }                                                                            \
    map_core_clear(m);                                                           \
}                                                                            \
void lucis_map_keys_##KS##_str(lucis_map_header* m,                       \
                                 lucis_map_vec_out* out) {                 \
    KT* arr = (KT*)malloc(m->len * sizeof(KT));                             \
    size_t n = 0;                                                            \
    for (size_t i = 0; i < m->cap; i++) {                                    \
        if (m->states[i] == MAP_STATE_OCCUPIED)                              \
            arr[n++] = *(KT*)((uint8_t*)m->keys + i * m->key_size);         \
    }                                                                        \
    out->ptr = arr; out->len = n; out->cap = n;                              \
}                                                                            \
void lucis_map_values_##KS##_str(lucis_map_header* m,                     \
                                   lucis_map_vec_out* out) {               \
    lucis_map_string* arr =                                                 \
        (lucis_map_string*)malloc(m->len * sizeof(lucis_map_string));      \
    size_t n = 0;                                                            \
    for (size_t i = 0; i < m->cap; i++) {                                    \
        if (m->states[i] == MAP_STATE_OCCUPIED)                              \
            arr[n++] = *(lucis_map_string*)                                 \
                ((uint8_t*)m->values + i * m->val_size);                    \
    }                                                                        \
    out->ptr = arr; out->len = n; out->cap = n;                              \
}

// ═══════════════════════════════════════════════════════════════════════════
// Instantiate all supported K×V combinations
// ═══════════════════════════════════════════════════════════════════════════

// ── String key × numeric values ──────────────────────────────────────────
LUCIS_MAP_IMPL_STR(int8_t,   i8)
LUCIS_MAP_IMPL_STR(int16_t,  i16)
LUCIS_MAP_IMPL_STR(int32_t,  i32)
LUCIS_MAP_IMPL_STR(int64_t,  i64)
LUCIS_MAP_IMPL_STR(uint8_t,  u8)
LUCIS_MAP_IMPL_STR(uint16_t, u16)
LUCIS_MAP_IMPL_STR(uint32_t, u32)
LUCIS_MAP_IMPL_STR(uint64_t, u64)
LUCIS_MAP_IMPL_STR(float,    f32)
LUCIS_MAP_IMPL_STR(double,   f64)

// ── String key × string value ────────────────────────────────────────────
// Handled by LUCIS_MAP_IMPL_STR with VT=lucis_map_string
void lucis_map_init_str_str(lucis_map_header* m) {
    map_core_init(m, sizeof(lucis_map_string), sizeof(lucis_map_string));
}
void lucis_map_free_str_str(lucis_map_header* m) {
    for (size_t i = 0; i < m->cap; i++) {
        if (m->states[i] != MAP_STATE_OCCUPIED) continue;
        lucis_map_string* key =
            (lucis_map_string*)((uint8_t*)m->keys + i * m->key_size);
        lucis_map_string* val =
            (lucis_map_string*)((uint8_t*)m->values + i * m->val_size);
        lucis_freeStr(key->ptr, key->len);
        lucis_freeStr(val->ptr, val->len);
    }
    map_core_free(m);
}
size_t lucis_map_len_str_str(const lucis_map_header* m) {
    return m->len;
}
int lucis_map_isEmpty_str_str(const lucis_map_header* m) {
    return m->len == 0;
}
void lucis_map_set_str_str(lucis_map_header* m,
                            lucis_map_string key, lucis_map_string val) {
    uint64_t h = hash_key_str(&key);
    int found;
    size_t slot = map_core_find(m, &key, h, eq_key_str, &found);
    if (found) {
        lucis_map_string* stored_key = key_at(m, slot);
        if (stored_key->ptr != key.ptr)
            lucis_freeStr(stored_key->ptr, stored_key->len);
        memcpy(stored_key, &key, m->key_size);

        lucis_map_string* old_val = val_at(m, slot);
        if (old_val->ptr != val.ptr || old_val->len != val.len)
            lucis_freeStr(old_val->ptr, old_val->len);
        memcpy(old_val, &val, m->val_size);
        return;
    }
    map_core_set(m, &key, &val, hash_key_str, eq_key_str);
}
lucis_map_string lucis_map_get_str_str(lucis_map_header* m,
                                          lucis_map_string key) {
    lucis_map_string result;
    if (!map_core_get(m, &key, &result, hash_key_str, eq_key_str)) {
        fprintf(stderr, "lucis panic: Map::get key not found\n");
        abort();
    }
    return result;
}
lucis_map_string lucis_map_getOrDefault_str_str(lucis_map_header* m,
                                                   lucis_map_string key,
                                                   lucis_map_string def) {
    lucis_map_string result;
    if (map_core_get(m, &key, &result, hash_key_str, eq_key_str))
        return result;
    return def;
}
int lucis_map_has_str_str(lucis_map_header* m, lucis_map_string key) {
    return map_core_has(m, &key, hash_key_str, eq_key_str);
}
int lucis_map_remove_str_str(lucis_map_header* m, lucis_map_string key) {
    if (m->len == 0) return 0;
    uint64_t h = hash_key_str(&key);
    int found;
    size_t slot = map_core_find(m, &key, h, eq_key_str, &found);
    if (!found) return 0;
    lucis_map_string* stored_key =
        (lucis_map_string*)((uint8_t*)m->keys + slot * m->key_size);
    lucis_map_string* stored_val =
        (lucis_map_string*)((uint8_t*)m->values + slot * m->val_size);
    lucis_freeStr(stored_key->ptr, stored_key->len);
    lucis_freeStr(stored_val->ptr, stored_val->len);
    m->states[slot] = MAP_STATE_TOMBSTONE;
    m->len--;
    return 1;
}
void lucis_map_clear_str_str(lucis_map_header* m) {
    for (size_t i = 0; i < m->cap; i++) {
        if (m->states[i] != MAP_STATE_OCCUPIED) continue;
        lucis_map_string* key_ptr =
            (lucis_map_string*)((uint8_t*)m->keys + i * m->key_size);
        lucis_map_string* val_ptr =
            (lucis_map_string*)((uint8_t*)m->values + i * m->val_size);
        lucis_freeStr(key_ptr->ptr, key_ptr->len);
        lucis_freeStr(val_ptr->ptr, val_ptr->len);
    }
    map_core_clear(m);
}
void lucis_map_keys_str_str(lucis_map_header* m, lucis_map_vec_out* out) {
    lucis_map_string* arr =
        (lucis_map_string*)malloc(m->len * sizeof(lucis_map_string));
    size_t n = 0;
    for (size_t i = 0; i < m->cap; i++) {
        if (m->states[i] == MAP_STATE_OCCUPIED)
            arr[n++] = *(lucis_map_string*)
                ((uint8_t*)m->keys + i * m->key_size);
    }
    out->ptr = arr; out->len = n; out->cap = n;
}
void lucis_map_values_str_str(lucis_map_header* m, lucis_map_vec_out* out) {
    lucis_map_string* arr =
        (lucis_map_string*)malloc(m->len * sizeof(lucis_map_string));
    size_t n = 0;
    for (size_t i = 0; i < m->cap; i++) {
        if (m->states[i] == MAP_STATE_OCCUPIED)
            arr[n++] = *(lucis_map_string*)
                ((uint8_t*)m->values + i * m->val_size);
    }
    out->ptr = arr; out->len = n; out->cap = n;
}

// ── Int32 key × all numeric values ───────────────────────────────────────
LUCIS_MAP_IMPL_INT(int32_t, i32, int8_t,   i8,  hash_key_i32, eq_key_i32)
LUCIS_MAP_IMPL_INT(int32_t, i32, int16_t,  i16, hash_key_i32, eq_key_i32)
LUCIS_MAP_IMPL_INT(int32_t, i32, int32_t,  i32, hash_key_i32, eq_key_i32)
LUCIS_MAP_IMPL_INT(int32_t, i32, int64_t,  i64, hash_key_i32, eq_key_i32)
LUCIS_MAP_IMPL_INT(int32_t, i32, uint8_t,  u8,  hash_key_i32, eq_key_i32)
LUCIS_MAP_IMPL_INT(int32_t, i32, uint16_t, u16, hash_key_i32, eq_key_i32)
LUCIS_MAP_IMPL_INT(int32_t, i32, uint32_t, u32, hash_key_i32, eq_key_i32)
LUCIS_MAP_IMPL_INT(int32_t, i32, uint64_t, u64, hash_key_i32, eq_key_i32)
LUCIS_MAP_IMPL_INT(int32_t, i32, float,    f32, hash_key_i32, eq_key_i32)
LUCIS_MAP_IMPL_INT(int32_t, i32, double,   f64, hash_key_i32, eq_key_i32)
LUCIS_MAP_IMPL_INT_STR(int32_t, i32, hash_key_i32, eq_key_i32)

// ── Int64 key × all numeric values ───────────────────────────────────────
LUCIS_MAP_IMPL_INT(int64_t, i64, int8_t,   i8,  hash_key_i64, eq_key_i64)
LUCIS_MAP_IMPL_INT(int64_t, i64, int16_t,  i16, hash_key_i64, eq_key_i64)
LUCIS_MAP_IMPL_INT(int64_t, i64, int32_t,  i32, hash_key_i64, eq_key_i64)
LUCIS_MAP_IMPL_INT(int64_t, i64, int64_t,  i64, hash_key_i64, eq_key_i64)
LUCIS_MAP_IMPL_INT(int64_t, i64, uint8_t,  u8,  hash_key_i64, eq_key_i64)
LUCIS_MAP_IMPL_INT(int64_t, i64, uint16_t, u16, hash_key_i64, eq_key_i64)
LUCIS_MAP_IMPL_INT(int64_t, i64, uint32_t, u32, hash_key_i64, eq_key_i64)
LUCIS_MAP_IMPL_INT(int64_t, i64, uint64_t, u64, hash_key_i64, eq_key_i64)
LUCIS_MAP_IMPL_INT(int64_t, i64, float,    f32, hash_key_i64, eq_key_i64)
LUCIS_MAP_IMPL_INT(int64_t, i64, double,   f64, hash_key_i64, eq_key_i64)
LUCIS_MAP_IMPL_INT_STR(int64_t, i64, hash_key_i64, eq_key_i64)

// ── Uint64 key × all numeric values ──────────────────────────────────────
LUCIS_MAP_IMPL_INT(uint64_t, u64, int8_t,   i8,  hash_key_u64, eq_key_u64)
LUCIS_MAP_IMPL_INT(uint64_t, u64, int16_t,  i16, hash_key_u64, eq_key_u64)
LUCIS_MAP_IMPL_INT(uint64_t, u64, int32_t,  i32, hash_key_u64, eq_key_u64)
LUCIS_MAP_IMPL_INT(uint64_t, u64, int64_t,  i64, hash_key_u64, eq_key_u64)
LUCIS_MAP_IMPL_INT(uint64_t, u64, uint8_t,  u8,  hash_key_u64, eq_key_u64)
LUCIS_MAP_IMPL_INT(uint64_t, u64, uint16_t, u16, hash_key_u64, eq_key_u64)
LUCIS_MAP_IMPL_INT(uint64_t, u64, uint32_t, u32, hash_key_u64, eq_key_u64)
LUCIS_MAP_IMPL_INT(uint64_t, u64, uint64_t, u64, hash_key_u64, eq_key_u64)
LUCIS_MAP_IMPL_INT(uint64_t, u64, float,    f32, hash_key_u64, eq_key_u64)
LUCIS_MAP_IMPL_INT(uint64_t, u64, double,   f64, hash_key_u64, eq_key_u64)
LUCIS_MAP_IMPL_INT_STR(uint64_t, u64, hash_key_u64, eq_key_u64)

// ── Uint128 key × all numeric values ─────────────────────────────────────
LUCIS_MAP_IMPL_INT(__uint128_t, u128, int8_t,   i8,  hash_key_u128, eq_key_u128)
LUCIS_MAP_IMPL_INT(__uint128_t, u128, int16_t,  i16, hash_key_u128, eq_key_u128)
LUCIS_MAP_IMPL_INT(__uint128_t, u128, int32_t,  i32, hash_key_u128, eq_key_u128)
LUCIS_MAP_IMPL_INT(__uint128_t, u128, int64_t,  i64, hash_key_u128, eq_key_u128)
LUCIS_MAP_IMPL_INT(__uint128_t, u128, uint8_t,  u8,  hash_key_u128, eq_key_u128)
LUCIS_MAP_IMPL_INT(__uint128_t, u128, uint16_t, u16, hash_key_u128, eq_key_u128)
LUCIS_MAP_IMPL_INT(__uint128_t, u128, uint32_t, u32, hash_key_u128, eq_key_u128)
LUCIS_MAP_IMPL_INT(__uint128_t, u128, uint64_t, u64, hash_key_u128, eq_key_u128)
LUCIS_MAP_IMPL_INT(__uint128_t, u128, float,    f32, hash_key_u128, eq_key_u128)
LUCIS_MAP_IMPL_INT(__uint128_t, u128, double,   f64, hash_key_u128, eq_key_u128)
LUCIS_MAP_IMPL_INT_STR(__uint128_t, u128, hash_key_u128, eq_key_u128)

// ── Int128 key × all numeric values ──────────────────────────────────────
LUCIS_MAP_IMPL_INT(__int128_t, i128, int8_t,   i8,  hash_key_i128, eq_key_i128)
LUCIS_MAP_IMPL_INT(__int128_t, i128, int16_t,  i16, hash_key_i128, eq_key_i128)
LUCIS_MAP_IMPL_INT(__int128_t, i128, int32_t,  i32, hash_key_i128, eq_key_i128)
LUCIS_MAP_IMPL_INT(__int128_t, i128, int64_t,  i64, hash_key_i128, eq_key_i128)
LUCIS_MAP_IMPL_INT(__int128_t, i128, uint8_t,  u8,  hash_key_i128, eq_key_i128)
LUCIS_MAP_IMPL_INT(__int128_t, i128, uint16_t, u16, hash_key_i128, eq_key_i128)
LUCIS_MAP_IMPL_INT(__int128_t, i128, uint32_t, u32, hash_key_i128, eq_key_i128)
LUCIS_MAP_IMPL_INT(__int128_t, i128, uint64_t, u64, hash_key_i128, eq_key_i128)
LUCIS_MAP_IMPL_INT(__int128_t, i128, float,    f32, hash_key_i128, eq_key_i128)
LUCIS_MAP_IMPL_INT(__int128_t, i128, double,   f64, hash_key_i128, eq_key_i128)
LUCIS_MAP_IMPL_INT_STR(__int128_t, i128, hash_key_i128, eq_key_i128)

// ── Int32 key × uint128/int128 values ────────────────────────────────────
LUCIS_MAP_IMPL_INT(int32_t, i32, __uint128_t, u128, hash_key_i32, eq_key_i32)
LUCIS_MAP_IMPL_INT(int32_t, i32, __int128_t,  i128, hash_key_i32, eq_key_i32)

// ── Int64 key × uint128/int128 values ────────────────────────────────────
LUCIS_MAP_IMPL_INT(int64_t, i64, __uint128_t, u128, hash_key_i64, eq_key_i64)
LUCIS_MAP_IMPL_INT(int64_t, i64, __int128_t,  i128, hash_key_i64, eq_key_i64)

// ── Uint64 key × uint128/int128 values ───────────────────────────────────
LUCIS_MAP_IMPL_INT(uint64_t, u64, __uint128_t, u128, hash_key_u64, eq_key_u64)
LUCIS_MAP_IMPL_INT(uint64_t, u64, __int128_t,  i128, hash_key_u64, eq_key_u64)

// ── String key × uint128/int128 values ───────────────────────────────────
LUCIS_MAP_IMPL_STR(__uint128_t, u128)
LUCIS_MAP_IMPL_STR(__int128_t,  i128)

// ── Int32/i64/u64 key × intinf value ────────────────────────────────────
LUCIS_MAP_IMPL_INT(int32_t, i32, lucis_int256_t, iinf, hash_key_i32, eq_key_i32)
LUCIS_MAP_IMPL_INT(int64_t, i64, lucis_int256_t, iinf, hash_key_i64, eq_key_i64)
LUCIS_MAP_IMPL_INT(uint64_t, u64, lucis_int256_t, iinf, hash_key_u64, eq_key_u64)

// ── String key × intinf value ───────────────────────────────────────────
LUCIS_MAP_IMPL_STR(lucis_int256_t, iinf)

// ── Uint1 key × all numeric values ──────────────────────────────────────
LUCIS_MAP_IMPL_INT(uint8_t, u1, int8_t,   i8,  hash_key_u1, eq_key_u1)
LUCIS_MAP_IMPL_INT(uint8_t, u1, int16_t,  i16, hash_key_u1, eq_key_u1)
LUCIS_MAP_IMPL_INT(uint8_t, u1, int32_t,  i32, hash_key_u1, eq_key_u1)
LUCIS_MAP_IMPL_INT(uint8_t, u1, int64_t,  i64, hash_key_u1, eq_key_u1)
LUCIS_MAP_IMPL_INT(uint8_t, u1, uint8_t,  u8,  hash_key_u1, eq_key_u1)
LUCIS_MAP_IMPL_INT(uint8_t, u1, uint16_t, u16, hash_key_u1, eq_key_u1)
LUCIS_MAP_IMPL_INT(uint8_t, u1, uint32_t, u32, hash_key_u1, eq_key_u1)
LUCIS_MAP_IMPL_INT(uint8_t, u1, uint64_t, u64, hash_key_u1, eq_key_u1)
LUCIS_MAP_IMPL_INT(uint8_t, u1, float,    f32, hash_key_u1, eq_key_u1)
LUCIS_MAP_IMPL_INT(uint8_t, u1, double,   f64, hash_key_u1, eq_key_u1)
LUCIS_MAP_IMPL_INT_STR(uint8_t, u1, hash_key_u1, eq_key_u1)

// ── Int1 key × all numeric values ───────────────────────────────────────
LUCIS_MAP_IMPL_INT(uint8_t, i1, int8_t,   i8,  hash_key_i1, eq_key_i1)
LUCIS_MAP_IMPL_INT(uint8_t, i1, int16_t,  i16, hash_key_i1, eq_key_i1)
LUCIS_MAP_IMPL_INT(uint8_t, i1, int32_t,  i32, hash_key_i1, eq_key_i1)
LUCIS_MAP_IMPL_INT(uint8_t, i1, int64_t,  i64, hash_key_i1, eq_key_i1)
LUCIS_MAP_IMPL_INT(uint8_t, i1, uint8_t,  u8,  hash_key_i1, eq_key_i1)
LUCIS_MAP_IMPL_INT(uint8_t, i1, uint16_t, u16, hash_key_i1, eq_key_i1)
LUCIS_MAP_IMPL_INT(uint8_t, i1, uint32_t, u32, hash_key_i1, eq_key_i1)
LUCIS_MAP_IMPL_INT(uint8_t, i1, uint64_t, u64, hash_key_i1, eq_key_i1)
LUCIS_MAP_IMPL_INT(uint8_t, i1, float,    f32, hash_key_i1, eq_key_i1)
LUCIS_MAP_IMPL_INT(uint8_t, i1, double,   f64, hash_key_i1, eq_key_i1)
LUCIS_MAP_IMPL_INT_STR(uint8_t, i1, hash_key_i1, eq_key_i1)

// ── Int32 key × u1/i1 values ────────────────────────────────────────────
LUCIS_MAP_IMPL_INT(int32_t, i32, uint8_t, u1, hash_key_i32, eq_key_i32)
LUCIS_MAP_IMPL_INT(int32_t, i32, uint8_t, i1, hash_key_i32, eq_key_i32)

// ── Int64 key × u1/i1 values ────────────────────────────────────────────
LUCIS_MAP_IMPL_INT(int64_t, i64, uint8_t, u1, hash_key_i64, eq_key_i64)
LUCIS_MAP_IMPL_INT(int64_t, i64, uint8_t, i1, hash_key_i64, eq_key_i64)

// ── Uint64 key × u1/i1 values ───────────────────────────────────────────
LUCIS_MAP_IMPL_INT(uint64_t, u64, uint8_t, u1, hash_key_u64, eq_key_u64)
LUCIS_MAP_IMPL_INT(uint64_t, u64, uint8_t, i1, hash_key_u64, eq_key_u64)

// ── String key × u1/i1 values ───────────────────────────────────────────
LUCIS_MAP_IMPL_STR(uint8_t, u1)
LUCIS_MAP_IMPL_STR(uint8_t, i1)

// ── Uint1 key × uint128/int128 values ───────────────────────────────────
LUCIS_MAP_IMPL_INT(uint8_t, u1, __uint128_t, u128, hash_key_u1, eq_key_u1)
LUCIS_MAP_IMPL_INT(uint8_t, u1, __int128_t,  i128, hash_key_u1, eq_key_u1)

// ── Int1 key × uint128/int128 values ────────────────────────────────────
LUCIS_MAP_IMPL_INT(uint8_t, i1, __uint128_t, u128, hash_key_i1, eq_key_i1)
LUCIS_MAP_IMPL_INT(uint8_t, i1, __int128_t,  i128, hash_key_i1, eq_key_i1)

// ── Uint1/Int1 key × intinf value ───────────────────────────────────────
LUCIS_MAP_IMPL_INT(uint8_t, u1, lucis_int256_t, iinf, hash_key_u1, eq_key_u1)
LUCIS_MAP_IMPL_INT(uint8_t, i1, lucis_int256_t, iinf, hash_key_i1, eq_key_i1)

// ── Uint128/Int128 key × u1/i1 values ───────────────────────────────────
LUCIS_MAP_IMPL_INT(__uint128_t, u128, uint8_t, u1, hash_key_u128, eq_key_u128)
LUCIS_MAP_IMPL_INT(__uint128_t, u128, uint8_t, i1, hash_key_u128, eq_key_u128)
LUCIS_MAP_IMPL_INT(__int128_t,  i128, uint8_t, u1, hash_key_i128,  eq_key_i128)
LUCIS_MAP_IMPL_INT(__int128_t,  i128, uint8_t, i1, hash_key_i128,  eq_key_i128)

// ── Intinf key × u1/i1 values ───────────────────────────────────────────
LUCIS_MAP_IMPL_INT(lucis_int256_t, iinf, uint8_t, u1, hash_key_iinf, eq_key_iinf)
LUCIS_MAP_IMPL_INT(lucis_int256_t, iinf, uint8_t, i1, hash_key_iinf, eq_key_iinf)

// ═══════════════════════════════════════════════════════════════════════════
// Raw (opaque struct) value — str key, struct val of runtime-known size
// ═══════════════════════════════════════════════════════════════════════════

void lucis_map_init_str_raw(lucis_map_header* m, size_t val_size) {
    map_core_init(m, sizeof(lucis_map_string), val_size);
}

void lucis_map_free_str_raw(lucis_map_header* m) {
    for (size_t i = 0; i < m->cap; i++) {
        if (m->states[i] != MAP_STATE_OCCUPIED) continue;
        lucis_map_string* key =
            (lucis_map_string*)((uint8_t*)m->keys + i * m->key_size);
        lucis_freeStr(key->ptr, key->len);
    }
    map_core_free(m);
}

size_t lucis_map_len_str_raw(const lucis_map_header* m) {
    return m->len;
}

void lucis_map_set_str_raw(lucis_map_header* m, lucis_map_string key,
                          const void* val) {
    // Deep-copy the key string so the map owns its data independently
    char* owned_data = key.len > 0
        ? (char*)lucis_allocString(key.len)
        : (char*)lucis_allocString(1);
    if (key.len > 0)
        memcpy(owned_data, key.ptr, key.len);
    else
        owned_data[0] = '\0';

    lucis_map_string owned_key = { owned_data, key.len };

    uint64_t h = hash_key_str(&owned_key);
    int found;
    size_t slot = map_core_find(m, &owned_key, h, eq_key_str, &found);
    if (found) {
        lucis_map_string* stored_key = key_at(m, slot);
        if (stored_key->ptr != owned_key.ptr)
            lucis_freeStr(stored_key->ptr, stored_key->len);
        memcpy(stored_key, &owned_key, m->key_size);
        memcpy(val_at(m, slot), val, m->val_size);
        return;
    }
    map_core_set(m, &owned_key, val, hash_key_str, eq_key_str);
}

void lucis_map_get_str_raw(lucis_map_header* m, lucis_map_string key,
                          void* val_out) {
    if (!map_core_get(m, &key, val_out, hash_key_str, eq_key_str)) {
        fprintf(stderr, "lucis panic: Map::get key not found\n");
        abort();
    }
}

int lucis_map_has_str_raw(lucis_map_header* m, lucis_map_string key) {
    return map_core_has(m, &key, hash_key_str, eq_key_str);
}

void lucis_map_values_str_raw(lucis_map_header* m, lucis_map_vec_out* out) {
    char* arr = (char*)malloc(m->len * m->val_size);
    size_t n = 0;
    for (size_t i = 0; i < m->cap; i++) {
        if (m->states[i] == MAP_STATE_OCCUPIED) {
            memcpy(arr + n * m->val_size,
                   (uint8_t*)m->values + i * m->val_size,
                   m->val_size);
            n++;
        }
    }
    out->ptr = arr; out->len = n; out->cap = n;
}

int lucis_map_isEmpty_str_raw(const lucis_map_header* m) {
    return m->len == 0;
}

int lucis_map_remove_str_raw(lucis_map_header* m, lucis_map_string key) {
    if (m->len == 0) return 0;
    uint64_t h = hash_key_str(&key);
    int found;
    size_t slot = map_core_find(m, &key, h, eq_key_str, &found);
    if (!found) return 0;
    lucis_map_string* stored_key =
        (lucis_map_string*)((uint8_t*)m->keys + slot * m->key_size);
    lucis_freeStr(stored_key->ptr, stored_key->len);
    m->states[slot] = MAP_STATE_TOMBSTONE;
    m->len--;
    return 1;
}

void lucis_map_clear_str_raw(lucis_map_header* m) {
    for (size_t i = 0; i < m->cap; i++) {
        if (m->states[i] != MAP_STATE_OCCUPIED) continue;
        lucis_map_string* key_ptr =
            (lucis_map_string*)((uint8_t*)m->keys + i * m->key_size);
        lucis_freeStr(key_ptr->ptr, key_ptr->len);
    }
    map_core_clear(m);
}
