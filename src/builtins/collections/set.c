#include "collections/set.h"
#include "../string/string.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ═══════════════════════════════════════════════════════════════════════════
// Generic hash set core — open addressing with linear probing
// ═══════════════════════════════════════════════════════════════════════════

#define SET_STATE_EMPTY     0
#define SET_STATE_OCCUPIED  1
#define SET_STATE_TOMBSTONE 2

#define SET_INITIAL_CAP     16
#define SET_LOAD_FACTOR_NUM 3
#define SET_LOAD_FACTOR_DEN 4  // 75%

typedef uint64_t (*set_hash_fn)(const void* key);
typedef int      (*set_eq_fn)(const void* a, const void* b);

// ── Hash functions ───────────────────────────────────────────────────────

static uint64_t splitmix64(uint64_t x) {
    x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27; x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return x;
}

// Size-based hash/eq for integer types
static uint64_t hash_1(const void* k) { return splitmix64((uint64_t)*(const uint8_t*)k); }
static int eq_1(const void* a, const void* b) { return *(const uint8_t*)a == *(const uint8_t*)b; }

static uint64_t hash_2(const void* k) { return splitmix64((uint64_t)*(const uint16_t*)k); }
static int eq_2(const void* a, const void* b) { return *(const uint16_t*)a == *(const uint16_t*)b; }

static uint64_t hash_4(const void* k) { return splitmix64((uint64_t)*(const uint32_t*)k); }
static int eq_4(const void* a, const void* b) { return *(const uint32_t*)a == *(const uint32_t*)b; }

static uint64_t hash_8(const void* k) { return splitmix64(*(const uint64_t*)k); }
static int eq_8(const void* a, const void* b) { return *(const uint64_t*)a == *(const uint64_t*)b; }

// String hash/eq — FNV-1a
static uint64_t hash_str(const void* key) {
    const lucis_set_string* s = (const lucis_set_string*)key;
    uint64_t h = 14695981039346656037ULL;
    for (size_t i = 0; i < s->len; i++) {
        h ^= (uint8_t)s->ptr[i];
        h *= 1099511628211ULL;
    }
    return h;
}

static int eq_str(const void* a, const void* b) {
    const lucis_set_string* sa = (const lucis_set_string*)a;
    const lucis_set_string* sb = (const lucis_set_string*)b;
    return sa->len == sb->len &&
           (sa->len == 0 || memcmp(sa->ptr, sb->ptr, sa->len) == 0);
}

// ── Core operations ──────────────────────────────────────────────────────

static void set_core_init(lucis_set_header* s, size_t key_size) {
    s->states   = (uint8_t*)calloc(SET_INITIAL_CAP, 1);
    s->keys     = calloc(SET_INITIAL_CAP, key_size);
    s->hashes   = (uint64_t*)calloc(SET_INITIAL_CAP, sizeof(uint64_t));
    s->len      = 0;
    s->cap      = SET_INITIAL_CAP;
    s->key_size = key_size;
}

static void set_core_free(lucis_set_header* s) {
    free(s->states);
    free(s->keys);
    free(s->hashes);
    s->states = NULL;
    s->keys   = NULL;
    s->hashes = NULL;
    s->len    = 0;
    s->cap    = 0;
}

static void set_core_clear(lucis_set_header* s) {
    memset(s->states, 0, s->cap);
    s->len = 0;
}

static inline void* set_key_at(lucis_set_header* s, size_t idx) {
    return (uint8_t*)s->keys + idx * s->key_size;
}

// Find slot for a key. Sets *found = 1 if key exists, else 0.
static size_t set_core_find(lucis_set_header* s, const void* key,
                            uint64_t hash, set_eq_fn eq, int* found) {
    size_t mask = s->cap - 1;
    size_t idx  = (size_t)(hash & mask);
    size_t first_tombstone = SIZE_MAX;

    for (size_t i = 0; i < s->cap; i++) {
        size_t slot = (idx + i) & mask;
        uint8_t state = s->states[slot];

        if (state == SET_STATE_EMPTY) {
            *found = 0;
            return (first_tombstone != SIZE_MAX) ? first_tombstone : slot;
        }
        if (state == SET_STATE_TOMBSTONE) {
            if (first_tombstone == SIZE_MAX)
                first_tombstone = slot;
            continue;
        }
        // state == OCCUPIED
        if (s->hashes[slot] == hash && eq(set_key_at(s, slot), key)) {
            *found = 1;
            return slot;
        }
    }

    *found = 0;
    return (first_tombstone != SIZE_MAX) ? first_tombstone : 0;
}

static void set_core_grow(lucis_set_header* s, set_hash_fn hash_fn, set_eq_fn eq) {
    size_t old_cap = s->cap;
    uint8_t*  old_states = s->states;
    void*     old_keys   = s->keys;
    uint64_t* old_hashes = s->hashes;

    size_t new_cap = old_cap * 2;
    s->states = (uint8_t*)calloc(new_cap, 1);
    s->keys   = calloc(new_cap, s->key_size);
    s->hashes = (uint64_t*)calloc(new_cap, sizeof(uint64_t));
    s->cap    = new_cap;
    s->len    = 0;

    for (size_t i = 0; i < old_cap; i++) {
        if (old_states[i] == SET_STATE_OCCUPIED) {
            void* ok = (uint8_t*)old_keys + i * s->key_size;
            uint64_t h = old_hashes[i];

            int found;
            size_t slot = set_core_find(s, ok, h, eq, &found);
            s->states[slot] = SET_STATE_OCCUPIED;
            s->hashes[slot] = h;
            memcpy(set_key_at(s, slot), ok, s->key_size);
            s->len++;
        }
    }

    free(old_states);
    free(old_keys);
    free(old_hashes);
}

// Add element. Returns 1 if element was new, 0 if already existed.
static int set_core_add(lucis_set_header* s, const void* key,
                        set_hash_fn hash_fn, set_eq_fn eq) {
    if ((s->len + 1) * SET_LOAD_FACTOR_DEN > s->cap * SET_LOAD_FACTOR_NUM)
        set_core_grow(s, hash_fn, eq);

    uint64_t h = hash_fn(key);
    int found;
    size_t slot = set_core_find(s, key, h, eq, &found);

    if (found)
        return 0;  // already existed

    s->states[slot] = SET_STATE_OCCUPIED;
    s->hashes[slot] = h;
    memcpy(set_key_at(s, slot), key, s->key_size);
    s->len++;
    return 1;  // new element
}

static int set_core_has(lucis_set_header* s, const void* key,
                        set_hash_fn hash_fn, set_eq_fn eq) {
    if (s->len == 0) return 0;
    uint64_t h = hash_fn(key);
    int found;
    set_core_find(s, key, h, eq, &found);
    return found;
}

// Remove element. Returns 1 if existed, 0 if not found.
static int set_core_remove(lucis_set_header* s, const void* key,
                           set_hash_fn hash_fn, set_eq_fn eq) {
    if (s->len == 0) return 0;
    uint64_t h = hash_fn(key);
    int found;
    size_t slot = set_core_find(s, key, h, eq, &found);
    if (found) {
        s->states[slot] = SET_STATE_TOMBSTONE;
        s->len--;
        return 1;
    }
    return 0;
}

// ═══════════════════════════════════════════════════════════════════════════
// Macro-generated typed wrappers
// ═══════════════════════════════════════════════════════════════════════════

// ── Integer element types ────────────────────────────────────────────────

#define LUCIS_SET_IMPL_INT(ET, ES, HASH_FN, EQ_FN)                         \
void lucis_set_init_##ES(lucis_set_header* s) {                            \
    set_core_init(s, sizeof(ET));                                            \
}                                                                            \
void lucis_set_free_##ES(lucis_set_header* s) {                            \
    set_core_free(s);                                                        \
}                                                                            \
size_t lucis_set_len_##ES(const lucis_set_header* s) {                     \
    return s->len;                                                           \
}                                                                            \
int lucis_set_isEmpty_##ES(const lucis_set_header* s) {                    \
    return s->len == 0;                                                      \
}                                                                            \
int lucis_set_add_##ES(lucis_set_header* s, ET elem) {                     \
    return set_core_add(s, &elem, HASH_FN, EQ_FN);                          \
}                                                                            \
int lucis_set_has_##ES(lucis_set_header* s, ET elem) {                     \
    return set_core_has(s, &elem, HASH_FN, EQ_FN);                          \
}                                                                            \
int lucis_set_remove_##ES(lucis_set_header* s, ET elem) {                  \
    return set_core_remove(s, &elem, HASH_FN, EQ_FN);                       \
}                                                                            \
void lucis_set_clear_##ES(lucis_set_header* s) {                           \
    set_core_clear(s);                                                       \
}                                                                            \
void lucis_set_values_##ES(lucis_set_header* s,                            \
                             lucis_set_vec_out* out) {                     \
    ET* arr = (ET*)malloc(s->len * sizeof(ET));                              \
    size_t n = 0;                                                            \
    for (size_t i = 0; i < s->cap; i++) {                                    \
        if (s->states[i] == SET_STATE_OCCUPIED)                              \
            arr[n++] = *(ET*)((uint8_t*)s->keys + i * s->key_size);         \
    }                                                                        \
    out->ptr = arr; out->len = n; out->cap = n;                              \
}

// Instantiate integer types
LUCIS_SET_IMPL_INT(int8_t,   i8,  hash_1, eq_1)
LUCIS_SET_IMPL_INT(uint8_t,  u8,  hash_1, eq_1)
LUCIS_SET_IMPL_INT(int16_t,  i16, hash_2, eq_2)
LUCIS_SET_IMPL_INT(uint16_t, u16, hash_2, eq_2)
LUCIS_SET_IMPL_INT(int32_t,  i32, hash_4, eq_4)
LUCIS_SET_IMPL_INT(uint32_t, u32, hash_4, eq_4)
LUCIS_SET_IMPL_INT(int64_t,  i64, hash_8, eq_8)
LUCIS_SET_IMPL_INT(uint64_t, u64, hash_8, eq_8)

// ── String element type ──────────────────────────────────────────────────

void lucis_set_init_str(lucis_set_header* s) {
    set_core_init(s, sizeof(lucis_set_string));
}

void lucis_set_free_str(lucis_set_header* s) {
    for (size_t i = 0; i < s->cap; i++) {
        if (s->states[i] != SET_STATE_OCCUPIED) continue;
        lucis_set_string* elem =
            (lucis_set_string*)((uint8_t*)s->keys + i * s->key_size);
        lucis_freeStr(elem->ptr, elem->len);
    }
    set_core_free(s);
}

size_t lucis_set_len_str(const lucis_set_header* s) {
    return s->len;
}

int lucis_set_isEmpty_str(const lucis_set_header* s) {
    return s->len == 0;
}

int lucis_set_add_str(lucis_set_header* s, lucis_set_string elem) {
    return set_core_add(s, &elem, hash_str, eq_str);
}

int lucis_set_has_str(lucis_set_header* s, lucis_set_string elem) {
    return set_core_has(s, &elem, hash_str, eq_str);
}

int lucis_set_remove_str(lucis_set_header* s, lucis_set_string elem) {
    if (s->len == 0) return 0;
    uint64_t h = hash_str(&elem);
    int found;
    size_t slot = set_core_find(s, &elem, h, eq_str, &found);
    if (!found) return 0;
    lucis_set_string* stored =
        (lucis_set_string*)((uint8_t*)s->keys + slot * s->key_size);
    lucis_freeStr(stored->ptr, stored->len);
    s->states[slot] = SET_STATE_TOMBSTONE;
    s->len--;
    return 1;
}

void lucis_set_clear_str(lucis_set_header* s) {
    for (size_t i = 0; i < s->cap; i++) {
        if (s->states[i] != SET_STATE_OCCUPIED) continue;
        lucis_set_string* elem =
            (lucis_set_string*)((uint8_t*)s->keys + i * s->key_size);
        lucis_freeStr(elem->ptr, elem->len);
    }
    set_core_clear(s);
}

void lucis_set_values_str(lucis_set_header* s, lucis_set_vec_out* out) {
    lucis_set_string* arr =
        (lucis_set_string*)malloc(s->len * sizeof(lucis_set_string));
    size_t n = 0;
    for (size_t i = 0; i < s->cap; i++) {
        if (s->states[i] == SET_STATE_OCCUPIED)
            arr[n++] = *(lucis_set_string*)
                ((uint8_t*)s->keys + i * s->key_size);
    }
    out->ptr = arr; out->len = n; out->cap = n;
}

// ═══════════════════════════════════════════════════════════════════════════
// Raw (opaque struct) element — byte-level FNV-1a hash + memcmp equality
// ═══════════════════════════════════════════════════════════════════════════

static size_t raw_key_size_ctx;  // thread-unsafe but Lucis is single-threaded for now

static uint64_t hash_raw(const void* key) {
    const uint8_t* b = (const uint8_t*)key;
    uint64_t h = 14695981039346656037ULL;
    for (size_t i = 0; i < raw_key_size_ctx; i++) {
        h ^= b[i];
        h *= 1099511628211ULL;
    }
    return h;
}

static int eq_raw(const void* a, const void* b) {
    return memcmp(a, b, raw_key_size_ctx) == 0;
}

void lucis_set_init_raw(lucis_set_header* s, size_t elem_size) {
    set_core_init(s, elem_size);
}

void lucis_set_free_raw(lucis_set_header* s) {
    set_core_free(s);
}

size_t lucis_set_len_raw(const lucis_set_header* s) {
    return s->len;
}

int lucis_set_add_raw(lucis_set_header* s, const void* elem) {
    raw_key_size_ctx = s->key_size;
    return set_core_add(s, elem, hash_raw, eq_raw);
}

int lucis_set_has_raw(lucis_set_header* s, const void* elem) {
    raw_key_size_ctx = s->key_size;
    return set_core_has(s, elem, hash_raw, eq_raw);
}

int lucis_set_remove_raw(lucis_set_header* s, const void* elem) {
    raw_key_size_ctx = s->key_size;
    return set_core_remove(s, elem, hash_raw, eq_raw);
}

void lucis_set_values_raw(lucis_set_header* s, lucis_set_vec_out* out) {
    char* arr = (char*)malloc(s->len * s->key_size);
    size_t n = 0;
    for (size_t i = 0; i < s->cap; i++) {
        if (s->states[i] == SET_STATE_OCCUPIED) {
            memcpy(arr + n * s->key_size,
                   (uint8_t*)s->keys + i * s->key_size,
                   s->key_size);
            n++;
        }
    }
    out->ptr = arr; out->len = n; out->cap = n;
}

int lucis_set_isEmpty_raw(const lucis_set_header* s) {
    return s->len == 0;
}

void lucis_set_clear_raw(lucis_set_header* s) {
    set_core_clear(s);
}
