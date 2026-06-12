#include "thread.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ── Task implementation ───────────────────────────────────────────────── */

struct lucis_Task {
    pthread_t thread;
    void*     result;
    int       completed;
};

lucis_Task* lucis_taskCreate(void* (*fn)(void*), void* arg) {
    lucis_Task* t = (lucis_Task*)calloc(1, sizeof(lucis_Task));
    pthread_create(&t->thread, NULL, fn, arg);
    return t;
}

void* lucis_taskAwait(lucis_Task* task) {
    if (!task) return NULL;
    if (!task->completed) {
        void* retval;
        pthread_join(task->thread, &retval);
        task->result    = retval;
        task->completed = 1;
    }
    return task->result;
}

void lucis_taskFree(lucis_Task* task) {
    if (task) free(task);
}

/* ── Mutex implementation ──────────────────────────────────────────────── */

struct lucis_Mutex {
    pthread_mutex_t mtx;
};

lucis_Mutex* lucis_mutexCreate(void) {
    lucis_Mutex* m = (lucis_Mutex*)malloc(sizeof(lucis_Mutex));
    pthread_mutex_init(&m->mtx, NULL);
    return m;
}

void lucis_mutexLock(lucis_Mutex* m) {
    if (m) pthread_mutex_lock(&m->mtx);
}

void lucis_mutexUnlock(lucis_Mutex* m) {
    if (m) pthread_mutex_unlock(&m->mtx);
}

int32_t lucis_mutexTryLock(lucis_Mutex* m) {
    if (!m) return 0;
    return pthread_mutex_trylock(&m->mtx) == 0 ? 1 : 0;
}

void lucis_mutexFree(lucis_Mutex* m) {
    if (m) {
        pthread_mutex_destroy(&m->mtx);
        free(m);
    }
}

/* ── Utilities ─────────────────────────────────────────────────────────── */

uint32_t lucis_cpuCount(void) {
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (uint32_t)n : 1;
}

uint64_t lucis_threadId(void) {
    return (uint64_t)pthread_self();
}

void lucis_yield(void) {
    sched_yield();
}
