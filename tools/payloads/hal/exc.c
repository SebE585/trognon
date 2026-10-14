/* Exception handling on the exception console: asm stubs (own stack, breadcrumb bar), C handler
 * that draws FSR/FAR/LR/SPSR and returns the resume address, and the vector hook. */
#include "hal/hal.h"
#include "hal/console.h"
#include "hal/exc.h"

#define BREADCRUMB_ADDR (SOC_FB_BASE + SOC_BREADCRUMB_ROW * SOC_FB_W * 4)

/* Called from the asm stubs. kind: 0 undef, 1 prefetch abort, 2 data abort.
 * Returns the address to resume at (even: the Thumb state comes from SPSR). */
u32 exc_c(u32 kind, u32 lr, u32 spsr, u32 *r)
{
	static const char *const name[] = { "UNDEF", "PREFETCH ABT", "DATA ABT" };
	u32 fsr = 0, far = 0;
	if (kind == 2) {
		asm volatile("mrc p15, 0, %0, c5, c0, 0" : "=r"(fsr));
		asm volatile("mrc p15, 0, %0, c6, c0, 0" : "=r"(far));
	} else if (kind == 1) {
		asm volatile("mrc p15, 0, %0, c5, c0, 1" : "=r"(fsr));
		asm volatile("mrc p15, 0, %0, c6, c0, 2" : "=r"(far));
	}
	if (kind == 2) {                       /* drop SCTLR.A first: the alignment test arms it */
		u32 c;
		asm volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(c));
		asm volatile("mcr p15, 0, %0, c1, c0, 0\n\tisb sy" :: "r"(c & ~2u));
	}
	int expected = SC->expected;
	if (expected)
		SC->hit = SC->hit + 1;
	if (expected && SC->quiet)
		return SC->resume;
	at(4, kind ? 306 : 134, expected ? 0x0000B050 : 0x00E02020);   /* UNDEF and ABT dumps stack, not overlap */
	puts_(expected ? "EXPECTED " : "UNEXPECTED ");
	puts_(name[kind]);
	putc_('\n');
	kv("LR ", lr);
	kv("SPSR", spsr);
	if (kind) { kv("FSR", fsr); kv("FAR", far); }
	for (int i = 0; i < 2; i++) { putc_('R'); putc_('0' + i); putc_(' '); hex(r[i]); putc_('\n'); }
	if (expected)
		return SC->resume;
	for (volatile u32 i = 0; i < 0x20000000; i++) ;   /* leave time to read the screen */
	return SC->old[kind];                              /* chain to the iBSS handler (will reset) */
}

/* Asm stubs: own stack (banked SP of the exception mode), save r0-r12, call exc_c, restore, return.
 * 13 regs + 4 pad keeps SP 8-byte aligned at the call. The pointer returned by exc_c becomes the
 * return address through `subs pc, lr, #0` (restores CPSR from SPSR). */
#define STUB(NAME, KIND) \
__attribute__((naked, target("arm"))) void NAME(void) \
{ \
	asm volatile( \
		"movw sp, #:lower16:" STR(SOC_HANDLER_STACK_TOP) "\n" \
		"movt sp, #:upper16:" STR(SOC_HANDLER_STACK_TOP) "\n" \
		"push {r0-r12}\n" \
		/* breadcrumb, no C and no stack: 10 rows at y=466, red/green/blue for kind 0/1/2 */ \
		"movw r1, #:lower16:" STR(BREADCRUMB_ADDR) "\n" \
		"movt r1, #:upper16:" STR(BREADCRUMB_ADDR) "\n" \
		"mov r2, #(0xFF0000 >> (8 * " STR(KIND) "))\n" \
		"movw r3, #" STR(SOC_BREADCRUMB_WORDS) "\n" \
		"1: str r2, [r1], #4\n" \
		"subs r3, r3, #1\n" \
		"bne 1b\n" \
		"movw r1, #:lower16:" STR(BREADCRUMB_ADDR) "\n" \
		"movt r1, #:upper16:" STR(BREADCRUMB_ADDR) "\n" \
		"movw r3, #" STR(SOC_BREADCRUMB_LINES) "\n" \
		"2: mcr p15, 0, r1, c7, c10, 1\n" \
		"add r1, r1, #32\n" \
		"subs r3, r3, #1\n" \
		"bne 2b\n" \
		"dsb sy\n" \
		"sub sp, sp, #4\n" \
		"mov r0, #" STR(KIND) "\n" \
		"mov r1, lr\n" \
		"mrs r2, spsr\n" \
		"add r3, sp, #4\n" \
		"bl exc_c\n" \
		"mov lr, r0\n" \
		"add sp, sp, #4\n" \
		"pop {r0-r12}\n" \
		"subs pc, lr, #0\n"); \
}
STUB(exc_undef, 0)
STUB(exc_pabt, 1)
STUB(exc_dabt, 2)

void hook(int on)
{
	static const u32 vec[3] = { SOC_VEC_UNDEF, SOC_VEC_PABT, SOC_VEC_DABT };
	const u32 handler[3] = { (u32)exc_undef, (u32)exc_pabt, (u32)exc_dabt };
	for (int i = 0; i < 3; i++) {
		if (on)
			SC->old[i] = *(volatile u32 *)vec[i];
		*(volatile u32 *)vec[i] = on ? handler[i] : SC->old[i];
	}
}

