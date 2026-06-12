#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ── String return type (matches lucis string ABI) ──────────────────────────
typedef struct { const char* ptr; size_t len; } lucis_str_result;

// ── Search & Match ──────────────────────────────────────────────────────────

int      lucis_contains(const char* s, size_t sLen,
                         const char* sub, size_t subLen);
int      lucis_startsWith(const char* s, size_t sLen,
                           const char* prefix, size_t prefixLen);
int      lucis_endsWith(const char* s, size_t sLen,
                         const char* suffix, size_t suffixLen);
int64_t  lucis_indexOf(const char* s, size_t sLen,
                        const char* sub, size_t subLen);
int64_t  lucis_lastIndexOf(const char* s, size_t sLen,
                            const char* sub, size_t subLen);
size_t   lucis_count(const char* s, size_t sLen,
                      const char* sub, size_t subLen);

// ── Transformation ──────────────────────────────────────────────────────────

lucis_str_result lucis_toUpper(const char* s, size_t sLen);
lucis_str_result lucis_toLower(const char* s, size_t sLen);
lucis_str_result lucis_trim(const char* s, size_t sLen);
lucis_str_result lucis_trimLeft(const char* s, size_t sLen);
lucis_str_result lucis_trimRight(const char* s, size_t sLen);
lucis_str_result lucis_replace(const char* s, size_t sLen,
                                 const char* old, size_t oldLen,
                                 const char* rep, size_t repLen);
lucis_str_result lucis_replaceFirst(const char* s, size_t sLen,
                                      const char* old, size_t oldLen,
                                      const char* rep, size_t repLen);
lucis_str_result lucis_repeat(const char* s, size_t sLen, size_t n);
lucis_str_result lucis_reverse(const char* s, size_t sLen);

// ── Formatting ──────────────────────────────────────────────────────────────

lucis_str_result lucis_padLeft(const char* s, size_t sLen,
                                 size_t width, char fill);
lucis_str_result lucis_padRight(const char* s, size_t sLen,
                                  size_t width, char fill);

// ── Extraction ──────────────────────────────────────────────────────────────

lucis_str_result lucis_substring(const char* s, size_t sLen,
                                   size_t start, size_t length);
char              lucis_charAt(const char* s, size_t sLen, size_t index);
lucis_str_result lucis_slice(const char* s, size_t sLen,
                               int64_t start, int64_t end);

// ── Parsing ─────────────────────────────────────────────────────────────────

int64_t  lucis_parseInt(const char* s, size_t sLen);
int64_t  lucis_parseIntRadix(const char* s, size_t sLen, uint32_t radix);
double   lucis_parseFloat(const char* s, size_t sLen);

// ── Conversion ──────────────────────────────────────────────────────────────

char     lucis_fromCharCode(int32_t code);

// ── Vec-returning functions ─────────────────────────────────────────────────
typedef struct { void* ptr; size_t len; size_t cap; } lucis_str_vec_header;

void lucis_split(lucis_str_vec_header* out,
                  const char* s, size_t sLen,
                  const char* delim, size_t delimLen);
void lucis_splitN(lucis_str_vec_header* out,
                   const char* s, size_t sLen,
                   const char* delim, size_t delimLen, size_t maxParts);
lucis_str_result lucis_joinVec(const lucis_str_vec_header* vec,
                                 const char* sep, size_t sepLen);
void lucis_lines(lucis_str_vec_header* out, const char* s, size_t sLen);
void lucis_chars(lucis_str_vec_header* out, const char* s, size_t sLen);
lucis_str_result lucis_fromCharsVec(const lucis_str_vec_header* vec);
void lucis_toBytes(lucis_str_vec_header* out, const char* s, size_t sLen);
lucis_str_result lucis_fromBytesVec(const lucis_str_vec_header* vec);

// ── C FFI String Conversion ─────────────────────────────────────────────────

char*             lucis_cstr(const char* s, size_t sLen);
void*             lucis_allocString(size_t size);
lucis_str_result  lucis_fromCStr(const char* cstr);
lucis_str_result  lucis_fromCStrCopy(const char* cstr);
lucis_str_result  lucis_fromCStrLen(const char* cstr, size_t len);
void            lucis_freeStr(const char* ptr, size_t len);

// ── Additional String Methods ───────────────────────────────────────────────

lucis_str_result lucis_trimChar(const char* s, size_t sLen, char ch);
lucis_str_result lucis_capitalize(const char* s, size_t sLen);
lucis_str_result lucis_removePrefix(const char* s, size_t sLen,
                                const char* prefix, size_t prefixLen);
lucis_str_result lucis_removeSuffix(const char* s, size_t sLen,
                                const char* suffix, size_t suffixLen);
lucis_str_result lucis_strInsert(const char* s, size_t sLen,
                             size_t pos, const char* ins, size_t insLen);
lucis_str_result lucis_strRemove(const char* s, size_t sLen,
                             size_t start, size_t count);
lucis_str_result lucis_concat(const char* a, size_t aLen,
                          const char* b, size_t bLen);
int32_t        lucis_compareTo(const char* a, size_t aLen,
                             const char* b, size_t bLen);
int            lucis_equalsIgnoreCase(const char* a, size_t aLen,
                                    const char* b, size_t bLen);
int            lucis_strIsNumeric(const char* s, size_t sLen);
int            lucis_strIsAlpha(const char* s, size_t sLen);
int            lucis_strIsAlphaNum(const char* s, size_t sLen);
int            lucis_strIsUpper(const char* s, size_t sLen);
int            lucis_strIsLower(const char* s, size_t sLen);
int            lucis_strIsBlank(const char* s, size_t sLen);
int            lucis_strToBool(const char* s, size_t sLen);
void           lucis_words(lucis_str_vec_header* out, const char* s, size_t sLen);

#ifdef __cplusplus
}
#endif
