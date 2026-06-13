#define _POSIX_C_SOURCE 200809L

#include "fs.h"
#include "../string/string.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <limits.h>

/* ── helper: null-terminate a (ptr, len) pair ────────────────────────── */
static char* make_cstr(const char* s, size_t len) {
    char* buf = (char*)malloc(len + 1);
    if (!buf) return NULL;
    memcpy(buf, s, len);
    buf[len] = '\0';
    return buf;
}

/* ── readFile ────────────────────────────────────────────────────────── */
lucis_fs_str_result lucis_readFile(const char* path, size_t path_len) {
    lucis_fs_str_result res = {NULL, 0};
    char* cpath = make_cstr(path, path_len);
    if (!cpath) return res;

    FILE* f = fopen(cpath, "rb");
    free(cpath);
    if (!f) return res;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz < 0) { fclose(f); return res; }

    char* buf = (char*)lucis_allocString((size_t)sz);
    if (!buf) { fclose(f); return res; }

    size_t nread = fread(buf, 1, (size_t)sz, f);
    fclose(f);

    res.ptr = buf;
    res.len = nread;
    return res;
}

/* ── writeFile ───────────────────────────────────────────────────────── */
void lucis_writeFile(const char* path, size_t path_len,
                      const char* data, size_t data_len) {
    char* cpath = make_cstr(path, path_len);
    if (!cpath) return;

    FILE* f = fopen(cpath, "wb");
    free(cpath);
    if (!f) return;

    fwrite(data, 1, data_len, f);
    fclose(f);
}

/* ── appendFile ──────────────────────────────────────────────────────── */
void lucis_appendFile(const char* path, size_t path_len,
                       const char* data, size_t data_len) {
    char* cpath = make_cstr(path, path_len);
    if (!cpath) return;

    FILE* f = fopen(cpath, "ab");
    free(cpath);
    if (!f) return;

    fwrite(data, 1, data_len, f);
    fclose(f);
}

/* ── exists ──────────────────────────────────────────────────────────── */
int32_t lucis_exists(const char* path, size_t path_len) {
    char* cpath = make_cstr(path, path_len);
    if (!cpath) return 0;
    struct stat st;
    int r = stat(cpath, &st);
    free(cpath);
    return r == 0 ? 1 : 0;
}

/* ── isFile ──────────────────────────────────────────────────────────── */
int32_t lucis_isFile(const char* path, size_t path_len) {
    char* cpath = make_cstr(path, path_len);
    if (!cpath) return 0;
    struct stat st;
    int r = stat(cpath, &st);
    free(cpath);
    return (r == 0 && S_ISREG(st.st_mode)) ? 1 : 0;
}

/* ── isDir ───────────────────────────────────────────────────────────── */
int32_t lucis_isDir(const char* path, size_t path_len) {
    char* cpath = make_cstr(path, path_len);
    if (!cpath) return 0;
    struct stat st;
    int r = stat(cpath, &st);
    free(cpath);
    return (r == 0 && S_ISDIR(st.st_mode)) ? 1 : 0;
}

/* ── fileSize ────────────────────────────────────────────────────────── */
int64_t lucis_fileSize(const char* path, size_t path_len) {
    char* cpath = make_cstr(path, path_len);
    if (!cpath) return -1;
    struct stat st;
    int r = stat(cpath, &st);
    free(cpath);
    return (r == 0) ? (int64_t)st.st_size : -1;
}

/* ── remove ──────────────────────────────────────────────────────────── */
int32_t lucis_remove(const char* path, size_t path_len) {
    char* cpath = make_cstr(path, path_len);
    if (!cpath) return 0;
    int r = unlink(cpath);
    free(cpath);
    return r == 0 ? 1 : 0;
}

/* ── removeDir ───────────────────────────────────────────────────────── */
int32_t lucis_removeDir(const char* path, size_t path_len) {
    char* cpath = make_cstr(path, path_len);
    if (!cpath) return 0;
    int r = rmdir(cpath);
    free(cpath);
    return r == 0 ? 1 : 0;
}

/* ── rename (C symbol: lucis_fsRename to avoid clash with C rename) ─ */
int32_t lucis_fsRename(const char* from, size_t from_len,
                        const char* to,   size_t to_len) {
    char* cfrom = make_cstr(from, from_len);
    char* cto   = make_cstr(to,   to_len);
    if (!cfrom || !cto) { free(cfrom); free(cto); return 0; }
    int r = rename(cfrom, cto);
    free(cfrom);
    free(cto);
    return r == 0 ? 1 : 0;
}

/* ── mkdir ───────────────────────────────────────────────────────────── */
int32_t lucis_mkdir(const char* path, size_t path_len) {
    char* cpath = make_cstr(path, path_len);
    if (!cpath) return 0;
    int r = mkdir(cpath, 0755);
    free(cpath);
    return r == 0 ? 1 : 0;
}

/* ── mkdirAll (recursive mkdir -p) ───────────────────────────────────── */
int32_t lucis_mkdirAll(const char* path, size_t path_len) {
    char* cpath = make_cstr(path, path_len);
    if (!cpath) return 0;

    char* p = cpath;
    /* skip leading '/' */
    if (*p == '/') p++;

    for (; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(cpath, 0755) != 0 && errno != EEXIST) {
                free(cpath);
                return 0;
            }
            *p = '/';
        }
    }
    int r = mkdir(cpath, 0755);
    free(cpath);
    return (r == 0 || errno == EEXIST) ? 1 : 0;
}

/* ── cwd ─────────────────────────────────────────────────────────────── */
lucis_fs_str_result lucis_cwd(void) {
    lucis_fs_str_result res = {NULL, 0};
    char buf[PATH_MAX];
    if (getcwd(buf, sizeof(buf))) {
        size_t len = strlen(buf);
        char* out = (char*)lucis_allocString(len);
        if (out) {
            memcpy(out, buf, len);
            res.ptr = out;
            res.len = len;
        }
    }
    return res;
}

/* ── setCwd ──────────────────────────────────────────────────────────── */
int32_t lucis_setCwd(const char* path, size_t path_len) {
    char* cpath = make_cstr(path, path_len);
    if (!cpath) return 0;
    int r = chdir(cpath);
    free(cpath);
    return r == 0 ? 1 : 0;
}

/* ── tempDir ─────────────────────────────────────────────────────────── */
lucis_fs_str_result lucis_tempDir(void) {
    const char* tmp = getenv("TMPDIR");
    if (!tmp) tmp = "/tmp";
    size_t len = strlen(tmp);
    char* out = (char*)lucis_allocString(len);
    lucis_fs_str_result res;
    if (out) {
        memcpy(out, tmp, len);
        res.ptr = out;
        res.len = len;
    } else {
        res.ptr = NULL;
        res.len = 0;
    }
    return res;
}

/* ── Vec-returning functions ─────────────────────────────────────────── */

void lucis_listDir(lucis_fs_vec_header* out, const char* path, size_t path_len) {
    typedef struct { const char* ptr; size_t len; } str_elem;

    out->ptr = NULL;
    out->len = 0;
    out->cap = 0;

    char tmp[4096];
    size_t cpLen = path_len < sizeof(tmp) - 1 ? path_len : sizeof(tmp) - 1;
    memcpy(tmp, path, cpLen);
    tmp[cpLen] = '\0';

    DIR* dir = opendir(tmp);
    if (!dir) return;

    size_t cap = 16;
    str_elem* arr = (str_elem*)malloc(cap * sizeof(str_elem));
    size_t count = 0;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.' &&
            (entry->d_name[1] == '\0' ||
             (entry->d_name[1] == '.' && entry->d_name[2] == '\0')))
            continue;

        size_t nLen = strlen(entry->d_name);
        char* copy = (char*)lucis_allocString(nLen + 1);
        if (!copy) continue;
        memcpy(copy, entry->d_name, nLen);
        copy[nLen] = '\0';

        if (count == cap) { cap *= 2; arr = (str_elem*)realloc(arr, cap * sizeof(str_elem)); }
        arr[count].ptr = copy;
        arr[count].len = nLen;
        count++;
    }
    closedir(dir);

    out->ptr = arr;
    out->len = count;
    out->cap = cap;
}

void lucis_readFileBytes(lucis_fs_vec_header* out, const char* path, size_t path_len) {
    out->ptr = NULL;
    out->len = 0;
    out->cap = 0;

    char tmp[4096];
    size_t cpLen = path_len < sizeof(tmp) - 1 ? path_len : sizeof(tmp) - 1;
    memcpy(tmp, path, cpLen);
    tmp[cpLen] = '\0';

    FILE* f = fopen(tmp, "rb");
    if (!f) return;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz < 0) { fclose(f); return; }

    uint8_t* buf = (uint8_t*)malloc((size_t)sz);
    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);

    out->ptr = buf;
    out->len = got;
    out->cap = (size_t)sz;
}

void lucis_writeFileBytes(const char* path, size_t path_len,
                           const lucis_fs_vec_header* data) {
    char tmp[4096];
    size_t cpLen = path_len < sizeof(tmp) - 1 ? path_len : sizeof(tmp) - 1;
    memcpy(tmp, path, cpLen);
    tmp[cpLen] = '\0';

    FILE* f = fopen(tmp, "wb");
    if (!f) return;

    if (data->ptr && data->len > 0) {
        fwrite(data->ptr, 1, data->len, f);
    }
    fclose(f);
}
