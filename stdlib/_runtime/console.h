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

/* Style attributes (bitmask-compatible) */

#define LUCIS_STYLE_BOLD       (1 << 0)
#define LUCIS_STYLE_DIM        (1 << 1)
#define LUCIS_STYLE_ITALIC     (1 << 2)
#define LUCIS_STYLE_UNDERLINE  (1 << 3)
#define LUCIS_STYLE_REVERSE    (1 << 4)
#define LUCIS_STYLE_STRIKE     (1 << 5)

/* Attribute control (SGR) */

bool __lucis_console_sgr(int fd, const char* seq);          /* raw SGR sequence */
bool __lucis_console_set_style(int fd, int32_t style, bool on);
bool __lucis_console_set_fg(int fd, int32_t color);
bool __lucis_console_set_bg(int fd, int32_t color);
bool __lucis_console_set_fg_256(int fd, int32_t color);
bool __lucis_console_set_bg_256(int fd, int32_t color);
bool __lucis_console_set_fg_rgb(int fd, int32_t r, int32_t g, int32_t b);
bool __lucis_console_set_bg_rgb(int fd, int32_t r, int32_t g, int32_t b);
bool __lucis_console_reset_attrs(int fd);

/* Alternate screen buffer */

bool __lucis_console_enter_alt_screen(int fd);
bool __lucis_console_leave_alt_screen(int fd);

/* Cursor save / restore */

bool __lucis_console_save_cursor(int fd);
bool __lucis_console_restore_cursor(int fd);

/* Insert / delete lines */

bool __lucis_console_insert_lines(int fd, int32_t n);
bool __lucis_console_delete_lines(int fd, int32_t n);

/* Scroll region */

bool __lucis_console_set_scroll_region(int fd, int32_t top, int32_t bottom);
bool __lucis_console_reset_scroll_region(int fd);

/* Mouse tracking */

bool __lucis_console_enable_mouse(int fd, int32_t mode);  /* 1000=click, 1002=drag, 1003=motion */
bool __lucis_console_disable_mouse(int fd);
bool __lucis_console_enable_sgr_mouse(int fd);
bool __lucis_console_disable_sgr_mouse(int fd);

#ifdef __cplusplus
}
#endif

#endif