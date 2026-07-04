#ifndef LUCIS_RUNTIME_CONSOLE_H
#define LUCIS_RUNTIME_CONSOLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    int32_t rows;
    int32_t cols;
} LucisConsoleSize;

/* Window */

bool __lucis_console_get_size(
    int fd,
    LucisConsoleSize* size
);

/* Screen */

bool __lucis_console_clear(
    int fd
);

bool __lucis_console_clear_line(
    int fd
);

/* Cursor */

bool __lucis_console_show_cursor(
    int fd
);

bool __lucis_console_hide_cursor(
    int fd
);

bool __lucis_console_move_cursor(
    int fd,
    int32_t row,
    int32_t col
);

bool __lucis_console_move_up(
    int fd,
    int32_t amount
);

bool __lucis_console_move_down(
    int fd,
    int32_t amount
);

bool __lucis_console_move_left(
    int fd,
    int32_t amount
);

bool __lucis_console_move_right(
    int fd,
    int32_t amount
);

#ifdef __cplusplus
}
#endif

#endif