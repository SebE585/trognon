/* On-screen console + exception handlers for the iPhone 3GS (S5L8920), as a Recovery payload.
 *
 * State facts (all read on the device 2026-10-15, docs/journal/2026-10-15.md):
 *  - CLCD window 0 framebuffer 0x4fd00000, 320x480 RGB888 (0x00RRGGBB), mapped C=1 B=1
 *    (write-back, no write-allocate): NEVER READ the framebuffer, or clean it after writing.
 *  - MMU + caches on, vectors at VA 0 (= PA 0x84000000, the iBSS image). Each vector is
 *    `ldr pc,[pc,#0x18]` through a pointer word: undef 0x24, prefetch 0x2c, data 0x30.
 *    We overwrite those words in RAM and restore them at the end; a reboot undoes it anyway.
 *  - Our payload section 0x410xxxxx is mapped; scratch and handler stack live at its top.
 *
 * Sequence: banner + CPU info, hook handlers, deliberate UNDEF, deliberate DATA ABORT on a VA
 * that the page table (TTBR0) says is unmapped, unhook, set env marker. Handlers show the
 * exception on screen then RESUME at scratch->resume (no reset, DFU is not needed again).
 * An unexpected exception shows its dump, waits, then chains to the original iBSS handler.
 */
typedef unsigned int u32;
typedef unsigned char u8;
#include "font.h"

#define FB       ((volatile u32 *)0x4fd00000)
#define W        320
#define H        480
#define SCALE    2
#define CELL_W   (6 * SCALE)
#define CELL_H   (9 * SCALE)
#define ENV_SET  ((void (*)(const char *, const char *, int))0x84011799) /* iBSS 4.3.5 n88ap */

#define VEC_UNDEF 0x24
#define VEC_PABT  0x2c
#define VEC_DABT  0x30

struct scratch {
	u32 old[3];       /* original vector pointers: undef, prefetch, data */
	u32 expected;     /* 1 while a deliberate exception is in flight */
	u32 resume;       /* where a handled, expected exception resumes */
	u32 cx, cy;       /* console cursor in pixels */
	u32 fg;
};
#define SC ((volatile struct scratch *)0x41080000)
#define HANDLER_STACK_TOP 0x410f8000

static void rect(int x, int y, int w, int h, u32 color)
{
	if (x < 0 || y < 0 || x + w > W || y + h > H)
		return;
	for (int j = y; j < y + h; j++)
		for (int i = x; i < x + w; i++)
			FB[j * W + i] = color;
}

static void putc_(char c)
{
	if (c == '\n' || SC->cx + CELL_W > W) {
		SC->cx = 4;
		SC->cy += CELL_H;
		if (c == '\n')
			return;
	}
	if (SC->cy + CELL_H > H)
		return;                       /* no scrolling: it would read the framebuffer */
	const u8 *g = font5x7[c & 0x7f];
	for (int r = 0; r < 7; r++)
		for (int k = 0; k < 5; k++)
			if (g[r] & (0x10 >> k))
				rect(SC->cx + k * SCALE, SC->cy + r * SCALE, SCALE, SCALE, SC->fg);
	SC->cx += CELL_W;
}

static void puts_(const char *s) { while (*s) putc_(*s++); }

static void hex(u32 v)
{
	for (int i = 28; i >= 0; i -= 4)
		putc_("0123456789ABCDEF"[(v >> i) & 15]);
}

static void kv(const char *k, u32 v) { puts_(k); putc_(' '); hex(v); putc_('\n'); }

static void at(int x, int y, u32 fg) { SC->cx = x; SC->cy = y; SC->fg = fg; }

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
	int expected = SC->expected;
	at(4, kind ? 240 : 134, expected ? 0x0000B050 : 0x00E02020);   /* UNDEF and ABT dumps stack, not overlap */
	puts_(expected ? "EXPECTED " : "UNEXPECTED ");
	puts_(name[kind]);
	putc_('\n');
	kv("LR ", lr);
	kv("SPSR", spsr);
	if (kind) { kv("FSR", fsr); kv("FAR", far); }
	for (int i = 0; i < 4; i++) { putc_('R'); putc_('0' + i); putc_(' '); hex(r[i]); putc_('\n'); }
	if (expected)
		return SC->resume;
	for (volatile u32 i = 0; i < 0x20000000; i++) ;   /* leave time to read the screen */
	return SC->old[kind];                              /* chain to the iBSS handler (will reset) */
}

/* Asm stubs: own stack (banked SP of the exception mode), save r0-r12, call exc_c, restore, return.
 * 13 regs + 4 pad keeps SP 8-byte aligned at the call. The pointer returned by exc_c becomes the
 * return address through `subs pc, lr, #0` (restores CPSR from SPSR). */
#define STR_(x) #x
#define STR(x) STR_(x)
#define STUB(NAME, KIND) \
__attribute__((naked, target("arm"))) void NAME(void) \
{ \
	asm volatile( \
		"movw sp, #:lower16:" STR(HANDLER_STACK_TOP) "\n" \
		"movt sp, #:upper16:" STR(HANDLER_STACK_TOP) "\n" \
		"push {r0-r12}\n" \
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

static void hook(int on)
{
	static const u32 vec[3] = { VEC_UNDEF, VEC_PABT, VEC_DABT };
	const u32 handler[3] = { (u32)exc_undef, (u32)exc_pabt, (u32)exc_dabt };
	for (int i = 0; i < 3; i++) {
		if (on)
			SC->old[i] = *(volatile u32 *)vec[i];
		*(volatile u32 *)vec[i] = on ? handler[i] : SC->old[i];
	}
}

#ifndef STAGE
#define STAGE 3
#endif
#ifdef OFFLINE
#undef STAGE
#define STAGE 0
#endif

#if STAGE >= 3
static u32 find_unmapped_va(void)
{
	u32 ttbr;
	asm volatile("mrc p15, 0, %0, c2, c0, 0" : "=r"(ttbr));
	volatile u32 *tt = (volatile u32 *)(ttbr & ~0x3fffu);
	for (u32 idx = 0x300; idx < 0x800; idx++)
		if ((tt[idx] & 3) == 0)
			return idx << 20;           /* section descriptor type 00 = translation fault */
	return 0;
}
#endif

__attribute__((section(".text.start"))) void payload_main(void)
{
	u32 midr, sctlr;
	asm volatile("mrc p15, 0, %0, c0, c0, 0" : "=r"(midr));
	asm volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(sctlr));

	rect(0, 0, W, H, 0x00101820);
	rect(0, 0, W, 6, 0x000D32B2);
	at(4, 12, 0x00FFFFFF);
	puts_("TROGNON CONSOLE\n");
	SC->fg = 0x00A0B0C0;
	kv("MIDR ", midr);
	kv("SCTLR", sctlr);
	SC->fg = 0x00FFFFFF;

#if STAGE >= 1
	/* Each risky step is announced on screen BEFORE it runs, so a freeze shows how far we got. */
	SC->expected = 0;
	at(4, 80, 0x00F0C040); puts_("STEP 1 HOOK\n");
	hook(1);
	at(4, 98, 0x0000B050); puts_("HOOKED\n");
#endif
#if STAGE >= 2
	/* Test 1: deliberate undefined instruction (Thumb UDF). */
	at(4, 116, 0x00F0C040); puts_("STEP 2 UDF\n");
	SC->expected = 1;
	SC->resume = (u32)&&after_udf & ~1u;
	asm volatile("udf #0");
after_udf:
	at(4, 224, 0x0000B050); puts_("UDF RESUMED\n");
#endif
#if STAGE >= 3
	/* Test 2: deliberate data abort on a VA the page table says is unmapped. */
	u32 va = find_unmapped_va();
	at(4, 380, 0x00A0B0C0);
	kv("UNMAPPED VA", va);
	if (va) {
		at(4, 398, 0x00F0C040); puts_("STEP 3 DATA ABT\n");
		SC->resume = (u32)&&after_dabt & ~1u;
		asm volatile("ldr r0, [%0]" :: "r"(va) : "r0", "memory");
		at(4, 416, 0x00E02020);
		puts_("NO ABORT\n");
	}
after_dabt:
#endif
#if STAGE >= 1
	SC->expected = 0;
	hook(0);
	at(4, 450, 0x0000B050);
	puts_("UNHOOKED\n");
#endif
#if STAGE == 0
	ENV_SET("trognon", "console-1", 1);   /* console only: no vector touched */
#elif STAGE == 1
	ENV_SET("trognon", "hook-1", 1);
#elif STAGE == 2
	ENV_SET("trognon", "udf-1", 1);
#else
	ENV_SET("trognon", "trace-1", 1);
#endif
}
