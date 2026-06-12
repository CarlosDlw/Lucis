#define _POSIX_C_SOURCE 200809L

#include "regex.h"

#include <regex.h>
#include <stdlib.h>
#include <string.h>

/* ── helper: null-terminate a (ptr, len) pair ────────────────────────── */
static char* make_cstr(const char* s, size_t len) {
    char* buf = (char*)malloc(len + 1);
    if (!buf) return NULL;
    memcpy(buf, s, len);
    buf[len] = '\0';
    return buf;
}

/* ── helper: build result from buffer + length ───────────────────────── */
static lucis_regex_str_result make_result(const char* src, size_t len) {
    lucis_regex_str_result res = {NULL, 0};
    if (len == 0) { res.ptr = ""; return res; }
    char* buf = (char*)malloc(len);
    if (!buf) return res;
    memcpy(buf, src, len);
    res.ptr = buf;
    res.len = len;
    return res;
}

/* ── match ────────────────────────────────────────────────────────────── */
int32_t lucis_regexMatch(const char* text, size_t text_len,
                           const char* pat, size_t pat_len) {
    char* ctext = make_cstr(text, text_len);
    char* cpat = make_cstr(pat, pat_len);
    if (!ctext || !cpat) { free(ctext); free(cpat); return 0; }

    regex_t re;
    int rc = regcomp(&re, cpat, REG_EXTENDED | REG_NOSUB);
    free(cpat);
    if (rc != 0) { free(ctext); return 0; }

    int result = regexec(&re, ctext, 0, NULL, 0) == 0 ? 1 : 0;
    regfree(&re);
    free(ctext);
    return result;
}

/* ── find ─────────────────────────────────────────────────────────────── */
lucis_regex_str_result lucis_regexFind(const char* text, size_t text_len,
                                          const char* pat, size_t pat_len) {
    lucis_regex_str_result empty = {NULL, 0};
    empty.ptr = "";

    char* ctext = make_cstr(text, text_len);
    char* cpat = make_cstr(pat, pat_len);
    if (!ctext || !cpat) { free(ctext); free(cpat); return empty; }

    regex_t re;
    if (regcomp(&re, cpat, REG_EXTENDED) != 0) {
        free(ctext); free(cpat); return empty;
    }
    free(cpat);

    regmatch_t match;
    if (regexec(&re, ctext, 1, &match, 0) != 0) {
        regfree(&re); free(ctext); return empty;
    }

    size_t mlen = (size_t)(match.rm_eo - match.rm_so);
    lucis_regex_str_result res = make_result(ctext + match.rm_so, mlen);
    regfree(&re);
    free(ctext);
    return res;
}

/* ── findIndex ────────────────────────────────────────────────────────── */
int64_t lucis_regexFindIndex(const char* text, size_t text_len,
                               const char* pat, size_t pat_len) {
    char* ctext = make_cstr(text, text_len);
    char* cpat = make_cstr(pat, pat_len);
    if (!ctext || !cpat) { free(ctext); free(cpat); return -1; }

    regex_t re;
    if (regcomp(&re, cpat, REG_EXTENDED) != 0) {
        free(ctext); free(cpat); return -1;
    }
    free(cpat);

    regmatch_t match;
    int64_t result;
    if (regexec(&re, ctext, 1, &match, 0) == 0) {
        result = (int64_t)match.rm_so;
    } else {
        result = -1;
    }
    regfree(&re);
    free(ctext);
    return result;
}

/* ── helper: regex replace (all or first) ─────────────────────────────── */
static lucis_regex_str_result regex_replace_impl(
        const char* text, size_t text_len,
        const char* pat, size_t pat_len,
        const char* rep, size_t rep_len,
        int all) {
    lucis_regex_str_result empty = {NULL, 0};

    char* ctext = make_cstr(text, text_len);
    char* cpat = make_cstr(pat, pat_len);
    if (!ctext || !cpat) { free(ctext); free(cpat); return empty; }

    regex_t re;
    if (regcomp(&re, cpat, REG_EXTENDED) != 0) {
        free(ctext); free(cpat); return empty;
    }
    free(cpat);

    /* Build result buffer dynamically */
    size_t cap = text_len + rep_len * 4 + 64;
    char* buf = (char*)malloc(cap);
    if (!buf) { regfree(&re); free(ctext); return empty; }
    size_t pos = 0;

    const char* cursor = ctext;
    size_t remaining = text_len;
    regmatch_t match;

    while (regexec(&re, cursor, 1, &match, 0) == 0) {
        size_t before = (size_t)match.rm_so;
        size_t mlen = (size_t)(match.rm_eo - match.rm_so);

        /* Ensure capacity */
        size_t needed = pos + before + rep_len + remaining + 1;
        if (needed > cap) {
            cap = needed * 2;
            char* nb = (char*)realloc(buf, cap);
            if (!nb) { free(buf); regfree(&re); free(ctext); return empty; }
            buf = nb;
        }

        /* Copy text before match */
        memcpy(buf + pos, cursor, before);
        pos += before;

        /* Copy replacement */
        memcpy(buf + pos, rep, rep_len);
        pos += rep_len;

        cursor += match.rm_eo;
        remaining -= match.rm_eo;

        /* Avoid infinite loop on zero-length match */
        if (mlen == 0) {
            if (remaining > 0) {
                buf[pos++] = *cursor;
                cursor++;
                remaining--;
            } else {
                break;
            }
        }

        if (!all) break;
    }

    /* Copy remaining text */
    if (remaining > 0) {
        size_t needed = pos + remaining;
        if (needed > cap) {
            cap = needed;
            char* nb = (char*)realloc(buf, cap);
            if (!nb) { free(buf); regfree(&re); free(ctext); return empty; }
            buf = nb;
        }
        memcpy(buf + pos, cursor, remaining);
        pos += remaining;
    }

    regfree(&re);
    free(ctext);

    lucis_regex_str_result res;
    res.ptr = buf;
    res.len = pos;
    return res;
}

/* ── regexReplace ─────────────────────────────────────────────────────── */
lucis_regex_str_result lucis_regexReplace(const char* text, size_t text_len,
                                             const char* pat, size_t pat_len,
                                             const char* rep, size_t rep_len) {
    return regex_replace_impl(text, text_len, pat, pat_len, rep, rep_len, 1);
}

/* ── regexReplaceFirst ────────────────────────────────────────────────── */
lucis_regex_str_result lucis_regexReplaceFirst(const char* text, size_t text_len,
                                                  const char* pat, size_t pat_len,
                                                  const char* rep, size_t rep_len) {
    return regex_replace_impl(text, text_len, pat, pat_len, rep, rep_len, 0);
}

/* ── isValid ──────────────────────────────────────────────────────────── */
int32_t lucis_regexIsValid(const char* pat, size_t pat_len) {
    char* cpat = make_cstr(pat, pat_len);
    if (!cpat) return 0;

    regex_t re;
    int rc = regcomp(&re, cpat, REG_EXTENDED | REG_NOSUB);
    free(cpat);

    if (rc == 0) {
        regfree(&re);
        return 1;
    }
    return 0;
}

/* ── Vec-returning regex functions ────────────────────────────── */

void lucis_regexFindAll(lucis_regex_vec_header* out,
                         const char* text, size_t text_len,
                         const char* pat, size_t pat_len) {
    typedef struct { const char* ptr; size_t len; } str_elem;

    out->ptr = NULL;
    out->len = 0;
    out->cap = 0;

    char* ptmp = (char*)malloc(pat_len + 1);
    memcpy(ptmp, pat, pat_len);
    ptmp[pat_len] = '\0';

    char* ttmp = (char*)malloc(text_len + 1);
    memcpy(ttmp, text, text_len);
    ttmp[text_len] = '\0';

    regex_t re;
    if (regcomp(&re, ptmp, REG_EXTENDED) != 0) {
        free(ptmp); free(ttmp);
        return;
    }
    free(ptmp);

    size_t cap = 8;
    str_elem* arr = (str_elem*)malloc(cap * sizeof(str_elem));
    size_t count = 0;

    const char* cursor = ttmp;
    regmatch_t match;
    while (regexec(&re, cursor, 1, &match, 0) == 0) {
        size_t mLen = (size_t)(match.rm_eo - match.rm_so);
        char* copy = (char*)malloc(mLen + 1);
        memcpy(copy, cursor + match.rm_so, mLen);
        copy[mLen] = '\0';

        if (count == cap) { cap *= 2; arr = (str_elem*)realloc(arr, cap * sizeof(str_elem)); }
        arr[count].ptr = copy;
        arr[count].len = mLen;
        count++;

        cursor += match.rm_eo;
        if (match.rm_eo == 0) cursor++; // avoid infinite loop on zero-length match
        if (*cursor == '\0') break;
    }
    regfree(&re);
    free(ttmp);

    out->ptr = arr;
    out->len = count;
    out->cap = cap;
}

void lucis_regexSplit(lucis_regex_vec_header* out,
                       const char* text, size_t text_len,
                       const char* pat, size_t pat_len) {
    typedef struct { const char* ptr; size_t len; } str_elem;

    out->ptr = NULL;
    out->len = 0;
    out->cap = 0;

    char* ptmp = (char*)malloc(pat_len + 1);
    memcpy(ptmp, pat, pat_len);
    ptmp[pat_len] = '\0';

    char* ttmp = (char*)malloc(text_len + 1);
    memcpy(ttmp, text, text_len);
    ttmp[text_len] = '\0';

    regex_t re;
    if (regcomp(&re, ptmp, REG_EXTENDED) != 0) {
        free(ptmp); free(ttmp);
        // Return single element with full string
        str_elem* arr = (str_elem*)malloc(sizeof(str_elem));
        char* copy = (char*)malloc(text_len + 1);
        memcpy(copy, text, text_len);
        copy[text_len] = '\0';
        arr[0].ptr = copy;
        arr[0].len = text_len;
        out->ptr = arr;
        out->len = 1;
        out->cap = 1;
        return;
    }
    free(ptmp);

    size_t cap = 8;
    str_elem* arr = (str_elem*)malloc(cap * sizeof(str_elem));
    size_t count = 0;

    const char* cursor = ttmp;
    regmatch_t match;
    while (regexec(&re, cursor, 1, &match, 0) == 0) {
        size_t partLen = (size_t)match.rm_so;
        char* part = (char*)malloc(partLen + 1);
        memcpy(part, cursor, partLen);
        part[partLen] = '\0';

        if (count == cap) { cap *= 2; arr = (str_elem*)realloc(arr, cap * sizeof(str_elem)); }
        arr[count].ptr = part;
        arr[count].len = partLen;
        count++;

        cursor += match.rm_eo;
        if (match.rm_eo == 0) { cursor++; }
        if (*cursor == '\0') {
            // Add trailing empty string
            char* empty = (char*)malloc(1);
            empty[0] = '\0';
            if (count == cap) { cap *= 2; arr = (str_elem*)realloc(arr, cap * sizeof(str_elem)); }
            arr[count].ptr = empty;
            arr[count].len = 0;
            count++;
            break;
        }
    }

    // Add remaining
    if (*cursor != '\0' || count == 0) {
        size_t remLen = strlen(cursor);
        char* rem = (char*)malloc(remLen + 1);
        memcpy(rem, cursor, remLen);
        rem[remLen] = '\0';
        if (count == cap) { cap *= 2; arr = (str_elem*)realloc(arr, cap * sizeof(str_elem)); }
        arr[count].ptr = rem;
        arr[count].len = remLen;
        count++;
    }
    regfree(&re);
    free(ttmp);

    out->ptr = arr;
    out->len = count;
    out->cap = cap;
}
