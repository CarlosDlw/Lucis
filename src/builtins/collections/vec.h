#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ═══════════════════════════════════════════════════════════════════════════════
// Vec<T> runtime — monomorphized per element type
//
// Naming convention: lucis_vec_<method>_<suffix>(vec_ptr, ...)
//
// Vec layout (same for all element types):
//   struct { void* ptr; size_t len; size_t cap; }
//
// The vec struct is always passed and manipulated by pointer.
// ═══════════════════════════════════════════════════════════════════════════════

// Generic vec struct layout (element-type-agnostic header)
typedef struct {
    void*  ptr;
    size_t len;
    size_t cap;
} lucis_vec_header;

// ── Macro to declare all vec functions for a given element type ──────────────
#define LUCIS_VEC_DECLARE(T, SUFFIX)                                           \
    /* Creation / destruction */                                                \
    void lucis_vec_init_##SUFFIX(lucis_vec_header* v);                        \
    void lucis_vec_init_cap_##SUFFIX(lucis_vec_header* v, size_t cap);        \
    void lucis_vec_free_##SUFFIX(lucis_vec_header* v);                        \
    /* Size / capacity */                                                       \
    size_t lucis_vec_len_##SUFFIX(const lucis_vec_header* v);                 \
    size_t lucis_vec_capacity_##SUFFIX(const lucis_vec_header* v);            \
    int    lucis_vec_isEmpty_##SUFFIX(const lucis_vec_header* v);             \
    /* Element access */                                                        \
    T    lucis_vec_at_##SUFFIX(const lucis_vec_header* v, size_t idx);        \
    T    lucis_vec_first_##SUFFIX(const lucis_vec_header* v);                 \
    T    lucis_vec_last_##SUFFIX(const lucis_vec_header* v);                  \
    void lucis_vec_set_##SUFFIX(lucis_vec_header* v, size_t idx, T val);     \
    /* Mutation */                                                              \
    void lucis_vec_push_##SUFFIX(lucis_vec_header* v, T val);                 \
    T    lucis_vec_pop_##SUFFIX(lucis_vec_header* v);                         \
    void lucis_vec_insert_##SUFFIX(lucis_vec_header* v, size_t idx, T val);   \
    T    lucis_vec_removeAt_##SUFFIX(lucis_vec_header* v, size_t idx);        \
    T    lucis_vec_removeSwap_##SUFFIX(lucis_vec_header* v, size_t idx);      \
    void lucis_vec_clear_##SUFFIX(lucis_vec_header* v);                       \
    void lucis_vec_fill_##SUFFIX(lucis_vec_header* v, T val);                 \
    void lucis_vec_swap_##SUFFIX(lucis_vec_header* v, size_t i, size_t j);    \
    /* Memory */                                                                \
    void lucis_vec_reserve_##SUFFIX(lucis_vec_header* v, size_t cap);         \
    void lucis_vec_shrink_##SUFFIX(lucis_vec_header* v);                      \
    void lucis_vec_resize_##SUFFIX(lucis_vec_header* v, size_t len, T fill);  \
    void lucis_vec_truncate_##SUFFIX(lucis_vec_header* v, size_t len);        \
    /* Search */                                                                \
    int    lucis_vec_contains_##SUFFIX(const lucis_vec_header* v, T val);     \
    long long lucis_vec_indexOf_##SUFFIX(const lucis_vec_header* v, T val);   \
    long long lucis_vec_lastIndexOf_##SUFFIX(const lucis_vec_header* v, T val);\
    size_t lucis_vec_count_##SUFFIX(const lucis_vec_header* v, T val);        \
    /* Comparison */                                                            \
    int    lucis_vec_equals_##SUFFIX(const lucis_vec_header* a,               \
                                      const lucis_vec_header* b);              \
    int    lucis_vec_isSorted_##SUFFIX(const lucis_vec_header* v);            \
    /* Reorder */                                                               \
    void lucis_vec_reverse_##SUFFIX(lucis_vec_header* v);                     \
    void lucis_vec_sort_##SUFFIX(lucis_vec_header* v);                        \
    void lucis_vec_sortDesc_##SUFFIX(lucis_vec_header* v);                    \
    void lucis_vec_rotate_##SUFFIX(lucis_vec_header* v, int32_t steps);       \
    /* Aggregation */                                                           \
    T    lucis_vec_sum_##SUFFIX(const lucis_vec_header* v);                   \
    T    lucis_vec_product_##SUFFIX(const lucis_vec_header* v);               \
    T    lucis_vec_min_##SUFFIX(const lucis_vec_header* v);                   \
    T    lucis_vec_max_##SUFFIX(const lucis_vec_header* v);                   \
    double lucis_vec_average_##SUFFIX(const lucis_vec_header* v);             \
    /* Clone */                                                                 \
    void lucis_vec_clone_##SUFFIX(const lucis_vec_header* src, lucis_vec_header* dst);

// Declare for all primitive types
LUCIS_VEC_DECLARE(int8_t,               i8)
LUCIS_VEC_DECLARE(int16_t,              i16)
LUCIS_VEC_DECLARE(int32_t,              i32)
LUCIS_VEC_DECLARE(int64_t,              i64)
LUCIS_VEC_DECLARE(uint8_t,              u8)
LUCIS_VEC_DECLARE(uint16_t,             u16)
LUCIS_VEC_DECLARE(uint32_t,             u32)
LUCIS_VEC_DECLARE(uint64_t,             u64)
LUCIS_VEC_DECLARE(float,                f32)
LUCIS_VEC_DECLARE(double,               f64)
LUCIS_VEC_DECLARE(char,                 char)

// ── String type for Vec<string> ─────────────────────────────────────────────
// String is a fat pointer { ptr, len }, not a simple scalar.
// Search functions (contains, indexOf, etc.) need custom comparison,
// so we declare them manually instead of using the macro.

typedef struct {
    const char* ptr;
    size_t      len;
} lucis_string;

// ── toString / join — declared per type (need lucis_string defined above) ──
#define LUCIS_VEC_DECLARE_TOSTRING(SUFFIX)                                     \
    lucis_string lucis_vec_toString_##SUFFIX(const lucis_vec_header* v);      \
    lucis_string lucis_vec_join_##SUFFIX(const lucis_vec_header* v,           \
                                           lucis_string sep);

LUCIS_VEC_DECLARE_TOSTRING(i8)
LUCIS_VEC_DECLARE_TOSTRING(i16)
LUCIS_VEC_DECLARE_TOSTRING(i32)
LUCIS_VEC_DECLARE_TOSTRING(i64)
LUCIS_VEC_DECLARE_TOSTRING(u8)
LUCIS_VEC_DECLARE_TOSTRING(u16)
LUCIS_VEC_DECLARE_TOSTRING(u32)
LUCIS_VEC_DECLARE_TOSTRING(u64)
LUCIS_VEC_DECLARE_TOSTRING(f32)
LUCIS_VEC_DECLARE_TOSTRING(f64)
LUCIS_VEC_DECLARE_TOSTRING(char)

// Creation / destruction
void   lucis_vec_init_str(lucis_vec_header* v);
void   lucis_vec_init_cap_str(lucis_vec_header* v, size_t cap);
void   lucis_vec_free_str(lucis_vec_header* v);

// Size / capacity
size_t lucis_vec_len_str(const lucis_vec_header* v);
size_t lucis_vec_capacity_str(const lucis_vec_header* v);
int    lucis_vec_isEmpty_str(const lucis_vec_header* v);

// Element access
lucis_string lucis_vec_at_str(const lucis_vec_header* v, size_t idx);
lucis_string lucis_vec_first_str(const lucis_vec_header* v);
lucis_string lucis_vec_last_str(const lucis_vec_header* v);
void          lucis_vec_set_str(lucis_vec_header* v, size_t idx, lucis_string val);

// Mutation
void          lucis_vec_push_str(lucis_vec_header* v, lucis_string val);
lucis_string lucis_vec_pop_str(lucis_vec_header* v);
void          lucis_vec_insert_str(lucis_vec_header* v, size_t idx, lucis_string val);
lucis_string lucis_vec_removeAt_str(lucis_vec_header* v, size_t idx);
lucis_string lucis_vec_removeSwap_str(lucis_vec_header* v, size_t idx);
void          lucis_vec_clear_str(lucis_vec_header* v);
void          lucis_vec_fill_str(lucis_vec_header* v, lucis_string val);
void          lucis_vec_swap_str(lucis_vec_header* v, size_t i, size_t j);

// Memory
void   lucis_vec_reserve_str(lucis_vec_header* v, size_t cap);
void   lucis_vec_shrink_str(lucis_vec_header* v);
void   lucis_vec_resize_str(lucis_vec_header* v, size_t len, lucis_string fill);
void   lucis_vec_truncate_str(lucis_vec_header* v, size_t len);

// Search (uses memcmp for string comparison)
int       lucis_vec_contains_str(const lucis_vec_header* v, lucis_string val);
long long lucis_vec_indexOf_str(const lucis_vec_header* v, lucis_string val);
long long lucis_vec_lastIndexOf_str(const lucis_vec_header* v, lucis_string val);
size_t    lucis_vec_count_str(const lucis_vec_header* v, lucis_string val);

// Comparison
int       lucis_vec_equals_str(const lucis_vec_header* a, const lucis_vec_header* b);

// Reorder
void lucis_vec_reverse_str(lucis_vec_header* v);
void lucis_vec_rotate_str(lucis_vec_header* v, int32_t steps);

// Conversion
lucis_string lucis_vec_toString_str(const lucis_vec_header* v);
lucis_string lucis_vec_join_str(const lucis_vec_header* v, lucis_string sep);

// Clone
void lucis_vec_clone_str(const lucis_vec_header* src, lucis_vec_header* dst);

// ── Args helper ─────────────────────────────────────────────────────────────
// Converts C main(argc, argv) into a Vec<string> for lucis programs.
void lucis_args_init(lucis_vec_header* out, int argc, const char** argv);

// ── Raw (opaque struct) variant — used for vec<UserStruct> ──────────────────
// Element type is opaque; functions take elem_size to handle arbitrary layouts.
void   lucis_vec_init_raw(lucis_vec_header* v);
void   lucis_vec_init_cap_raw(lucis_vec_header* v, size_t cap, size_t elem_size);
void   lucis_vec_push_raw(lucis_vec_header* v, const void* elem, size_t elem_size);
void   lucis_vec_free_raw(lucis_vec_header* v);
size_t lucis_vec_len_raw(const lucis_vec_header* v);
void*  lucis_vec_ptr_raw(const lucis_vec_header* v, size_t idx, size_t elem_size);
int    lucis_vec_isEmpty_raw(const lucis_vec_header* v);
size_t lucis_vec_capacity_raw(const lucis_vec_header* v);
void   lucis_vec_clear_raw(lucis_vec_header* v);

#ifdef __cplusplus
}
#endif
