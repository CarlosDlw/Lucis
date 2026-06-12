#ifndef LUCIS_REGEX_H
#define LUCIS_REGEX_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char* ptr;
    size_t      len;
} lucis_regex_str_result;

/* Check if string matches pattern (full or partial match) */
int32_t lucis_regexMatch(const char* text, size_t text_len,
                           const char* pat, size_t pat_len);

/* Find first match, return matched substring */
lucis_regex_str_result lucis_regexFind(const char* text, size_t text_len,
                                          const char* pat, size_t pat_len);

/* Index of first match (-1 if none) */
int64_t lucis_regexFindIndex(const char* text, size_t text_len,
                               const char* pat, size_t pat_len);

/* Replace all matches with replacement string */
lucis_regex_str_result lucis_regexReplace(const char* text, size_t text_len,
                                             const char* pat, size_t pat_len,
                                             const char* rep, size_t rep_len);

/* Replace first match only */
lucis_regex_str_result lucis_regexReplaceFirst(const char* text, size_t text_len,
                                                  const char* pat, size_t pat_len,
                                                  const char* rep, size_t rep_len);

/* Check if pattern is valid regex */
int32_t lucis_regexIsValid(const char* pat, size_t pat_len);

/* Vec-returning regex functions */
typedef struct { void* ptr; size_t len; size_t cap; } lucis_regex_vec_header;

void lucis_regexFindAll(lucis_regex_vec_header* out,
                         const char* text, size_t text_len,
                         const char* pat, size_t pat_len);
void lucis_regexSplit(lucis_regex_vec_header* out,
                       const char* text, size_t text_len,
                       const char* pat, size_t pat_len);

#ifdef __cplusplus
}
#endif

#endif /* LUCIS_REGEX_H */
