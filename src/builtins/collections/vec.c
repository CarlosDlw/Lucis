#include "vec.h"
#include "../string/string.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ═══════════════════════════════════════════════════════════════════════════════
// vec<T> runtime — macro-generated monomorphized implementations
// ═══════════════════════════════════════════════════════════════════════════════

#define LUCIS_VEC_INITIAL_CAP 8
#define LUCIS_VEC_GROWTH_FACTOR 2

#define LUCIS_VEC_IMPL(T, SUFFIX)                                              \
                                                                                \
/* ── Typed data pointer helper ─────────────────────────────────────────── */  \
static inline T* vec_data_##SUFFIX(lucis_vec_header* v) {                      \
    return (T*)v->ptr;                                                          \
}                                                                               \
static inline const T* vec_cdata_##SUFFIX(const lucis_vec_header* v) {         \
    return (const T*)v->ptr;                                                    \
}                                                                               \
                                                                                \
/* ── Ensure capacity ───────────────────────────────────────────────────── */  \
static void vec_grow_##SUFFIX(lucis_vec_header* v, size_t needed) {            \
    if (needed <= v->cap) return;                                               \
    size_t newCap = v->cap ? v->cap : LUCIS_VEC_INITIAL_CAP;                   \
    while (newCap < needed) newCap *= LUCIS_VEC_GROWTH_FACTOR;                 \
    v->ptr = realloc(v->ptr, newCap * sizeof(T));                               \
    v->cap = newCap;                                                            \
}                                                                               \
                                                                                \
/* ── Creation / destruction ────────────────────────────────────────────── */  \
void lucis_vec_init_##SUFFIX(lucis_vec_header* v) {                           \
    v->ptr = NULL;                                                              \
    v->len = 0;                                                                 \
    v->cap = 0;                                                                 \
}                                                                               \
                                                                                \
void lucis_vec_init_cap_##SUFFIX(lucis_vec_header* v, size_t cap) {           \
    v->ptr = malloc(cap * sizeof(T));                                           \
    v->len = 0;                                                                 \
    v->cap = cap;                                                               \
}                                                                               \
                                                                                \
void lucis_vec_free_##SUFFIX(lucis_vec_header* v) {                           \
    free(v->ptr);                                                               \
    v->ptr = NULL;                                                              \
    v->len = 0;                                                                 \
    v->cap = 0;                                                                 \
}                                                                               \
                                                                                \
/* ── Size / capacity ───────────────────────────────────────────────────── */  \
size_t lucis_vec_len_##SUFFIX(const lucis_vec_header* v) {                    \
    return v->len;                                                              \
}                                                                               \
                                                                                \
size_t lucis_vec_capacity_##SUFFIX(const lucis_vec_header* v) {               \
    return v->cap;                                                              \
}                                                                               \
                                                                                \
int lucis_vec_isEmpty_##SUFFIX(const lucis_vec_header* v) {                   \
    return v->len == 0;                                                         \
}                                                                               \
                                                                                \
/* ── Element access ────────────────────────────────────────────────────── */  \
T lucis_vec_at_##SUFFIX(const lucis_vec_header* v, size_t idx) {              \
    if (idx >= v->len) {                                                        \
        fprintf(stderr, "lucis: vec index out of bounds: %zu >= %zu\n",        \
                idx, v->len);                                                   \
        exit(1);                                                                \
    }                                                                           \
    return vec_cdata_##SUFFIX(v)[idx];                                          \
}                                                                               \
                                                                                \
T lucis_vec_first_##SUFFIX(const lucis_vec_header* v) {                       \
    if (v->len == 0) {                                                          \
        fprintf(stderr, "lucis: vec.first() on empty vec\n");                  \
        exit(1);                                                                \
    }                                                                           \
    return vec_cdata_##SUFFIX(v)[0];                                            \
}                                                                               \
                                                                                \
T lucis_vec_last_##SUFFIX(const lucis_vec_header* v) {                        \
    if (v->len == 0) {                                                          \
        fprintf(stderr, "lucis: vec.last() on empty vec\n");                   \
        exit(1);                                                                \
    }                                                                           \
    return vec_cdata_##SUFFIX(v)[v->len - 1];                                   \
}                                                                               \
                                                                                \
void lucis_vec_set_##SUFFIX(lucis_vec_header* v, size_t idx, T val) {       \
    if (idx >= v->len) {                                                        \
        fprintf(stderr, "lucis: vec index out of bounds: %zu >= %zu\n",        \
                idx, v->len);                                                   \
        exit(1);                                                                \
    }                                                                           \
    vec_data_##SUFFIX(v)[idx] = val;                                            \
}                                                                               \
                                                                                \
/* ── Mutation ──────────────────────────────────────────────────────────── */  \
void lucis_vec_push_##SUFFIX(lucis_vec_header* v, T val) {                    \
    vec_grow_##SUFFIX(v, v->len + 1);                                           \
    vec_data_##SUFFIX(v)[v->len++] = val;                                       \
}                                                                               \
                                                                                \
T lucis_vec_pop_##SUFFIX(lucis_vec_header* v) {                               \
    if (v->len == 0) {                                                          \
        fprintf(stderr, "lucis: vec.pop() on empty vec\n");                    \
        exit(1);                                                                \
    }                                                                           \
    return vec_data_##SUFFIX(v)[--v->len];                                      \
}                                                                               \
                                                                                \
void lucis_vec_insert_##SUFFIX(lucis_vec_header* v, size_t idx, T val) {      \
    if (idx > v->len) {                                                         \
        fprintf(stderr, "lucis: vec.insert() index out of bounds\n");          \
        exit(1);                                                                \
    }                                                                           \
    vec_grow_##SUFFIX(v, v->len + 1);                                           \
    T* d = vec_data_##SUFFIX(v);                                                \
    memmove(&d[idx + 1], &d[idx], (v->len - idx) * sizeof(T));                 \
    d[idx] = val;                                                               \
    v->len++;                                                                   \
}                                                                               \
                                                                                \
T lucis_vec_removeAt_##SUFFIX(lucis_vec_header* v, size_t idx) {              \
    if (idx >= v->len) {                                                        \
        fprintf(stderr, "lucis: vec.removeAt() index out of bounds\n");        \
        exit(1);                                                                \
    }                                                                           \
    T* d = vec_data_##SUFFIX(v);                                                \
    T val = d[idx];                                                             \
    memmove(&d[idx], &d[idx + 1], (v->len - idx - 1) * sizeof(T));             \
    v->len--;                                                                   \
    return val;                                                                 \
}                                                                               \
                                                                                \
T lucis_vec_removeSwap_##SUFFIX(lucis_vec_header* v, size_t idx) {            \
    if (idx >= v->len) {                                                        \
        fprintf(stderr, "lucis: vec.removeSwap() index out of bounds\n");      \
        exit(1);                                                                \
    }                                                                           \
    T* d = vec_data_##SUFFIX(v);                                                \
    T val = d[idx];                                                             \
    d[idx] = d[v->len - 1];                                                     \
    v->len--;                                                                   \
    return val;                                                                 \
}                                                                               \
                                                                                \
void lucis_vec_clear_##SUFFIX(lucis_vec_header* v) {                          \
    v->len = 0;                                                                 \
}                                                                               \
                                                                                \
void lucis_vec_fill_##SUFFIX(lucis_vec_header* v, T val) {                    \
    T* d = vec_data_##SUFFIX(v);                                                \
    for (size_t i = 0; i < v->len; i++) d[i] = val;                            \
}                                                                               \
                                                                                \
void lucis_vec_swap_##SUFFIX(lucis_vec_header* v, size_t i, size_t j) {       \
    if (i >= v->len || j >= v->len) {                                           \
        fprintf(stderr, "lucis: vec.swap() index out of bounds\n");            \
        exit(1);                                                                \
    }                                                                           \
    T* d = vec_data_##SUFFIX(v);                                                \
    T tmp = d[i]; d[i] = d[j]; d[j] = tmp;                                     \
}                                                                               \
                                                                                \
/* ── Memory management ─────────────────────────────────────────────────── */  \
void lucis_vec_reserve_##SUFFIX(lucis_vec_header* v, size_t cap) {            \
    vec_grow_##SUFFIX(v, cap);                                                  \
}                                                                               \
                                                                                \
void lucis_vec_shrink_##SUFFIX(lucis_vec_header* v) {                         \
    if (v->len == 0) {                                                          \
        free(v->ptr);                                                           \
        v->ptr = NULL;                                                          \
        v->cap = 0;                                                             \
    } else if (v->cap > v->len) {                                               \
        v->ptr = realloc(v->ptr, v->len * sizeof(T));                           \
        v->cap = v->len;                                                        \
    }                                                                           \
}                                                                               \
                                                                                \
void lucis_vec_resize_##SUFFIX(lucis_vec_header* v, size_t len, T fill) {     \
    vec_grow_##SUFFIX(v, len);                                                  \
    T* d = vec_data_##SUFFIX(v);                                                \
    for (size_t i = v->len; i < len; i++) d[i] = fill;                         \
    v->len = len;                                                               \
}                                                                               \
                                                                                \
void lucis_vec_truncate_##SUFFIX(lucis_vec_header* v, size_t len) {           \
    if (len < v->len) v->len = len;                                             \
}                                                                               \
                                                                                \
/* ── Search ────────────────────────────────────────────────────────────── */  \
int lucis_vec_contains_##SUFFIX(const lucis_vec_header* v, T val) {           \
    const T* d = vec_cdata_##SUFFIX(v);                                         \
    for (size_t i = 0; i < v->len; i++)                                         \
        if (d[i] == val) return 1;                                              \
    return 0;                                                                   \
}                                                                               \
                                                                                \
long long lucis_vec_indexOf_##SUFFIX(const lucis_vec_header* v, T val) {      \
    const T* d = vec_cdata_##SUFFIX(v);                                         \
    for (size_t i = 0; i < v->len; i++)                                         \
        if (d[i] == val) return (long long)i;                                   \
    return -1;                                                                  \
}                                                                               \
                                                                                \
long long lucis_vec_lastIndexOf_##SUFFIX(const lucis_vec_header* v, T val) {  \
    const T* d = vec_cdata_##SUFFIX(v);                                         \
    for (size_t i = v->len; i > 0; i--)                                         \
        if (d[i - 1] == val) return (long long)(i - 1);                         \
    return -1;                                                                  \
}                                                                               \
                                                                                \
size_t lucis_vec_count_##SUFFIX(const lucis_vec_header* v, T val) {           \
    const T* d = vec_cdata_##SUFFIX(v);                                         \
    size_t c = 0;                                                               \
    for (size_t i = 0; i < v->len; i++)                                         \
        if (d[i] == val) c++;                                                   \
    return c;                                                                   \
}                                                                               \
                                                                                \
/* ── Reorder ───────────────────────────────────────────────────────────── */  \
void lucis_vec_reverse_##SUFFIX(lucis_vec_header* v) {                        \
    T* d = vec_data_##SUFFIX(v);                                                \
    for (size_t i = 0, j = v->len - 1; i < j; i++, j--) {                      \
        T tmp = d[i]; d[i] = d[j]; d[j] = tmp;                                 \
    }                                                                           \
}                                                                               \
                                                                                \
/* ── Comparison ────────────────────────────────────────────────────────── */  \
int lucis_vec_equals_##SUFFIX(const lucis_vec_header* a,                      \
                               const lucis_vec_header* b) {                    \
    if (a->len != b->len) return 0;                                             \
    const T* da = vec_cdata_##SUFFIX(a);                                        \
    const T* db = (const T*)b->ptr;                                             \
    for (size_t i = 0; i < a->len; i++)                                         \
        if (da[i] != db[i]) return 0;                                           \
    return 1;                                                                   \
}                                                                               \
                                                                                \
int lucis_vec_isSorted_##SUFFIX(const lucis_vec_header* v) {                  \
    if (v->len <= 1) return 1;                                                  \
    const T* d = vec_cdata_##SUFFIX(v);                                         \
    for (size_t i = 1; i < v->len; i++)                                         \
        if (d[i] < d[i - 1]) return 0;                                         \
    return 1;                                                                   \
}                                                                               \
                                                                                \
/* ── Sort ──────────────────────────────────────────────────────────────── */  \
static int vec_cmp_asc_##SUFFIX(const void* a, const void* b) {                 \
    T va = *(const T*)a, vb = *(const T*)b;                                     \
    return (va > vb) - (va < vb);                                               \
}                                                                               \
static int vec_cmp_desc_##SUFFIX(const void* a, const void* b) {                \
    T va = *(const T*)a, vb = *(const T*)b;                                     \
    return (vb > va) - (vb < va);                                               \
}                                                                               \
void lucis_vec_sort_##SUFFIX(lucis_vec_header* v) {                           \
    if (v->len > 1)                                                             \
        qsort(v->ptr, v->len, sizeof(T), vec_cmp_asc_##SUFFIX);                \
}                                                                               \
void lucis_vec_sortDesc_##SUFFIX(lucis_vec_header* v) {                       \
    if (v->len > 1)                                                             \
        qsort(v->ptr, v->len, sizeof(T), vec_cmp_desc_##SUFFIX);               \
}                                                                               \
                                                                                \
/* ── Rotate ────────────────────────────────────────────────────────────── */  \
void lucis_vec_rotate_##SUFFIX(lucis_vec_header* v, int32_t steps) {          \
    if (v->len <= 1) return;                                                    \
    int64_t n = (int64_t)v->len;                                                \
    int64_t s = ((int64_t)steps % n + n) % n;                                   \
    if (s == 0) return;                                                         \
    T* d = vec_data_##SUFFIX(v);                                                \
    /* reverse [0..n), then [0..s), then [s..n) */                              \
    for (size_t i = 0, j = v->len - 1; i < j; i++, j--) {                      \
        T tmp = d[i]; d[i] = d[j]; d[j] = tmp; }                               \
    for (size_t i = 0, j = (size_t)s - 1; i < j; i++, j--) {                   \
        T tmp = d[i]; d[i] = d[j]; d[j] = tmp; }                               \
    for (size_t i = (size_t)s, j = v->len - 1; i < j; i++, j--) {              \
        T tmp = d[i]; d[i] = d[j]; d[j] = tmp; }                               \
}                                                                               \
                                                                                \
/* ── Aggregation ───────────────────────────────────────────────────────── */  \
T lucis_vec_sum_##SUFFIX(const lucis_vec_header* v) {                         \
    const T* d = vec_cdata_##SUFFIX(v);                                         \
    T acc = 0;                                                                  \
    for (size_t i = 0; i < v->len; i++) acc += d[i];                            \
    return acc;                                                                 \
}                                                                               \
T lucis_vec_product_##SUFFIX(const lucis_vec_header* v) {                     \
    const T* d = vec_cdata_##SUFFIX(v);                                         \
    T acc = 1;                                                                  \
    for (size_t i = 0; i < v->len; i++) acc *= d[i];                            \
    return acc;                                                                 \
}                                                                               \
T lucis_vec_min_##SUFFIX(const lucis_vec_header* v) {                         \
    if (v->len == 0) {                                                          \
        fprintf(stderr, "lucis: vec.min() on empty vec\n"); exit(1); }         \
    const T* d = vec_cdata_##SUFFIX(v);                                         \
    T m = d[0];                                                                 \
    for (size_t i = 1; i < v->len; i++) if (d[i] < m) m = d[i];               \
    return m;                                                                   \
}                                                                               \
T lucis_vec_max_##SUFFIX(const lucis_vec_header* v) {                         \
    if (v->len == 0) {                                                          \
        fprintf(stderr, "lucis: vec.max() on empty vec\n"); exit(1); }         \
    const T* d = vec_cdata_##SUFFIX(v);                                         \
    T m = d[0];                                                                 \
    for (size_t i = 1; i < v->len; i++) if (d[i] > m) m = d[i];               \
    return m;                                                                   \
}                                                                               \
double lucis_vec_average_##SUFFIX(const lucis_vec_header* v) {                \
    if (v->len == 0) return 0.0;                                                \
    const T* d = vec_cdata_##SUFFIX(v);                                         \
    double sum = 0.0;                                                           \
    for (size_t i = 0; i < v->len; i++) sum += (double)d[i];                   \
    return sum / (double)v->len;                                                \
}                                                                               \
                                                                                \
/* ── Clone ─────────────────────────────────────────────────────────────── */  \
void lucis_vec_clone_##SUFFIX(const lucis_vec_header* src,                    \
                               lucis_vec_header* dst) {                        \
    dst->len = src->len;                                                        \
    dst->cap = src->len;                                                        \
    if (src->len > 0) {                                                         \
        dst->ptr = malloc(src->len * sizeof(T));                                \
        memcpy(dst->ptr, src->ptr, src->len * sizeof(T));                       \
    } else {                                                                    \
        dst->ptr = NULL;                                                        \
        dst->cap = 0;                                                           \
    }                                                                           \
}

// ═══════════════════════════════════════════════════════════════════════════════
// Instantiate for all primitive types
// ═══════════════════════════════════════════════════════════════════════════════

LUCIS_VEC_IMPL(int8_t,    i8)
LUCIS_VEC_IMPL(int16_t,   i16)
LUCIS_VEC_IMPL(int32_t,   i32)
LUCIS_VEC_IMPL(int64_t,   i64)
LUCIS_VEC_IMPL(uint8_t,   u8)
LUCIS_VEC_IMPL(uint16_t,  u16)
LUCIS_VEC_IMPL(uint32_t,  u32)
LUCIS_VEC_IMPL(uint64_t,  u64)
LUCIS_VEC_IMPL(float,     f32)
LUCIS_VEC_IMPL(double,    f64)
LUCIS_VEC_IMPL(char,      char)

// ═══════════════════════════════════════════════════════════════════════════════
// toString / join — format-specific implementations
// ═══════════════════════════════════════════════════════════════════════════════

// Helper: build a string by iterating elements with a format specifier
#define LUCIS_VEC_TOSTRING_IMPL(T, SUFFIX, FMT)                                \
lucis_string lucis_vec_toString_##SUFFIX(const lucis_vec_header* v) {         \
    if (v->len == 0) {                                                          \
        const char* s = "[]"; lucis_string r = { s, 2 }; return r;            \
    }                                                                           \
    size_t cap = 64;                                                            \
    char* buf = (char*)malloc(cap);                                             \
    size_t pos = 0;                                                             \
    buf[pos++] = '[';                                                           \
    const T* d = (const T*)v->ptr;                                              \
    for (size_t i = 0; i < v->len; i++) {                                       \
        if (i > 0) { buf[pos++] = ','; buf[pos++] = ' '; }                     \
        while (cap - pos < 32) { cap *= 2; buf = (char*)realloc(buf, cap); }   \
        pos += (size_t)snprintf(buf + pos, cap - pos, FMT, d[i]);              \
    }                                                                           \
    if (pos + 1 >= cap) { cap = pos + 2; buf = (char*)realloc(buf, cap); }     \
    buf[pos++] = ']';                                                           \
    buf[pos] = '\0';                                                            \
    char* _t = (char*)lucis_allocString(pos);                                  \
    if (_t) { memcpy(_t, buf, pos); _t[pos] = '\0'; free(buf); buf = _t; }    \
    lucis_string r = { buf, pos }; return r;                                   \
}                                                                               \
                                                                                \
lucis_string lucis_vec_join_##SUFFIX(const lucis_vec_header* v,              \
                                        lucis_string sep) {                     \
    if (v->len == 0) { lucis_string r = { "", 0 }; return r; }                \
    size_t cap = 64;                                                            \
    char* buf = (char*)malloc(cap);                                             \
    size_t pos = 0;                                                             \
    const T* d = (const T*)v->ptr;                                              \
    for (size_t i = 0; i < v->len; i++) {                                       \
        if (i > 0) {                                                            \
            while (cap - pos < sep.len + 1) { cap *= 2; buf = (char*)realloc(buf, cap); } \
            memcpy(buf + pos, sep.ptr, sep.len); pos += sep.len;                \
        }                                                                       \
        while (cap - pos < 32) { cap *= 2; buf = (char*)realloc(buf, cap); }   \
        pos += (size_t)snprintf(buf + pos, cap - pos, FMT, d[i]);              \
    }                                                                           \
    buf[pos] = '\0';                                                            \
    char* _t = (char*)lucis_allocString(pos);                                  \
    if (_t) { memcpy(_t, buf, pos); _t[pos] = '\0'; free(buf); buf = _t; }    \
    lucis_string r = { buf, pos }; return r;                                   \
}

LUCIS_VEC_TOSTRING_IMPL(int8_t,   i8,   "%d")
LUCIS_VEC_TOSTRING_IMPL(int16_t,  i16,  "%d")
LUCIS_VEC_TOSTRING_IMPL(int32_t,  i32,  "%d")
LUCIS_VEC_TOSTRING_IMPL(int64_t,  i64,  "%lld")
LUCIS_VEC_TOSTRING_IMPL(uint8_t,  u8,   "%u")
LUCIS_VEC_TOSTRING_IMPL(uint16_t, u16,  "%u")
LUCIS_VEC_TOSTRING_IMPL(uint32_t, u32,  "%u")
LUCIS_VEC_TOSTRING_IMPL(uint64_t, u64,  "%llu")
LUCIS_VEC_TOSTRING_IMPL(float,    f32,  "%g")
LUCIS_VEC_TOSTRING_IMPL(double,   f64,  "%g")
LUCIS_VEC_TOSTRING_IMPL(char,     char, "%c")

// ═══════════════════════════════════════════════════════════════════════════════
// Vec<string> — manual implementation (struct comparison needs memcmp)
// ═══════════════════════════════════════════════════════════════════════════════

static inline lucis_string* vec_data_str(lucis_vec_header* v) {
    return (lucis_string*)v->ptr;
}
static inline const lucis_string* vec_cdata_str(const lucis_vec_header* v) {
    return (const lucis_string*)v->ptr;
}

static void vec_grow_str(lucis_vec_header* v, size_t needed) {
    if (needed <= v->cap) return;
    size_t newCap = v->cap ? v->cap : LUCIS_VEC_INITIAL_CAP;
    while (newCap < needed) newCap *= LUCIS_VEC_GROWTH_FACTOR;
    v->ptr = realloc(v->ptr, newCap * sizeof(lucis_string));
    v->cap = newCap;
}

static int str_equal(lucis_string a, lucis_string b) {
    return a.len == b.len && (a.ptr == b.ptr || memcmp(a.ptr, b.ptr, a.len) == 0);
}

static lucis_string clone_string_value(lucis_string s) {
    char* buf = (char*)lucis_allocString(s.len + 1);
    if (!buf) return (lucis_string){ "", 0 };
    if (s.len > 0 && s.ptr) memcpy(buf, s.ptr, s.len);
    buf[s.len] = '\0';
    return (lucis_string){ buf, s.len };
}

// ── Creation / destruction ──────────────────────────────────────────────────
void lucis_vec_init_str(lucis_vec_header* v) {
    v->ptr = NULL; v->len = 0; v->cap = 0;
}
void lucis_vec_init_cap_str(lucis_vec_header* v, size_t cap) {
    v->ptr = malloc(cap * sizeof(lucis_string)); v->len = 0; v->cap = cap;
}
void lucis_vec_free_str(lucis_vec_header* v) {
    lucis_string* d = vec_data_str(v);
    for (size_t i = 0; i < v->len; i++) {
        lucis_freeStr(d[i].ptr, d[i].len);
    }
    free(v->ptr); v->ptr = NULL; v->len = 0; v->cap = 0;
}

// ── Size / capacity ─────────────────────────────────────────────────────────
size_t lucis_vec_len_str(const lucis_vec_header* v) { return v->len; }
size_t lucis_vec_capacity_str(const lucis_vec_header* v) { return v->cap; }
int    lucis_vec_isEmpty_str(const lucis_vec_header* v) { return v->len == 0; }

// ── Element access ──────────────────────────────────────────────────────────
lucis_string lucis_vec_at_str(const lucis_vec_header* v, size_t idx) {
    if (idx >= v->len) {
        fprintf(stderr, "lucis: vec index out of bounds: %zu >= %zu\n", idx, v->len);
        exit(1);
    }
    return vec_cdata_str(v)[idx];
}
lucis_string lucis_vec_first_str(const lucis_vec_header* v) {
    if (v->len == 0) { fprintf(stderr, "lucis: vec.first() on empty vec\n"); exit(1); }
    return vec_cdata_str(v)[0];
}
lucis_string lucis_vec_last_str(const lucis_vec_header* v) {
    if (v->len == 0) { fprintf(stderr, "lucis: vec.last() on empty vec\n"); exit(1); }
    return vec_cdata_str(v)[v->len - 1];
}
void lucis_vec_set_str(lucis_vec_header* v, size_t idx, lucis_string val) {
    if (idx >= v->len) {
        fprintf(stderr, "lucis: vec index out of bounds: %zu >= %zu\n", idx, v->len);
        exit(1);
    }
    lucis_string* d = vec_data_str(v);
    if (d[idx].ptr == val.ptr && d[idx].len == val.len) return;
    lucis_freeStr(d[idx].ptr, d[idx].len);
    d[idx] = val;
}

// ── Mutation ────────────────────────────────────────────────────────────────
void lucis_vec_push_str(lucis_vec_header* v, lucis_string val) {
    vec_grow_str(v, v->len + 1);
    vec_data_str(v)[v->len++] = val;
}
lucis_string lucis_vec_pop_str(lucis_vec_header* v) {
    if (v->len == 0) { fprintf(stderr, "lucis: vec.pop() on empty vec\n"); exit(1); }
    return vec_data_str(v)[--v->len];
}
void lucis_vec_insert_str(lucis_vec_header* v, size_t idx, lucis_string val) {
    if (idx > v->len) {
        fprintf(stderr, "lucis: vec.insert() index out of bounds\n"); exit(1);
    }
    vec_grow_str(v, v->len + 1);
    lucis_string* d = vec_data_str(v);
    memmove(&d[idx + 1], &d[idx], (v->len - idx) * sizeof(lucis_string));
    d[idx] = val;
    v->len++;
}
lucis_string lucis_vec_removeAt_str(lucis_vec_header* v, size_t idx) {
    if (idx >= v->len) {
        fprintf(stderr, "lucis: vec.removeAt() index out of bounds\n"); exit(1);
    }
    lucis_string* d = vec_data_str(v);
    lucis_string val = d[idx];
    memmove(&d[idx], &d[idx + 1], (v->len - idx - 1) * sizeof(lucis_string));
    v->len--;
    return val;
}
lucis_string lucis_vec_removeSwap_str(lucis_vec_header* v, size_t idx) {
    if (idx >= v->len) {
        fprintf(stderr, "lucis: vec.removeSwap() index out of bounds\n"); exit(1);
    }
    lucis_string* d = vec_data_str(v);
    lucis_string val = d[idx];
    d[idx] = d[v->len - 1];
    v->len--;
    return val;
}
void lucis_vec_clear_str(lucis_vec_header* v) {
    lucis_string* d = vec_data_str(v);
    for (size_t i = 0; i < v->len; i++) {
        lucis_freeStr(d[i].ptr, d[i].len);
    }
    v->len = 0;
}
void lucis_vec_fill_str(lucis_vec_header* v, lucis_string val) {
    lucis_string* d = vec_data_str(v);
    for (size_t i = 0; i < v->len; i++) {
        lucis_freeStr(d[i].ptr, d[i].len);
        d[i] = clone_string_value(val);
    }
}
void lucis_vec_swap_str(lucis_vec_header* v, size_t i, size_t j) {
    if (i >= v->len || j >= v->len) {
        fprintf(stderr, "lucis: vec.swap() index out of bounds\n"); exit(1);
    }
    lucis_string* d = vec_data_str(v);
    lucis_string tmp = d[i]; d[i] = d[j]; d[j] = tmp;
}

// ── Memory ──────────────────────────────────────────────────────────────────
void lucis_vec_reserve_str(lucis_vec_header* v, size_t cap) {
    vec_grow_str(v, cap);
}
void lucis_vec_shrink_str(lucis_vec_header* v) {
    if (v->len == 0) { free(v->ptr); v->ptr = NULL; v->cap = 0; }
    else if (v->cap > v->len) {
        v->ptr = realloc(v->ptr, v->len * sizeof(lucis_string));
        v->cap = v->len;
    }
}
void lucis_vec_resize_str(lucis_vec_header* v, size_t len, lucis_string fill) {
    lucis_string* d = vec_data_str(v);
    if (len < v->len) {
        for (size_t i = len; i < v->len; i++)
            lucis_freeStr(d[i].ptr, d[i].len);
        v->len = len;
        return;
    }
    vec_grow_str(v, len);
    d = vec_data_str(v);
    for (size_t i = v->len; i < len; i++) d[i] = clone_string_value(fill);
    v->len = len;
}
void lucis_vec_truncate_str(lucis_vec_header* v, size_t len) {
    if (len < v->len) {
        lucis_string* d = vec_data_str(v);
        for (size_t i = len; i < v->len; i++) {
            lucis_freeStr(d[i].ptr, d[i].len);
        }
        v->len = len;
    }
}

// ── Search (string comparison via memcmp) ───────────────────────────────────
int lucis_vec_contains_str(const lucis_vec_header* v, lucis_string val) {
    const lucis_string* d = vec_cdata_str(v);
    for (size_t i = 0; i < v->len; i++)
        if (str_equal(d[i], val)) return 1;
    return 0;
}
long long lucis_vec_indexOf_str(const lucis_vec_header* v, lucis_string val) {
    const lucis_string* d = vec_cdata_str(v);
    for (size_t i = 0; i < v->len; i++)
        if (str_equal(d[i], val)) return (long long)i;
    return -1;
}
long long lucis_vec_lastIndexOf_str(const lucis_vec_header* v, lucis_string val) {
    const lucis_string* d = vec_cdata_str(v);
    for (size_t i = v->len; i > 0; i--)
        if (str_equal(d[i - 1], val)) return (long long)(i - 1);
    return -1;
}
size_t lucis_vec_count_str(const lucis_vec_header* v, lucis_string val) {
    const lucis_string* d = vec_cdata_str(v);
    size_t c = 0;
    for (size_t i = 0; i < v->len; i++)
        if (str_equal(d[i], val)) c++;
    return c;
}

// ── Reorder ─────────────────────────────────────────────────────────────────
void lucis_vec_reverse_str(lucis_vec_header* v) {
    if (v->len <= 1) return;
    lucis_string* d = vec_data_str(v);
    for (size_t i = 0, j = v->len - 1; i < j; i++, j--) {
        lucis_string tmp = d[i]; d[i] = d[j]; d[j] = tmp;
    }
}

// ── Comparison ──────────────────────────────────────────────────────────────
int lucis_vec_equals_str(const lucis_vec_header* a, const lucis_vec_header* b) {
    if (a->len != b->len) return 0;
    const lucis_string* da = vec_cdata_str(a);
    const lucis_string* db = (const lucis_string*)b->ptr;
    for (size_t i = 0; i < a->len; i++)
        if (!str_equal(da[i], db[i])) return 0;
    return 1;
}

// ── Rotate ──────────────────────────────────────────────────────────────────
void lucis_vec_rotate_str(lucis_vec_header* v, int32_t steps) {
    if (v->len <= 1) return;
    int64_t n = (int64_t)v->len;
    int64_t s = ((int64_t)steps % n + n) % n;
    if (s == 0) return;
    lucis_string* d = vec_data_str(v);
    for (size_t i = 0, j = v->len - 1; i < j; i++, j--) {
        lucis_string tmp = d[i]; d[i] = d[j]; d[j] = tmp; }
    for (size_t i = 0, j = (size_t)s - 1; i < j; i++, j--) {
        lucis_string tmp = d[i]; d[i] = d[j]; d[j] = tmp; }
    for (size_t i = (size_t)s, j = v->len - 1; i < j; i++, j--) {
        lucis_string tmp = d[i]; d[i] = d[j]; d[j] = tmp; }
}

// ── Conversion ──────────────────────────────────────────────────────────────
lucis_string lucis_vec_toString_str(const lucis_vec_header* v) {
    if (v->len == 0) {
        lucis_string r = { "[]", 2 }; return r;
    }
    const lucis_string* d = vec_cdata_str(v);
    size_t total = 2; // '[' and ']'
    for (size_t i = 0; i < v->len; i++) {
        if (i > 0) total += 2; // ", "
        total += d[i].len + 2; // quotes + content
    }

    char* buf = (char*)lucis_allocString(total + 1);
    if (!buf) {
        lucis_string r = { "", 0 }; return r;
    }

    size_t pos = 0;
    buf[pos++] = '[';
    for (size_t i = 0; i < v->len; i++) {
        if (i > 0) { buf[pos++] = ','; buf[pos++] = ' '; }
        buf[pos++] = '"';
        memcpy(buf + pos, d[i].ptr, d[i].len); pos += d[i].len;
        buf[pos++] = '"';
    }
    buf[pos++] = ']';
    buf[pos] = '\0';
    lucis_string r = { buf, pos }; return r;
}

lucis_string lucis_vec_join_str(const lucis_vec_header* v, lucis_string sep) {
    if (v->len == 0) { lucis_string r = { "", 0 }; return r; }
    const lucis_string* d = vec_cdata_str(v);
    // Calculate total size
    size_t total = 0;
    for (size_t i = 0; i < v->len; i++) {
        if (i > 0) total += sep.len;
        total += d[i].len;
    }
    char* buf = (char*)lucis_allocString(total + 1);
    if (!buf) {
        lucis_string r = { "", 0 }; return r;
    }
    size_t pos = 0;
    for (size_t i = 0; i < v->len; i++) {
        if (i > 0) { memcpy(buf + pos, sep.ptr, sep.len); pos += sep.len; }
        memcpy(buf + pos, d[i].ptr, d[i].len); pos += d[i].len;
    }
    buf[pos] = '\0';
    lucis_string r = { buf, pos }; return r;
}

// ── Clone ───────────────────────────────────────────────────────────────────
void lucis_vec_clone_str(const lucis_vec_header* src, lucis_vec_header* dst) {
    dst->len = src->len;
    dst->cap = src->len;
    if (src->len > 0) {
        dst->ptr = malloc(src->len * sizeof(lucis_string));
        memcpy(dst->ptr, src->ptr, src->len * sizeof(lucis_string));
    } else {
        dst->ptr = NULL;
        dst->cap = 0;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Args helper — converts C main(argc, argv) → Vec<string>
// ═══════════════════════════════════════════════════════════════════════════════

void lucis_args_init(lucis_vec_header* out, int argc, const char** argv) {
    lucis_vec_init_cap_str(out, (size_t)argc);
    for (int i = 0; i < argc; i++) {
        lucis_string s;
        s.ptr = argv[i];
        s.len = strlen(argv[i]);
        lucis_vec_push_str(out, s);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Raw (opaque struct) vec — for vec<UserStruct> and other non-primitive types
// ═══════════════════════════════════════════════════════════════════════════════

#define LUCIS_VEC_RAW_INITIAL_CAP 8
#define LUCIS_VEC_RAW_GROWTH 2

void lucis_vec_init_raw(lucis_vec_header* v) {
    v->ptr = NULL;
    v->len = 0;
    v->cap = 0;
}

void lucis_vec_init_cap_raw(lucis_vec_header* v, size_t cap, size_t elem_size) {
    v->ptr = malloc(cap * elem_size);
    v->len = 0;
    v->cap = cap;
}

void lucis_vec_push_raw(lucis_vec_header* v, const void* elem, size_t elem_size) {
    if (v->len >= v->cap) {
        size_t new_cap = v->cap ? v->cap * LUCIS_VEC_RAW_GROWTH : LUCIS_VEC_RAW_INITIAL_CAP;
        v->ptr = realloc(v->ptr, new_cap * elem_size);
        v->cap = new_cap;
    }
    memcpy((char*)v->ptr + v->len * elem_size, elem, elem_size);
    v->len++;
}

void lucis_vec_free_raw(lucis_vec_header* v) {
    free(v->ptr);
    v->ptr = NULL;
    v->len = 0;
    v->cap = 0;
}

size_t lucis_vec_len_raw(const lucis_vec_header* v) {
    return v->len;
}

void* lucis_vec_ptr_raw(const lucis_vec_header* v, size_t idx, size_t elem_size) {
    if (idx >= v->len) {
        fprintf(stderr, "lucis: vec index out of bounds: %zu >= %zu\n", idx, v->len);
        exit(1);
    }
    return (char*)v->ptr + idx * elem_size;
}

int lucis_vec_isEmpty_raw(const lucis_vec_header* v) {
    return v->len == 0;
}

size_t lucis_vec_capacity_raw(const lucis_vec_header* v) {
    return v->cap;
}

void lucis_vec_clear_raw(lucis_vec_header* v) {
    v->len = 0;
}
