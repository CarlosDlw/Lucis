#ifndef LUCIS_PROCESS_H
#define LUCIS_PROCESS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char* ptr;
    size_t      len;
} lucis_proc_str_result;

/* Process control */
void lucis_abort(void);

/* Environment */
lucis_proc_str_result lucis_env(const char* name, size_t name_len);
void lucis_setEnv(const char* name, size_t name_len,
                   const char* value, size_t value_len);
int32_t lucis_hasEnv(const char* name, size_t name_len);

/* Execution */
int32_t lucis_exec(const char* cmd, size_t cmd_len);
lucis_proc_str_result lucis_execOutput(const char* cmd, size_t cmd_len);

/* Info */
int32_t lucis_pid(void);
lucis_proc_str_result lucis_platform(void);
lucis_proc_str_result lucis_arch(void);
lucis_proc_str_result lucis_homeDir(void);
lucis_proc_str_result lucis_executablePath(void);

#ifdef __cplusplus
}
#endif

#endif /* LUCIS_PROCESS_H */
