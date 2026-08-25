#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Lucis fuzz runner runtime (`lucis fuzz`).
 *
 * The synthesized `main` delegates to lucis_fuzz_main, which runs the classic
 * AFL-style loop in-process: mutate an input -> fork -> child executes the
 * user's #[fuzz] target with that input -> parent reads the coverage bitmap
 * written by luucis_fuzz_trace hooks -> inputs that reveal new coverage join
 * the queue. Crashes (signals), timeouts and assertion aborts are isolated in
 * the child; crashing inputs are minimized and saved.
 */

/* Coverage hook inserted by IRGen at the top of every user basic block. */
void lucis_fuzz_trace(int32_t id);

/* Runner entry point (called by the synthesized main). Exit status:
 * 0 = finished, no crashes; 1 = at least one crash was found. */
int32_t lucis_fuzz_main(int32_t argc, char** argv);

#ifdef __cplusplus
}
#endif