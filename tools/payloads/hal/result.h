#ifndef HAL_RESULT_H
#define HAL_RESULT_H
#include "hal/hal.h"
/* Publish a 32-bit value to the host as 8 hex digits in an iBSS environment variable, read back with
 * `getenv <name>`: a result channel that needs no photo of the screen. */
void result_hex(const char *name, u32 v);

/* Publish a short string (up to 60 characters) in an env variable. */
void result_str(const char *name, const char *value);

/* Republish the host-appended nonce (the 32-bit word right after this binary, linker symbol
 * payload_end) as env variable `nonce`. The host compares it with the one it generated for this
 * run: a match proves THIS binary, sent THIS time, executed (a stale marker cannot match). */
void result_nonce(void);

/* Publish the console transcript as env variables s00..s25, value "RRGGBB text" (non-empty rows only). */
void result_screen(void);
#endif
