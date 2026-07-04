#include "console.h"

#include <sys/ioctl.h>
#include <unistd.h>

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

    int len = snprintf(
        buffer,
        sizeof(buffer),
        "\x1b[%d;%dH",
        row,
        col
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