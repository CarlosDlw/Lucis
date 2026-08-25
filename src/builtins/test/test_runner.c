#include "test_runner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <signal.h>
#include <errno.h>

/* ── Configuration ─────────────────────────────────────────────────── */

#define MAX_CAPTURED_OUTPUT (16u * 1024u)   /* per-test capture cap */

/* ── ANSI colors (only when stderr is a TTY) ──────────────────────── */

#define COL_RESET   "\033[0m"
#define COL_GREEN   "\033[32m"
#define COL_RED     "\033[31m"
#define COL_YELLOW  "\033[33m"
#define COL_CYAN    "\033[36m"

/* ── Outcome kinds ────────────────────────────────────────────────── */

typedef enum {
    OUTCOME_PASS,
    OUTCOME_FAIL,
    OUTCOME_SKIP,
} OutcomeKind;

typedef struct {
    OutcomeKind kind;
    char detail[192];       /* failure/timeout/skip detail ("" when clean) */
    char* output;           /* captured child output (malloc'd, may be NULL) */
    size_t outputLen;
} SlotResult;

typedef struct {
    const char* name;       /* display name (static storage from IR) */
    const char* skipReason;
    int skip;
    int expectFail;
    int64_t timeoutMs;
    void (*body)(void);

    pid_t pid;              /* 0 = not running / finished */
    int pipeFd;             /* read end of the capture pipe */
    int running;

    SlotResult result;
} TestSlot;

static TestSlot* g_slots = NULL;
static int64_t  g_total = 0;
static int      g_jobs = 1;
static int      g_verbose = 0;

static int64_t g_passed = 0;
static int64_t g_failed = 0;
static int64_t g_skipped = 0;

static int64_t g_nextToReport = 0;   /* slots < this are already reported */
static int64_t g_runningCount = 0;   /* children currently in flight */

/* ── Small helpers ────────────────────────────────────────────────── */

static int useColor(void) {
    return isatty(STDOUT_FILENO) == 1;
}

static void printStatus(const char* color, const char* tag,
                        const char* name, const char* detail) {
    if (useColor())
        printf("%s[%s]%s %s", color, tag, COL_RESET, name);
    else
        printf("[%s] %s", tag, name);
    if (detail && detail[0])
        printf(" — %s", detail);
    printf("\n");
    fflush(stdout);
}

static void appendOutput(TestSlot* slot, const char* buf, size_t len) {
    if (len == 0) return;
    const size_t cap = MAX_CAPTURED_OUTPUT;
    if (!slot->result.output) {
        slot->result.output = (char*)malloc(cap + 1);
        if (!slot->result.output) return;
        slot->result.output[0] = '\0';
    }
    size_t used = slot->result.outputLen;
    if (used >= cap) return;
    size_t room = cap - used;
    size_t take = len < room ? len : room;
    memcpy(slot->result.output + used, buf, take);
    slot->result.outputLen = used + take;
    slot->result.output[slot->result.outputLen] = '\0';
}

static void echoOutput(const SlotResult* r) {
    if (r->output && r->outputLen)
        fwrite(r->output, 1, r->outputLen, stdout);
}

/* ── Job lifecycle ────────────────────────────────────────────────── */

static int spawnJob(TestSlot* slot) {
    int fds[2];
    if (pipe(fds) != 0) return -1;

    pid_t pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        return -1;
    }

    if (pid == 0) {
        /* Child: capture stdout+stderr, arm the timeout, run the test. */
        close(fds[0]);
        dup2(fds[1], STDOUT_FILENO);
        dup2(fds[1], STDERR_FILENO);
        if (fds[1] > STDERR_FILENO) close(fds[1]);

        if (slot->timeoutMs > 0) {
            struct itimerval timer;
            memset(&timer, 0, sizeof(timer));
            timer.it_value.tv_sec  = slot->timeoutMs / 1000;
            timer.it_value.tv_usec = (slot->timeoutMs % 1000) * 1000;
            setitimer(ITIMER_REAL, &timer, NULL);  /* SIGALRM terminates us */
        }

        slot->body();
        fflush(NULL);
        _exit(0);
    }

    /* Parent. */
    close(fds[1]);
    slot->pid = pid;
    slot->pipeFd = fds[0];
    slot->running = 1;
    return 0;
}

/* Wait for one specific child (optionally non-blocking sweep), drain its
 * capture pipe and decode the outcome into the slot. Returns 1 when the
 * slot actually finished, 0 when it is still running (WNOHANG only). */
static int reapOne(TestSlot* slot, int blocking) {
    if (!slot->running) return 1;

    int status = 0;
    pid_t r;
    if (blocking) {
        do {
            r = waitpid(slot->pid, &status, 0);
        } while (r < 0 && errno == EINTR);
    } else {
        r = waitpid(slot->pid, &status, WNOHANG);
        if (r == 0) return 0;                   /* still running */
        if (r < 0 && errno == EINTR) return 0;  /* retry on next sweep */
    }
    slot->running = 0;
    slot->pid = 0;
    g_runningCount -= 1;

    /* Drain the capture pipe. Safe after the child is dead: EOF is
     * guaranteed once every writer is gone. */
    char buf[4096];
    ssize_t n;
    while ((n = read(slot->pipeFd, buf, sizeof(buf))) > 0)
        appendOutput(slot, buf, (size_t)n);
    close(slot->pipeFd);
    slot->pipeFd = -1;

    /* Decode the raw outcome. */
    SlotResult* res = &slot->result;
    res->detail[0] = '\0';
    int childFailed = 1;
    if (WIFEXITED(status)) {
        int code = WEXITSTATUS(status);
        if (code == 0) {
            childFailed = 0;
        } else {
            snprintf(res->detail, sizeof(res->detail), "exit code %d", code);
        }
    } else if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        if (sig == SIGALRM) {
            snprintf(res->detail, sizeof(res->detail),
                     "timed out after %lld ms", (long long)slot->timeoutMs);
        } else {
            const char* sigName = "signal";
            switch (sig) {
                case SIGSEGV: sigName = "SIGSEGV"; break;
                case SIGABRT: sigName = "SIGABRT"; break;
                case SIGILL:  sigName = "SIGILL";  break;
                case SIGFPE:  sigName = "SIGFPE";  break;
                case SIGBUS:  sigName = "SIGBUS";  break;
                case SIGKILL: sigName = "SIGKILL"; break;
                default: break;
            }
            snprintf(res->detail, sizeof(res->detail), "crashed (%s)", sigName);
        }
    } else {
        snprintf(res->detail, sizeof(res->detail), "unknown termination");
    }

    /* Apply expected_fail inversion. */
    int passed = !childFailed;
    if (slot->expectFail) {
        if (passed) {
            passed = 0;
            snprintf(res->detail, sizeof(res->detail),
                     "expected failure but passed");
        } else {
            passed = 1;
            res->detail[0] = '\0';
        }
    }
    res->kind = passed ? OUTCOME_PASS : OUTCOME_FAIL;
    return 1;
}

/* Report the outcome of one slot (must be finished or skip). */
static void reportOne(int64_t index) {
    TestSlot* slot = &g_slots[index];
    SlotResult* res = &slot->result;

    if (slot->skip) {
        const char* reason = slot->skipReason;
        res->kind = OUTCOME_SKIP;
        g_skipped += 1;
        printStatus(COL_YELLOW, "SKIP", slot->name,
                    (reason && reason[0]) ? reason : "");
        return;
    }
    if (res->kind == OUTCOME_PASS) {
        g_passed += 1;
        if (g_verbose) echoOutput(res);
        printStatus(COL_GREEN, "PASS", slot->name, "");
    } else {
        g_failed += 1;
        if (!g_verbose) echoOutput(res);
        printStatus(COL_RED, "FAIL", slot->name, res->detail);
    }
}

/* Reap oldest pending slots up to and including `index`, reporting them in
 * index order. Blocks on the oldest child, but first sweeps non-blocking so
 * pipes of already-finished siblings are drained (no pipe deadlock). */
static void reapThrough(int64_t index) {
    while (g_nextToReport <= index && g_nextToReport < g_total) {
        TestSlot* slot = &g_slots[g_nextToReport];
        if (slot->skip) {
            reportOne(g_nextToReport);
            g_nextToReport += 1;
            continue;
        }
        if (!slot->running) break;   /* not spawned yet — caller handles */

        /* Non-blocking sweep: drain children that finished meanwhile. */
        for (int64_t i = g_nextToReport; i < g_total && g_runningCount > 0; ++i) {
            if (g_slots[i].running)
                reapOne(&g_slots[i], /*blocking=*/0);
        }
        /* Block on the oldest pending child if it is still running. */
        slot = &g_slots[g_nextToReport];
        if (slot->running)
            reapOne(slot, /*blocking=*/1);
        reportOne(g_nextToReport);
        g_nextToReport += 1;
    }
}

/* Report finished/skip slots without blocking (used for skip bookkeeping). */
static void reportThrough(int64_t index) {
    while (g_nextToReport <= index && g_nextToReport < g_total) {
        TestSlot* slot = &g_slots[g_nextToReport];
        if (!slot->skip && slot->running) break;
        reportOne(g_nextToReport);
        g_nextToReport += 1;
    }
}

/* ── Harness API ──────────────────────────────────────────────────── */

int32_t lucis_test_begin(int64_t total, int32_t jobs, int32_t verbose) {
    if (total < 0) total = 0;
    if (jobs < 1) jobs = 1;
    if (jobs > total) jobs = (int)total;
    if (jobs < 1) jobs = 1;

    g_total = total;
    g_jobs = jobs;
    g_verbose = verbose;
    g_nextToReport = 0;
    g_runningCount = 0;

    g_slots = (TestSlot*)calloc(total > 0 ? (size_t)total : 1, sizeof(TestSlot));
    if (!g_slots) return 1;

    if (total > 0) {
        if (useColor())
            printf("%s[test]%s running %lld test%s\n", COL_CYAN, COL_RESET,
                   (long long)total, total == 1 ? "" : "s");
        else
            printf("[test] running %lld test%s\n", (long long)total,
                   total == 1 ? "" : "s");
        fflush(stdout);
    }
    return 0;
}

int32_t lucis_test_run(int32_t index, const char* displayName,
                       int32_t skip, const char* skipReason,
                       int32_t expectFail, int64_t timeoutMs,
                       void (*body)(void)) {
    if (index < 0 || index >= g_total || !g_slots) return 1;

    TestSlot* slot = &g_slots[index];
    slot->name = displayName ? displayName : "test";
    slot->skipReason = skipReason ? skipReason : "";
    slot->skip = skip != 0;
    slot->expectFail = expectFail != 0;
    slot->timeoutMs = timeoutMs;
    slot->body = body;

    if (slot->skip) {
        reportThrough(index);   /* report any finished slots before it */
        return 0;
    }

    if (spawnJob(slot) != 0) {
        /* fork/pipe failed: report a harness-level failure. */
        slot->result.kind = OUTCOME_FAIL;
        snprintf(slot->result.detail, sizeof(slot->result.detail),
                 "harness error (fork failed)");
        reportThrough(index);
        return 0;
    }
    g_runningCount += 1;

    if (g_jobs == 1) {
        reapThrough(index);
    } else if (g_runningCount >= g_jobs) {
        reapThrough(index);
    }
    return 0;
}

int32_t lucis_test_end(void) {
    /* Reap everything still pending (in index order). */
    while (g_nextToReport < g_total) {
        TestSlot* slot = &g_slots[g_nextToReport];
        if (!slot->skip && slot->running)
            reapOne(slot, /*blocking=*/1);
        reportOne(g_nextToReport);
        g_nextToReport += 1;
    }

    /* Summary. */
    if (g_total > 0) {
        printf("\n");
        if (useColor())
            printf("%s[test]%s %lld %s, %lld %s, %lld %s\n",
                   COL_CYAN, COL_RESET,
                   (long long)g_passed, g_passed == 1 ? "passed" : "passed",
                   (long long)g_failed, g_failed == 1 ? "failed" : "failed",
                   (long long)g_skipped, g_skipped == 1 ? "skipped" : "skipped");
        else
            printf("%lld passed, %lld failed, %lld skipped\n",
                   (long long)g_passed, (long long)g_failed, (long long)g_skipped);
        fflush(stdout);
    }

    for (int64_t i = 0; i < g_total; ++i) {
        free(g_slots[i].result.output);
        g_slots[i].result.output = NULL;
    }
    free(g_slots);
    g_slots = NULL;

    return g_failed > 0 ? 1 : 0;
}