#pragma once

#include <stdint.h>
#include <stddef.h>
#include <setjmp.h>

// Error struct layout — must match LLVM struct { {ptr,usize}, {ptr,usize}, i32, i32 }
typedef struct {
    const char* message_ptr;
    size_t      message_len;
    const char* file_ptr;
    size_t      file_len;
    int32_t     line;
    int32_t     column;
} lucis_error;

// Exception handler stack frame
typedef struct lucis_eh_frame {
    jmp_buf                  buf;
    struct lucis_eh_frame*  prev;
    lucis_error             error;
    int                      active;  // 1 if an error was thrown
} lucis_eh_frame;

// Push/pop exception handler frames
void  lucis_eh_push(lucis_eh_frame* frame);
void  lucis_eh_pop(void);

// Throw an error — longjmps to the nearest handler
void  lucis_eh_throw(const lucis_error* err);

// Get the current exception handler (for setjmp)
lucis_eh_frame* lucis_eh_current(void);

// Get jmp_buf pointer from a frame (for setjmp call)
void* lucis_eh_get_jmpbuf(lucis_eh_frame* frame);

// Get error struct pointer from a frame (for reading caught error)
lucis_error* lucis_eh_get_error(lucis_eh_frame* frame);

// Allocate a frame on the heap (platform-independent sizing)
lucis_eh_frame* lucis_eh_alloc(void);

// Free a heap-allocated frame
void lucis_eh_free(lucis_eh_frame* frame);
