/* Numbers over USB: the iBSS env_set() copies the string, so a stack buffer is fine. */
#include "hal/hal.h"
#include "hal/result.h"
#include "hal/console.h"

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

void result_str(const char *name, const char *value)
{
	env_set_safe(name, value, 1);
}

extern const u32 payload_end[];   /* defined by payload.ld */

void result_nonce(void)
{
	result_hex("nonce", *(volatile const u32 *)payload_end);
}

void result_screen(void)
{
	const volatile struct tr_row *t = transcript();
	for (u32 r = 0; r < TR_ROWS; r++) {
		if (!t[r].text[0])
			continue;
		u32 tens = r >= 20 ? 2 : r >= 10 ? 1 : 0;           /* no libgcc division in a payload */
		char name[4] = { 's', (char)('0' + tens), (char)('0' + (r - tens * 10)), 0 };
		char val[8 + TR_COLS + 1];
		u32 n = 0;
		for (int i = 20; i >= 0; i -= 4)
			val[n++] = "0123456789ABCDEF"[(t[r].fg >> i) & 15];
		val[n++] = ' ';
		for (u32 i = 0; i < TR_COLS && t[r].text[i]; i++)
			val[n++] = t[r].text[i];
		while (n > 7 && val[n - 1] == ' ')          /* trailing blanks carry nothing */
			n--;
		val[n] = 0;
		env_set_safe(name, val, 1);
	}
}
