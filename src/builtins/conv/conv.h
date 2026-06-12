#ifndef LUCIS_CONV_H
#define LUCIS_CONV_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char* ptr;
    size_t      len;
} lucis_conv_str_result;

/* Integer → string */
lucis_conv_str_result lucis_itoa(int64_t value);
lucis_conv_str_result lucis_itoaRadix(int64_t value, uint32_t radix);
lucis_conv_str_result lucis_utoa(uint64_t value);

/* Float → string */
lucis_conv_str_result lucis_ftoa(double value);
lucis_conv_str_result lucis_ftoaPrecision(double value, uint32_t precision);

/* String → number */
int64_t lucis_atoi(const char* data, size_t len);
double  lucis_atof(const char* data, size_t len);

/* Radix formatting */
lucis_conv_str_result lucis_toHex(uint64_t value);
lucis_conv_str_result lucis_toOctal(uint64_t value);
lucis_conv_str_result lucis_toBinary(uint64_t value);
uint64_t lucis_fromHex(const char* data, size_t len);

/* Char ↔ int */
int32_t lucis_charToInt(int8_t c);
int8_t  lucis_intToChar(int32_t code);

#ifdef __cplusplus
}
#endif

#endif /* LUCIS_CONV_H */
