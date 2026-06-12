#ifndef LUCIS_OS_H
#define LUCIS_OS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char* ptr;
    size_t      len;
} lucis_os_str_result;

/* Process info */
int32_t  lucis_osGetpid(void);
int32_t  lucis_osGetppid(void);
uint32_t lucis_osGetuid(void);
uint32_t lucis_osGetgid(void);

/* System info */
lucis_os_str_result lucis_osHostname(void);
size_t   lucis_osPageSize(void);

/* Error handling */
int32_t  lucis_osErrno(void);
lucis_os_str_result lucis_osStrerror(int32_t code);

/* Signals */
int32_t  lucis_osKill(int32_t pid, int32_t sig);

/* File descriptors */
int32_t  lucis_osDup(int32_t fd);
int32_t  lucis_osDup2(int32_t oldfd, int32_t newfd);
int32_t  lucis_osCloseFd(int32_t fd);

#ifdef __cplusplus
}
#endif

#endif /* LUCIS_OS_H */
