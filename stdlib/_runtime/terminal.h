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

/* Terminal mode helpers */

bool __lucis_terminal_enable_raw(int fd);
bool __lucis_terminal_disable_raw(int fd);
bool __lucis_terminal_enable_echo(int fd);
bool __lucis_terminal_disable_echo(int fd);
bool __lucis_terminal_enable_canonical(int fd);
bool __lucis_terminal_disable_canonical(int fd);

/* Terminal attribute control */

bool __lucis_terminal_set_input_speed(int fd, uint32_t speed);
bool __lucis_terminal_set_output_speed(int fd, uint32_t speed);
bool __lucis_terminal_get_control_char(int fd, int idx, uint8_t* out);
bool __lucis_terminal_set_control_char(int fd, int idx, uint8_t val);

/* Special control characters (indices match termios c_cc) */

bool __lucis_terminal_set_intr(int fd, uint8_t c);      // VINTR
bool __lucis_terminal_set_quit(int fd, uint8_t c);      // VQUIT
bool __lucis_terminal_set_susp(int fd, uint8_t c);      // VSUSP
bool __lucis_terminal_set_eof(int fd, uint8_t c);       // VEOF
bool __lucis_terminal_set_erase(int fd, uint8_t c);     // VERASE
bool __lucis_terminal_set_kill(int fd, uint8_t c);      // VKILL
bool __lucis_terminal_set_eol(int fd, uint8_t c);       // VEOL
bool __lucis_terminal_set_eol2(int fd, uint8_t c);      // VEOL2
bool __lucis_terminal_set_swtch(int fd, uint8_t c);     // VSWTCH
bool __lucis_terminal_set_start(int fd, uint8_t c);     // VSTART
bool __lucis_terminal_set_stop(int fd, uint8_t c);      // VSTOP
bool __lucis_terminal_set_susp_char(int fd, uint8_t c); // VSUSP (alias)
bool __lucis_terminal_set_dsusp(int fd, uint8_t c);     // VDSUSP
bool __lucis_terminal_set_reprint(int fd, uint8_t c);   // VREPRINT
bool __lucis_terminal_set_werase(int fd, uint8_t c);    // VWERASE
bool __lucis_terminal_set_lnext(int fd, uint8_t c);     // VLNEXT
bool __lucis_terminal_set_discard(int fd, uint8_t c);   // VDISCARD

/* Flow control & blocking modes */

bool __lucis_terminal_enable_flow_control(int fd);   // IXON | IXOFF
bool __lucis_terminal_disable_flow_control(int fd);  // ~(IXON | IXOFF)
bool __lucis_terminal_set_nonblocking(int fd);       // VMIN=0, VTIME=0
bool __lucis_terminal_set_blocking(int fd, int min, int time); // VMIN, VTIME

/* Signal handling */

bool __lucis_terminal_enable_signals(int fd);   // ISIG on
bool __lucis_terminal_disable_signals(int fd);  // ISIG off

#ifdef __cplusplus
}
#endif

#endif