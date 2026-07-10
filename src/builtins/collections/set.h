#ifndef LUCIS_SET_H
#define LUCIS_SET_H

#include <stddef.h>
#include <stdint.h>
#include "int256.h"

// ═══════════════════════════════════════════════════════════════════════════
// Set<T> — Open-addressing hash set
//
// LLVM struct: { ptr states, ptr keys, ptr hashes,
//                usize len, usize cap, usize key_size }
// C struct mirrors LLVM layout exactly.
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
    const char* ptr;
    size_t      len;
} lucis_set_string;

typedef struct {
    uint8_t*  states;     // 0=empty, 1=occupied, 2=tombstone
    void*     keys;       // flat key array (key_size * cap bytes)
    uint64_t* hashes;     // cached hashes per slot
    size_t    len;        // active entries
    size_t    cap;        // total slots
    size_t    key_size;   // bytes per key
} lucis_set_header;

// Vec out-parameter (mirrors lucis_vec_header layout)
typedef struct {
    void*  ptr;
    size_t len;
    size_t cap;
} lucis_set_vec_out;

// ── Macro-generated declarations ─────────────────────────────────────────

// For integer element types: ET = C type, ES = suffix
#define LUCIS_SET_DECL_INT(ET, ES)                                          \
void   lucis_set_init_##ES(lucis_set_header* s);                           \
void   lucis_set_free_##ES(lucis_set_header* s);                           \
size_t lucis_set_len_##ES(const lucis_set_header* s);                      \
int    lucis_set_isEmpty_##ES(const lucis_set_header* s);                  \
int    lucis_set_add_##ES(lucis_set_header* s, ET elem);                   \
int    lucis_set_has_##ES(lucis_set_header* s, ET elem);                   \
int    lucis_set_remove_##ES(lucis_set_header* s, ET elem);                \
void   lucis_set_clear_##ES(lucis_set_header* s);                          \
void   lucis_set_values_##ES(lucis_set_header* s, lucis_set_vec_out* out);

// For string element type
#define LUCIS_SET_DECL_STR()                                                \
void   lucis_set_init_str(lucis_set_header* s);                            \
void   lucis_set_free_str(lucis_set_header* s);                            \
size_t lucis_set_len_str(const lucis_set_header* s);                       \
int    lucis_set_isEmpty_str(const lucis_set_header* s);                   \
int    lucis_set_add_str(lucis_set_header* s, lucis_set_string elem);     \
int    lucis_set_has_str(lucis_set_header* s, lucis_set_string elem);     \
int    lucis_set_remove_str(lucis_set_header* s, lucis_set_string elem);  \
void   lucis_set_clear_str(lucis_set_header* s);                           \
void   lucis_set_values_str(lucis_set_header* s, lucis_set_vec_out* out);

// ── Instantiate declarations ─────────────────────────────────────────────

// Integer element types
LUCIS_SET_DECL_INT(int8_t,   i8)
LUCIS_SET_DECL_INT(int16_t,  i16)
LUCIS_SET_DECL_INT(int32_t,  i32)
LUCIS_SET_DECL_INT(int64_t,  i64)
LUCIS_SET_DECL_INT(uint8_t,  u8)
LUCIS_SET_DECL_INT(uint16_t, u16)
LUCIS_SET_DECL_INT(uint32_t, u32)
LUCIS_SET_DECL_INT(uint64_t, u64)
LUCIS_SET_DECL_INT(__int128_t,  i128)
LUCIS_SET_DECL_INT(__uint128_t, u128)
LUCIS_SET_DECL_INT(lucis_int256_t, iinf)

// String element type
LUCIS_SET_DECL_STR()

// ── Raw (opaque struct) element variant ───────────────────────────────
// Used when element is a user-defined struct (builtinSuffix == "raw").
// Hashing is byte-level FNV-1a; equality is bitwise memcmp.
void   lucis_set_init_raw(lucis_set_header* s, size_t elem_size);
void   lucis_set_free_raw(lucis_set_header* s);
size_t lucis_set_len_raw(const lucis_set_header* s);
int    lucis_set_add_raw(lucis_set_header* s, const void* elem);
int    lucis_set_has_raw(lucis_set_header* s, const void* elem);
int    lucis_set_remove_raw(lucis_set_header* s, const void* elem);
void   lucis_set_values_raw(lucis_set_header* s, lucis_set_vec_out* out);
int    lucis_set_isEmpty_raw(const lucis_set_header* s);
void   lucis_set_clear_raw(lucis_set_header* s);

#endif // LUCIS_SET_H
