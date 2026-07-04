#ifndef LUCIS_RUNTIME_TTY_H
#define LUCIS_RUNTIME_TTY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Standard descriptors */

#define LUCIS_STDIN   0
#define LUCIS_STDOUT  1
#define LUCIS_STDERR  2

/* Query */

bool __lucis_tty_isatty(int fd);

/* Input / Output */

int __lucis_tty_read(int fd, void* buffer, size_t size);

int __lucis_tty_write(int fd, const void* buffer, size_t size);

/* Flush */

bool __lucis_tty_flush_input(int fd);

bool __lucis_tty_flush_output(int fd);

bool __lucis_tty_flush_io(int fd);

#ifdef __cplusplus
}
#endif

#endif /* LUCIS_RUNTIME_TTY_H */