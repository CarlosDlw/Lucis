#ifndef LUCIS_THREAD_H
#define LUCIS_THREAD_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Task ──────────────────────────────────────────────────────────────── */
typedef struct lucis_Task lucis_Task;

/* Create a task that runs `fn(arg)` on a new thread.
   `fn` must return a heap-allocated result pointer (or NULL for void). */
lucis_Task* lucis_taskCreate(void* (*fn)(void*), void* arg);

/* Block until the task finishes, return the result pointer.
   The caller owns the returned pointer and must free() it. */
void* lucis_taskAwait(lucis_Task* task);

/* Free the task handle (call after await). */
void lucis_taskFree(lucis_Task* task);

/* ── Mutex ─────────────────────────────────────────────────────────────── */
typedef struct lucis_Mutex lucis_Mutex;

lucis_Mutex* lucis_mutexCreate(void);
void          lucis_mutexLock(lucis_Mutex* m);
void          lucis_mutexUnlock(lucis_Mutex* m);
int32_t       lucis_mutexTryLock(lucis_Mutex* m);
void          lucis_mutexFree(lucis_Mutex* m);

/* ── Utilities ─────────────────────────────────────────────────────────── */
uint32_t lucis_cpuCount(void);
uint64_t lucis_threadId(void);
void     lucis_yield(void);

#ifdef __cplusplus
}
#endif

#endif /* LUCIS_THREAD_H */
