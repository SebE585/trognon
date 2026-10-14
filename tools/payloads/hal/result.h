#ifndef HAL_RESULT_H
#define HAL_RESULT_H
#include "hal/hal.h"
/* Publish a 32-bit value to the host as 8 hex digits in an iBSS environment variable, read back with
 * `getenv <name>`: a result channel that needs no photo of the screen. */
void result_hex(const char *name, u32 v);
#endif
