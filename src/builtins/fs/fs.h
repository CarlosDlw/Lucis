#ifndef LUCIS_FS_H
#define LUCIS_FS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char* ptr;
    size_t      len;
} lucis_fs_str_result;

/* Read / Write */
lucis_fs_str_result lucis_readFile(const char* path, size_t path_len);
void lucis_writeFile(const char* path, size_t path_len,
                      const char* data, size_t data_len);
void lucis_appendFile(const char* path, size_t path_len,
                       const char* data, size_t data_len);

/* Queries */
int32_t lucis_exists(const char* path, size_t path_len);
int32_t lucis_isFile(const char* path, size_t path_len);
int32_t lucis_isDir(const char* path, size_t path_len);
int64_t lucis_fileSize(const char* path, size_t path_len);

/* Mutation */
int32_t lucis_remove(const char* path, size_t path_len);
int32_t lucis_removeDir(const char* path, size_t path_len);
int32_t lucis_fsRename(const char* from, size_t from_len,
                        const char* to,   size_t to_len);
int32_t lucis_mkdir(const char* path, size_t path_len);
int32_t lucis_mkdirAll(const char* path, size_t path_len);

/* Working directory */
lucis_fs_str_result lucis_cwd(void);
int32_t lucis_setCwd(const char* path, size_t path_len);

/* Temp */
lucis_fs_str_result lucis_tempDir(void);

/* Vec-returning functions */
typedef struct { void* ptr; size_t len; size_t cap; } lucis_fs_vec_header;

void lucis_listDir(lucis_fs_vec_header* out, const char* path, size_t path_len);
void lucis_readFileBytes(lucis_fs_vec_header* out, const char* path, size_t path_len);
void lucis_writeFileBytes(const char* path, size_t path_len,
                           const lucis_fs_vec_header* data);

#ifdef __cplusplus
}
#endif

#endif /* LUCIS_FS_H */
