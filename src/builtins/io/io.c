#include "io.h"
#include "../string/string.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>

// ── Internal helpers ────────────────────────────────────────────────────────

// Read a line from stdin, stripping trailing newline.
// Returns a tracked heap-allocated string (freed via lucis_freeStr).
static lucis_io_string read_line_internal(void) {
    char*  raw  = NULL;
    size_t cap  = 0;
    ssize_t len = getline(&raw, &cap, stdin);

    if (len < 0) {
        free(raw);
        char* empty = (char*)lucis_allocString(1);
        if (!empty) empty = (char*)malloc(1);
        empty[0] = '\0';
        return (lucis_io_string){ empty, 0 };
    }

    // Strip trailing newline
    if (len > 0 && raw[len - 1] == '\n') {
        raw[--len] = '\0';
        if (len > 0 && raw[len - 1] == '\r')
            raw[--len] = '\0';
    }

    // Copy into tracked memory so lucis_freeStr can free it
    size_t n = (size_t)len;
    char* tracked = (char*)lucis_allocString(n + 1);
    if (tracked) {
        memcpy(tracked, raw, n);
        tracked[n] = '\0';
    } else {
        tracked = raw; // fallback — may leak but better than crash
        raw = NULL;
    }
    free(raw);
    return (lucis_io_string){ tracked ? tracked : "", n };
}

// ── Stdin — Text ────────────────────────────────────────────────────────────

lucis_io_string lucis_readLine(void) {
    return read_line_internal();
}

char lucis_readChar(void) {
    int c = fgetc(stdin);
    return (c == EOF) ? '\0' : (char)c;
}

int64_t lucis_readInt(void) {
    lucis_io_string line = read_line_internal();
    int64_t result = 0;

    if (line.len > 0) {
        char* end = NULL;
        result = strtoll(line.ptr, &end, 10);
    }

    free((void*)line.ptr);
    return result;
}

double lucis_readFloat(void) {
    lucis_io_string line = read_line_internal();
    double result = 0.0;

    if (line.len > 0) {
        char* end = NULL;
        result = strtod(line.ptr, &end);
    }

    free((void*)line.ptr);
    return result;
}

int lucis_readBool(void) {
    lucis_io_string line = read_line_internal();
    int result = 0;

    if (line.len > 0) {
        result = (strcmp(line.ptr, "true") == 0 ||
                  strcmp(line.ptr, "1") == 0 ||
                  strcmp(line.ptr, "yes") == 0);
    }

    free((void*)line.ptr);
    return result;
}

lucis_io_string lucis_readAll(void) {
    size_t cap = 4096;
    size_t len = 0;
    char*  raw = (char*)malloc(cap);
    if (!raw) return (lucis_io_string){ "", 0 };

    while (1) {
        size_t n = fread(raw + len, 1, cap - len, stdin);
        len += n;
        if (n == 0) break;

        if (len == cap) {
            cap *= 2;
            char* nr = (char*)realloc(raw, cap);
            if (!nr) break;
            raw = nr;
        }
    }

    raw[len] = '\0';

    // Copy into tracked memory
    char* tracked = (char*)lucis_allocString(len + 1);
    if (tracked) {
        memcpy(tracked, raw, len);
        tracked[len] = '\0';
    }
    free(raw);
    return (lucis_io_string){ tracked ? tracked : "", len };
}

// ── Prompt variants ─────────────────────────────────────────────────────────

lucis_io_string lucis_prompt(const char* msg, size_t msgLen) {
    printf("%.*s", (int)msgLen, msg);
    fflush(stdout);
    return read_line_internal();
}

int64_t lucis_promptInt(const char* msg, size_t msgLen) {
    printf("%.*s", (int)msgLen, msg);
    fflush(stdout);

    lucis_io_string line = read_line_internal();
    int64_t result = 0;

    if (line.len > 0) {
        char* end = NULL;
        result = strtoll(line.ptr, &end, 10);
    }

    free((void*)line.ptr);
    return result;
}

double lucis_promptFloat(const char* msg, size_t msgLen) {
    printf("%.*s", (int)msgLen, msg);
    fflush(stdout);

    lucis_io_string line = read_line_internal();
    double result = 0.0;

    if (line.len > 0) {
        char* end = NULL;
        result = strtod(line.ptr, &end);
    }

    free((void*)line.ptr);
    return result;
}

int lucis_promptBool(const char* msg, size_t msgLen) {
    printf("%.*s", (int)msgLen, msg);
    fflush(stdout);

    lucis_io_string line = read_line_internal();
    int result = 0;

    if (line.len > 0) {
        result = (strcmp(line.ptr, "true") == 0 ||
                  strcmp(line.ptr, "1") == 0 ||
                  strcmp(line.ptr, "yes") == 0);
    }

    free((void*)line.ptr);
    return result;
}

// ── Stdin — Binary ──────────────────────────────────────────────────────────

uint8_t lucis_readByte(void) {
    int c = fgetc(stdin);
    return (c == EOF) ? 0 : (uint8_t)c;
}

// ── Stdin — Status ──────────────────────────────────────────────────────────

int lucis_isEOF(void) {
    return feof(stdin) ? 1 : 0;
}

// ── Secure Input ────────────────────────────────────────────────────────────

static lucis_io_string read_password_internal(void) {
    struct termios old, new_term;

    // Only disable echo if stdin is a terminal
    int is_tty = isatty(STDIN_FILENO);

    if (is_tty) {
        tcgetattr(STDIN_FILENO, &old);
        new_term = old;
        new_term.c_lflag &= ~(ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &new_term);
    }

    lucis_io_string result = read_line_internal();

    if (is_tty) {
        tcsetattr(STDIN_FILENO, TCSANOW, &old);
        putchar('\n');  // echo newline after hidden input
    }

    return result;
}

lucis_io_string lucis_readPassword(void) {
    return read_password_internal();
}

lucis_io_string lucis_promptPassword(const char* msg, size_t msgLen) {
    printf("%.*s", (int)msgLen, msg);
    fflush(stdout);
    return read_password_internal();
}

// ── Stream Control ──────────────────────────────────────────────────────────

void lucis_flush(void) {
    fflush(stdout);
}

void lucis_flushErr(void) {
    fflush(stderr);
}

// ── Terminal Detection ──────────────────────────────────────────────────────

int lucis_isTTY(void) {
    return isatty(STDIN_FILENO) ? 1 : 0;
}

int lucis_isStdoutTTY(void) {
    return isatty(STDOUT_FILENO) ? 1 : 0;
}

int lucis_isStderrTTY(void) {
    return isatty(STDERR_FILENO) ? 1 : 0;
}

// ── Vec-returning functions ─────────────────────────────────────────────────

void lucis_readLines(lucis_io_vec_header* out) {
    out->ptr = NULL;
    out->len = 0;
    out->cap = 0;

    typedef struct { const char* ptr; size_t len; } str_elem;

    size_t cap = 8;
    str_elem* arr = (str_elem*)malloc(cap * sizeof(str_elem));
    if (!arr) return;
    size_t count = 0;

    char* rawLine = NULL;
    size_t rawCap = 0;
    ssize_t n;
    while ((n = getline(&rawLine, &rawCap, stdin)) != -1) {
        if (n > 0 && rawLine[n - 1] == '\n') n--;
        if (n > 0 && rawLine[n - 1] == '\r') n--;

        size_t nlen = (size_t)n;
        char* tracked = (char*)lucis_allocString(nlen + 1);
        if (!tracked) continue;
        memcpy(tracked, rawLine, nlen);
        tracked[nlen] = '\0';

        if (count == cap) {
            cap *= 2;
            str_elem* nr = (str_elem*)realloc(arr, cap * sizeof(str_elem));
            if (!nr) { free(tracked); break; }
            arr = nr;
        }
        arr[count].ptr = tracked;
        arr[count].len = nlen;
        count++;
    }
    free(rawLine);

    out->ptr = arr;
    out->len = count;
    out->cap = cap;
}

void lucis_readNBytes(lucis_io_vec_header* out, size_t n) {
    out->ptr = NULL;
    out->len = 0;
    out->cap = 0;

    uint8_t* buf = (uint8_t*)malloc(n);
    size_t total = 0;
    while (total < n) {
        size_t got = fread(buf + total, 1, n - total, stdin);
        if (got == 0) break;
        total += got;
    }

    out->ptr = buf;
    out->len = total;
    out->cap = n;
}
