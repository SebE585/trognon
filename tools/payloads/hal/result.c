/* Numbers over USB: the iBSS env_set() copies the string, so a stack buffer is fine. */
#include "hal/hal.h"
#include "hal/result.h"

#define ENV_SET ((void (*)(const char *, const char *, int))SOC_ENV_SET)

void result_hex(const char *name, u32 v)
{
	char buf[9];
	for (int i = 0; i < 8; i++)
		buf[i] = "0123456789ABCDEF"[(v >> (28 - 4 * i)) & 15];
	buf[8] = 0;
	ENV_SET(name, buf, 1);
}

extern const u32 payload_end[];   /* defined by payload.ld */

void result_nonce(void)
{
	result_hex("nonce", *(volatile const u32 *)payload_end);
}
