#ifndef LUCIS_PATH_H
#define LUCIS_PATH_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char* ptr;
    size_t      len;
} lucis_path_str_result;

/* Join */
lucis_path_str_result lucis_pathJoin(const char* a, size_t a_len,
                                       const char* b, size_t b_len);

/* Components */
lucis_path_str_result lucis_parent(const char* p, size_t len);
lucis_path_str_result lucis_fileName(const char* p, size_t len);
lucis_path_str_result lucis_stem(const char* p, size_t len);
lucis_path_str_result lucis_extension(const char* p, size_t len);

/* Queries */
int32_t lucis_isAbsolute(const char* p, size_t len);
int32_t lucis_isRelative(const char* p, size_t len);

/* Transform */
lucis_path_str_result lucis_normalize(const char* p, size_t len);
lucis_path_str_result lucis_toAbsolute(const char* p, size_t len);

/* Separator */
uint8_t lucis_separator(void);

/* Modify */
lucis_path_str_result lucis_withExtension(const char* p, size_t p_len,
                                             const char* ext, size_t ext_len);
lucis_path_str_result lucis_withFileName(const char* p, size_t p_len,
                                            const char* name, size_t name_len);

/* Vec-based path operations */
typedef struct { void* ptr; size_t len; size_t cap; } lucis_path_vec_header;

lucis_path_str_result lucis_joinAllVec(const lucis_path_vec_header* parts);

#ifdef __cplusplus
}
#endif

#endif /* LUCIS_PATH_H */
