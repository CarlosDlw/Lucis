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

/* Terminal mode helpers */

bool __lucis_terminal_enable_raw(int fd)
{
    struct termios t;

    if (tcgetattr(fd, &t) != 0)
        return false;

    cfmakeraw(&t);

    return tcsetattr(fd, TCSAFLUSH, &t) == 0;
}

bool __lucis_terminal_disable_raw(int fd)
{
    struct termios t;

    if (tcgetattr(fd, &t) != 0)
        return false;

    // Reset to cooked mode: enable canonical mode and echo
    t.c_lflag |= (ICANON | ECHO);

    return tcsetattr(fd, TCSAFLUSH, &t) == 0;
}

bool __lucis_terminal_enable_echo(int fd)
{
    struct termios t;

    if (tcgetattr(fd, &t) != 0)
        return false;

    t.c_lflag |= ECHO;

    return tcsetattr(fd, TCSAFLUSH, &t) == 0;
}

bool __lucis_terminal_disable_echo(int fd)
{
    struct termios t;

    if (tcgetattr(fd, &t) != 0)
        return false;

    t.c_lflag &= ~ECHO;

    return tcsetattr(fd, TCSAFLUSH, &t) == 0;
}

bool __lucis_terminal_enable_canonical(int fd)
{
    struct termios t;

    if (tcgetattr(fd, &t) != 0)
        return false;

    t.c_lflag |= ICANON;

    return tcsetattr(fd, TCSAFLUSH, &t) == 0;
}

bool __lucis_terminal_disable_canonical(int fd)
{
    struct termios t;

    if (tcgetattr(fd, &t) != 0)
        return false;

    t.c_lflag &= ~ICANON;

    return tcsetattr(fd, TCSAFLUSH, &t) == 0;
}

/* Terminal attribute control */

bool __lucis_terminal_set_input_speed(int fd, uint32_t speed)
{
    struct termios t;

    if (tcgetattr(fd, &t) != 0)
        return false;

    cfsetispeed(&t, (speed_t)speed);

    return tcsetattr(fd, TCSAFLUSH, &t) == 0;
}

bool __lucis_terminal_set_output_speed(int fd, uint32_t speed)
{
    struct termios t;

    if (tcgetattr(fd, &t) != 0)
        return false;

    cfsetospeed(&t, (speed_t)speed);

    return tcsetattr(fd, TCSAFLUSH, &t) == 0;
}

bool __lucis_terminal_get_control_char(int fd, int idx, uint8_t* out)
{
    if (!out || idx < 0 || idx >= NCCS)
        return false;

    struct termios t;

    if (tcgetattr(fd, &t) != 0)
        return false;

    *out = t.c_cc[idx];
    return true;
}

bool __lucis_terminal_set_control_char(int fd, int idx, uint8_t val)
{
    if (idx < 0 || idx >= NCCS)
        return false;

    struct termios t;

    if (tcgetattr(fd, &t) != 0)
        return false;

    t.c_cc[idx] = val;
    return tcsetattr(fd, TCSAFLUSH, &t) == 0;
}

/* Special control characters */

#define SET_CC(name, idx) \
bool __lucis_terminal_set_##name(int fd, uint8_t c) \
{ \
    struct termios t; \
    if (tcgetattr(fd, &t) != 0) return false; \
    t.c_cc[idx] = c; \
    return tcsetattr(fd, TCSAFLUSH, &t) == 0; \
}

SET_CC(intr, VINTR)
SET_CC(quit, VQUIT)
SET_CC(susp, VSUSP)
SET_CC(eof, VEOF)
SET_CC(erase, VERASE)
SET_CC(kill, VKILL)
SET_CC(eol, VEOL)
SET_CC(eol2, VEOL2)
/* VSWTCH and VDSUSP are BSD-specific, not available on all platforms */
#if defined(VSWTCH)
SET_CC(swtch, VSWTCH)
#endif
SET_CC(start, VSTART)
SET_CC(stop, VSTOP)
SET_CC(susp_char, VSUSP)
#if defined(VDSUSP)
SET_CC(dsusp, VDSUSP)
#endif
SET_CC(reprint, VREPRINT)
SET_CC(werase, VWERASE)
SET_CC(lnext, VLNEXT)
SET_CC(discard, VDISCARD)

#undef SET_CC

/* Flow control & blocking modes */

bool __lucis_terminal_enable_flow_control(int fd)
{
    struct termios t;

    if (tcgetattr(fd, &t) != 0)
        return false;

    t.c_iflag |= (IXON | IXOFF);
    return tcsetattr(fd, TCSAFLUSH, &t) == 0;
}

bool __lucis_terminal_disable_flow_control(int fd)
{
    struct termios t;

    if (tcgetattr(fd, &t) != 0)
        return false;

    t.c_iflag &= ~(IXON | IXOFF);
    return tcsetattr(fd, TCSAFLUSH, &t) == 0;
}

bool __lucis_terminal_set_nonblocking(int fd)
{
    struct termios t;

    if (tcgetattr(fd, &t) != 0)
        return false;

    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;
    return tcsetattr(fd, TCSAFLUSH, &t) == 0;
}

bool __lucis_terminal_set_blocking(int fd, int min, int time)
{
    struct termios t;

    if (tcgetattr(fd, &t) != 0)
        return false;

    t.c_cc[VMIN] = (cc_t)min;
    t.c_cc[VTIME] = (cc_t)time;
    return tcsetattr(fd, TCSAFLUSH, &t) == 0;
}

/* Signal handling */

bool __lucis_terminal_enable_signals(int fd)
{
    struct termios t;

    if (tcgetattr(fd, &t) != 0)
        return false;

    t.c_lflag |= ISIG;
    return tcsetattr(fd, TCSAFLUSH, &t) == 0;
}

bool __lucis_terminal_disable_signals(int fd)
{
    struct termios t;

    if (tcgetattr(fd, &t) != 0)
        return false;

    t.c_lflag &= ~ISIG;
    return tcsetattr(fd, TCSAFLUSH, &t) == 0;
}