# `lucis::io` — Low-Level File I/O

The `io` namespace provides **unbuffered file descriptor I/O** — `open`, `read`, `write`, `close`, and `lseek`. These match POSIX semantics and are backed by cross-platform C wrappers (`#ifdef _WIN32`).

> **Always available** — no `use lucis::io` declaration needed.

All functions return `-1` on error (except `lseek` which returns `-1i64`).

---

## `lucis::io::open(path, flags, mode) -> int32`

```lucis
int32 fd = lucis::io::open(c"file.txt", 0, 0);  // O_RDONLY
```

Opens a file and returns a file descriptor. Flags follow POSIX conventions:

| Flag | Value | Description |
|------|-------|-------------|
| `O_RDONLY` | `0` | Read only |
| `O_WRONLY` | `1` | Write only |
| `O_RDWR` | `2` | Read and write |
| `O_CREAT` | `64` | Create file if it doesn't exist |
| `O_TRUNC` | `512` | Truncate to zero length |
| `O_APPEND` | `1024` | Append mode |

| Param | Type | Description |
|-------|------|-------------|
| `path` | `*char` | Null-terminated file path (use `c"..."`) |
| `flags` | `int32` | Bitwise OR of POSIX flags |
| `mode` | `int32` | Permission bits (e.g. `420` for `0644`) |

> **Note**: Flag values are platform-dependent. The table above matches Linux/x86-64. On Windows, the C wrapper maps to `_open` with `_O_*` constants.

---

## `lucis::io::read(fd, buf, count) -> int32`

```lucis
int32 n = lucis::io::read(fd, &buf, 256);
```

Reads up to `count` bytes from file descriptor `fd` into `buf`. Returns the number of bytes actually read, `0` at EOF, or `-1` on error.

| Param | Type | Description |
|-------|------|-------------|
| `fd` | `int32` | File descriptor |
| `buf` | `*_any` | Pointer to buffer |
| `count` | `int32` | Maximum bytes to read |

---

## `lucis::io::write(fd, buf, count) -> int32`

```lucis
int32 n = lucis::io::write(fd, &buf, 256);
```

Writes up to `count` bytes from `buf` to file descriptor `fd`. Returns the number of bytes written, or `-1` on error.

---

## `lucis::io::close(fd) -> int32`

```lucis
int32 res = lucis::io::close(fd);
```

Closes a file descriptor. Returns `0` on success, `-1` on error.

---

## `lucis::io::lseek(fd, offset, whence) -> int64`

```lucis
int64 pos = lucis::io::lseek(fd, 0, 0);  // SEEK_SET → start
int64 end = lucis::io::lseek(fd, 0, 2);  // SEEK_END   → end
```

Repositions the file offset of an open file descriptor. Returns the resulting offset, or `-1i64` on error.

---

## File Management

### `lucis::io::fsync(fd) -> int32`

```lucis
lucis::io::fsync(fd);
```

Synchronises a file's in-core state (written data) with the storage device. Returns `0` on success, `-1` on error.

### `lucis::io::unlink(path) -> int32`

```lucis
lucis::io::unlink(c"file.txt");
```

Deletes a file from the filesystem. Returns `0` on success, `-1` on error.

### `lucis::io::file_size(fd) -> int64`

```lucis
int64 sz = lucis::io::file_size(fd);
```

Returns the size of an open file in bytes via `fstat`/`_fstat`. Returns `-1i64` on error.

| Whence | Value | Description |
|--------|-------|-------------|
| `SEEK_SET` | `0` | Relative to beginning of file |
| `SEEK_CUR` | `1` | Relative to current position |
| `SEEK_END` | `2` | Relative to end of file |

---

## Example: Copy File

```lucis
fn copy_file(*char src, *char dst) int32 {
    int32 sfd = lucis::io::open(src, 0, 0);
    if sfd < 0 { ret -1; }

    int32 dfd = lucis::io::open(dst, 65, 420);  // O_WRONLY | O_CREAT
    if dfd < 0 {
        lucis::io::close(sfd);
        ret -1;
    }

    [4096]uint8 buf;
    loop {
        int32 n = lucis::io::read(sfd, &buf[0], 4096);
        if n <= 0 { break; }
        lucis::io::write(dfd, &buf[0], n);
    }

    lucis::io::close(sfd);
    lucis::io::close(dfd);
    ret 0;
}
```
