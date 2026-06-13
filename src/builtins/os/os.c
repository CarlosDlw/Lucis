#include "os.h"
#include "../string/string.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>

/* ── Process info ───────────────────────────────────────────────── */

int32_t lucis_osGetpid(void) {
    return (int32_t)getpid();
}

int32_t lucis_osGetppid(void) {
    return (int32_t)getppid();
}

uint32_t lucis_osGetuid(void) {
    return (uint32_t)getuid();
}

uint32_t lucis_osGetgid(void) {
    return (uint32_t)getgid();
}

/* ── System info ────────────────────────────────────────────────── */

lucis_os_str_result lucis_osHostname(void) {
    char buf[256];
    if (gethostname(buf, sizeof(buf)) != 0) {
        char* empty = (char*)lucis_allocString(1);
        if (!empty) return (lucis_os_str_result){ "", 0 };
        empty[0] = '\0';
        return (lucis_os_str_result){ empty, 0 };
    }
    buf[sizeof(buf) - 1] = '\0';
    size_t len = strlen(buf);
    char* out = (char*)lucis_allocString(len + 1);
    if (!out) return (lucis_os_str_result){ "", 0 };
    memcpy(out, buf, len + 1);
    return (lucis_os_str_result){ out, len };
}

size_t lucis_osPageSize(void) {
    return (size_t)sysconf(_SC_PAGESIZE);
}

/* ── Error handling ─────────────────────────────────────────────── */

int32_t lucis_osErrno(void) {
    return (int32_t)errno;
}

lucis_os_str_result lucis_osStrerror(int32_t code) {
    const char* msg = strerror(code);
    size_t len = strlen(msg);
    char* out = (char*)lucis_allocString(len + 1);
    if (!out) return (lucis_os_str_result){ "", 0 };
    memcpy(out, msg, len + 1);
    return (lucis_os_str_result){ out, len };
}

/* ── Signals ────────────────────────────────────────────────────── */

int32_t lucis_osKill(int32_t pid, int32_t sig) {
    return (int32_t)kill((pid_t)pid, (int)sig);
}

/* ── File descriptors ───────────────────────────────────────────── */

int32_t lucis_osDup(int32_t fd) {
    return (int32_t)dup(fd);
}

int32_t lucis_osDup2(int32_t oldfd, int32_t newfd) {
    return (int32_t)dup2(oldfd, newfd);
}

int32_t lucis_osCloseFd(int32_t fd) {
    return (int32_t)close(fd);
}
