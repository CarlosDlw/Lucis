#include "fuzz_runner.h"

#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

/* ── Configuration ─────────────────────────────────────────────────── */

#define MAP_BITS    (1u << 16)      /* coverage bitmap size (bytes) */
#define FUZZ_MAX_INPUT (1u << 16)    /* max input length */
#define MAX_QUEUE   8192            /* max interesting inputs kept */
#define MAX_MUT_PER_BASE 256        /* mutations tried per queue item */
#define DEFAULT_TIMEOUT_MS 1000

/* ── State ────────────────────────────────────────────────────────── */

static uint8_t* g_bitmap = NULL;             /* shared coverage map */
static uint8_t  g_total[MAP_BITS];           /* accumulated coverage */

static uint64_t g_rng = 0x9E3779B97F4A7C15ull;

/* Input queue (corpus + interesting discoveries). */
static uint8_t** g_queue = NULL;
static size_t*   g_queueLen = NULL;
static size_t    g_queueCount = 0;
static unsigned char* g_detDone = NULL;   /* deterministic stage per seed */

/* Scratch buffers. */
static uint8_t  g_input[FUZZ_MAX_INPUT];
static size_t   g_inputLen = 0;

/* Options. */
static const char* g_corpusDir = "fuzz/corpus";
static const char* g_crashDir  = "fuzz/crashes";
static unsigned    g_maxIters   = 0;      /* 0 = until interrupted */
static unsigned    g_timeoutMs  = DEFAULT_TIMEOUT_MS;
static unsigned    g_workerId   = 0;
static int         g_verbose    = 0;

/* Stats. */
static uint64_t g_runs = 0;
static unsigned g_crashes = 0;
static unsigned g_timeouts = 0;
static unsigned g_interesting = 0;
static int      g_stop = 0;

/* ── RNG (xorshift64*) ────────────────────────────────────────────── */

static uint64_t rngNext(void) {
    uint64_t x = g_rng;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    g_rng = x;
    return x * 0x2545F4914F6CDD1Dull;
}

static size_t rngBelow(size_t n) {
    return n ? (size_t)(rngNext() % n) : 0;
}

/* ── Coverage hook ────────────────────────────────────────────────── */

void lucis_fuzz_trace(int32_t id) {
    if (g_bitmap)
        g_bitmap[(uint32_t)id & (MAP_BITS - 1)] = 1;
}

/* Weak stub: overridden by the strong definition synthesized by IRGen in
 * fuzz mode. Keeps every binary linkable; never called outside `lucis fuzz`.
 * (The definition lives in the IRGen-generated module only when a #[fuzz]
 * target is present.) */
__attribute__((weak))
void lucis_fuzz_dispatch(const uint8_t* data, size_t len) {
    (void)data;
    (void)len;
    fprintf(stderr, "[fuzz] internal error: dispatch called without a #[fuzz] target\n");
    abort();
}

/* ── Queue ────────────────────────────────────────────────────────── */

static void queuePush(const uint8_t* data, size_t len) {
    if (g_queueCount >= MAX_QUEUE) return;
    uint8_t* copy = (uint8_t*)malloc(len ? len : 1);
    if (!copy) return;
    if (len) memcpy(copy, data, len);
    g_queue[g_queueCount] = copy;
    g_queueLen[g_queueCount] = len;
    g_queueCount += 1;
}

/* ── Mutation (AFL-style havoc) ───────────────────────────────────── */
/* In-place on g_input; returns the new length. */

static size_t mutateBitFlip(uint8_t* buf, size_t len) {
    if (len == 0) return 0;
    buf[rngBelow(len)] ^= (uint8_t)(1u << rngBelow(8));
    return len;
}

static size_t mutateInteresting(uint8_t* buf, size_t len) {
    static const uint8_t interesting8[] = {0x00, 0x01, 0x7F, 0x80, 0xFF};
    static const uint16_t interesting16[] = {0x0000, 0x0001, 0x7FFF, 0x8000, 0xFFFF};
    if (len == 0) return 0;
    size_t pos = rngBelow(len);
    switch (rngBelow(2)) {
        case 0:
            buf[pos] = interesting8[rngBelow(sizeof(interesting8))];
            break;
        case 1:
            if (pos + 1 < len) {
                uint16_t v = interesting16[rngBelow(sizeof(interesting16) / sizeof(uint16_t))];
                buf[pos] = (uint8_t)(v & 0xFF);
                buf[pos + 1] = (uint8_t)(v >> 8);
            } else {
                buf[pos] = interesting8[rngBelow(sizeof(interesting8))];
            }
            break;
    }
    return len;
}

static size_t mutateArith(uint8_t* buf, size_t len) {
    if (len == 0) return 0;
    size_t pos = rngBelow(len);
    int delta = (int)rngBelow(71) - 35;   /* -35..+35 */
    if (delta == 0) delta = 1;
    buf[pos] = (uint8_t)(buf[pos] + delta);
    return len;
}

static size_t mutateInsert(uint8_t* buf, size_t len) {
    if (len >= FUZZ_MAX_INPUT) return len;
    size_t pos = rngBelow(len + 1);
    uint8_t v = (uint8_t)rngNext();
    memmove(buf + pos + 1, buf + pos, len - pos);
    buf[pos] = v;
    return len + 1;
}

static size_t mutateDelete(uint8_t* buf, size_t len) {
    if (len == 0) return 0;
    size_t pos = rngBelow(len);
    size_t span = 1 + rngBelow(len - pos);
    memmove(buf + pos, buf + pos + span, len - pos - span);
    return len - span;
}

static size_t mutateDuplicate(uint8_t* buf, size_t len) {
    if (len == 0 || len >= FUZZ_MAX_INPUT) return len;
    size_t pos = rngBelow(len);
    size_t span = 1 + rngBelow(len - pos);
    if (len + span > FUZZ_MAX_INPUT) span = FUZZ_MAX_INPUT - len;
    memmove(buf + pos + span, buf + pos, len - pos);
    memcpy(buf + pos, buf + pos, span);   /* duplicate the chunk */
    return len + span;
}

static size_t mutateSplice(uint8_t* buf, size_t len, const uint8_t* other, size_t otherLen) {
    if (otherLen == 0 || len == 0) return len;
    size_t cut = 1 + rngBelow(len - 1);
    size_t take = 1 + rngBelow(otherLen < FUZZ_MAX_INPUT ? otherLen : FUZZ_MAX_INPUT - 1);
    if (cut + take > FUZZ_MAX_INPUT) take = FUZZ_MAX_INPUT - cut;
    if (take == 0) return len;
    size_t from = rngBelow(otherLen - take + 1);
    memcpy(buf + cut, other + from, take);
    return cut + take;
}

/* Pick a random mutation and apply it to `len` bytes in g_input. */
static size_t mutateOnceStep(size_t len) {
    switch (rngBelow(6)) {
        case 0: len = mutateBitFlip(g_input, len); break;
        case 1: len = mutateInteresting(g_input, len); break;
        case 2: len = mutateArith(g_input, len); break;
        case 3: len = mutateInsert(g_input, len); break;
        case 4: len = mutateDelete(g_input, len); break;
        case 5:
            if (g_queueCount > 1) {
                size_t other = rngBelow(g_queueCount);
                len = mutateSplice(g_input, len, g_queue[other], g_queueLen[other]);
            } else {
                len = mutateInsert(g_input, len);
            }
            break;
    }
    return len;
}

/* Apply a havoc stack: 1..16 chained mutations so paths that need longer
 * inputs are reachable from short seeds (AFL model). */
static size_t mutateHavoc(void) {
    size_t len = g_inputLen;
    unsigned steps = 1 + (unsigned)(rngNext() & 15);
    for (unsigned h = 0; h < steps; ++h)
        len = mutateOnceStep(len);
    return len;
}

/* ── Execution ────────────────────────────────────────────────────── */

typedef struct {
    int finished;       /* 1 if the child exited normally */
    int exitCode;
    int termSig;        /* signal number when killed */
    uint8_t* output;    /* captured child stdout+stderr, malloc'd */
    size_t outputLen;
    int newCoverage;    /* true when novel coverage bits appeared */
} ExecResult;

static void runOne(ExecResult* out, unsigned timeoutMs) {
    memset(out, 0, sizeof(*out));

    int fds[2];
    if (pipe(fds) != 0) return;

    memset(g_bitmap, 0, MAP_BITS);   /* fresh coverage per execution */

    pid_t pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        return;
    }

    if (pid == 0) {
        close(fds[0]);
        dup2(fds[1], STDOUT_FILENO);
        dup2(fds[1], STDERR_FILENO);
        if (fds[1] > STDERR_FILENO) close(fds[1]);

        if (timeoutMs > 0) {
            struct itimerval timer;
            memset(&timer, 0, sizeof(timer));
            timer.it_value.tv_sec  = timeoutMs / 1000;
            timer.it_value.tv_usec = (timeoutMs % 1000) * 1000;
            setitimer(ITIMER_REAL, &timer, NULL);
        }

        lucis_fuzz_dispatch(g_input, g_inputLen);
        fflush(NULL);
        _exit(0);
    }

    /* Parent. */
    close(fds[1]);

    int status = 0;
    pid_t r;
    do {
        r = waitpid(pid, &status, 0);
    } while (r < 0 && errno == EINTR);

    /* Drain the capture pipe (EOF is guaranteed once the child is dead). */
    char buf[4096];
    ssize_t n;
    while ((n = read(fds[0], buf, sizeof(buf))) > 0) {
        out->output = (uint8_t*)realloc(out->output, out->outputLen + (size_t)n + 1);
        if (!out->output) break;
        memcpy(out->output + out->outputLen, buf, (size_t)n);
        out->outputLen += (size_t)n;
        out->output[out->outputLen] = '\0';
    }
    close(fds[0]);

    if (WIFEXITED(status)) {
        out->finished = 1;
        out->exitCode = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        out->termSig = WTERMSIG(status);
    }

    /* Coverage: any bit set by the child that was not seen before? */
    for (size_t i = 0; i < MAP_BITS; ++i) {
        uint8_t fresh = g_bitmap[i] & ~g_total[i];
        if (fresh) {
            out->newCoverage = 1;
            g_total[i] |= g_bitmap[i];
        }
    }
}

static const char* signalName(int sig) {
    switch (sig) {
        case SIGSEGV: return "SIGSEGV";
        case SIGABRT: return "SIGABRT";
        case SIGILL:  return "SIGILL";
        case SIGFPE:  return "SIGFPE";
        case SIGBUS:  return "SIGBUS";
        case SIGALRM: return "timeout";
        default:      return "signal";
    }
}

static size_t countCoverage(void) {
    size_t cov = 0;
    for (size_t i = 0; i < MAP_BITS; ++i)
        if (g_total[i]) cov += 1;
    return cov;
}

/* mkdir -p: create every component of a path. */
static int mkdirs(const char* path) {
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);
    if (len == 0) return -1;
    for (size_t i = 1; i < len; ++i) {
        if (tmp[i] == '/') {
            tmp[i] = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return -1;
            tmp[i] = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return -1;
    return 0;
}

/* ── Crash saving + minimization ──────────────────────────────────── */

static void saveCrash(const char* what, const uint8_t* data, size_t len,
                      const ExecResult* out, const char* detail) {
    char dir[512];
    snprintf(dir, sizeof(dir), "%s/crash_%u_%u", g_crashDir, g_workerId, g_crashes);
    if (mkdirs(dir) != 0) {
        fprintf(stderr, "[fuzz] cannot create crash dir '%s'\n", dir);
        return;
    }

    char file[640];
    snprintf(file, sizeof(file), "%s/input.bin", dir);
    FILE* f = fopen(file, "wb");
    if (f) {
        fwrite(data, 1, len, f);
        fclose(f);
    }
    snprintf(file, sizeof(file), "%s/log.txt", dir);
    f = fopen(file, "w");
    if (f) {
        fprintf(f, "kind: %s\n", what);
        fprintf(f, "detail: %s\n", detail);
        fprintf(f, "input length: %zu\n", len);
        fprintf(f, "runs at find: %llu\n", (unsigned long long)g_runs);
        if (out->outputLen)
            fwrite(out->output, 1, out->outputLen, f);
        fclose(f);
    }
    fprintf(stderr, "\n[fuzz] crash found: %s (%s) — %zu bytes — saved in %s\n",
            what, detail, len, dir);
}

/* Re-run a candidate input; returns 1 when it reproduces the same crash. */
static int reproduces(const uint8_t* data, size_t len, int wantSig) {
    size_t savedLen = g_inputLen;
    memcpy(g_input, data, len);
    g_inputLen = len;

    ExecResult out;
    runOne(&out, g_timeoutMs);
    free(out.output);

    int ok = !out.finished && out.termSig == wantSig;
    g_inputLen = savedLen;
    return ok;
}

/* Simple delta-debugging: drop chunks while the crash still reproduces. */
static void minimize(const uint8_t* data, size_t len, int sig,
                     uint8_t* out, size_t* outLen, size_t* reductions) {
    uint8_t* buf = (uint8_t*)malloc(len ? len : 1);
    if (!buf) { *outLen = 0; return; }
    memcpy(buf, data, len);
    size_t cur = len;

    size_t step = cur / 2;
    while (step >= 1) {
        int changed = 1;
        while (changed) {
            changed = 0;
            size_t pos = 0;   /* restart at the head on every reduction pass */
            while (pos < cur) {
                size_t end = pos + step;
                if (end > cur) end = cur;
                size_t block = end - pos;
                if (block == 0) { pos = end; continue; }

                /* Try removing [pos, end). */
                size_t newLen = cur - block;
                if (newLen == 0) { pos = end; continue; }
                uint8_t* tmp = (uint8_t*)malloc(newLen ? newLen : 1);
                if (!tmp) { pos = end; continue; }
                if (pos > 0) memcpy(tmp, buf, pos);
                if (pos < cur) memcpy(tmp + pos, buf + end, cur - end);

                if (reproduces(tmp, newLen, sig)) {
                    memcpy(buf, tmp, newLen);
                    cur = newLen;
                    *reductions += 1;
                    changed = 1;
                } else {
                    pos = end;
                }
                free(tmp);
            }
        }
        step /= 2;
    }

    memcpy(out, buf, cur);
    *outLen = cur;
    free(buf);
}

/* ── Runner ───────────────────────────────────────────────────────── */

static void printStats(void) {
    fprintf(stderr, "\r[fuzz] runs %llu | cov %zu | queue %zu | crashes %u | timeouts %u  ",
            (unsigned long long)g_runs, countCoverage(), g_queueCount,
            g_crashes, g_timeouts);
    fflush(stderr);
}

static void handleStop(int) { g_stop = 1; }

/* Handle one execution outcome: crashes are saved + minimized, novel
 * coverage inputs join the queue and persist back into the corpus. */
static void onExecDone(const uint8_t* input, size_t len, const ExecResult* out) {
    if (out->termSig) {
        if (out->termSig == SIGALRM) {
            g_timeouts += 1;
            return;
        }
        char detail[64];
        snprintf(detail, sizeof(detail), "%s", signalName(out->termSig));
        saveCrash("crash", input, len, out,
                  out->outputLen ? (const char*)out->output : detail);

        /* Minimize and save the minimal reproducer. */
        uint8_t* minimal = (uint8_t*)malloc(len ? len : 1);
        size_t minLen = 0, reductions = 0;
        if (minimal) {
            minimize(input, len, out->termSig, minimal, &minLen, &reductions);
            if (minLen) {
                char dir[512], file[640];
                snprintf(dir, sizeof(dir), "%s/crash_%u_%u", g_crashDir,
                         g_workerId, g_crashes);
                snprintf(file, sizeof(file), "%s/minimal.bin", dir);
                FILE* f = fopen(file, "wb");
                if (f) { fwrite(minimal, 1, minLen, f); fclose(f); }
                fprintf(stderr, "[fuzz] minimized %zu -> %zu bytes (%zu reductions)\n",
                        len, minLen, reductions);
            }
            free(minimal);
        }
        g_crashes += 1;
        if (g_crashes >= 10) g_stop = 1;   /* stop after a handful */
        return;
    }

    if (out->newCoverage && out->finished) {
        queuePush(input, len);
        g_interesting += 1;
        /* Persist new seeds into the corpus directory so later runs start
         * from a richer base (libFuzzer behaviour). */
        char file[512];
        snprintf(file, sizeof(file), "%s/id_%llu", g_corpusDir,
                 (unsigned long long)g_runs);
        FILE* f = fopen(file, "wb");
        if (f) { fwrite(input, 1, len, f); fclose(f); }
    }
}

/* Run the target with g_input[0..len) and route the outcome. */
static void runOneAndHandle(size_t len) {
    ExecResult out;
    runOne(&out, g_timeoutMs);
    g_runs += 1;
    if (g_verbose && out.outputLen)
        fwrite(out.output, 1, out.outputLen, stderr);
    onExecDone(g_input, len, &out);
    free(out.output);
}

/* Deterministic stage (AFL model): perturb ONE byte of the base at a time
 * keeping the rest intact, so matched prefixes anchor progressively
 * (essential for multi-byte magic values and checks). */
static void deterministicStage(size_t base) {
    static const uint8_t kInteresting[] = {0x00, 0x01, 0x7F, 0x80, 0xFF};
    size_t len = g_queueLen[base];

    for (size_t pos = 0; pos < len; ++pos) {
        /* Single-bit flips. */
        for (unsigned bit = 0; bit < 8; ++bit) {
            memcpy(g_input, g_queue[base], len);
            g_input[pos] ^= (uint8_t)(1u << bit);
            runOneAndHandle(len);
        }
        /* Small arith deltas (both signs, full 8-bit reach: 1..35). */
        for (unsigned step = 1; step <= 35; ++step) {
            for (int sign = -1; sign <= 1; sign += 2) {
                memcpy(g_input, g_queue[base], len);
                g_input[pos] = (uint8_t)((int)g_input[pos] + (int)step * sign);
                runOneAndHandle(len);
            }
        }
        /* Interesting values. */
        for (size_t v = 0; v < sizeof(kInteresting); ++v) {
            memcpy(g_input, g_queue[base], len);
            g_input[pos] = kInteresting[v];
            runOneAndHandle(len);
        }
    }
    /* Shape growth so havoc can extend anchored prefixes. */
    if (len + 1 < FUZZ_MAX_INPUT) {
        static const uint8_t kGrowth[] = {0x00, 0xFF, 0x41, 0x52};
        for (size_t g = 0; g < sizeof(kGrowth); ++g) {
            memcpy(g_input, g_queue[base], len);
            g_input[len] = kGrowth[g];
            runOneAndHandle(len + 1);
        }
    }
}

static void usage(void) {
    fprintf(stderr,
        "usage: <fuzz binary> [--corpus DIR] [--iters N] [--timeout MS]\n"
        "                     [--seed N] [--crash-dir DIR] [--worker-id N]\n"
        "                     [--verbose] [--repro FILE]\n");
}

int32_t lucis_fuzz_main(int32_t argc, char** argv) {
    const char* reproFile = NULL;
    uint64_t seed = 0x12345678;

    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        const char* next = (i + 1 < argc) ? argv[i + 1] : NULL;
        if (!strcmp(a, "--corpus") && next) { g_corpusDir = next; ++i; }
        else if (!strcmp(a, "--iters") && next) { g_maxIters = (unsigned)strtoul(next, NULL, 10); ++i; }
        else if (!strcmp(a, "--timeout") && next) { g_timeoutMs = (unsigned)strtoul(next, NULL, 10); ++i; }
        else if (!strcmp(a, "--seed") && next) { seed = strtoull(next, NULL, 10); ++i; }
        else if (!strcmp(a, "--crash-dir") && next) { g_crashDir = next; ++i; }
        else if (!strcmp(a, "--worker-id") && next) { g_workerId = (unsigned)strtoul(next, NULL, 10); ++i; }
        else if (!strcmp(a, "--verbose")) { g_verbose = 1; }
        else if (!strcmp(a, "--repro") && next) { reproFile = next; ++i; }
        else { usage(); return 2; }
    }

    /* Shared, fork-inherited coverage map. */
    g_bitmap = (uint8_t*)mmap(NULL, MAP_BITS, PROT_READ | PROT_WRITE,
                              MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (g_bitmap == MAP_FAILED) {
        fprintf(stderr, "[fuzz] mmap failed: %s\n", strerror(errno));
        return 2;
    }

    /* ── Repro mode: run the target once with a saved input. ─────────── */
    if (reproFile) {
        FILE* f = fopen(reproFile, "rb");
        if (!f) {
            fprintf(stderr, "[fuzz] cannot open '%s'\n", reproFile);
            return 2;
        }
        size_t len = fread(g_input, 1, FUZZ_MAX_INPUT, f);
        fclose(f);
        g_inputLen = len;

        ExecResult out;
        runOne(&out, g_timeoutMs);
        if (out.finished) {
            fprintf(stderr, "[fuzz] repro: target completed (exit %d), no crash\n",
                    out.exitCode);
            free(out.output);
            return 0;
        }
        fprintf(stderr, "[fuzz] repro: crash reproduced — %s\n",
                signalName(out.termSig));
        if (out.outputLen) fwrite(out.output, 1, out.outputLen, stderr);
        free(out.output);
        return 1;
    }

    /* ── Seed queue from the corpus directory. ───────────────────────── */
    g_queue = (uint8_t**)calloc(MAX_QUEUE, sizeof(uint8_t*));
    g_queueLen = (size_t*)calloc(MAX_QUEUE, sizeof(size_t));
    g_detDone = (unsigned char*)calloc(MAX_QUEUE, 1);
    if (!g_queue || !g_queueLen || !g_detDone) {
        fprintf(stderr, "[fuzz] out of memory\n");
        return 2;
    }

    /* Corpus files: every regular file in <corpus>/, sorted for determinism. */
    {
        char path[1024];
        struct dirent** entries = NULL;
        int count = scandir(g_corpusDir, &entries, NULL, alphasort);
        if (count < 0) {
            fprintf(stderr, "[fuzz] warning: cannot read corpus dir '%s' (%s)\n",
                    g_corpusDir, strerror(errno));
        } else {
            for (int i = 0; i < count && g_queueCount < MAX_QUEUE; ++i) {
                if (entries[i]->d_name[0] == '.') continue;
                snprintf(path, sizeof(path), "%s/%s", g_corpusDir, entries[i]->d_name);
                free(entries[i]);
                FILE* f = fopen(path, "rb");
                if (!f) continue;
                uint8_t* buf = (uint8_t*)malloc(FUZZ_MAX_INPUT);
                if (!buf) { fclose(f); continue; }
                size_t len = fread(buf, 1, FUZZ_MAX_INPUT, f);
                fclose(f);
                g_queue[g_queueCount] = buf;
                g_queueLen[g_queueCount] = len;
                g_queueCount += 1;
            }
            free(entries);
        }
    }
    if (g_queueCount == 0) {
        fprintf(stderr, "[fuzz] no corpus files in '%s' — starting from a single byte seed\n",
                g_corpusDir);
        uint8_t zero = 0x00;
        queuePush(&zero, 1);
    }

    mkdir(g_crashDir, 0755);

    g_rng = seed ? seed : (uint64_t)getpid() ^ 0x9E3779B97F4A7C15ull;
    signal(SIGINT, handleStop);
    signal(SIGTERM, handleStop);

    struct timeval t0, t1;
    gettimeofday(&t0, NULL);

    fprintf(stderr, "[fuzz] worker %u | corpus %zu | seed %llu | timeout %u ms\n",
            g_workerId, g_queueCount, (unsigned long long)g_rng, g_timeoutMs);

    /* ── Fuzz loop ───────────────────────────────────────────────────── */
    for (uint64_t iter = 0; !g_stop && (g_maxIters == 0 || g_runs < g_maxIters); ++iter) {
        size_t base = (size_t)(iter % g_queueCount);
        size_t baseLen = g_queueLen[base];
        if (!g_detDone[base]) {
            g_detDone[base] = 1;
            deterministicStage(base);
        }

        for (unsigned m = 0; m < MAX_MUT_PER_BASE && !g_stop; ++m) {
            /* Start from the base input, mutate into g_input. */
            if (baseLen) memcpy(g_input, g_queue[base], baseLen);
            g_inputLen = baseLen;
            g_inputLen = mutateHavoc();

            runOneAndHandle(g_inputLen);

            if ((g_runs & 0x3FFu) == 0)
                printStats();
        }
    }

    gettimeofday(&t1, NULL);
    double secs = (double)(t1.tv_sec - t0.tv_sec)
                + (double)(t1.tv_usec - t0.tv_usec) / 1e6;
    fprintf(stderr, "\n[fuzz] done: %llu runs in %.1fs (%.0f exec/s) | cov %zu | "
                    "queue %zu | crashes %u | timeouts %u\n",
            (unsigned long long)g_runs, secs,
            secs > 0 ? (double)g_runs / secs : 0.0,
            countCoverage(), g_queueCount, g_crashes, g_timeouts);

    return g_crashes > 0 ? 1 : 0;
}