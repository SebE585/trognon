/* On-screen console + exception handlers for the iPhone 3GS (S5L8920), as a Recovery payload.
 * Bring-up payload: assembles hal/ (portable) and soc/ (per-chip constants), see docs/en/07-code-layout.md.
 *
 * State facts (all read on the device 2026-10-15, docs/en/03-hardware-map.md):
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
#include "hal/hal.h"
#include "hal/console.h"
#include "hal/exc.h"
#include "hal/vic.h"
#include "hal/result.h"

#define W        SOC_FB_W
#define H        SOC_FB_H
#define ENV_SET  ((void (*)(const char *, const char *, int))SOC_ENV_SET) /* iBSS 4.3.5 n88ap */

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

#if STAGE == 9
	/* Tick-rate probe: read the free-running counter FIRST, publish it for the host, nothing else.
	 * The host runs this twice a measured time apart and divides (tools/tick_rate.py). */
	u32 t_lo = *(volatile u32 *)(SOC_TIMER_BASE + 0x200);
	u32 t_hi = *(volatile u32 *)(SOC_TIMER_BASE + 0x204);
	result_hex("ticks", t_lo);
	result_hex("ticks_hi", t_hi);
	ENV_SET("trognon", "ticks-1", 1);
	return;
#endif
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
#if STAGE == 5 || STAGE == 6 || STAGE == 8
	/* Read-only register survey, shown on screen. Each load is armed as an expected data abort, so
	 * an unmapped register prints ABORT instead of resetting the phone. GPIOIC (0x39A00000) is NOT
	 * read: it dropped the USB link once (docs/en/03-hardware-map.md). Offsets from OpeniBoot
	 * (plat-s5l8920/clcd.c, gpio.c, includes/hardware/gpio.h): to be checked against the answer. */
#if STAGE == 5
	static const struct { const char *n; u32 a; } regs[] = {
		{ "VIDCON1 ", (SOC_CLCD_BASE + 0x1B14) }, { "VIDCON1b", (SOC_CLCD_BASE + 0x1B14) }, { "VIDCON1c", (SOC_CLCD_BASE + 0x1B14) },
		{ "VIDEN   ", (SOC_CLCD_BASE + 0x1B10) }, { "VIDTCON2", (SOC_CLCD_BASE + 0x1B24) }, { "FBADDR  ", (SOC_CLCD_BASE + 0x24) },
		{ "GPIO+00 ", SOC_GPIO_BASE }, { "GPIO+80 ", (SOC_GPIO_BASE + 0x80) }, { "GPIO+A0 ", (SOC_GPIO_BASE + 0xA0) },
		{ "GPIO+320", (SOC_GPIO_BASE + 0x320) }, { "CLKGT2C ", SOC_CLOCK_GATE_2C },
	};
#elif STAGE == 8
	/* Timer block, read-only. Offsets from OpeniBoot (plat-s5l8920 hardware/timer.h, SOURCE): free-running
	 * ticks at +0x200/+0x204, per-timer STATE at +0x04, 0x24, 0x44, 0x64, 0xA4, 0xC4, 0xE4, timer 4 (the
	 * event timer) registers at +0xA0.., timer clock gate 0x25 at +0x10C. IRQSTAT/IRQLATCH (+0xF8,
	 * +0x10000) are NOT read: they may be read-to-clear. VIC0 bit 6 (timer IRQ 6) is enabled by the iBSS. */
	static const struct { const char *n; u32 a; } regs[] = {
		{ "TICK LO ", SOC_TIMER_BASE + 0x200 }, { "TICK HI ", SOC_TIMER_BASE + 0x204 },
		{ "TICK LO2", SOC_TIMER_BASE + 0x200 }, { "TICK LO3", SOC_TIMER_BASE + 0x200 },
		{ "T0 STATE", SOC_TIMER_BASE + 0x04 }, { "T1 STATE", SOC_TIMER_BASE + 0x24 },
		{ "T2 STATE", SOC_TIMER_BASE + 0x44 }, { "T3 STATE", SOC_TIMER_BASE + 0x64 },
		{ "T4 STATE", SOC_TIMER_BASE + 0xA4 }, { "T5 STATE", SOC_TIMER_BASE + 0xC4 },
		{ "T6 STATE", SOC_TIMER_BASE + 0xE4 },
		{ "T4 CFG  ", SOC_TIMER_BASE + 0xA0 }, { "T4 CNT  ", SOC_TIMER_BASE + 0xA8 },
		{ "T4 CNT2 ", SOC_TIMER_BASE + 0xAC }, { "T4 PRES ", SOC_TIMER_BASE + 0xB0 },
		{ "CLKGT25 ", SOC_TIMER_BASE + 0x10C },
	};
#else
	/* Interrupt controllers, read-only. VICADDRESS (0xF00) is NOT read: reading it tells the VIC an
	 * interrupt is being serviced. Vector words 0x28/0x38/0x3c = where the iBSS sends SWI/IRQ/FIQ. */
	static const struct { const char *n; u32 a; } regs[] = {
		{ "V0 STAT ", SOC_VIC0 }, { "V0 RAW  ", (SOC_VIC0 + 0x08) }, { "V0 SEL  ", (SOC_VIC0 + 0x0C) },
		{ "V0 EN   ", (SOC_VIC0 + 0x10) }, { "V0 SOFT ", (SOC_VIC0 + 0x18) },
		{ "V1 STAT ", SOC_VIC1 }, { "V1 RAW  ", (SOC_VIC1 + 0x08) }, { "V1 SEL  ", (SOC_VIC1 + 0x0C) },
		{ "V1 EN   ", (SOC_VIC1 + 0x10) }, { "V1 SOFT ", (SOC_VIC1 + 0x18) },
		{ "V2 STAT ", SOC_VIC2 }, { "V2 RAW  ", (SOC_VIC2 + 0x08) }, { "V2 SEL  ", (SOC_VIC2 + 0x0C) },
		{ "V2 EN   ", (SOC_VIC2 + 0x10) }, { "V2 SOFT ", (SOC_VIC2 + 0x18) },
		{ "VEC IRQ ", SOC_VEC_IRQ }, { "VEC FIQ ", (SOC_VEC_IRQ + 4) },
	};
#endif
	at(4, 116, 0x00F0C040); puts_(STAGE == 5 ? "STEP 5 REGS\n" : STAGE == 8 ? "STEP 8 TIMER\n" : "STEP 6 VIC\n");
	SC->quiet = 1;
	for (u32 i = 0; i < sizeof regs / sizeof *regs; i++) {
		if ((STAGE == 5 && (i == 1 || i == 2)) || (STAGE == 8 && i == 2))
			for (volatile u32 d = 0; d < 0x300000; d++) ;   /* spacing: is it a live counter? */
		if (STAGE == 8 && i == 3)
			for (volatile u32 d = 0; d < 0x1000000; d++) ;  /* longer gap: gives a rate estimate */
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
#if STAGE == 7
	/* Test 4: software-triggered IRQ on VIC0. IRQs masked while we set up; every VIC0 source the iBSS
	 * left enabled is disabled (and restored after) so only ours can fire. */
	at(4, 116, 0x00F0C040); puts_("STEP 7 SOFT IRQ\n");
	at(4, 134, 0x00FFFFFF);
	u32 cpsr0, en0 = VICREG(SOC_VIC0, 0x010);
	asm volatile("mrs %0, cpsr" : "=r"(cpsr0));
	u32 bit = ~en0 & -(~en0);                           /* lowest source not enabled by the iBSS */
	SC->irq = 0; SC->stray = 0; SC->irq_bit = bit; SC->irq_status = 0; SC->irq_vec = 0;
	kv("CPSR  ", cpsr0);
	kv("EN0   ", en0);
	kv("BIT   ", bit);
	asm volatile("cpsid i" ::: "memory");
	VICREG(SOC_VIC0, 0x014) = 0xFFFFFFFF;               /* INTENCLEAR: all of VIC0 off */
	u32 en_off = VICREG(SOC_VIC0, 0x010);
	hook_irq(1);
	VICREG(SOC_VIC0, 0x010) = bit;                      /* INTENABLE: ours only */
	VICREG(SOC_VIC0, 0x018) = bit;                      /* SOFTINT: raise it */
	asm volatile("dsb sy" ::: "memory");
	u32 raw = VICREG(SOC_VIC0, 0x008), stat = VICREG(SOC_VIC0, 0x000);
	kv("EN OFF", en_off);
	kv("RAW   ", raw);
	kv("STAT  ", stat);
	at(4, 134 + 6 * 18, 0x00F0C040); puts_("UNMASKING\n");
	asm volatile("cpsie i" ::: "memory");
	for (volatile u32 w = 0; w < 0x4000000 && !SC->irq && !SC->stray; w++) ;
	asm volatile("cpsid i" ::: "memory");
	hook_irq(0);                                         /* vector back BEFORE the iBSS sources return */
	u32 raw2 = VICREG(SOC_VIC0, 0x008), stat2 = VICREG(SOC_VIC0, 0x000);
	VICREG(SOC_VIC0, 0x014) = 0xFFFFFFFF;
	VICREG(SOC_VIC0, 0x01C) = bit;                      /* SOFTINTCLEAR, in case nothing took it */
	VICREG(SOC_VIC0, 0x010) = en0;                      /* restore the iBSS's enables */
	u32 en_back = VICREG(SOC_VIC0, 0x010);
	if (!(cpsr0 & 0x80))
		asm volatile("cpsie i" ::: "memory");
	at(4, 134 + 7 * 18, SC->irq ? 0x0000B050 : 0x00E02020);
	puts_(SC->irq ? "IRQ TAKEN\n" : "NO IRQ\n");
	at(4, 134 + 8 * 18, 0x00FFFFFF);
	kv("HITS  ", SC->irq);
	kv("STRAY ", SC->stray);
	kv("H STAT", SC->irq_status);
	kv("H VEC ", SC->irq_vec);
	kv("RAW2  ", raw2);
	kv("STAT2 ", stat2);
	kv("EN BCK", en_back);
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
#elif STAGE == 7
	ENV_SET("trognon", "irq-1", 1);
#elif STAGE == 8
	ENV_SET("trognon", "timer-1", 1);
#elif STAGE == 5
	ENV_SET("trognon", "regs-1", 1);
#elif STAGE == 6
	ENV_SET("trognon", "vic-1", 1);
#else
	ENV_SET("trognon", "trace-1", 1);
#endif
}
