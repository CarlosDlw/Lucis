#include "terminal.h"

#include <string.h>
#include <termios.h>
#include <unistd.h>

static bool __lucis_is_raw(const struct termios* t)
{
    struct termios raw = *t;

    cfmakeraw(&raw);

    return memcmp(&raw, t, sizeof(struct termios)) == 0;
}

bool __lucis_terminal_get_state(
    int fd,
    LucisTerminalState* state)
{
    if (!state)
        return false;

    struct termios t;

    if (tcgetattr(fd, &t) != 0)
        return false;

    state->raw = __lucis_is_raw(&t);

    state->echo = (t.c_lflag & ECHO) != 0;

    state->canonical = (t.c_lflag & ICANON) != 0;

    state->inputSpeed = (uint32_t)cfgetispeed(&t);

    state->outputSpeed = (uint32_t)cfgetospeed(&t);

    memset(state->controlChars, 0, LUCIS_TERM_CONTROL_CHARS);

    for (int i = 0; i < NCCS && i < LUCIS_TERM_CONTROL_CHARS; ++i)
        state->controlChars[i] = t.c_cc[i];

    return true;
}

bool __lucis_terminal_set_state(
    int fd,
    const LucisTerminalState* state)
{
    if (!state)
        return false;

    struct termios t;

    if (tcgetattr(fd, &t) != 0)
        return false;

    if (state->raw)
        cfmakeraw(&t);

    if (state->echo)
        t.c_lflag |= ECHO;
    else
        t.c_lflag &= ~ECHO;

    if (state->canonical)
        t.c_lflag |= ICANON;
    else
        t.c_lflag &= ~ICANON;

    cfsetispeed(&t, (speed_t)state->inputSpeed);

    cfsetospeed(&t, (speed_t)state->outputSpeed);

    for (int i = 0; i < NCCS && i < LUCIS_TERM_CONTROL_CHARS; ++i)
        t.c_cc[i] = state->controlChars[i];

    return tcsetattr(fd, TCSAFLUSH, &t) == 0;
}