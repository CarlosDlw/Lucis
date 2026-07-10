#ifndef LUCIS_MAP_H
#define LUCIS_MAP_H

#include <stddef.h>
#include <stdint.h>
#include "int256.h"

// ═══════════════════════════════════════════════════════════════════════════
// Map<K,V> — Open-addressing hash map
//
// LLVM struct: { ptr states, ptr keys, ptr values, ptr hashes,
//                usize len, usize cap, usize key_size, usize val_size }
// C struct mirrors LLVM layout exactly.
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
    const char* ptr;
    size_t      len;
} lucis_map_string;

typedef struct {
    uint8_t*  states;     // 0=empty, 1=occupied, 2=tombstone
    void*     keys;       // flat key array   (key_size * cap bytes)
    void*     values;     // flat value array  (val_size * cap bytes)
    uint64_t* hashes;     // cached hashes per slot
    size_t    len;        // active entries
    size_t    cap;        // total slots
    size_t    key_size;   // bytes per key
    size_t    val_size;   // bytes per value
} lucis_map_header;

// Vec out-parameter (mirrors lucis_vec_header layout)
typedef struct {
    void*  ptr;
    size_t len;
    size_t cap;
} lucis_map_vec_out;

// ── Macro-generated declarations ─────────────────────────────────────────

// For integer keys: KT = C type, KS = suffix
#define LUCIS_MAP_DECL_INT_KEY(KT, KS, VT, VS)                             \
void   lucis_map_init_##KS##_##VS(lucis_map_header* m);                    \
void   lucis_map_free_##KS##_##VS(lucis_map_header* m);                    \
size_t lucis_map_len_##KS##_##VS(const lucis_map_header* m);               \
int    lucis_map_isEmpty_##KS##_##VS(const lucis_map_header* m);           \
void   lucis_map_set_##KS##_##VS(lucis_map_header* m, KT key, VT val);    \
VT     lucis_map_get_##KS##_##VS(lucis_map_header* m, KT key);            \
VT     lucis_map_getOrDefault_##KS##_##VS(lucis_map_header* m,             \
                                           KT key, VT def);                 \
int    lucis_map_has_##KS##_##VS(lucis_map_header* m, KT key);            \
int    lucis_map_remove_##KS##_##VS(lucis_map_header* m, KT key);         \
void   lucis_map_clear_##KS##_##VS(lucis_map_header* m);                  \
void   lucis_map_keys_##KS##_##VS(lucis_map_header* m,                     \
                                    lucis_map_vec_out* out);               \
void   lucis_map_values_##KS##_##VS(lucis_map_header* m,                   \
                                      lucis_map_vec_out* out);

// For string keys: key is always lucis_map_string
#define LUCIS_MAP_DECL_STR_KEY(VT, VS)                                      \
void   lucis_map_init_str_##VS(lucis_map_header* m);                       \
void   lucis_map_free_str_##VS(lucis_map_header* m);                       \
size_t lucis_map_len_str_##VS(const lucis_map_header* m);                  \
int    lucis_map_isEmpty_str_##VS(const lucis_map_header* m);              \
void   lucis_map_set_str_##VS(lucis_map_header* m,                         \
                               lucis_map_string key, VT val);              \
VT     lucis_map_get_str_##VS(lucis_map_header* m,                         \
                               lucis_map_string key);                      \
VT     lucis_map_getOrDefault_str_##VS(lucis_map_header* m,                \
                               lucis_map_string key, VT def);              \
int    lucis_map_has_str_##VS(lucis_map_header* m,                         \
                               lucis_map_string key);                      \
int    lucis_map_remove_str_##VS(lucis_map_header* m,                      \
                               lucis_map_string key);                      \
void   lucis_map_clear_str_##VS(lucis_map_header* m);                      \
void   lucis_map_keys_str_##VS(lucis_map_header* m,                        \
                                 lucis_map_vec_out* out);                  \
void   lucis_map_values_str_##VS(lucis_map_header* m,                      \
                                   lucis_map_vec_out* out);

// String key + string value (special return type)
#define LUCIS_MAP_DECL_STR_STR()                                            \
void             lucis_map_init_str_str(lucis_map_header* m);              \
void             lucis_map_free_str_str(lucis_map_header* m);              \
size_t           lucis_map_len_str_str(const lucis_map_header* m);         \
int              lucis_map_isEmpty_str_str(const lucis_map_header* m);     \
void             lucis_map_set_str_str(lucis_map_header* m,                \
                                        lucis_map_string key,              \
                                        lucis_map_string val);             \
lucis_map_string lucis_map_get_str_str(lucis_map_header* m,               \
                                        lucis_map_string key);             \
lucis_map_string lucis_map_getOrDefault_str_str(lucis_map_header* m,      \
                                        lucis_map_string key,              \
                                        lucis_map_string def);             \
int              lucis_map_has_str_str(lucis_map_header* m,                \
                                        lucis_map_string key);             \
int              lucis_map_remove_str_str(lucis_map_header* m,             \
                                        lucis_map_string key);             \
void             lucis_map_clear_str_str(lucis_map_header* m);             \
void             lucis_map_keys_str_str(lucis_map_header* m,               \
                                          lucis_map_vec_out* out);         \
void             lucis_map_values_str_str(lucis_map_header* m,             \
                                            lucis_map_vec_out* out);

// Integer key + string value
#define LUCIS_MAP_DECL_INT_KEY_STR_VAL(KT, KS)                             \
void             lucis_map_init_##KS##_str(lucis_map_header* m);           \
void             lucis_map_free_##KS##_str(lucis_map_header* m);           \
size_t           lucis_map_len_##KS##_str(const lucis_map_header* m);      \
int              lucis_map_isEmpty_##KS##_str(const lucis_map_header* m);  \
void             lucis_map_set_##KS##_str(lucis_map_header* m,             \
                                           KT key, lucis_map_string val);  \
lucis_map_string lucis_map_get_##KS##_str(lucis_map_header* m, KT key);   \
lucis_map_string lucis_map_getOrDefault_##KS##_str(lucis_map_header* m,   \
                                           KT key, lucis_map_string def);  \
int              lucis_map_has_##KS##_str(lucis_map_header* m, KT key);    \
int              lucis_map_remove_##KS##_str(lucis_map_header* m, KT key); \
void             lucis_map_clear_##KS##_str(lucis_map_header* m);          \
void             lucis_map_keys_##KS##_str(lucis_map_header* m,            \
                                             lucis_map_vec_out* out);      \
void             lucis_map_values_##KS##_str(lucis_map_header* m,          \
                                               lucis_map_vec_out* out);

// ── Instantiate declarations ─────────────────────────────────────────────

// String key × numeric values
LUCIS_MAP_DECL_STR_KEY(int8_t,   i8)
LUCIS_MAP_DECL_STR_KEY(int16_t,  i16)
LUCIS_MAP_DECL_STR_KEY(int32_t,  i32)
LUCIS_MAP_DECL_STR_KEY(int64_t,  i64)
LUCIS_MAP_DECL_STR_KEY(uint8_t,  u8)
LUCIS_MAP_DECL_STR_KEY(uint16_t, u16)
LUCIS_MAP_DECL_STR_KEY(uint32_t, u32)
LUCIS_MAP_DECL_STR_KEY(uint64_t, u64)
LUCIS_MAP_DECL_STR_KEY(float,    f32)
LUCIS_MAP_DECL_STR_KEY(double,   f64)

// String key × string value
LUCIS_MAP_DECL_STR_STR()

// Int32 key × all values
LUCIS_MAP_DECL_INT_KEY(int32_t, i32, int8_t,   i8)
LUCIS_MAP_DECL_INT_KEY(int32_t, i32, int16_t,  i16)
LUCIS_MAP_DECL_INT_KEY(int32_t, i32, int32_t,  i32)
LUCIS_MAP_DECL_INT_KEY(int32_t, i32, int64_t,  i64)
LUCIS_MAP_DECL_INT_KEY(int32_t, i32, uint8_t,  u8)
LUCIS_MAP_DECL_INT_KEY(int32_t, i32, uint16_t, u16)
LUCIS_MAP_DECL_INT_KEY(int32_t, i32, uint32_t, u32)
LUCIS_MAP_DECL_INT_KEY(int32_t, i32, uint64_t, u64)
LUCIS_MAP_DECL_INT_KEY(int32_t, i32, float,    f32)
LUCIS_MAP_DECL_INT_KEY(int32_t, i32, double,   f64)
LUCIS_MAP_DECL_INT_KEY_STR_VAL(int32_t, i32)

// Int64 key × all values
LUCIS_MAP_DECL_INT_KEY(int64_t, i64, int8_t,   i8)
LUCIS_MAP_DECL_INT_KEY(int64_t, i64, int16_t,  i16)
LUCIS_MAP_DECL_INT_KEY(int64_t, i64, int32_t,  i32)
LUCIS_MAP_DECL_INT_KEY(int64_t, i64, int64_t,  i64)
LUCIS_MAP_DECL_INT_KEY(int64_t, i64, uint8_t,  u8)
LUCIS_MAP_DECL_INT_KEY(int64_t, i64, uint16_t, u16)
LUCIS_MAP_DECL_INT_KEY(int64_t, i64, uint32_t, u32)
LUCIS_MAP_DECL_INT_KEY(int64_t, i64, uint64_t, u64)
LUCIS_MAP_DECL_INT_KEY(int64_t, i64, float,    f32)
LUCIS_MAP_DECL_INT_KEY(int64_t, i64, double,   f64)
LUCIS_MAP_DECL_INT_KEY_STR_VAL(int64_t, i64)

// Uint64 key × all values
LUCIS_MAP_DECL_INT_KEY(uint64_t, u64, int8_t,   i8)
LUCIS_MAP_DECL_INT_KEY(uint64_t, u64, int16_t,  i16)
LUCIS_MAP_DECL_INT_KEY(uint64_t, u64, int32_t,  i32)
LUCIS_MAP_DECL_INT_KEY(uint64_t, u64, int64_t,  i64)
LUCIS_MAP_DECL_INT_KEY(uint64_t, u64, uint8_t,  u8)
LUCIS_MAP_DECL_INT_KEY(uint64_t, u64, uint16_t, u16)
LUCIS_MAP_DECL_INT_KEY(uint64_t, u64, uint32_t, u32)
LUCIS_MAP_DECL_INT_KEY(uint64_t, u64, uint64_t, u64)
LUCIS_MAP_DECL_INT_KEY(uint64_t, u64, float,    f32)
LUCIS_MAP_DECL_INT_KEY(uint64_t, u64, double,   f64)
LUCIS_MAP_DECL_INT_KEY_STR_VAL(uint64_t, u64)

// Uint128 key × all values
LUCIS_MAP_DECL_INT_KEY(__uint128_t, u128, int8_t,   i8)
LUCIS_MAP_DECL_INT_KEY(__uint128_t, u128, int16_t,  i16)
LUCIS_MAP_DECL_INT_KEY(__uint128_t, u128, int32_t,  i32)
LUCIS_MAP_DECL_INT_KEY(__uint128_t, u128, int64_t,  i64)
LUCIS_MAP_DECL_INT_KEY(__uint128_t, u128, uint8_t,  u8)
LUCIS_MAP_DECL_INT_KEY(__uint128_t, u128, uint16_t, u16)
LUCIS_MAP_DECL_INT_KEY(__uint128_t, u128, uint32_t, u32)
LUCIS_MAP_DECL_INT_KEY(__uint128_t, u128, uint64_t, u64)
LUCIS_MAP_DECL_INT_KEY(__uint128_t, u128, float,    f32)
LUCIS_MAP_DECL_INT_KEY(__uint128_t, u128, double,   f64)
LUCIS_MAP_DECL_INT_KEY_STR_VAL(__uint128_t, u128)

// Int128 key × all values
LUCIS_MAP_DECL_INT_KEY(__int128_t, i128, int8_t,   i8)
LUCIS_MAP_DECL_INT_KEY(__int128_t, i128, int16_t,  i16)
LUCIS_MAP_DECL_INT_KEY(__int128_t, i128, int32_t,  i32)
LUCIS_MAP_DECL_INT_KEY(__int128_t, i128, int64_t,  i64)
LUCIS_MAP_DECL_INT_KEY(__int128_t, i128, uint8_t,  u8)
LUCIS_MAP_DECL_INT_KEY(__int128_t, i128, uint16_t, u16)
LUCIS_MAP_DECL_INT_KEY(__int128_t, i128, uint32_t, u32)
LUCIS_MAP_DECL_INT_KEY(__int128_t, i128, uint64_t, u64)
LUCIS_MAP_DECL_INT_KEY(__int128_t, i128, float,    f32)
LUCIS_MAP_DECL_INT_KEY(__int128_t, i128, double,   f64)
LUCIS_MAP_DECL_INT_KEY_STR_VAL(__int128_t, i128)

// Int32 key × uint128/int128 values
LUCIS_MAP_DECL_INT_KEY(int32_t, i32, __uint128_t, u128)
LUCIS_MAP_DECL_INT_KEY(int32_t, i32, __int128_t,  i128)

// Int64 key × uint128/int128 values
LUCIS_MAP_DECL_INT_KEY(int64_t, i64, __uint128_t, u128)
LUCIS_MAP_DECL_INT_KEY(int64_t, i64, __int128_t,  i128)

// Uint64 key × uint128/int128 values
LUCIS_MAP_DECL_INT_KEY(uint64_t, u64, __uint128_t, u128)
LUCIS_MAP_DECL_INT_KEY(uint64_t, u64, __int128_t,  i128)

// String key × uint128/int128 values
LUCIS_MAP_DECL_STR_KEY(__uint128_t, u128)
LUCIS_MAP_DECL_STR_KEY(__int128_t,  i128)

// Int32/i64/u64 key × intinf values
LUCIS_MAP_DECL_INT_KEY(int32_t, i32, lucis_int256_t, iinf)
LUCIS_MAP_DECL_INT_KEY(int64_t, i64, lucis_int256_t, iinf)
LUCIS_MAP_DECL_INT_KEY(uint64_t, u64, lucis_int256_t, iinf)

// String key × intinf value
LUCIS_MAP_DECL_STR_KEY(lucis_int256_t, iinf)

// Uint1 key × all numeric values
LUCIS_MAP_DECL_INT_KEY(uint8_t, u1, int8_t,   i8)
LUCIS_MAP_DECL_INT_KEY(uint8_t, u1, int16_t,  i16)
LUCIS_MAP_DECL_INT_KEY(uint8_t, u1, int32_t,  i32)
LUCIS_MAP_DECL_INT_KEY(uint8_t, u1, int64_t,  i64)
LUCIS_MAP_DECL_INT_KEY(uint8_t, u1, uint8_t,  u8)
LUCIS_MAP_DECL_INT_KEY(uint8_t, u1, uint16_t, u16)
LUCIS_MAP_DECL_INT_KEY(uint8_t, u1, uint32_t, u32)
LUCIS_MAP_DECL_INT_KEY(uint8_t, u1, uint64_t, u64)
LUCIS_MAP_DECL_INT_KEY(uint8_t, u1, float,    f32)
LUCIS_MAP_DECL_INT_KEY(uint8_t, u1, double,   f64)
LUCIS_MAP_DECL_INT_KEY_STR_VAL(uint8_t, u1)

// Int1 key × all numeric values
LUCIS_MAP_DECL_INT_KEY(uint8_t, i1, int8_t,   i8)
LUCIS_MAP_DECL_INT_KEY(uint8_t, i1, int16_t,  i16)
LUCIS_MAP_DECL_INT_KEY(uint8_t, i1, int32_t,  i32)
LUCIS_MAP_DECL_INT_KEY(uint8_t, i1, int64_t,  i64)
LUCIS_MAP_DECL_INT_KEY(uint8_t, i1, uint8_t,  u8)
LUCIS_MAP_DECL_INT_KEY(uint8_t, i1, uint16_t, u16)
LUCIS_MAP_DECL_INT_KEY(uint8_t, i1, uint32_t, u32)
LUCIS_MAP_DECL_INT_KEY(uint8_t, i1, uint64_t, u64)
LUCIS_MAP_DECL_INT_KEY(uint8_t, i1, float,    f32)
LUCIS_MAP_DECL_INT_KEY(uint8_t, i1, double,   f64)
LUCIS_MAP_DECL_INT_KEY_STR_VAL(uint8_t, i1)

// Int32/i64/u64 key × u1/i1 values
LUCIS_MAP_DECL_INT_KEY(int32_t, i32, uint8_t, u1)
LUCIS_MAP_DECL_INT_KEY(int32_t, i32, uint8_t, i1)
LUCIS_MAP_DECL_INT_KEY(int64_t, i64, uint8_t, u1)
LUCIS_MAP_DECL_INT_KEY(int64_t, i64, uint8_t, i1)
LUCIS_MAP_DECL_INT_KEY(uint64_t, u64, uint8_t, u1)
LUCIS_MAP_DECL_INT_KEY(uint64_t, u64, uint8_t, i1)

// String key × u1/i1 values
LUCIS_MAP_DECL_STR_KEY(uint8_t, u1)
LUCIS_MAP_DECL_STR_KEY(uint8_t, i1)

// Uint1 key × uint128/int128 values
LUCIS_MAP_DECL_INT_KEY(uint8_t, u1, __uint128_t, u128)
LUCIS_MAP_DECL_INT_KEY(uint8_t, u1, __int128_t,  i128)

// Int1 key × uint128/int128 values
LUCIS_MAP_DECL_INT_KEY(uint8_t, i1, __uint128_t, u128)
LUCIS_MAP_DECL_INT_KEY(uint8_t, i1, __int128_t,  i128)

// Uint1/Int1 key × intinf value
LUCIS_MAP_DECL_INT_KEY(uint8_t, u1, lucis_int256_t, iinf)
LUCIS_MAP_DECL_INT_KEY(uint8_t, i1, lucis_int256_t, iinf)

// Uint128/Int128 key × u1/i1 values
LUCIS_MAP_DECL_INT_KEY(__uint128_t, u128, uint8_t, u1)
LUCIS_MAP_DECL_INT_KEY(__uint128_t, u128, uint8_t, i1)
LUCIS_MAP_DECL_INT_KEY(__int128_t,  i128, uint8_t, u1)
LUCIS_MAP_DECL_INT_KEY(__int128_t,  i128, uint8_t, i1)

// Intinf key × u1/i1 values
LUCIS_MAP_DECL_INT_KEY(lucis_int256_t, iinf, uint8_t, u1)
LUCIS_MAP_DECL_INT_KEY(lucis_int256_t, iinf, uint8_t, i1)

// ── Raw (opaque struct) value variants ─────────────────────────────────
// Used when value is a user-defined struct (builtinSuffix == "raw").
// val_size is passed explicitly to init; stored in m->val_size afterward.
void   lucis_map_init_str_raw(lucis_map_header* m, size_t val_size);
void   lucis_map_free_str_raw(lucis_map_header* m);
size_t lucis_map_len_str_raw(const lucis_map_header* m);
void   lucis_map_set_str_raw(lucis_map_header* m, lucis_map_string key,
                            const void* val);
void   lucis_map_get_str_raw(lucis_map_header* m, lucis_map_string key,
                            void* val_out);
int    lucis_map_has_str_raw(lucis_map_header* m, lucis_map_string key);
void   lucis_map_values_str_raw(lucis_map_header* m, lucis_map_vec_out* out);
int    lucis_map_isEmpty_str_raw(const lucis_map_header* m);
int    lucis_map_remove_str_raw(lucis_map_header* m, lucis_map_string key);
void   lucis_map_clear_str_raw(lucis_map_header* m);

#endif // LUCIS_MAP_H
