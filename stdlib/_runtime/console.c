#include "console.h"

#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>

static bool ansi_write(int fd, const char* seq, size_t len)
{
    return write(fd, seq, len) == (ssize_t)len;
}

bool __lucis_console_get_size(
    int fd,
    LucisConsoleSize* size)
{
    if (!size)
        return false;

    struct winsize ws;

    if (ioctl(fd, TIOCGWINSZ, &ws) != 0)
        return false;

    size->rows = ws.ws_row;
    size->cols = ws.ws_col;

    return true;
}

bool __lucis_console_clear(int fd)
{
    return ansi_write(fd, "\x1b[2J\x1b[H", 7);
}

bool __lucis_console_clear_line(int fd)
{
    return ansi_write(fd, "\x1b[2K\r", 5);
}

bool __lucis_console_show_cursor(int fd)
{
    return ansi_write(fd, "\x1b[?25h", 6);
}

bool __lucis_console_hide_cursor(int fd)
{
    return ansi_write(fd, "\x1b[?25l", 6);
}

bool __lucis_console_move_cursor(
    int fd,
    int32_t row,
    int32_t col)
{
    char buffer[32];

    // ANSI escape sequences are 1-indexed; add 1 for POSITIONAL CURSOR
    int len = snprintf(
        buffer,
        sizeof(buffer),
        "\x1b[%d;%dH",
        row + 1,
        col + 1
    );

    return ansi_write(fd, buffer, (size_t)len);
}

bool __lucis_console_move_up(
    int fd,
    int32_t amount)
{
    char buffer[16];

    int len = snprintf(
        buffer,
        sizeof(buffer),
        "\x1b[%dA",
        amount
    );

    return ansi_write(fd, buffer, (size_t)len);
}

bool __lucis_console_move_down(
    int fd,
    int32_t amount)
{
    char buffer[16];

    int len = snprintf(
        buffer,
        sizeof(buffer),
        "\x1b[%dB",
        amount
    );

    return ansi_write(fd, buffer, (size_t)len);
}

bool __lucis_console_move_left(
    int fd,
    int32_t amount)
{
    char buffer[16];

    int len = snprintf(
        buffer,
        sizeof(buffer),
        "\x1b[%dD",
        amount
    );

    return ansi_write(fd, buffer, (size_t)len);
}

bool __lucis_console_move_right(
    int fd,
    int32_t amount)
{
    char buffer[16];

    int len = snprintf(
        buffer,
        sizeof(buffer),
        "\x1b[%dC",
        amount
    );

    return ansi_write(fd, buffer, (size_t)len);
}

/* ── Style / attribute helpers ─────────────────────────────────── */

bool __lucis_console_sgr(int fd, const char* seq)
{
    return ansi_write(fd, seq, strlen(seq));
}

bool __lucis_console_set_style(int fd, int32_t style, bool on)
{
    if (on) {
        if (style & LUCIS_STYLE_BOLD)      dprintf(fd, "\x1b[1m");
        if (style & LUCIS_STYLE_DIM)       dprintf(fd, "\x1b[2m");
        if (style & LUCIS_STYLE_ITALIC)    dprintf(fd, "\x1b[3m");
        if (style & LUCIS_STYLE_UNDERLINE) dprintf(fd, "\x1b[4m");
        if (style & LUCIS_STYLE_REVERSE)   dprintf(fd, "\x1b[7m");
        if (style & LUCIS_STYLE_STRIKE)    dprintf(fd, "\x1b[9m");
    } else {
        if (style & LUCIS_STYLE_BOLD)      dprintf(fd, "\x1b[22m");
        if (style & LUCIS_STYLE_DIM)       dprintf(fd, "\x1b[22m");
        if (style & LUCIS_STYLE_ITALIC)    dprintf(fd, "\x1b[23m");
        if (style & LUCIS_STYLE_UNDERLINE) dprintf(fd, "\x1b[24m");
        if (style & LUCIS_STYLE_REVERSE)   dprintf(fd, "\x1b[27m");
        if (style & LUCIS_STYLE_STRIKE)    dprintf(fd, "\x1b[29m");
    }
    return true;
}

bool __lucis_console_set_fg(int fd, int32_t color)
{
    return dprintf(fd, "\x1b[%dm", 30 + color) > 0;
}

bool __lucis_console_set_bg(int fd, int32_t color)
{
    return dprintf(fd, "\x1b[%dm", 40 + color) > 0;
}

bool __lucis_console_set_fg_256(int fd, int32_t color)
{
    return dprintf(fd, "\x1b[38;5;%dm", color) > 0;
}

bool __lucis_console_set_bg_256(int fd, int32_t color)
{
    return dprintf(fd, "\x1b[48;5;%dm", color) > 0;
}

bool __lucis_console_set_fg_rgb(int fd, int32_t r, int32_t g, int32_t b)
{
    return dprintf(fd, "\x1b[38;2;%d;%d;%dm", r, g, b) > 0;
}

bool __lucis_console_set_bg_rgb(int fd, int32_t r, int32_t g, int32_t b)
{
    return dprintf(fd, "\x1b[48;2;%d;%d;%dm", r, g, b) > 0;
}

bool __lucis_console_reset_attrs(int fd)
{
    return ansi_write(fd, "\x1b[0m", 4);
}

/* ── Alternate screen buffer ──────────────────────────────────── */

bool __lucis_console_enter_alt_screen(int fd)
{
    return ansi_write(fd, "\x1b[?1049h", 8);
}

bool __lucis_console_leave_alt_screen(int fd)
{
    return ansi_write(fd, "\x1b[?1049l", 8);
}

/* ── Cursor save / restore ────────────────────────────────────── */

bool __lucis_console_save_cursor(int fd)
{
    return ansi_write(fd, "\x1b7", 2);
}

bool __lucis_console_restore_cursor(int fd)
{
    return ansi_write(fd, "\x1b8", 2);
}

/* ── Insert / delete lines ────────────────────────────────────── */

bool __lucis_console_insert_lines(int fd, int32_t n)
{
    return dprintf(fd, "\x1b[%dL", n) > 0;
}

bool __lucis_console_delete_lines(int fd, int32_t n)
{
    return dprintf(fd, "\x1b[%dM", n) > 0;
}

/* ── Scroll region ────────────────────────────────────────────── */

bool __lucis_console_set_scroll_region(int fd, int32_t top, int32_t bottom)
{
    return dprintf(fd, "\x1b[%d;%dr", top + 1, bottom + 1) > 0;
}

bool __lucis_console_reset_scroll_region(int fd)
{
    return ansi_write(fd, "\x1b[r", 3);
}

/* ── Mouse tracking ───────────────────────────────────────────── */

bool __lucis_console_enable_mouse(int fd, int32_t mode)
{
    return dprintf(fd, "\x1b[?%dh", mode) > 0;
}

bool __lucis_console_disable_mouse(int fd)
{
    return dprintf(fd, "\x1b[?1000l\x1b[?1002l\x1b[?1003l") > 0;
}

bool __lucis_console_enable_sgr_mouse(int fd)
{
    return dprintf(fd, "\x1b[?1006h") > 0;
}

bool __lucis_console_disable_sgr_mouse(int fd)
{
    return dprintf(fd, "\x1b[?1006l") > 0;
}