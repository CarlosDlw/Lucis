#include "char/char.h"

// ═══════════════════════════════════════════════════════════════════════════
// std::char — Character Classification & Conversion
// ═══════════════════════════════════════════════════════════════════════════

// ── Classification ───────────────────────────────────────────────────────

int lucis_isAlpha(uint8_t c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

int lucis_isDigit(uint8_t c) {
    return c >= '0' && c <= '9';
}

int lucis_isAlphaNum(uint8_t c) {
    return lucis_isAlpha(c) || lucis_isDigit(c);
}

int lucis_isUpper(uint8_t c) {
    return c >= 'A' && c <= 'Z';
}

int lucis_isLower(uint8_t c) {
    return c >= 'a' && c <= 'z';
}

int lucis_isWhitespace(uint8_t c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
           c == '\f' || c == '\v';
}

int lucis_isPrintable(uint8_t c) {
    return c >= 0x20 && c <= 0x7E;
}

int lucis_isControl(uint8_t c) {
    return c < 0x20 || c == 0x7F;
}

int lucis_isHexDigit(uint8_t c) {
    return (c >= '0' && c <= '9') ||
           (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

int lucis_isAscii(uint8_t c) {
    return c <= 127;
}

// ── Conversion ───────────────────────────────────────────────────────────

uint8_t lucis_char_toUpper(uint8_t c) {
    if (c >= 'a' && c <= 'z') return c - 32;
    return c;
}

uint8_t lucis_char_toLower(uint8_t c) {
    if (c >= 'A' && c <= 'Z') return c + 32;
    return c;
}

int32_t lucis_toDigit(uint8_t c) {
    if (c >= '0' && c <= '9') return (int32_t)(c - '0');
    return -1;
}

uint8_t lucis_fromDigit(int32_t d) {
    if (d >= 0 && d <= 9) return (uint8_t)('0' + d);
    return '0';
}
