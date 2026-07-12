#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// std::log builtins — signed
void lucis_println_i1(int val);
void lucis_println_i8(signed char val);
void lucis_println_i16(short val);
void lucis_println_i32(int val);
void lucis_println_i64(long long val);
void lucis_println_i128(__int128 val);
void lucis_println_f32(float val);
void lucis_println_f64(double val);
void lucis_println_bool(int val);
void lucis_println_char(char val);
void lucis_println_str(const char* data, unsigned long len);
void lucis_println_cstr(const char* str);  // C string (null-terminated)
void lucis_println_ptr(const void* val);   // pointer

void lucis_print_i1(int val);
void lucis_print_i8(signed char val);
void lucis_print_i16(short val);
void lucis_print_i32(int val);
void lucis_print_i64(long long val);
void lucis_print_i128(__int128 val);
void lucis_print_f32(float val);
void lucis_print_f64(double val);
void lucis_print_bool(int val);
void lucis_print_char(char val);
void lucis_print_str(const char* data, unsigned long len);
void lucis_print_cstr(const char* str);  // C string (null-terminated)
void lucis_print_ptr(const void* val);   // pointer

// std::log builtins — unsigned
void lucis_println_u1(int val);
void lucis_println_u8(unsigned char val);
void lucis_println_u16(unsigned short val);
void lucis_println_u32(unsigned int val);
void lucis_println_u64(unsigned long long val);
void lucis_println_u128(unsigned __int128 val);

void lucis_print_u1(int val);
void lucis_print_u8(unsigned char val);
void lucis_print_u16(unsigned short val);
void lucis_print_u32(unsigned int val);
void lucis_print_u64(unsigned long long val);
void lucis_print_u128(unsigned __int128 val);

// std::log builtins — eprintln (stderr + newline) — signed
void lucis_eprintln_i1(int val);
void lucis_eprintln_i8(signed char val);
void lucis_eprintln_i16(short val);
void lucis_eprintln_i32(int val);
void lucis_eprintln_i64(long long val);
void lucis_eprintln_i128(__int128 val);
void lucis_eprintln_f32(float val);
void lucis_eprintln_f64(double val);
void lucis_eprintln_bool(int val);
void lucis_eprintln_char(char val);
void lucis_eprintln_str(const char* data, unsigned long len);
void lucis_eprintln_cstr(const char* str);  // C string (null-terminated)

// std::log builtins — eprint (stderr, no newline) — signed
void lucis_eprint_i1(int val);
void lucis_eprint_i8(signed char val);
void lucis_eprint_i16(short val);
void lucis_eprint_i32(int val);
void lucis_eprint_i64(long long val);
void lucis_eprint_i128(__int128 val);
void lucis_eprint_f32(float val);
void lucis_eprint_f64(double val);
void lucis_eprint_bool(int val);
void lucis_eprint_char(char val);
void lucis_eprint_str(const char* data, unsigned long len);
void lucis_eprint_cstr(const char* str);  // C string (null-terminated)

// std::log builtins — eprintln (stderr + newline) — unsigned
void lucis_eprintln_u1(int val);
void lucis_eprintln_u8(unsigned char val);
void lucis_eprintln_u16(unsigned short val);
void lucis_eprintln_u32(unsigned int val);
void lucis_eprintln_u64(unsigned long long val);
void lucis_eprintln_u128(unsigned __int128 val);

// std::log builtins — eprint (stderr, no newline) — unsigned
void lucis_eprint_u1(int val);
void lucis_eprint_u8(unsigned char val);
void lucis_eprint_u16(unsigned short val);
void lucis_eprint_u32(unsigned int val);
void lucis_eprint_u64(unsigned long long val);
void lucis_eprint_u128(unsigned __int128 val);

// std::log builtins — dbg (stderr debug output, returns value)
void lucis_dbg_i1(const char* file, unsigned long flen, int line, int val);
void lucis_dbg_i8(const char* file, unsigned long flen, int line, signed char val);
void lucis_dbg_i16(const char* file, unsigned long flen, int line, short val);
void lucis_dbg_i32(const char* file, unsigned long flen, int line, int val);
void lucis_dbg_i64(const char* file, unsigned long flen, int line, long long val);
void lucis_dbg_i128(const char* file, unsigned long flen, int line, __int128 val);
void lucis_dbg_f32(const char* file, unsigned long flen, int line, float val);
void lucis_dbg_f64(const char* file, unsigned long flen, int line, double val);
void lucis_dbg_bool(const char* file, unsigned long flen, int line, int val);
void lucis_dbg_char(const char* file, unsigned long flen, int line, char val);
void lucis_dbg_str(const char* file, unsigned long flen, int line,
                    const char* data, unsigned long len);
void lucis_dbg_cstr(const char* file, unsigned long flen, int line,
                    const char* str);  // C string (null-terminated)

void lucis_dbg_u1(const char* file, unsigned long flen, int line, int val);
void lucis_dbg_u8(const char* file, unsigned long flen, int line, unsigned char val);
void lucis_dbg_u16(const char* file, unsigned long flen, int line, unsigned short val);
void lucis_dbg_u32(const char* file, unsigned long flen, int line, unsigned int val);
void lucis_dbg_u64(const char* file, unsigned long flen, int line, unsigned long long val);
void lucis_dbg_u128(const char* file, unsigned long flen, int line, unsigned __int128 val);

// std::log builtins — sprintf (format string with {} placeholders)
typedef struct {
    const char*   ptr;
    unsigned long len;
} lucis_str_slice;

lucis_str_slice lucis_sprintf(const char* fmt, unsigned long fmtLen,
                                const lucis_str_slice* args, unsigned long argCount);

// Shell command execution — runs cmd via popen, returns stdout
lucis_str_slice lucis_system(const char* cmd, unsigned long cmdLen);

#ifdef __cplusplus
}
#endif
