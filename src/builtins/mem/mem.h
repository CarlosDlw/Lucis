#ifndef LUCIS_MEM_H
#define LUCIS_MEM_H

#include <stddef.h>
#include <stdint.h>

// alloc(usize) -> *void
void* lucis_alloc(size_t size);

// allocZeroed(usize) -> *void
void* lucis_allocZeroed(size_t size);

// realloc(*void, usize) -> *void
void* lucis_realloc(void* ptr, size_t size);

// free(*void)
void lucis_free(void* ptr);

// copy(*void, *void, usize)  — dst, src, n
void lucis_copy(void* dst, const void* src, size_t n);

// move(*void, *void, usize)  — dst, src, n (overlap-safe)
void lucis_move(void* dst, const void* src, size_t n);

// set(*void, uint8, usize)
void lucis_set(void* ptr, uint8_t value, size_t n);

// zero(*void, usize)
void lucis_zero(void* ptr, size_t n);

// compare(*void, *void, usize) -> int32
int32_t lucis_compare(const void* a, const void* b, size_t n);

#endif // LUCIS_MEM_H
