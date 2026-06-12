#ifndef LUCIS_CHAR_H
#define LUCIS_CHAR_H

#include <stdint.h>

// ═══════════════════════════════════════════════════════════════════════════
// std::char — Character Classification & Conversion
//
// All classification functions take uint8_t (char = i8) and return int (bool).
// Conversion functions: toUpper/toLower return uint8_t, toDigit returns int32_t.
// ═══════════════════════════════════════════════════════════════════════════

// Classification
int     lucis_isAlpha(uint8_t c);
int     lucis_isDigit(uint8_t c);
int     lucis_isAlphaNum(uint8_t c);
int     lucis_isUpper(uint8_t c);
int     lucis_isLower(uint8_t c);
int     lucis_isWhitespace(uint8_t c);
int     lucis_isPrintable(uint8_t c);
int     lucis_isControl(uint8_t c);
int     lucis_isHexDigit(uint8_t c);
int     lucis_isAscii(uint8_t c);

// Conversion
uint8_t lucis_char_toUpper(uint8_t c);
uint8_t lucis_char_toLower(uint8_t c);
int32_t lucis_toDigit(uint8_t c);
uint8_t lucis_fromDigit(int32_t d);

#endif // LUCIS_CHAR_H
