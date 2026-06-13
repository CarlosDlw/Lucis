#define _POSIX_C_SOURCE 200809L

#include "process.h"
#include "../string/string.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/utsname.h>

/* ── helper: null-terminate a (ptr, len) pair ────────────────────────── */
static char* make_cstr(const char* s, size_t len) {
    char* buf = (char*)malloc(len + 1);
    if (!buf) return NULL;
    memcpy(buf, s, len);
    buf[len] = '\0';
    return buf;
}

static lucis_proc_str_result make_result(const char* s) {
    lucis_proc_str_result res;
    if (s) {
        size_t len = strlen(s);
        char* out = (char*)lucis_allocString(len);
        if (out) {
            memcpy(out, s, len);
            res.ptr = out;
            res.len = len;
        } else {
            res.ptr = NULL;
            res.len = 0;
        }
    } else {
        res.ptr = NULL;
        res.len = 0;
    }
    return res;
}

/* ── abort ───────────────────────────────────────────────────────────── */
void lucis_abort(void) {
    abort();
}

/* ── env ─────────────────────────────────────────────────────────────── */
lucis_proc_str_result lucis_env(const char* name, size_t name_len) {
    char* cname = make_cstr(name, name_len);
    if (!cname) {
        lucis_proc_str_result r = {NULL, 0};
        return r;
    }
    const char* val = getenv(cname);
    free(cname);
    return make_result(val);
}

/* ── setEnv ──────────────────────────────────────────────────────────── */
void lucis_setEnv(const char* name, size_t name_len,
                   const char* value, size_t value_len) {
    char* cname  = make_cstr(name,  name_len);
    char* cvalue = make_cstr(value, value_len);
    if (cname && cvalue)
        setenv(cname, cvalue, 1);
    free(cname);
    free(cvalue);
}

/* ── hasEnv ──────────────────────────────────────────────────────────── */
int32_t lucis_hasEnv(const char* name, size_t name_len) {
    char* cname = make_cstr(name, name_len);
    if (!cname) return 0;
    const char* val = getenv(cname);
    free(cname);
    return val != NULL ? 1 : 0;
}

/* ── exec ────────────────────────────────────────────────────────────── */
int32_t lucis_exec(const char* cmd, size_t cmd_len) {
    char* ccmd = make_cstr(cmd, cmd_len);
    if (!ccmd) return -1;
    int status = system(ccmd);
    free(ccmd);
    if (status == -1) return -1;
    /* WEXITSTATUS extracts the exit code */
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

/* ── execOutput ──────────────────────────────────────────────────────── */
lucis_proc_str_result lucis_execOutput(const char* cmd, size_t cmd_len) {
    lucis_proc_str_result res = {NULL, 0};
    char* ccmd = make_cstr(cmd, cmd_len);
    if (!ccmd) return res;

    FILE* fp = popen(ccmd, "r");
    free(ccmd);
    if (!fp) return res;

    size_t cap = 4096;
    size_t len = 0;
    char* buf = (char*)malloc(cap);
    if (!buf) { pclose(fp); return res; }

    size_t nread;
    while ((nread = fread(buf + len, 1, cap - len, fp)) > 0) {
        len += nread;
        if (len == cap) {
            cap *= 2;
            char* tmp = (char*)realloc(buf, cap);
            if (!tmp) { free(buf); pclose(fp); return res; }
            buf = tmp;
        }
    }
    pclose(fp);

    // Copy into tracked memory
    char* tracked = (char*)lucis_allocString(len);
    if (tracked) {
        memcpy(tracked, buf, len);
        free(buf);
        res.ptr = tracked;
    } else {
        res.ptr = buf;
    }
    res.len = len;
    return res;
}

/* ── pid ─────────────────────────────────────────────────────────────── */
int32_t lucis_pid(void) {
    return (int32_t)getpid();
}

/* ── platform ────────────────────────────────────────────────────────── */
lucis_proc_str_result lucis_platform(void) {
#if defined(__linux__)
    return make_result("linux");
#elif defined(__APPLE__)
    return make_result("macos");
#elif defined(_WIN32)
    return make_result("windows");
#else
    return make_result("unknown");
#endif
}

/* ── arch ────────────────────────────────────────────────────────────── */
lucis_proc_str_result lucis_arch(void) {
#if defined(__x86_64__) || defined(_M_X64)
    return make_result("x86_64");
#elif defined(__aarch64__) || defined(_M_ARM64)
    return make_result("aarch64");
#elif defined(__i386__) || defined(_M_IX86)
    return make_result("x86");
#elif defined(__arm__)
    return make_result("arm");
#elif defined(__riscv)
    return make_result("riscv");
#else
    return make_result("unknown");
#endif
}

/* ── homeDir ─────────────────────────────────────────────────────────── */
lucis_proc_str_result lucis_homeDir(void) {
    const char* home = getenv("HOME");
    return make_result(home);
}

/* ── executablePath ──────────────────────────────────────────────────── */
lucis_proc_str_result lucis_executablePath(void) {
    lucis_proc_str_result res = {NULL, 0};
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0) {
        char* out = (char*)lucis_allocString((size_t)len);
        if (out) {
            memcpy(out, buf, (size_t)len);
            res.ptr = out;
            res.len = (size_t)len;
        }
    }
    return res;
}
