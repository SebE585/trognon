/* Numbers over USB: the iBSS env_set() copies the string, so a stack buffer is fine. */
#include "hal/hal.h"
#include "hal/result.h"

/* The iBSS env_set() does not preserve r4-r11 (observed 2026-10-15: locals kept in registers across the
 * first call came back as garbage, e.g. a counter that read 0). Save and restore them around the call. */
static void env_set_safe(const char *name, const char *value, int flag)
{
	register u32 r0 asm("r0") = (u32)name;
	register u32 r1 asm("r1") = (u32)value;
	register u32 r2 asm("r2") = (u32)flag;
	register u32 r3 asm("r3") = SOC_ENV_SET;
	asm volatile("push {r4-r11}\n\tblx r3\n\tpop {r4-r11}"
		     : "+r"(r0), "+r"(r1), "+r"(r2), "+r"(r3) :: "r12", "lr", "memory", "cc");
}

void result_hex(const char *name, u32 v)
{
	char buf[9];
	for (int i = 0; i < 8; i++)
		buf[i] = "0123456789ABCDEF"[(v >> (28 - 4 * i)) & 15];
	buf[8] = 0;
	env_set_safe(name, buf, 1);
}

extern const u32 payload_end[];   /* defined by payload.ld */

void result_nonce(void)
{
	result_hex("nonce", *(volatile const u32 *)payload_end);
}
