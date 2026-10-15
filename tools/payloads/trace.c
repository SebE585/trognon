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
	result_nonce();
	result_hex("ticks", t_lo);
	result_hex("ticks_hi", t_hi);
	ENV_SET("trognon", "ticks-1", 1);
	return;
#endif
	SC->quiet = 0;                        /* scratch RAM keeps the previous payload's values */
	transcript_clear();
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
#if STAGE == 10 || STAGE == 11
	/* Test 5 (STAGE 11 = control run: identical, but the timer is never started): a real timer interrupt. Timer 5 (STATE read 0 on the device: looks unused) is programmed
	 * with the OpeniBoot recipe (SOURCE): config 0x7000 = INT0/INT1/OVF enables, divider code 1, count N,
	 * STATE 2 (manual update) then 1 (start). The IRQ line is VIC0 source 6 (also used by the iBSS's own
	 * timers). Same guards as the soft-IRQ test; the timeout uses the 24 MHz free-running tick. */
	at(4, 116, 0x00F0C040); puts_(STAGE == 10 ? "STEP 10 TIMER IRQ\n" : "STEP 11 CONTROL\n");
	at(4, 134, 0x00FFFFFF);
	u32 ts0 = 0xDEAD0000;
	SC->expected = 1; SC->quiet = 1;
	u32 hit0 = SC->hit;
	asm volatile("adr.w r1, 1f\n\tstr r1, [%2]\n\tldr %0, [%1]\n1:"
		     : "+r"(ts0) : "r"(SOC_TIMER_IRQSTAT), "r"(&SC->resume) : "r1", "memory");
	SC->expected = 0; SC->quiet = 0;
	if (SC->hit != hit0) {
		SC->fg = 0x00E02020; puts_("NO IRQSTAT, STOP\n");
	} else {
		kv("IRQSTAT0", ts0);
		volatile u32 *t5 = (volatile u32 *)SOC_TIMER5_BASE;       /* +0 CFG, +4 STATE, +8 CNT, +C CNT2, +10 PRES */
		kv("T5 STATE", t5[1]);
		u32 cpsr1, en0 = VICREG(SOC_VIC0, 0x010), bit = 1u << SOC_TIMER_VIC0_BIT;
		asm volatile("mrs %0, cpsr" : "=r"(cpsr1));
		SC->irq = 0; SC->stray = 0; SC->irq_bit = bit; SC->irq_status = 0; SC->irq_vec = 0;
		SC->irq_mode = 1; SC->irq_tick = 0; SC->tim_stat = 0; SC->tim_stop = (u32)&t5[1];
		kv("EN0     ", en0);
		asm volatile("cpsid i" ::: "memory");
		VICREG(SOC_VIC0, 0x014) = 0xFFFFFFFF;                      /* INTENCLEAR: all of VIC0 off */
		hook_irq(1);
		VICREG(SOC_VIC0, 0x010) = bit;                             /* INTENABLE: the timer line only */
		t5[1] = 0;                                                 /* STATE: stop */
		t5[0] = 0x7000 | (1 << 8);                                 /* CFG: INT0/INT1/OVF enable, divider code 1 */
		t5[2] = 0x00200000;                                        /* COUNT_BUFFER (N = 2097152 counts) */
		t5[3] = 0x00200000;                                        /* COUNT_BUFFER2 */
		t5[4] = 0;                                                 /* PRESCALER */
		t5[1] = 2;                                                 /* STATE: manual update */
		u32 t_start = *(volatile u32 *)(SOC_TIMER_BASE + 0x200);
		if (STAGE == 10)
			t5[1] = 1;                                             /* STATE: start (not in the control run) */
		asm volatile("dsb sy" ::: "memory");
		asm volatile("cpsie i" ::: "memory");
		while (!SC->irq && !SC->stray && (*(volatile u32 *)(SOC_TIMER_BASE + 0x200) - t_start) < 72000000u) ;
		asm volatile("cpsid i" ::: "memory");
		t5[1] = 0; t5[0] = 0;                                      /* our timer off, interrupts off */
		hook_irq(0);                                               /* vector back BEFORE the iBSS source returns */
		u32 ts1 = *(volatile u32 *)SOC_TIMER_IRQSTAT;
		VICREG(SOC_VIC0, 0x014) = 0xFFFFFFFF;
		/* give the line back only if nothing is still asserted */
		VICREG(SOC_VIC0, 0x010) = ts1 ? (en0 & ~bit) : en0;
		u32 en_back = VICREG(SOC_VIC0, 0x010);
		if (!(cpsr1 & 0x80))
			asm volatile("cpsie i" ::: "memory");
		SC->irq_mode = 0;
		at(4, 134 + 4 * 18, SC->irq ? 0x0000B050 : 0x00E02020);
		puts_(SC->irq ? "TIMER IRQ TAKEN\n" : "NO TIMER IRQ\n");
		at(4, 134 + 5 * 18, 0x00FFFFFF);
		u32 elapsed = SC->irq_tick - t_start;
		kv("HITS    ", SC->irq);
		kv("STRAY   ", SC->stray);
		kv("H STAT  ", SC->irq_status);
		kv("H TIMST ", SC->tim_stat);
		kv("ELAPSED ", elapsed);
		kv("IRQSTAT2", ts1);
		kv("T5 STATE", t5[1]);
		kv("EN BACK ", en_back);
		result_hex("t_hits", SC->irq);
		result_hex("t_elapsed", elapsed);
		result_hex("t_timst", SC->tim_stat);
		result_hex("t_stray", SC->stray);
		/* Diagnostic: the screen and the env variable disagreed on ELAPSED once (2026-10-15). */
		result_hex("t_el_b", elapsed);
		result_hex("t_itick", SC->irq_tick);
		result_hex("t_start", t_start);
		result_hex("t_const", 0x009170A8u);
	}
#endif
#if STAGE == 12
	/* Button discovery by differences, paced by a human. OpeniBoot's button ids for this SoC look copied from an
	 * older one, so we do not trust them: read bit 0 of every GPIO pin register (GPIO + (8*port+pin)*4, SOURCE
	 * layout), learn the pins that move by themselves (noise), then ask for each button in turn, wait up to 30 s
	 * for a pin to change and list the pins that moved. Read-only: no pin is ever written or reconfigured.
	 * Results stay on the screen (and so in the transcript): no extra env variables, the iBSS env store is small. */
	#define NPIN 376
	volatile u8 *base = (volatile u8 *)SOC_SNAPSHOTS;   /* level when the prompt appeared */
	volatile u8 *noisy = base + NPIN;                     /* moved with nobody touching */
	volatile u8 *mark = base + 2 * NPIN;                  /* moved while a button was handled */
	volatile u8 *readable = base + 3 * NPIN;
	volatile u32 *gp = (volatile u32 *)SOC_GPIO_BASE;
	#define TICK (*(volatile u32 *)(SOC_TIMER_BASE + 0x200))
	#define SEC 24000000u
	at(4, 116, 0x00F0C040); puts_("STEP 12 BUTTONS\n");
	u32 nread = 0;
	SC->quiet = 1;
	for (u32 i = 0; i < NPIN; i++) {                          /* which pin registers answer */
		u32 before = SC->hit, v = 0;
		SC->expected = 1;
		asm volatile("adr.w r1, 1f\n\tstr r1, [%2]\n\tldr %0, [%1]\n1:"
			     : "+r"(v) : "r"(&gp[i]), "r"(&SC->resume) : "r1", "memory");
		SC->expected = 0;
		readable[i] = SC->hit == before;
		base[i] = (u8)(v & 1); noisy[i] = 0; mark[i] = 0;
		nread += readable[i];
	}
	SC->quiet = 0;
	at(4, 134, 0x00FFFFFF);
	kv("READABLE", nread);
	/* 10 s to get ready, then 3 s of noise measurement with hands off. */
	for (u32 s = 10; s > 0; s--) {
		console_clear_row(152);
		at(4, 152, 0x00F0C040); puts_("GET READY ");
		hex(s); fb_clean();
		u32 t0 = TICK;
		while (TICK - t0 < SEC) ;
	}
	console_clear_row(152);
	at(4, 152, 0x00F0C040); puts_("HANDS OFF 3 S"); fb_clean();
	for (u32 i = 0; i < NPIN; i++) base[i] = (u8)(gp[i] & 1);
	{
		u32 t0 = TICK;
		while (TICK - t0 < 3 * SEC)
			for (u32 i = 0; i < NPIN; i++)
				if (readable[i] && (u8)(gp[i] & 1) != base[i])
					noisy[i] = 1;
	}
	u32 nn = 0;
	for (u32 i = 0; i < NPIN; i++) nn += noisy[i];
	console_clear_row(152);
	at(4, 152, 0x00FFFFFF); kv("NOISY   ", nn);
	static const char *const names[5] = { "HOME", "POWER", "VOL UP", "VOL DOWN", "RINGER" };
	static const char *const verbs[5] = { "PRESS ", "PRESS ", "PRESS ", "PRESS ", "FLIP " };
	u32 y = 170;
	for (u32 b = 0; b < 5; b++) {
		for (u32 i = 0; i < NPIN; i++) { base[i] = (u8)(gp[i] & 1); mark[i] = 0; }   /* fresh baseline for this button */
		at(4, y, 0x00F0C040);
		puts_(verbs[b]); puts_(names[b]); puts_(" NOW"); fb_clean();
		u32 t0 = TICK, t_found = 0, last_s = 99;
		for (;;) {
			u32 now = TICK - t0;
			u32 left = 30 - now / SEC;
			if (left != last_s) {                              /* live countdown on the next row */
				last_s = left;
				console_clear_row(y + 18);
				at(4, y + 18, 0x00A0B0C0); puts_("WAITING "); hex(left); fb_clean();
			}
			for (u32 i = 0; i < NPIN; i++)
				if (readable[i] && !noisy[i] && (u8)(gp[i] & 1) != base[i]) {
					mark[i] = 1;
					if (!t_found) t_found = now ? now : 1;
				}
			if (t_found && now - t_found > SEC)                /* a pin moved: 1 s more to catch the others */
				break;
			if (now > 30 * SEC)
				break;
		}
		console_clear_row(y); console_clear_row(y + 18);
		at(4, y, t_found ? 0x0000B050 : 0x00E02020);
		puts_(names[b]);
		char list[64]; u32 n = 0, shown = 0;
		for (u32 i = 0; i < NPIN && shown < 5; i++) {
			if (!mark[i]) continue;
			list[n++] = ' ';
			list[n++] = "0123456789ABCDEF"[(i >> 3) >> 4];     /* port */
			list[n++] = "0123456789ABCDEF"[(i >> 3) & 15];
			list[n++] = '.';
			list[n++] = (char)('0' + (i & 7));                 /* pin */
			shown++;
		}
		list[n] = 0;
		puts_(t_found ? list : " NONE"); putc_('\n'); fb_clean();
		y += 18;
		if (b < 4) {                                           /* let go before the next prompt */
			u32 t1 = TICK;
			while (TICK - t1 < 2 * SEC) ;
		}
	}
	at(4, y + 18, 0x00FFFFFF);
	puts_("DONE\n");
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
	result_screen();
	result_nonce();
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
#elif STAGE == 10
	ENV_SET("trognon", "tirq-1", 1);
#elif STAGE == 11
	ENV_SET("trognon", "tirqc-1", 1);
#elif STAGE == 12
	ENV_SET("trognon", "buttons-1", 1);
#elif STAGE == 5
	ENV_SET("trognon", "regs-1", 1);
#elif STAGE == 6
	ENV_SET("trognon", "vic-1", 1);
#else
	ENV_SET("trognon", "trace-1", 1);
#endif
}
