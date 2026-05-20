#include "string.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct lux_owned_node {
    void* ptr;
    struct lux_owned_node* next;
} lux_owned_node;

static void* (*lux_sys_malloc)(size_t) = malloc;
static void* (*lux_sys_realloc)(void*, size_t) = realloc;
static void  (*lux_sys_free)(void*) = free;
static lux_owned_node* lux_owned_head = NULL;

static void lux_owned_track_ptr(void* ptr) {
    if (!ptr) return;
    lux_owned_node* node = (lux_owned_node*)lux_sys_malloc(sizeof(lux_owned_node));
    if (!node) return;
    node->ptr = ptr;
    node->next = lux_owned_head;
    lux_owned_head = node;
}

static int lux_owned_untrack_ptr(void* ptr) {
    lux_owned_node** cur = &lux_owned_head;
    while (*cur) {
        if ((*cur)->ptr == ptr) {
            lux_owned_node* dead = *cur;
            *cur = dead->next;
            lux_sys_free(dead);
            return 1;
        }
        cur = &((*cur)->next);
    }
    return 0;
}

static void* lux_tracked_malloc(size_t size) {
    void* ptr = lux_sys_malloc(size);
    lux_owned_track_ptr(ptr);
    return ptr;
}

static void* lux_tracked_realloc(void* old_ptr, size_t size) {
    if (!old_ptr) return lux_tracked_malloc(size);
    int was_owned = lux_owned_untrack_ptr(old_ptr);
    void* new_ptr = lux_sys_realloc(old_ptr, size);
    if (!new_ptr && was_owned) {
        lux_owned_track_ptr(old_ptr);
        return NULL;
    }
    if (was_owned) lux_owned_track_ptr(new_ptr);
    return new_ptr;
}

static void lux_tracked_free(void* ptr) {
    if (!ptr) return;
    if (!lux_owned_untrack_ptr(ptr)) return;
    lux_sys_free(ptr);
}

#define malloc(sz) lux_tracked_malloc(sz)
#define realloc(p, sz) lux_tracked_realloc(p, sz)
#define free(p) lux_tracked_free(p)

// ── Search & Match ──────────────────────────────────────────────────────────

int lux_contains(const char* s, size_t sLen,
                    const char* sub, size_t subLen) {
    if (subLen == 0) return 1;
    if (subLen > sLen) return 0;

    for (size_t i = 0; i <= sLen - subLen; i++) {
        if (memcmp(s + i, sub, subLen) == 0) return 1;
    }
    return 0;
}

int lux_startsWith(const char* s, size_t sLen,
                      const char* prefix, size_t prefixLen) {
    if (prefixLen > sLen) return 0;
    return memcmp(s, prefix, prefixLen) == 0;
}

int lux_endsWith(const char* s, size_t sLen,
                    const char* suffix, size_t suffixLen) {
    if (suffixLen > sLen) return 0;
    return memcmp(s + sLen - suffixLen, suffix, suffixLen) == 0;
}

int64_t lux_indexOf(const char* s, size_t sLen,
                       const char* sub, size_t subLen) {
    if (subLen == 0) return 0;
    if (subLen > sLen) return -1;

    for (size_t i = 0; i <= sLen - subLen; i++) {
        if (memcmp(s + i, sub, subLen) == 0)
            return (int64_t)i;
    }
    return -1;
}

int64_t lux_lastIndexOf(const char* s, size_t sLen,
                           const char* sub, size_t subLen) {
    if (subLen == 0) return (int64_t)sLen;
    if (subLen > sLen) return -1;

    for (size_t i = sLen - subLen; ; i--) {
        if (memcmp(s + i, sub, subLen) == 0)
            return (int64_t)i;
        if (i == 0) break;
    }
    return -1;
}

size_t lux_count(const char* s, size_t sLen,
                    const char* sub, size_t subLen) {
    if (subLen == 0) return 0;
    if (subLen > sLen) return 0;

    size_t count = 0;
    for (size_t i = 0; i <= sLen - subLen; i++) {
        if (memcmp(s + i, sub, subLen) == 0) {
            count++;
            i += subLen - 1; // non-overlapping
        }
    }
    return count;
}

// ── Transformation ──────────────────────────────────────────────────────────

lux_str_result lux_toUpper(const char* s, size_t sLen) {
    char* buf = (char*)malloc(sLen + 1);
    for (size_t i = 0; i < sLen; i++)
        buf[i] = (char)toupper((unsigned char)s[i]);
    buf[sLen] = '\0';
    return (lux_str_result){ buf, sLen };
}

lux_str_result lux_toLower(const char* s, size_t sLen) {
    char* buf = (char*)malloc(sLen + 1);
    for (size_t i = 0; i < sLen; i++)
        buf[i] = (char)tolower((unsigned char)s[i]);
    buf[sLen] = '\0';
    return (lux_str_result){ buf, sLen };
}

lux_str_result lux_trim(const char* s, size_t sLen) {
    size_t start = 0;
    while (start < sLen && isspace((unsigned char)s[start])) start++;
    size_t end = sLen;
    while (end > start && isspace((unsigned char)s[end - 1])) end--;

    size_t newLen = end - start;
    char* buf = (char*)malloc(newLen + 1);
    memcpy(buf, s + start, newLen);
    buf[newLen] = '\0';
    return (lux_str_result){ buf, newLen };
}

lux_str_result lux_trimLeft(const char* s, size_t sLen) {
    size_t start = 0;
    while (start < sLen && isspace((unsigned char)s[start])) start++;

    size_t newLen = sLen - start;
    char* buf = (char*)malloc(newLen + 1);
    memcpy(buf, s + start, newLen);
    buf[newLen] = '\0';
    return (lux_str_result){ buf, newLen };
}

lux_str_result lux_trimRight(const char* s, size_t sLen) {
    size_t end = sLen;
    while (end > 0 && isspace((unsigned char)s[end - 1])) end--;

    char* buf = (char*)malloc(end + 1);
    memcpy(buf, s, end);
    buf[end] = '\0';
    return (lux_str_result){ buf, end };
}

lux_str_result lux_replace(const char* s, size_t sLen,
                                 const char* old, size_t oldLen,
                                 const char* rep, size_t repLen) {
    if (oldLen == 0) {
        char* buf = (char*)malloc(sLen + 1);
        memcpy(buf, s, sLen);
        buf[sLen] = '\0';
        return (lux_str_result){ buf, sLen };
    }

    // Count occurrences first
    size_t cnt = 0;
    for (size_t i = 0; i <= sLen - oldLen; i++) {
        if (memcmp(s + i, old, oldLen) == 0) {
            cnt++;
            i += oldLen - 1;
        }
    }

    if (cnt == 0) {
        char* buf = (char*)malloc(sLen + 1);
        memcpy(buf, s, sLen);
        buf[sLen] = '\0';
        return (lux_str_result){ buf, sLen };
    }

    size_t newLen = sLen + cnt * (repLen - oldLen);
    char* buf = (char*)malloc(newLen + 1);
    size_t wi = 0;

    for (size_t i = 0; i < sLen; ) {
        if (i <= sLen - oldLen && memcmp(s + i, old, oldLen) == 0) {
            memcpy(buf + wi, rep, repLen);
            wi += repLen;
            i += oldLen;
        } else {
            buf[wi++] = s[i++];
        }
    }

    buf[newLen] = '\0';
    return (lux_str_result){ buf, newLen };
}

lux_str_result lux_replaceFirst(const char* s, size_t sLen,
                                      const char* old, size_t oldLen,
                                      const char* rep, size_t repLen) {
    if (oldLen == 0 || oldLen > sLen) {
        char* buf = (char*)malloc(sLen + 1);
        memcpy(buf, s, sLen);
        buf[sLen] = '\0';
        return (lux_str_result){ buf, sLen };
    }

    // Find first occurrence
    int64_t pos = -1;
    for (size_t i = 0; i <= sLen - oldLen; i++) {
        if (memcmp(s + i, old, oldLen) == 0) {
            pos = (int64_t)i;
            break;
        }
    }

    if (pos < 0) {
        char* buf = (char*)malloc(sLen + 1);
        memcpy(buf, s, sLen);
        buf[sLen] = '\0';
        return (lux_str_result){ buf, sLen };
    }

    size_t newLen = sLen - oldLen + repLen;
    char* buf = (char*)malloc(newLen + 1);
    memcpy(buf, s, (size_t)pos);
    memcpy(buf + pos, rep, repLen);
    memcpy(buf + pos + repLen, s + pos + oldLen, sLen - (size_t)pos - oldLen);
    buf[newLen] = '\0';
    return (lux_str_result){ buf, newLen };
}

lux_str_result lux_repeat(const char* s, size_t sLen, size_t n) {
    size_t newLen = sLen * n;
    char* buf = (char*)malloc(newLen + 1);
    for (size_t i = 0; i < n; i++)
        memcpy(buf + i * sLen, s, sLen);
    buf[newLen] = '\0';
    return (lux_str_result){ buf, newLen };
}

lux_str_result lux_reverse(const char* s, size_t sLen) {
    char* buf = (char*)malloc(sLen + 1);
    for (size_t i = 0; i < sLen; i++)
        buf[i] = s[sLen - 1 - i];
    buf[sLen] = '\0';
    return (lux_str_result){ buf, sLen };
}

// ── Formatting ──────────────────────────────────────────────────────────────

lux_str_result lux_padLeft(const char* s, size_t sLen,
                                 size_t width, char fill) {
    if (sLen >= width) {
        char* buf = (char*)malloc(sLen + 1);
        memcpy(buf, s, sLen);
        buf[sLen] = '\0';
        return (lux_str_result){ buf, sLen };
    }

    char* buf = (char*)malloc(width + 1);
    size_t pad = width - sLen;
    memset(buf, fill, pad);
    memcpy(buf + pad, s, sLen);
    buf[width] = '\0';
    return (lux_str_result){ buf, width };
}

lux_str_result lux_padRight(const char* s, size_t sLen,
                                  size_t width, char fill) {
    if (sLen >= width) {
        char* buf = (char*)malloc(sLen + 1);
        memcpy(buf, s, sLen);
        buf[sLen] = '\0';
        return (lux_str_result){ buf, sLen };
    }

    char* buf = (char*)malloc(width + 1);
    memcpy(buf, s, sLen);
    memset(buf + sLen, fill, width - sLen);
    buf[width] = '\0';
    return (lux_str_result){ buf, width };
}

// ── Extraction ──────────────────────────────────────────────────────────────

lux_str_result lux_substring(const char* s, size_t sLen,
                                   size_t start, size_t length) {
    if (start >= sLen) {
        char* buf = (char*)malloc(1);
        buf[0] = '\0';
        return (lux_str_result){ buf, 0 };
    }

    if (start + length > sLen)
        length = sLen - start;

    char* buf = (char*)malloc(length + 1);
    memcpy(buf, s + start, length);
    buf[length] = '\0';
    return (lux_str_result){ buf, length };
}

char lux_charAt(const char* s, size_t sLen, size_t index) {
    if (index >= sLen) return '\0';
    return s[index];
}

lux_str_result lux_slice(const char* s, size_t sLen,
                               int64_t start, int64_t end) {
    // Resolve negative indices
    if (start < 0) start = (int64_t)sLen + start;
    if (end < 0)   end   = (int64_t)sLen + end;

    // Clamp to bounds
    if (start < 0)          start = 0;
    if (end > (int64_t)sLen) end  = (int64_t)sLen;
    if (start >= end) {
        char* buf = (char*)malloc(1);
        buf[0] = '\0';
        return (lux_str_result){ buf, 0 };
    }

    size_t newLen = (size_t)(end - start);
    char* buf = (char*)malloc(newLen + 1);
    memcpy(buf, s + start, newLen);
    buf[newLen] = '\0';
    return (lux_str_result){ buf, newLen };
}

// ── Parsing ─────────────────────────────────────────────────────────────────

int64_t lux_parseInt(const char* s, size_t sLen) {
    // Create null-terminated copy for strtoll
    char* tmp = (char*)malloc(sLen + 1);
    memcpy(tmp, s, sLen);
    tmp[sLen] = '\0';
    int64_t result = strtoll(tmp, NULL, 10);
    free(tmp);
    return result;
}

int64_t lux_parseIntRadix(const char* s, size_t sLen, uint32_t radix) {
    char* tmp = (char*)malloc(sLen + 1);
    memcpy(tmp, s, sLen);
    tmp[sLen] = '\0';
    int64_t result = strtoll(tmp, NULL, (int)radix);
    free(tmp);
    return result;
}

double lux_parseFloat(const char* s, size_t sLen) {
    char* tmp = (char*)malloc(sLen + 1);
    memcpy(tmp, s, sLen);
    tmp[sLen] = '\0';
    double result = strtod(tmp, NULL);
    free(tmp);
    return result;
}

// ── Conversion ──────────────────────────────────────────────────────────────

char lux_fromCharCode(int32_t code) {
    if (code < 0 || code > 127) return '\0';
    return (char)code;
}

// ── Vec-returning functions ─────────────────────────────────────────────────

void lux_split(lux_str_vec_header* out,
                  const char* s, size_t sLen,
                  const char* delim, size_t delimLen) {
    typedef struct { const char* ptr; size_t len; } str_elem;

    out->ptr = NULL;
    out->len = 0;
    out->cap = 0;

    size_t cap = 8;
    str_elem* arr = (str_elem*)malloc(cap * sizeof(str_elem));
    size_t count = 0;

    if (delimLen == 0) {
        // Empty delimiter: split into individual characters
        for (size_t i = 0; i < sLen; i++) {
            char* ch = (char*)malloc(2);
            ch[0] = s[i]; ch[1] = '\0';
            if (count == cap) { cap *= 2; arr = (str_elem*)realloc(arr, cap * sizeof(str_elem)); }
            arr[count].ptr = ch;
            arr[count].len = 1;
            count++;
        }
    } else {
        size_t start = 0;
        while (start <= sLen) {
            // Find next delimiter
            const char* found = NULL;
            for (size_t i = start; i + delimLen <= sLen; i++) {
                if (memcmp(s + i, delim, delimLen) == 0) {
                    found = s + i;
                    break;
                }
            }

            size_t end = found ? (size_t)(found - s) : sLen;
            size_t partLen = end - start;

            char* part = (char*)malloc(partLen + 1);
            memcpy(part, s + start, partLen);
            part[partLen] = '\0';

            if (count == cap) { cap *= 2; arr = (str_elem*)realloc(arr, cap * sizeof(str_elem)); }
            arr[count].ptr = part;
            arr[count].len = partLen;
            count++;

            if (!found) break;
            start = end + delimLen;
        }
    }

    out->ptr = arr;
    out->len = count;
    out->cap = cap;
}

void lux_splitN(lux_str_vec_header* out,
                   const char* s, size_t sLen,
                   const char* delim, size_t delimLen, size_t maxParts) {
    typedef struct { const char* ptr; size_t len; } str_elem;

    out->ptr = NULL;
    out->len = 0;
    out->cap = 0;

    if (maxParts == 0) return;

    size_t cap = maxParts < 8 ? maxParts : 8;
    str_elem* arr = (str_elem*)malloc(cap * sizeof(str_elem));
    size_t count = 0;

    size_t start = 0;
    while (start <= sLen && count < maxParts - 1) {
        const char* found = NULL;
        for (size_t i = start; i + delimLen <= sLen; i++) {
            if (memcmp(s + i, delim, delimLen) == 0) {
                found = s + i;
                break;
            }
        }
        if (!found) break;

        size_t end = (size_t)(found - s);
        size_t partLen = end - start;
        char* part = (char*)malloc(partLen + 1);
        memcpy(part, s + start, partLen);
        part[partLen] = '\0';

        if (count == cap) { cap *= 2; arr = (str_elem*)realloc(arr, cap * sizeof(str_elem)); }
        arr[count].ptr = part;
        arr[count].len = partLen;
        count++;

        start = end + delimLen;
    }

    // Remainder
    size_t remLen = sLen - start;
    char* rem = (char*)malloc(remLen + 1);
    memcpy(rem, s + start, remLen);
    rem[remLen] = '\0';

    if (count == cap) { cap *= 2; arr = (str_elem*)realloc(arr, cap * sizeof(str_elem)); }
    arr[count].ptr = rem;
    arr[count].len = remLen;
    count++;

    out->ptr = arr;
    out->len = count;
    out->cap = cap;
}

lux_str_result lux_joinVec(const lux_str_vec_header* vec,
                                 const char* sep, size_t sepLen) {
    typedef struct { const char* ptr; size_t len; } str_elem;
    str_elem* arr = (str_elem*)vec->ptr;
    size_t count = vec->len;

    if (count == 0) {
        char* r = (char*)malloc(1);
        r[0] = '\0';
        return (lux_str_result){ r, 0 };
    }

    // Calculate total length
    size_t total = 0;
    for (size_t i = 0; i < count; i++) {
        total += arr[i].len;
        if (i > 0) total += sepLen;
    }

    char* buf = (char*)malloc(total + 1);
    size_t pos = 0;
    for (size_t i = 0; i < count; i++) {
        if (i > 0 && sepLen > 0) {
            memcpy(buf + pos, sep, sepLen);
            pos += sepLen;
        }
        memcpy(buf + pos, arr[i].ptr, arr[i].len);
        pos += arr[i].len;
    }
    buf[total] = '\0';

    return (lux_str_result){ buf, total };
}

void lux_lines(lux_str_vec_header* out, const char* s, size_t sLen) {
    typedef struct { const char* ptr; size_t len; } str_elem;

    out->ptr = NULL;
    out->len = 0;
    out->cap = 0;

    size_t cap = 8;
    str_elem* arr = (str_elem*)malloc(cap * sizeof(str_elem));
    size_t count = 0;

    size_t start = 0;
    for (size_t i = 0; i <= sLen; i++) {
        if (i == sLen || s[i] == '\n') {
            size_t end = i;
            if (end > start && s[end - 1] == '\r') end--;

            size_t partLen = end - start;
            char* part = (char*)malloc(partLen + 1);
            memcpy(part, s + start, partLen);
            part[partLen] = '\0';

            if (count == cap) { cap *= 2; arr = (str_elem*)realloc(arr, cap * sizeof(str_elem)); }
            arr[count].ptr = part;
            arr[count].len = partLen;
            count++;

            start = i + 1;
        }
    }

    out->ptr = arr;
    out->len = count;
    out->cap = cap;
}

void lux_chars(lux_str_vec_header* out, const char* s, size_t sLen) {
    out->ptr = NULL;
    out->len = 0;
    out->cap = 0;

    uint8_t* arr = (uint8_t*)malloc(sLen);
    for (size_t i = 0; i < sLen; i++) {
        arr[i] = (uint8_t)s[i];
    }

    out->ptr = arr;
    out->len = sLen;
    out->cap = sLen;
}

lux_str_result lux_fromCharsVec(const lux_str_vec_header* vec) {
    uint8_t* arr = (uint8_t*)vec->ptr;
    size_t count = vec->len;

    char* buf = (char*)malloc(count + 1);
    for (size_t i = 0; i < count; i++) {
        buf[i] = (char)arr[i];
    }
    buf[count] = '\0';

    return (lux_str_result){ buf, count };
}

void lux_toBytes(lux_str_vec_header* out, const char* s, size_t sLen) {
    out->ptr = NULL;
    out->len = 0;
    out->cap = 0;

    uint8_t* arr = (uint8_t*)malloc(sLen);
    memcpy(arr, s, sLen);

    out->ptr = arr;
    out->len = sLen;
    out->cap = sLen;
}

lux_str_result lux_fromBytesVec(const lux_str_vec_header* vec) {
    uint8_t* arr = (uint8_t*)vec->ptr;
    size_t count = vec->len;

    char* buf = (char*)malloc(count + 1);
    memcpy(buf, arr, count);
    buf[count] = '\0';

    return (lux_str_result){ buf, count };
}

// ── C FFI String Conversion ─────────────────────────────────────────────────

char* lux_cstr(const char* s, size_t sLen) {
    char* buf = (char*)malloc(sLen + 1);
    if (!buf) return NULL;
    memcpy(buf, s, sLen);
    buf[sLen] = '\0';
    return buf;
}

void* lux_allocString(size_t size) {
    return malloc(size);
}

lux_str_result lux_fromCStr(const char* cstr) {
    if (!cstr) return (lux_str_result){ "", 0 };
    size_t len = strlen(cstr);
    return (lux_str_result){ cstr, len };
}

lux_str_result lux_fromCStrCopy(const char* cstr) {
    if (!cstr) return (lux_str_result){ "", 0 };
    size_t len = strlen(cstr);
    char* buf = (char*)malloc(len + 1);
    if (!buf) return (lux_str_result){ "", 0 };
    memcpy(buf, cstr, len);
    buf[len] = '\0';
    return (lux_str_result){ buf, len };
}

lux_str_result lux_fromCStrLen(const char* cstr, size_t len) {
    if (!cstr) return (lux_str_result){ "", 0 };
    return (lux_str_result){ cstr, len };
}

void lux_freeStr(const char* ptr, size_t len) {
    (void)len;
    if (ptr)
        free((void*)ptr);
}

// ── Additional String Methods ───────────────────────────────────────────────

lux_str_result lux_trimChar(const char* s, size_t sLen, char ch) {
    size_t start = 0;
    while (start < sLen && s[start] == ch) start++;
    size_t end = sLen;
    while (end > start && s[end - 1] == ch) end--;
    size_t newLen = end - start;
    char* buf = (char*)malloc(newLen + 1);
    if (!buf) return (lux_str_result){ "", 0 };
    memcpy(buf, s + start, newLen);
    buf[newLen] = '\0';
    return (lux_str_result){ buf, newLen };
}

lux_str_result lux_capitalize(const char* s, size_t sLen) {
    if (sLen == 0) return (lux_str_result){ "", 0 };
    char* buf = (char*)malloc(sLen + 1);
    if (!buf) return (lux_str_result){ "", 0 };
    memcpy(buf, s, sLen);
    buf[0] = (char)toupper((unsigned char)buf[0]);
    buf[sLen] = '\0';
    return (lux_str_result){ buf, sLen };
}

lux_str_result lux_removePrefix(const char* s, size_t sLen,
                                const char* prefix, size_t prefixLen) {
    if (prefixLen <= sLen && memcmp(s, prefix, prefixLen) == 0) {
        size_t newLen = sLen - prefixLen;
        char* buf = (char*)malloc(newLen + 1);
        if (!buf) return (lux_str_result){ "", 0 };
        memcpy(buf, s + prefixLen, newLen);
        buf[newLen] = '\0';
        return (lux_str_result){ buf, newLen };
    }
    char* buf = (char*)malloc(sLen + 1);
    if (!buf) return (lux_str_result){ "", 0 };
    memcpy(buf, s, sLen);
    buf[sLen] = '\0';
    return (lux_str_result){ buf, sLen };
}

lux_str_result lux_removeSuffix(const char* s, size_t sLen,
                                const char* suffix, size_t suffixLen) {
    if (suffixLen <= sLen && memcmp(s + sLen - suffixLen, suffix, suffixLen) == 0) {
        size_t newLen = sLen - suffixLen;
        char* buf = (char*)malloc(newLen + 1);
        if (!buf) return (lux_str_result){ "", 0 };
        memcpy(buf, s, newLen);
        buf[newLen] = '\0';
        return (lux_str_result){ buf, newLen };
    }
    char* buf = (char*)malloc(sLen + 1);
    if (!buf) return (lux_str_result){ "", 0 };
    memcpy(buf, s, sLen);
    buf[sLen] = '\0';
    return (lux_str_result){ buf, sLen };
}

lux_str_result lux_strInsert(const char* s, size_t sLen,
                             size_t pos, const char* ins, size_t insLen) {
    if (pos > sLen) pos = sLen;
    size_t newLen = sLen + insLen;
    char* buf = (char*)malloc(newLen + 1);
    if (!buf) return (lux_str_result){ "", 0 };
    memcpy(buf, s, pos);
    memcpy(buf + pos, ins, insLen);
    memcpy(buf + pos + insLen, s + pos, sLen - pos);
    buf[newLen] = '\0';
    return (lux_str_result){ buf, newLen };
}

lux_str_result lux_strRemove(const char* s, size_t sLen,
                             size_t start, size_t count) {
    if (start >= sLen) return (lux_str_result){ s, sLen };
    if (start + count > sLen) count = sLen - start;
    size_t newLen = sLen - count;
    char* buf = (char*)malloc(newLen + 1);
    if (!buf) return (lux_str_result){ "", 0 };
    memcpy(buf, s, start);
    memcpy(buf + start, s + start + count, sLen - start - count);
    buf[newLen] = '\0';
    return (lux_str_result){ buf, newLen };
}

lux_str_result lux_concat(const char* a, size_t aLen,
                          const char* b, size_t bLen) {
    size_t newLen = aLen + bLen;
    char* buf = (char*)malloc(newLen + 1);
    if (!buf) return (lux_str_result){ "", 0 };
    memcpy(buf, a, aLen);
    memcpy(buf + aLen, b, bLen);
    buf[newLen] = '\0';
    return (lux_str_result){ buf, newLen };
}

int32_t lux_compareTo(const char* a, size_t aLen,
                      const char* b, size_t bLen) {
    size_t minLen = aLen < bLen ? aLen : bLen;
    int cmp = memcmp(a, b, minLen);
    if (cmp != 0) return cmp < 0 ? -1 : 1;
    if (aLen < bLen) return -1;
    if (aLen > bLen) return 1;
    return 0;
}

int lux_equalsIgnoreCase(const char* a, size_t aLen,
                         const char* b, size_t bLen) {
    if (aLen != bLen) return 0;
    for (size_t i = 0; i < aLen; i++) {
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i]))
            return 0;
    }
    return 1;
}

int lux_strIsNumeric(const char* s, size_t sLen) {
    if (sLen == 0) return 0;
    for (size_t i = 0; i < sLen; i++) {
        if (!isdigit((unsigned char)s[i])) return 0;
    }
    return 1;
}

int lux_strIsAlpha(const char* s, size_t sLen) {
    if (sLen == 0) return 0;
    for (size_t i = 0; i < sLen; i++) {
        if (!isalpha((unsigned char)s[i])) return 0;
    }
    return 1;
}

int lux_strIsAlphaNum(const char* s, size_t sLen) {
    if (sLen == 0) return 0;
    for (size_t i = 0; i < sLen; i++) {
        if (!isalnum((unsigned char)s[i])) return 0;
    }
    return 1;
}

int lux_strIsUpper(const char* s, size_t sLen) {
    if (sLen == 0) return 0;
    for (size_t i = 0; i < sLen; i++) {
        if (isalpha((unsigned char)s[i]) && !isupper((unsigned char)s[i]))
            return 0;
    }
    return 1;
}

int lux_strIsLower(const char* s, size_t sLen) {
    if (sLen == 0) return 0;
    for (size_t i = 0; i < sLen; i++) {
        if (isalpha((unsigned char)s[i]) && !islower((unsigned char)s[i]))
            return 0;
    }
    return 1;
}

int lux_strIsBlank(const char* s, size_t sLen) {
    for (size_t i = 0; i < sLen; i++) {
        if (!isspace((unsigned char)s[i])) return 0;
    }
    return 1;
}

int lux_strToBool(const char* s, size_t sLen) {
    if (sLen == 4 && memcmp(s, "true", 4) == 0) return 1;
    if (sLen == 1 && s[0] == '1') return 1;
    return 0;
}

void lux_words(lux_str_vec_header* out, const char* s, size_t sLen) {
    size_t cap = 8;
    lux_str_result* arr = (lux_str_result*)malloc(cap * sizeof(lux_str_result));
    size_t count = 0;

    size_t i = 0;
    while (i < sLen) {
        while (i < sLen && isspace((unsigned char)s[i])) i++;
        if (i >= sLen) break;
        size_t start = i;
        while (i < sLen && !isspace((unsigned char)s[i])) i++;
        size_t wordLen = i - start;
        char* word = (char*)malloc(wordLen + 1);
        memcpy(word, s + start, wordLen);
        word[wordLen] = '\0';

        if (count >= cap) {
            cap *= 2;
            arr = (lux_str_result*)realloc(arr, cap * sizeof(lux_str_result));
        }
        arr[count++] = (lux_str_result){ word, wordLen };
    }

    out->ptr = arr;
    out->len = count;
    out->cap = cap;
}
