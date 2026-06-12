#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ── Process Control ─────────────────────────────────────────────────────────

void lucis_exit(int code);
void lucis_panic(const char* msg, size_t len);
void lucis_assert(int cond, const char* file, size_t fileLen, int line);
void lucis_assertMsg(int cond, const char* msg, size_t msgLen);
void lucis_unreachable(const char* file, size_t fileLen, int line);

// ── Type Conversions (string → T) ──────────────────────────────────────────

int64_t lucis_toInt(const char* data, size_t len);
double  lucis_toFloat(const char* data, size_t len);
int     lucis_toBool(const char* data, size_t len);

// ── toString (T → string) ──────────────────────────────────────────────────
// Returns { ptr, len } matching the lucis string ABI.
// The returned pointer is heap-allocated; caller owns the memory.

typedef struct { const char* ptr; size_t len; } lucis_string_ret;

lucis_string_ret lucis_toString_i8(int8_t val);
lucis_string_ret lucis_toString_i16(int16_t val);
lucis_string_ret lucis_toString_i32(int32_t val);
lucis_string_ret lucis_toString_i64(int64_t val);
lucis_string_ret lucis_toString_u8(uint8_t val);
lucis_string_ret lucis_toString_u16(uint16_t val);
lucis_string_ret lucis_toString_u32(uint32_t val);
lucis_string_ret lucis_toString_u64(uint64_t val);
lucis_string_ret lucis_toString_f32(float val);
lucis_string_ret lucis_toString_f64(double val);
lucis_string_ret lucis_toString_bool(int val);
lucis_string_ret lucis_toString_char(char val);

#ifdef __cplusplus
}
#endif
