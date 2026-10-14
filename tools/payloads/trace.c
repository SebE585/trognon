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
	u32 hit;          /* number of expected exceptions handled */
	u32 cx, cy;       /* console cursor in pixels */
	u32 fg;
	u32 quiet;        /* 1: a handled expected exception draws nothing (register survey) */
};
#define SC ((volatile struct scratch *)0x41080000)
#define HANDLER_STACK_TOP 0x410f8000
#define BREADCRUMB        (0x4fd00000 + 466 * 320 * 4)

static void rect(int x, int y, int w, int h, u32 color)
{
	if (x < 0 || y < 0 || x + w > W || y + h > H)
		return;
	for (int j = y; j < y + h; j++)
		for (int i = x; i < x + w; i++)
			FB[j * W + i] = color;
}

/* Debug aid: push the framebuffer out of the write-back D-cache so the screen shows what the CPU
 * really got through if it freezes (dirty lines are otherwise lost). Costs ~19200 MCRs. */
static void fb_clean(void)
{
	for (u32 a = 0x4fd00000; a < 0x4fd00000 + W * H * 4; a += 32)
		asm volatile("mcr p15, 0, %0, c7, c10, 1" :: "r"(a) : "memory");
	asm volatile("dsb sy" ::: "memory");
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

static void puts_(const char *s) { while (*s) putc_(*s++); fb_clean(); }

static void hex(u32 v)
{
	for (int i = 28; i >= 0; i -= 4)
		putc_("0123456789ABCDEF"[(v >> i) & 15]);
}

static void kv(const char *k, u32 v) { puts_(k); putc_(' '); hex(v); putc_('\n'); fb_clean(); }

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
#define STR_(x) #x
#define STR(x) STR_(x)
#define STUB(NAME, KIND) \
__attribute__((naked, target("arm"))) void NAME(void) \
{ \
	asm volatile( \
		"movw sp, #:lower16:" STR(HANDLER_STACK_TOP) "\n" \
		"movt sp, #:upper16:" STR(HANDLER_STACK_TOP) "\n" \
		"push {r0-r12}\n" \
		/* breadcrumb, no C and no stack: 10 rows at y=466, red/green/blue for kind 0/1/2 */ \
		"movw r1, #:lower16:" STR(BREADCRUMB) "\n" \
		"movt r1, #:upper16:" STR(BREADCRUMB) "\n" \
		"mov r2, #(0xFF0000 >> (8 * " STR(KIND) "))\n" \
		"movw r3, #3200\n" \
		"1: str r2, [r1], #4\n" \
		"subs r3, r3, #1\n" \
		"bne 1b\n" \
		"movw r1, #:lower16:" STR(BREADCRUMB) "\n" \
		"movt r1, #:upper16:" STR(BREADCRUMB) "\n" \
		"movw r3, #400\n" \
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


__attribute__((section(".text.start"))) void payload_main(void)
{
	u32 midr, sctlr;
	asm volatile("mrc p15, 0, %0, c0, c0, 0" : "=r"(midr));
	asm volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(sctlr));

	SC->quiet = 0;                        /* scratch RAM keeps the previous payload's values */
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
#if STAGE == 2 || STAGE == 3
	/* Test 1: deliberate undefined instruction (Thumb UDF).
	 * The resume address is computed by the assembler right after the faulting instruction:
	 * GCC resolved `&&label` to the function start here (seen on the device 2026-10-15), which
	 * made every handled exception restart the whole payload. */
	at(4, 116, 0x00F0C040); puts_("STEP 2 UDF\n");
	SC->expected = 1;
	asm volatile("adr.w r0, 1f\n\tstr r0, [%0]\n\tudf #0\n1:" :: "r"(&SC->resume) : "r0", "memory");
	SC->expected = 0;                     /* disarm: a later, unplanned exception must not resume here */
	at(4, 230, 0x0000B050); puts_("UDF RESUMED\n");
#endif
#if STAGE == 3
	/* Test 2: deliberate data abort = ALIGNMENT fault. The live page table has no translation-fault
	 * entry and every section is in domain 0 (DACR=3, manager), read on the device 2026-10-15, so
	 * neither a fault entry nor a domain fault is available. Instead set SCTLR.A (bit 1) around one
	 * unaligned load on mapped RAM: the fault is raised before the bus access, nothing is touched.
	 * exc_c() clears A on entry, so the handler never runs with strict alignment. */
	u32 va = (u32)&SC->hit + 1;
	at(4, 252, 0x00A0B0C0); kv("UNALIGNED VA", va);
	at(4, 288, 0x00F0C040); puts_("STEP 3 DATA ABT\n");
	u32 before = SC->hit, sctlr_a = sctlr | 2;
	SC->expected = 1;
	asm volatile("adr.w r1, 1f\n\tstr r1, [%1]\n\t"
		     "mcr p15, 0, %2, c1, c0, 0\n\tisb sy\n\t"
		     "ldr r0, [%0]\n1:\n\t"
		     "mcr p15, 0, %3, c1, c0, 0\n\tisb sy"
		     :: "r"(va), "r"(&SC->resume), "r"(sctlr_a), "r"(sctlr) : "r0", "r1", "memory");
	SC->expected = 0;
	at(4, 432, SC->hit == before ? 0x00E02020 : 0x0000B050);
	puts_(SC->hit == before ? "NO ABORT\n" : "ABT RESUMED\n");
#endif
#if STAGE == 5
	/* Read-only register survey, shown on screen. Each load is armed as an expected data abort, so
	 * an unmapped register prints ABORT instead of resetting the phone. GPIOIC (0x39A00000) is NOT
	 * read: it dropped the USB link once (docs/journal/2026-10-14.md). Offsets from OpeniBoot
	 * (plat-s5l8920/clcd.c, gpio.c, includes/hardware/gpio.h): to be checked against the answer. */
	static const struct { const char *n; u32 a; } regs[] = {
		{ "VIDCON1 ", 0x85401B14 }, { "VIDCON1b", 0x85401B14 }, { "VIDCON1c", 0x85401B14 },
		{ "VIDEN   ", 0x85401B10 }, { "VIDTCON2", 0x85401B24 }, { "FBADDR  ", 0x85400024 },
		{ "GPIO+00 ", 0x83000000 }, { "GPIO+80 ", 0x83000080 }, { "GPIO+A0 ", 0x830000A0 },
		{ "GPIO+320", 0x83000320 }, { "CLKGT2C ", 0xBF100128 },
	};
	at(4, 116, 0x00F0C040); puts_("STEP 5 REGS\n");
	SC->quiet = 1;
	for (u32 i = 0; i < sizeof regs / sizeof *regs; i++) {
		if (i == 1 || i == 2)
			for (volatile u32 d = 0; d < 0x300000; d++) ;   /* spacing: is VIDCON1 a live counter? */
		u32 before = SC->hit, v = 0;
		SC->expected = 1;
		asm volatile("adr.w r1, 1f\n\tstr r1, [%2]\n\tldr %0, [%1]\n1:"
			     : "+r"(v) : "r"(regs[i].a), "r"(&SC->resume) : "r1", "memory");
		SC->expected = 0;
		at(4, 134 + i * 18, 0x00FFFFFF);
		puts_(regs[i].n); putc_(' '); hex(regs[i].a); putc_(' ');
		if (SC->hit != before) { SC->fg = 0x00E02020; puts_("ABORT"); } else hex(v);
		putc_('\n'); fb_clean();
	}
	SC->quiet = 0;
#endif
#if STAGE == 4
	/* Test 3: deliberate PREFETCH ABORT. A Thumb BKPT with the debug unit not in halt mode raises
	 * a prefetch abort (IFSR = 0x2, "debug event"), with no memory fetch involved. Resume address
	 * computed in asm like for the UDF. */
	at(4, 116, 0x00F0C040); puts_("STEP 4 BKPT\n");
	u32 before = SC->hit;
	SC->expected = 1;
	asm volatile("adr.w r0, 1f\n\tstr r0, [%0]\n\tbkpt #0\n1:" :: "r"(&SC->resume) : "r0", "memory");
	SC->expected = 0;
	at(4, 270, SC->hit == before ? 0x00E02020 : 0x0000B050);
	puts_(SC->hit == before ? "NO ABORT\n" : "PABT RESUMED\n");
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
#elif STAGE == 4
	ENV_SET("trognon", "pabt-1", 1);
#elif STAGE == 5
	ENV_SET("trognon", "regs-1", 1);
#else
	ENV_SET("trognon", "trace-1", 1);
#endif
}
