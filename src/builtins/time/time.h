#ifndef LUCIS_TIME_H
#define LUCIS_TIME_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    const char* ptr;
    size_t len;
} lucis_time_str_result;

// now() -> uint64  (milliseconds since epoch)
uint64_t lucis_now(void);

// nowNanos() -> uint64
uint64_t lucis_nowNanos(void);

// nowMicros() -> uint64
uint64_t lucis_nowMicros(void);

// sleep(uint64)  — milliseconds
void lucis_sleep(uint64_t ms);

// sleepMicros(uint64)
void lucis_sleepMicros(uint64_t us);

// clock() -> uint64  — CPU clock ticks
uint64_t lucis_clock(void);

// year() -> int32
int32_t lucis_year(void);

// month() -> int32  (1-12)
int32_t lucis_month(void);

// day() -> int32  (1-31)
int32_t lucis_day(void);

// hour() -> int32  (0-23)
int32_t lucis_hour(void);

// minute() -> int32  (0-59)
int32_t lucis_minute(void);

// second() -> int32  (0-59)
int32_t lucis_second(void);

// weekday() -> int32  (0=Sunday, 6=Saturday)
int32_t lucis_weekday(void);

// timestamp() -> string  (ISO 8601)
lucis_time_str_result lucis_timestamp(void);

// elapsed(uint64) -> uint64  (ms elapsed since given timestamp)
uint64_t lucis_elapsed(uint64_t since);

// formatTime(uint64, string) -> string
lucis_time_str_result lucis_formatTime(uint64_t ms,
                                         const char* fmt_ptr, size_t fmt_len);

// parseTime(string, string) -> uint64
uint64_t lucis_parseTime(const char* str_ptr, size_t str_len,
                          const char* fmt_ptr, size_t fmt_len);

#endif // LUCIS_TIME_H
