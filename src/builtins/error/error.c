#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Thread-local exception handler stack
static __thread lucis_eh_frame* eh_stack = NULL;

void lucis_eh_push(lucis_eh_frame* frame) {
    frame->prev   = eh_stack;
    frame->active = 0;
    memset(&frame->error, 0, sizeof(lucis_error));
    eh_stack      = frame;
}

void lucis_eh_pop(void) {
    if (eh_stack) {
        eh_stack = eh_stack->prev;
    }
}

void lucis_eh_throw(const lucis_error* err) {
    if (!eh_stack) {
        // No handler — print and abort
        fprintf(stderr, "unhandled error: %.*s\n  at %.*s:%d:%d\n",
                (int)err->message_len, err->message_ptr,
                (int)err->file_len,    err->file_ptr,
                err->line, err->column);
        exit(1);
    }

    eh_stack->error  = *err;
    eh_stack->active = 1;
    longjmp(eh_stack->buf, 1);
}

lucis_eh_frame* lucis_eh_current(void) {
    return eh_stack;
}

void* lucis_eh_get_jmpbuf(lucis_eh_frame* frame) {
    return (void*)frame->buf;
}

lucis_error* lucis_eh_get_error(lucis_eh_frame* frame) {
    return &frame->error;
}

lucis_eh_frame* lucis_eh_alloc(void) {
    return (lucis_eh_frame*)calloc(1, sizeof(lucis_eh_frame));
}

void lucis_eh_free(lucis_eh_frame* frame) {
    free(frame);
}
