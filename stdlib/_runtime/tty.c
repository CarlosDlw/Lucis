#include "tty.h"

#include <unistd.h>
#include <termios.h>

bool __lucis_tty_isatty(int fd)
{
    return isatty(fd) == 1;
}

int __lucis_tty_read(int fd, void* buffer, size_t size)
{
    return (int)read(fd, buffer, size);
}

int __lucis_tty_write(int fd, const void* buffer, size_t size)
{
    return (int)write(fd, buffer, size);
}

bool __lucis_tty_flush_input(int fd)
{
    return tcflush(fd, TCIFLUSH) == 0;
}

bool __lucis_tty_flush_output(int fd)
{
    return tcflush(fd, TCOFLUSH) == 0;
}

bool __lucis_tty_flush_io(int fd)
{
    return tcflush(fd, TCIOFLUSH) == 0;
}