#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ── String return type (matches lux string ABI) ──────────────────────────
typedef struct { const char* ptr; size_t len; } lux_str_result;

// ── Search & Match ──────────────────────────────────────────────────────────

int      lux_contains(const char* s, size_t sLen,
                         const char* sub, size_t subLen);
int      lux_startsWith(const char* s, size_t sLen,
                           const char* prefix, size_t prefixLen);
int      lux_endsWith(const char* s, size_t sLen,
                         const char* suffix, size_t suffixLen);
int64_t  lux_indexOf(const char* s, size_t sLen,
                        const char* sub, size_t subLen);
int64_t  lux_lastIndexOf(const char* s, size_t sLen,
                            const char* sub, size_t subLen);
size_t   lux_count(const char* s, size_t sLen,
                      const char* sub, size_t subLen);

// ── Transformation ──────────────────────────────────────────────────────────

lux_str_result lux_toUpper(const char* s, size_t sLen);
lux_str_result lux_toLower(const char* s, size_t sLen);
lux_str_result lux_trim(const char* s, size_t sLen);
lux_str_result lux_trimLeft(const char* s, size_t sLen);
lux_str_result lux_trimRight(const char* s, size_t sLen);
lux_str_result lux_replace(const char* s, size_t sLen,
                                 const char* old, size_t oldLen,
                                 const char* rep, size_t repLen);
lux_str_result lux_replaceFirst(const char* s, size_t sLen,
                                      const char* old, size_t oldLen,
                                      const char* rep, size_t repLen);
lux_str_result lux_repeat(const char* s, size_t sLen, size_t n);
lux_str_result lux_reverse(const char* s, size_t sLen);

// ── Formatting ──────────────────────────────────────────────────────────────

lux_str_result lux_padLeft(const char* s, size_t sLen,
                                 size_t width, char fill);
lux_str_result lux_padRight(const char* s, size_t sLen,
                                  size_t width, char fill);

// ── Extraction ──────────────────────────────────────────────────────────────

lux_str_result lux_substring(const char* s, size_t sLen,
                                   size_t start, size_t length);
char              lux_charAt(const char* s, size_t sLen, size_t index);
lux_str_result lux_slice(const char* s, size_t sLen,
                               int64_t start, int64_t end);

// ── Parsing ─────────────────────────────────────────────────────────────────

int64_t  lux_parseInt(const char* s, size_t sLen);
int64_t  lux_parseIntRadix(const char* s, size_t sLen, uint32_t radix);
double   lux_parseFloat(const char* s, size_t sLen);

// ── Conversion ──────────────────────────────────────────────────────────────

char     lux_fromCharCode(int32_t code);

// ── Vec-returning functions ─────────────────────────────────────────────────
typedef struct { void* ptr; size_t len; size_t cap; } lux_str_vec_header;

void lux_split(lux_str_vec_header* out,
                  const char* s, size_t sLen,
                  const char* delim, size_t delimLen);
void lux_splitN(lux_str_vec_header* out,
                   const char* s, size_t sLen,
                   const char* delim, size_t delimLen, size_t maxParts);
lux_str_result lux_joinVec(const lux_str_vec_header* vec,
                                 const char* sep, size_t sepLen);
void lux_lines(lux_str_vec_header* out, const char* s, size_t sLen);
void lux_chars(lux_str_vec_header* out, const char* s, size_t sLen);
lux_str_result lux_fromCharsVec(const lux_str_vec_header* vec);
void lux_toBytes(lux_str_vec_header* out, const char* s, size_t sLen);
lux_str_result lux_fromBytesVec(const lux_str_vec_header* vec);

// ── C FFI String Conversion ─────────────────────────────────────────────────

char*             lux_cstr(const char* s, size_t sLen);
void*             lux_allocString(size_t size);
lux_str_result  lux_fromCStr(const char* cstr);
lux_str_result  lux_fromCStrCopy(const char* cstr);
lux_str_result  lux_fromCStrLen(const char* cstr, size_t len);
void            lux_freeStr(const char* ptr, size_t len);

// ── Additional String Methods ───────────────────────────────────────────────

lux_str_result lux_trimChar(const char* s, size_t sLen, char ch);
lux_str_result lux_capitalize(const char* s, size_t sLen);
lux_str_result lux_removePrefix(const char* s, size_t sLen,
                                const char* prefix, size_t prefixLen);
lux_str_result lux_removeSuffix(const char* s, size_t sLen,
                                const char* suffix, size_t suffixLen);
lux_str_result lux_strInsert(const char* s, size_t sLen,
                             size_t pos, const char* ins, size_t insLen);
lux_str_result lux_strRemove(const char* s, size_t sLen,
                             size_t start, size_t count);
lux_str_result lux_concat(const char* a, size_t aLen,
                          const char* b, size_t bLen);
int32_t        lux_compareTo(const char* a, size_t aLen,
                             const char* b, size_t bLen);
int            lux_equalsIgnoreCase(const char* a, size_t aLen,
                                    const char* b, size_t bLen);
int            lux_strIsNumeric(const char* s, size_t sLen);
int            lux_strIsAlpha(const char* s, size_t sLen);
int            lux_strIsAlphaNum(const char* s, size_t sLen);
int            lux_strIsUpper(const char* s, size_t sLen);
int            lux_strIsLower(const char* s, size_t sLen);
int            lux_strIsBlank(const char* s, size_t sLen);
int            lux_strToBool(const char* s, size_t sLen);
void           lux_words(lux_str_vec_header* out, const char* s, size_t sLen);

#ifdef __cplusplus
}
#endif
