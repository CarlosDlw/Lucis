#ifndef LUCIS_RUNTIME_TERMINAL_H
#define LUCIS_RUNTIME_TERMINAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#define LUCIS_TERM_CONTROL_CHARS 32

typedef struct
{
    bool raw;
    bool echo;
    bool canonical;

    uint32_t inputSpeed;
    uint32_t outputSpeed;

    uint8_t controlChars[LUCIS_TERM_CONTROL_CHARS];

} LucisTerminalState;

/* Terminal state */

bool __lucis_terminal_get_state(
    int fd,
    LucisTerminalState* state
);

bool __lucis_terminal_set_state(
    int fd,
    const LucisTerminalState* state
);

#ifdef __cplusplus
}
#endif

#endif