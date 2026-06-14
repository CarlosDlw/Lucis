#include "io_file.h"

#ifdef _WIN32
  #include <io.h>
  #include <fcntl.h>
  #include <sys/stat.h>
  #include <sys/types.h>
  #define HIDDEN_OPEN    _open
  #define HIDDEN_READ    _read
  #define HIDDEN_WRITE   _write
  #define HIDDEN_CLOSE   _close
  #define HIDDEN_LSEEK   _lseeki64
  #define HIDDEN_FSTAT   _fstat
  #define HIDDEN_FSYNC   _commit
  #define HIDDEN_UNLINK  _unlink
  #define HIDDEN_STAT    struct _stat
  typedef int hidden_ssize_t;
#else
  #include <unistd.h>
  #include <fcntl.h>
  #include <sys/stat.h>
  #include <sys/types.h>
  #define HIDDEN_OPEN    open
  #define HIDDEN_READ    read
  #define HIDDEN_WRITE   write
  #define HIDDEN_CLOSE   close
  #define HIDDEN_LSEEK   lseek
  #define HIDDEN_FSTAT   fstat
  #define HIDDEN_FSYNC   fsync
  #define HIDDEN_UNLINK  unlink
  #define HIDDEN_STAT    struct stat
  typedef ssize_t hidden_ssize_t;
#endif

int lucis_open(const char* path, int flags, int mode) {
    return HIDDEN_OPEN(path, flags, mode);
}

int lucis_read(int fd, void* buf, int count) {
    hidden_ssize_t ret = HIDDEN_READ(fd, buf, (size_t)count);
    return (int)ret;
}

int lucis_write(int fd, const void* buf, int count) {
    hidden_ssize_t ret = HIDDEN_WRITE(fd, buf, (size_t)count);
    return (int)ret;
}

int lucis_close(int fd) {
    return HIDDEN_CLOSE(fd);
}

int64_t lucis_lseek(int fd, int64_t offset, int whence) {
    return (int64_t)HIDDEN_LSEEK(fd, offset, whence);
}

int lucis_fsync(int fd) {
    return HIDDEN_FSYNC(fd);
}

int lucis_unlink(const char* path) {
    return HIDDEN_UNLINK(path);
}

int64_t lucis_fileSize_fd(int fd) {
    HIDDEN_STAT st;
    if (HIDDEN_FSTAT(fd, &st) != 0) return -1;
    return (int64_t)st.st_size;
}
