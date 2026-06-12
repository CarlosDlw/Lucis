#ifndef LUCIS_FMT_H
#define LUCIS_FMT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char* ptr;
    size_t      len;
} lucis_fmt_str_result;

/* Padding */
lucis_fmt_str_result lucis_lpad(const char* s, size_t s_len,
                                   size_t width, uint8_t fill);
lucis_fmt_str_result lucis_rpad(const char* s, size_t s_len,
                                   size_t width, uint8_t fill);
lucis_fmt_str_result lucis_center(const char* s, size_t s_len,
                                     size_t width, uint8_t fill);

/* Integer formatting */
lucis_fmt_str_result lucis_fmtHex(uint64_t val);
lucis_fmt_str_result lucis_fmtHexUpper(uint64_t val);
lucis_fmt_str_result lucis_fmtOct(uint64_t val);
lucis_fmt_str_result lucis_fmtBin(uint64_t val);

/* Float formatting */
lucis_fmt_str_result lucis_fixed(double val, uint32_t decimals);
lucis_fmt_str_result lucis_scientific(double val);

/* Human-readable */
lucis_fmt_str_result lucis_humanBytes(uint64_t bytes);
lucis_fmt_str_result lucis_commas(int64_t val);
lucis_fmt_str_result lucis_percent(double val);

#ifdef __cplusplus
}
#endif

#endif /* LUCIS_FMT_H */
