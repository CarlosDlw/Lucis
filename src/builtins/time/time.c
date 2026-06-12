#define _POSIX_C_SOURCE 199309L
#include "time/time.h"
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static struct tm get_local_tm(void) {
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    return tm;
}

uint64_t lucis_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

uint64_t lucis_nowNanos(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

uint64_t lucis_nowMicros(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

void lucis_sleep(uint64_t ms) {
    struct timespec req;
    req.tv_sec  = (time_t)(ms / 1000);
    req.tv_nsec = (long)((ms % 1000) * 1000000L);
    nanosleep(&req, NULL);
}

void lucis_sleepMicros(uint64_t us) {
    struct timespec req;
    req.tv_sec  = (time_t)(us / 1000000);
    req.tv_nsec = (long)((us % 1000000) * 1000L);
    nanosleep(&req, NULL);
}

uint64_t lucis_clock(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

int32_t lucis_year(void) {
    struct tm tm = get_local_tm();
    return (int32_t)(tm.tm_year + 1900);
}

int32_t lucis_month(void) {
    struct tm tm = get_local_tm();
    return (int32_t)(tm.tm_mon + 1);
}

int32_t lucis_day(void) {
    struct tm tm = get_local_tm();
    return (int32_t)tm.tm_mday;
}

int32_t lucis_hour(void) {
    struct tm tm = get_local_tm();
    return (int32_t)tm.tm_hour;
}

int32_t lucis_minute(void) {
    struct tm tm = get_local_tm();
    return (int32_t)tm.tm_min;
}

int32_t lucis_second(void) {
    struct tm tm = get_local_tm();
    return (int32_t)tm.tm_sec;
}

int32_t lucis_weekday(void) {
    struct tm tm = get_local_tm();
    return (int32_t)tm.tm_wday;
}

lucis_time_str_result lucis_timestamp(void) {
    // ISO 8601: "2026-03-23T14:30:45"
    struct tm tm = get_local_tm();
    char* buf = (char*)malloc(32);
    if (!buf) {
        lucis_time_str_result r = {"", 0};
        return r;
    }
    size_t n = strftime(buf, 32, "%Y-%m-%dT%H:%M:%S", &tm);
    lucis_time_str_result r = {buf, n};
    return r;
}

uint64_t lucis_elapsed(uint64_t since) {
    return lucis_now() - since;
}

lucis_time_str_result lucis_formatTime(uint64_t ms,
                                         const char* fmt_ptr, size_t fmt_len) {
    // Convert ms timestamp to time_t
    time_t t = (time_t)(ms / 1000);
    struct tm tm;
    localtime_r(&t, &tm);

    // Null-terminate the format string
    char* fmt = (char*)malloc(fmt_len + 1);
    if (!fmt) {
        lucis_time_str_result r = {"", 0};
        return r;
    }
    memcpy(fmt, fmt_ptr, fmt_len);
    fmt[fmt_len] = '\0';

    char* buf = (char*)malloc(256);
    if (!buf) {
        free(fmt);
        lucis_time_str_result r = {"", 0};
        return r;
    }
    size_t n = strftime(buf, 256, fmt, &tm);
    free(fmt);

    lucis_time_str_result r = {buf, n};
    return r;
}

uint64_t lucis_parseTime(const char* str_ptr, size_t str_len,
                          const char* fmt_ptr, size_t fmt_len) {
    // Null-terminate both strings
    char* str = (char*)malloc(str_len + 1);
    char* fmt = (char*)malloc(fmt_len + 1);
    if (!str || !fmt) {
        free(str);
        free(fmt);
        return 0;
    }
    memcpy(str, str_ptr, str_len);
    str[str_len] = '\0';
    memcpy(fmt, fmt_ptr, fmt_len);
    fmt[fmt_len] = '\0';

    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    tm.tm_isdst = -1;

    char* end = strptime(str, fmt, &tm);
    free(str);
    free(fmt);

    if (!end) return 0;

    time_t t = mktime(&tm);
    if (t == (time_t)-1) return 0;

    return (uint64_t)t * 1000ULL;
}
