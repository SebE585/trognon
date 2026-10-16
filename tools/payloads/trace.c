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
#if STAGE == 13
	/* NAND (H2FMI) chip ID probe, bus 0 (fmi0) / CE0 only, read-only. Reproduces by hand, from
	 * OpeniBoot source (plat-s5l8920/h2fmi.c): h2fmi_init_bus() -> h2fmi_nand_reset(chip 0) ->
	 * h2fmi_read_chipid(chip 0). No write to any page, no timing table, no ECC, no AES, no DMA:
	 * just the raw command sequence any NAND chip answers to a reset + READ ID. First real test
	 * of docs/research/2026-10-15-touch-wifi-nand-recon.md's decision to look at NAND before
	 * deciding bare-metal vs Linux. SOURCE for every register offset: soc/s5l8920.h. */
	#define FMI_SEC 24000000u
	volatile u32 *fmi_tick = (volatile u32 *)(SOC_TIMER_BASE + 0x200);
	at(4, 116, 0x00F0C040); puts_("STEP 13 NAND ID\n");

	/* h2fmi_init_bus(fmi0): clock on, timing cache, device_reset, device_reset_814. */
	*(volatile u32 *)SOC_CLOCK_GATE_26 |= 0xF;
	*(volatile u32 *)SOC_FMI_REG1_408 = 0xFFFF;
	*(volatile u32 *)SOC_FMI_REG2_400 = 1;
	*(volatile u32 *)SOC_FMI_REG1_408 = 0xFFFF;
	*(volatile u32 *)SOC_FMI_REG3_814 = 0;

	/* h2fmi_nand_reset(chip 0): enable CE0, reset command, wait for done (bit 0), disable CE0. */
	*(volatile u32 *)SOC_FMI_CHIPMASK_40C = 1;
	*(volatile u32 *)SOC_FMI_REG4_414 = 0xFF;
	*(volatile u32 *)SOC_FMI_REG5_410 = 1;
	u32 t0 = *fmi_tick, reset_ok = 0;
	while (*fmi_tick - t0 < FMI_SEC / 100) {         /* 10 ms, like h2fmi_wait_for_done()'s own timeout */
		if ((*(volatile u32 *)SOC_FMI_REG6_444 & 1) == 1) { reset_ok = 1; break; }
	}
	*(volatile u32 *)SOC_FMI_CHIPMASK_40C &= ~1u;
	kv("RESET_OK", reset_ok);

	u8 chipid[8] = {0};
	u32 id_ok = 0;
	if (reset_ok) {
		/* h2fmi_read_chipid(chip 0, unk=0): enable CE0, READ ID command, wait (bits 0x9), then
		 * h2fmi_read(): 8 bytes, one at a time, each preceded by command 0x50 + a short delay. */
		*(volatile u32 *)SOC_FMI_CHIPMASK_40C = 1;
		*(volatile u32 *)SOC_FMI_REG4_414 = 0x90;
		*(volatile u32 *)SOC_FMI_REG9_418 = 0;
		*(volatile u32 *)SOC_FMI_REG10_420 = 0;
		*(volatile u32 *)SOC_FMI_REG5_410 = 9;
		t0 = *fmi_tick;
		while (*fmi_tick - t0 < FMI_SEC / 100) {
			if ((*(volatile u32 *)SOC_FMI_REG6_444 & 9) == 9) { id_ok = 1; break; }
		}
		if (id_ok) {
			u32 old = *(volatile u32 *)SOC_FMI_REG1_408;
			*(volatile u32 *)SOC_FMI_REG1_408 = 0;
			*(volatile u32 *)SOC_FMI_REG8_424 = 0;
			for (u32 i = 0; i < 8; i++) {
				*(volatile u32 *)SOC_FMI_REG5_410 = 0x50;
				{ u32 d0 = *fmi_tick; while (*fmi_tick - d0 < 24) ; }   /* ~1us at 24MHz, udelay(1) */
				chipid[i] = (u8)(*(volatile u32 *)SOC_FMI_DATA_448 & 0xFF);
				*(volatile u32 *)SOC_FMI_REG5_410 = 0;
			}
			*(volatile u32 *)SOC_FMI_REG1_408 = old;
		}
		*(volatile u32 *)SOC_FMI_CHIPMASK_40C &= ~1u;
	}
	kv("ID_OK   ", id_ok);
	at(4, 152, 0x00FFFFFF); puts_("CHIPID ");
	for (u32 i = 0; i < 8; i++) {
		putc_("0123456789ABCDEF"[chipid[i] >> 4]);
		putc_("0123456789ABCDEF"[chipid[i] & 15]);
		putc_(' ');
	}
	putc_('\n'); fb_clean();
#endif
#if STAGE == 14
	/* NAND (H2FMI) chip ID sweep, CE 0-15, command sequencer on fmi0 throughout — this is
	 * exactly OpeniBoot's own first top-level call (h2fmi_init() -> h2fmi_reset_and_read_chipids
	 * (&fmi0, buff1, 0), h2fmi.c:1926, which itself loops chip 0..15). CE 8-15 route their
	 * enable/disable mask to fmi1's own register (h2fmi_enable_chip()/h2fmi_disable_chip(),
	 * h2fmi.c:338-354) even though the command/status registers stay on fmi0 for this sweep —
	 * reproduced as-is, not our own design. A second sweep with fmi1 as the command sequencer
	 * (OpeniBoot's own second call) is a separate next step, not done here. Read-only. */
	#define FMI_SEC 24000000u
	volatile u32 *fmi_tick = (volatile u32 *)(SOC_TIMER_BASE + 0x200);
	at(4, 116, 0x00F0C040); puts_("STEP 14 NAND SWEEP\n");

	*(volatile u32 *)SOC_CLOCK_GATE_26 |= 0xF;
	*(volatile u32 *)SOC_FMI_REG1_408 = 0xFFFF;
	*(volatile u32 *)SOC_FMI_REG2_400 = 1;
	*(volatile u32 *)SOC_FMI_REG1_408 = 0xFFFF;
	*(volatile u32 *)SOC_FMI_REG3_814 = 0;

	u32 y = 134;
	for (u32 chip = 0; chip < 16; chip++) {
		volatile u32 *mask_reg = (chip >> 3) ? (volatile u32 *)SOC_FMI1_CHIPMASK_40C
						      : (volatile u32 *)SOC_FMI_CHIPMASK_40C;
		u32 bit = 1u << (chip & 7);

		*mask_reg = bit;
		*(volatile u32 *)SOC_FMI_REG4_414 = 0xFF;
		*(volatile u32 *)SOC_FMI_REG5_410 = 1;
		u32 t0 = *fmi_tick, reset_ok = 0;
		while (*fmi_tick - t0 < FMI_SEC / 100)
			if ((*(volatile u32 *)SOC_FMI_REG6_444 & 1) == 1) { reset_ok = 1; break; }
		*mask_reg &= ~bit;

		u8 id[8] = {0};
		u32 id_ok = 0;
		if (reset_ok) {
			*mask_reg = bit;
			*(volatile u32 *)SOC_FMI_REG4_414 = 0x90;
			*(volatile u32 *)SOC_FMI_REG9_418 = 0;
			*(volatile u32 *)SOC_FMI_REG10_420 = 0;
			*(volatile u32 *)SOC_FMI_REG5_410 = 9;
			t0 = *fmi_tick;
			while (*fmi_tick - t0 < FMI_SEC / 100)
				if ((*(volatile u32 *)SOC_FMI_REG6_444 & 9) == 9) { id_ok = 1; break; }
			if (id_ok) {
				u32 old = *(volatile u32 *)SOC_FMI_REG1_408;
				*(volatile u32 *)SOC_FMI_REG1_408 = 0;
				*(volatile u32 *)SOC_FMI_REG8_424 = 0;
				for (u32 i = 0; i < 8; i++) {
					*(volatile u32 *)SOC_FMI_REG5_410 = 0x50;
					{ u32 d0 = *fmi_tick; while (*fmi_tick - d0 < 24) ; }
					id[i] = (u8)(*(volatile u32 *)SOC_FMI_DATA_448 & 0xFF);
					*(volatile u32 *)SOC_FMI_REG5_410 = 0;
				}
				*(volatile u32 *)SOC_FMI_REG1_408 = old;
			}
			*mask_reg &= ~bit;
		}

		at(4, y, reset_ok ? (id_ok ? 0x0000B050 : 0x00E02020) : 0x00808080);
		puts_("CE"); hex(chip); putc_(' ');
		if (reset_ok && id_ok) {
			for (u32 i = 0; i < 4; i++) {
				putc_("0123456789ABCDEF"[id[i] >> 4]);
				putc_("0123456789ABCDEF"[id[i] & 15]);
			}
		} else {
			puts_(reset_ok ? "NO ID" : "NO RESET");
		}
		putc_('\n'); fb_clean();
		y += 18;
	}
#endif
#if STAGE == 15
	/* Wi-Fi/baseband reset + SDIO wake GPIO, read-only. Pins decoded tonight from the device
	 * tree's own "function-device_reset"/"function-device_wake" properties (OIPG format),
	 * cross-checked against TheAppleWiki's independently decoded values for this exact device
	 * (both matched exactly): gpio_baseband_wlan_reset = 0x1605, gpio_sdio_device_wake = 0x1404.
	 * Same port.pin encoding as the button sweep (STAGE 12): port = high byte, pin = low byte,
	 * array index = 8*port+pin. No write, no SDIO command: just the current level of each line. */
	volatile u32 *gp = (volatile u32 *)SOC_GPIO_BASE;
	at(4, 116, 0x00F0C040); puts_("STEP 15 WIFI GPIO\n");

	u32 reset_idx = 8 * 0x16 + 0x05;   /* gpio_baseband_wlan_reset, 16.5 */
	u32 wake_idx  = 8 * 0x14 + 0x04;   /* gpio_sdio_device_wake,    14.4 */
	u32 reset_level = gp[reset_idx] & 1;
	u32 wake_level  = gp[wake_idx] & 1;

	at(4, 134, 0x00FFFFFF); puts_("RESET 16.5 = "); hex(reset_level); putc_('\n'); fb_clean();
	at(4, 152, 0x00FFFFFF); puts_("WAKE  14.4 = "); hex(wake_level); putc_('\n'); fb_clean();
	kv("RESET_LVL", reset_level);
	kv("WAKE_LVL ", wake_level);
#endif
#if STAGE == 16
	/* Wi-Fi/baseband reset line (16.5), first WRITE of the whole project to a live peripheral
	 * pin — everything before this was read-only. Reproduces gpio_custom_io() from OpeniBoot's
	 * plat-s5l8920/gpio.c (modes "clear_output"/"set_output": value/bitmask pairs read straight
	 * from that source, not invented) and the timing from plat-a4/sdio.c's sdio_reset(): assert
	 * (drive low) 12 ms, release (drive high) 150 ms. The line was already read LOW tonight
	 * (STAGE 15): this sequence re-asserts it (a no-op on the current level) then releases it —
	 * the normal "take the chip out of reset" step any boot sequence does, not a novel action on
	 * this pin. Reversible by construction: a reset line is designed to be toggled. No SDIO
	 * command sent, no NAND touched. */
	#define FMI_SEC 24000000u
	volatile u32 *fmi_tick = (volatile u32 *)(SOC_TIMER_BASE + 0x200);
	volatile u32 *gp = (volatile u32 *)SOC_GPIO_BASE;
	u32 idx = 8 * 0x16 + 0x05;   /* gpio_baseband_wlan_reset, 16.5 */
	volatile u32 *pin_reg = &gp[idx];

	at(4, 116, 0x00F0C040); puts_("STEP 16 WIFI RESET\n");
	kv("BEFORE  ", *pin_reg & 1);

	/* clear_output: value=0x212, bitmask=0x27F (gpio.c:242-245) -- assert (drive low) */
	*pin_reg = (*pin_reg & ~0x27Fu) | (0x212u & 0x27Fu);
	{ u32 t0 = *fmi_tick; while (*fmi_tick - t0 < FMI_SEC / 1000 * 12) ; }   /* 12 ms */
	kv("ASSERTED", *pin_reg & 1);

	/* set_output: value=0x213, bitmask=0x27F (gpio.c:247-250) -- release (drive high) */
	*pin_reg = (*pin_reg & ~0x27Fu) | (0x213u & 0x27Fu);
	{ u32 t0 = *fmi_tick; while (*fmi_tick - t0 < FMI_SEC / 1000 * 150) ; }   /* 150 ms */
	kv("RELEASED", *pin_reg & 1);
#endif
#if STAGE == 17
	/* SDIO register space, read-only dump. Base confirmed tonight from the device's own device
	 * tree (0x80000000), but the INTERNAL register layout is not: two incompatible candidates
	 * exist upstream (plat-s5l8900's simple CTRL/CMD/STATE layout vs plat-a4's SDHCI-standard
	 * BLK/CMD/RESP layout), neither confirmed for s5l8920. Enable the clock gate first (device
	 * tree clock-gates=0x10) so the block isn't gated off during the read, then just read 20
	 * words -- no command register write, no bus reset, nothing that assumes either layout. */
	at(4, 116, 0x00F0C040); puts_("STEP 17 SDIO DUMP\n");
	*(volatile u32 *)SOC_CLOCK_GATE_10 |= 0xF;
	{ volatile u32 *t = (volatile u32 *)(SOC_TIMER_BASE + 0x200); u32 t0 = *t; while (*t - t0 < 24000) ; }

	volatile u32 *sdio = (volatile u32 *)SOC_SDIO0_BASE;
	u32 y = 134;
	for (u32 i = 0; i < 17; i++) {   /* up to offset 0x40: standard SDHCI's "Capabilities"
					    register lives there -- a recognisable bit pattern
					    there (voltage/clock/block-size fields) would be a
					    strong signal for the plat-a4-style layout. */
		at(4, y, 0x00A0B0C0);
		puts_("+"); hex(i * 4); puts_(" "); hex(sdio[i]);
		putc_('\n'); fb_clean();
		y += 18;
	}
#endif
#if STAGE == 18
	/* Same SDIO dump as STAGE 17, but testing a different reading of the device tree's
	 * "clock-gates = 0x10" property: STAGE 17 used it as a plain gate INDEX (16, matching how
	 * h2fmi_struct_t hardcodes its own gate numbers, e.g. 0x26/0x27 directly). This stage tests
	 * it as a BITMASK instead (clock_gate_many()'s own convention elsewhere in OpeniBoot): a
	 * single bit set at position 4 (0x10 = 1<<4) would mean gate #4, not gate #16. Both gate
	 * writes are the same safe clock_gate_switch()-style read-modify-write already used
	 * tonight for the NAND bus; enabling the wrong one briefly powers an unrelated block, not a
	 * risk. */
	at(4, 116, 0x00F0C040); puts_("STEP 18 SDIO DUMP B\n");
	*(volatile u32 *)(0xBF100078 + 4 * 4) |= 0xF;   /* gate #4, the "bit 4 of 0x10" hypothesis */
	{ volatile u32 *t = (volatile u32 *)(SOC_TIMER_BASE + 0x200); u32 t0 = *t; while (*t - t0 < 24000) ; }

	volatile u32 *sdio = (volatile u32 *)SOC_SDIO0_BASE;
	u32 y = 134;
	for (u32 i = 0; i < 17; i++) {
		at(4, y, 0x00A0B0C0);
		puts_("+"); hex(i * 4); puts_(" "); hex(sdio[i]);
		putc_('\n'); fb_clean();
		y += 18;
	}
#endif
#if STAGE == 19
	/* First real SDIO bus command: CMD0 (GO_IDLE_STATE), the one SD command with no response
	 * and whose sole purpose is "reset the bus state" -- designed to be safe to send blind.
	 * Register layout hypothesis: plat-s5l8900's simple CTRL/CMD/ARGU/STATE/DSTA layout (SOURCE,
	 * not confirmed for s5l8920 -- the generation gap to A4's SDHCI-standard layout is assumed
	 * to be a later redesign, reasoned not proven). Sequence copied from h2fmi.c's own
	 * sdio_reset(), a block the ORIGINAL author left disabled behind "#if 0" (we don't know why
	 * they disabled it; citing it as the documented command format, not as proof it works):
	 *   ARGU=0; CMD=0 (index 0); wait DSTA bit0 ("ready"); CMD|=1<<31 ("execute");
	 *   wait DSTA bit2 ("transmitted"). All waits bounded (50 ms) -- a timeout here is itself
	 * the answer (bus never leaves idle), not a hang. */
	#define SDIO_CTRL  (SOC_SDIO0_BASE + 0x0)
	#define SDIO_CMD   (SOC_SDIO0_BASE + 0x8)
	#define SDIO_ARGU  (SOC_SDIO0_BASE + 0xC)
	#define SDIO_STATE (SOC_SDIO0_BASE + 0x10)
	#define SDIO_DSTA  (SOC_SDIO0_BASE + 0x18)
	volatile u32 *tick = (volatile u32 *)(SOC_TIMER_BASE + 0x200);
	#define WAIT_MS(ms, cond) do { \
		u32 _t0 = *tick; \
		while (!(cond) && (*tick - _t0) < (24000u * (ms))) ; \
	} while (0)

	at(4, 116, 0x00F0C040); puts_("STEP 19 SDIO CMD0\n");
	kv("STATE_B4", *(volatile u32 *)SDIO_STATE);
	kv("DSTA_B4 ", *(volatile u32 *)SDIO_DSTA);

	*(volatile u32 *)SDIO_ARGU = 0;
	*(volatile u32 *)SDIO_CMD = 0;

	WAIT_MS(50, *(volatile u32 *)SDIO_DSTA & 1);
	u32 dsta_ready = *(volatile u32 *)SDIO_DSTA;
	kv("DSTA_RDY", dsta_ready);

	if (dsta_ready & 1) {
		*(volatile u32 *)SDIO_CMD = *(volatile u32 *)SDIO_CMD | (1u << 31);
		WAIT_MS(50, (*(volatile u32 *)SDIO_DSTA >> 2) & 1);
	} else {
		/* Bus never left idle: that IS the result, not a reason to force the execute bit. */
	}
	kv("STATE_AF", *(volatile u32 *)SDIO_STATE);
	kv("DSTA_AF ", *(volatile u32 *)SDIO_DSTA);
	kv("CMD_AF  ", *(volatile u32 *)SDIO_CMD);
#endif
#if STAGE == 20
	/* Which timer actually causes the periodic VIC0 source-6 IRQ seen on 2026-10-15 (taken even
	 * when our own timer 5 was never started)? Timers 0-3 showed structured (non-zero) values
	 * that day -- already in active use by the iBSS, unlike timer 5. Read-only: sample each
	 * timer's CFG/STATE/COUNT_BUFFER twice, ~0.5 s apart (longer than the shortest period seen,
	 * 0.188 s), and show what changed. No IRQ taken here, no handler touched -- just two
	 * snapshots, same style as the button noise measurement (STAGE 12). Offsets: timer N base =
	 * N*0x20 for N<4 (STATE offsets 0x04/0x24/0x44/0x64 match the 2026-10-15 survey); CFG=+0,
	 * STATE=+4, COUNT_BUFFER=+8 (same layout already used for timer 5 at SOC_TIMER5_BASE). */
	at(4, 116, 0x00F0C040); puts_("STEP 20 WHICH TIMER\n");
	volatile u32 *tick = (volatile u32 *)(SOC_TIMER_BASE + 0x200);
	u32 cfg0[4], state0[4], cnt0[4];
	for (u32 n = 0; n < 4; n++) {
		volatile u32 *t = (volatile u32 *)(SOC_TIMER_BASE + n * 0x20);
		cfg0[n] = t[0]; state0[n] = t[1]; cnt0[n] = t[2];
	}
	{ u32 t0 = *tick; while (*tick - t0 < 24000000u / 2) ; }   /* 0.5 s */
	u32 y = 134;
	for (u32 n = 0; n < 4; n++) {
		volatile u32 *t = (volatile u32 *)(SOC_TIMER_BASE + n * 0x20);
		u32 cfg1 = t[0], state1 = t[1], cnt1 = t[2];
		at(4, y, (state1 != state0[n] || cnt1 != cnt0[n] || cfg1 != cfg0[n]) ? 0x0000B050 : 0x00A0B0C0);
		puts_("T"); hex(n); puts_(" "); fb_clean(); y += 18;
		at(4, y, cfg1 != cfg0[n] ? 0x0000B050 : 0x00FFFFFF);
		puts_("  CFG "); hex(cfg0[n]); puts_(" -> "); hex(cfg1); putc_('\n'); fb_clean(); y += 18;
		at(4, y, cnt1 != cnt0[n] ? 0x0000B050 : 0x00FFFFFF);
		puts_("  CNT "); hex(cnt0[n]); puts_(" -> "); hex(cnt1); putc_('\n'); fb_clean(); y += 18;
		at(4, y, state1 != state0[n] ? 0x0000B050 : 0x00FFFFFF);
		puts_("  STA "); hex(state0[n]); puts_(" -> "); hex(state1); putc_('\n'); fb_clean(); y += 18;
	}
#endif
#if STAGE == 21
	/* STAGE 20 sampled only CFG/STATE/COUNT_BUFFER (offsets +0/+4/+8) for timers 0-3 and found
	 * nothing moving over 0.5 s. Before concluding none of them is live: sample the two fields
	 * not checked yet, COUNT_BUFFER2 (+0xC) and PRESCALER (+0x10) -- the live down-counter, if
	 * there is one, may live there instead of in COUNT_BUFFER (a "reload value" register often
	 * isn't the live count itself). Same read-only, two-snapshot technique. */
	at(4, 116, 0x00F0C040); puts_("STEP 21 WHICH TIMER B\n");
	volatile u32 *tick = (volatile u32 *)(SOC_TIMER_BASE + 0x200);
	u32 b0[4], p0[4];
	for (u32 n = 0; n < 4; n++) {
		volatile u32 *t = (volatile u32 *)(SOC_TIMER_BASE + n * 0x20);
		b0[n] = t[3]; p0[n] = t[4];
	}
	{ u32 t0 = *tick; while (*tick - t0 < 24000000u / 2) ; }   /* 0.5 s */
	u32 y = 134;
	for (u32 n = 0; n < 4; n++) {
		volatile u32 *t = (volatile u32 *)(SOC_TIMER_BASE + n * 0x20);
		u32 b1 = t[3], p1 = t[4];
		u32 changed = (b1 != b0[n]) || (p1 != p0[n]);
		at(4, y, changed ? 0x0000B050 : 0x00A0B0C0);
		puts_("T"); hex(n); puts_(" B2 "); hex(b0[n]); puts_("->"); hex(b1);
		putc_('\n'); fb_clean(); y += 18;
		at(4, y, changed ? 0x0000B050 : 0x00FFFFFF);
		puts_("   PR "); hex(p0[n]); puts_("->"); hex(p1);
		putc_('\n'); fb_clean(); y += 18;
	}
#endif
#if STAGE == 22
	/* Full SDIO activation, sourced from a REAL working driver found tonight:
	 * iDroid-Project/iDroid-kernel, drivers/mmc/host/iphone-sdio.c, sdio_init() (lines 197-227).
	 * Everything in STAGE 17-19 only gated the clock and tried CMD0 directly -- this driver's
	 * own init does four more steps we never did: set SDCLK divider (CLKDIV), reset the FIFO
	 * (DCTRL), explicitly ENABLE the block (CTRL=1), and clear status/IRQ latches (STAC/IRQ).
	 * Register offsets (CTRL/CMD/ARGU/STATE/DSTA/CLKDIV/DCTRL/STAC/IRQ) match exactly what
	 * plat-s5l8900's own header already gave us -- same IP family, now confirmed by a driver
	 * that actually runs, not dead/disabled source. Clock gate: this device's own device-tree
	 * value (0x10), the most authoritative one we have (the driver's own SDIO_CLOCKGATE=0xB is
	 * for a different SoC generation, kept only as a documented alternative, not used here). */
	#define SDIO_CTRL   (SOC_SDIO0_BASE + 0x00)
	#define SDIO_DCTRL  (SOC_SDIO0_BASE + 0x04)
	#define SDIO_CMD2   (SOC_SDIO0_BASE + 0x08)
	#define SDIO_ARGU2  (SOC_SDIO0_BASE + 0x0C)
	#define SDIO_STATE2 (SOC_SDIO0_BASE + 0x10)
	#define SDIO_STAC   (SOC_SDIO0_BASE + 0x14)
	#define SDIO_DSTA2  (SOC_SDIO0_BASE + 0x18)
	#define SDIO_CLKDIV (SOC_SDIO0_BASE + 0x30)
	#define SDIO_IRQREG (SOC_SDIO0_BASE + 0x38)
	volatile u32 *tick2 = (volatile u32 *)(SOC_TIMER_BASE + 0x200);

	at(4, 116, 0x00F0C040); puts_("STEP 22 SDIO INIT\n");
	*(volatile u32 *)SOC_CLOCK_GATE_10 |= 0xF;                 /* clock gate ON (device tree value) */
	*(volatile u32 *)SDIO_CLKDIV = 1u << 7;                    /* SDCLK = PCLK/128 */
	*(volatile u32 *)SDIO_DCTRL = 0x3;                         /* reset FIFO */
	*(volatile u32 *)SDIO_DCTRL = 0x0;
	*(volatile u32 *)SDIO_CTRL = 0x1;                          /* enable SDIO, 1-bit bus */
	*(volatile u32 *)SDIO_STAC = 0xFFFFFFFF;                   /* clear status */
	*(volatile u32 *)SDIO_IRQREG = 0xFFFFFFFF;                 /* clear IRQ */

	kv("CTRL    ", *(volatile u32 *)SDIO_CTRL);
	kv("CLKDIV  ", *(volatile u32 *)SDIO_CLKDIV);
	kv("STATE   ", *(volatile u32 *)SDIO_STATE2);
	kv("DSTA    ", *(volatile u32 *)SDIO_DSTA2);

	/* Now the same CMD0 attempt as STAGE 19, with the block actually enabled this time. */
	*(volatile u32 *)SDIO_ARGU2 = 0;
	*(volatile u32 *)SDIO_CMD2 = 0;
	{ u32 t0 = *tick2; while (!(*(volatile u32 *)SDIO_DSTA2 & 1) && (*tick2 - t0) < 24000u * 50) ; }
	u32 dsta_rdy = *(volatile u32 *)SDIO_DSTA2;
	kv("DSTA_RDY", dsta_rdy);
	if (dsta_rdy & 1) {
		*(volatile u32 *)SDIO_CMD2 = *(volatile u32 *)SDIO_CMD2 | (1u << 31);
		{ u32 t0 = *tick2; while (!((*(volatile u32 *)SDIO_DSTA2 >> 2) & 1) && (*tick2 - t0) < 24000u * 50) ; }
	}
	kv("DSTA_AF ", *(volatile u32 *)SDIO_DSTA2);
	kv("STATE_AF", *(volatile u32 *)SDIO_STATE2);
#endif
#if STAGE == 23
	/* STAGE 22 wrote CTRL=1 but read it back as 0: the write itself didn't stick, not just "DSTA
	 * never goes ready". Check whether even the CLOCK GATE write persists -- if it doesn't
	 * either, this whole peripheral domain is likely not accessible for writes at this boot
	 * stage at all (gated by something above clock-gating, e.g. a power domain only the real OS
	 * enables later), not a sequence/offset mistake. Read-only except the one clock-gate write
	 * already done safely for NAND earlier. */
	at(4, 116, 0x00F0C040); puts_("STEP 23 GATE WRITE?\n");
	u32 before = *(volatile u32 *)SOC_CLOCK_GATE_10;
	kv("GATE_B4 ", before);
	*(volatile u32 *)SOC_CLOCK_GATE_10 |= 0xF;
	u32 after = *(volatile u32 *)SOC_CLOCK_GATE_10;
	kv("GATE_AF ", after);
	at(4, 170, after != before ? 0x0000B050 : 0x00E02020);
	puts_(after != before ? "WRITE STUCK\n" : "WRITE LOST\n");
	fb_clean();

	/* Same check on the NAND clock gate (0x26), already proven to work tonight -- a control. */
	u32 nb4 = *(volatile u32 *)SOC_CLOCK_GATE_26;
	*(volatile u32 *)SOC_CLOCK_GATE_26 |= 0xF;
	u32 naf = *(volatile u32 *)SOC_CLOCK_GATE_26;
	kv("NAND B4 ", nb4);
	kv("NAND AF ", naf);
#endif
#if STAGE == 24
	/* STAGE 23's clock-gate test was flawed (the gate was already 0xF from earlier runs this
	 * session, so the before/after comparison couldn't tell write-from-no-op). Proper test this
	 * time: write a recognisable, non-trivial value to the SDIO base itself and read it straight
	 * back. If THIS doesn't stick, the 0x80000000 region genuinely does not accept writes at this
	 * boot stage -- a mapping/bus-fabric fact, not a sequence mistake. */
	at(4, 116, 0x00F0C040); puts_("STEP 24 WRITE TEST\n");
	volatile u32 *sdio_base = (volatile u32 *)SOC_SDIO0_BASE;
	u32 before = *sdio_base;
	*sdio_base = 0xDEADBEEF;
	u32 after = *sdio_base;
	kv("BEFORE  ", before);
	kv("WROTE   ", 0xDEADBEEF);
	kv("READBACK", after);
	at(4, 188, after == 0xDEADBEEF ? 0x0000B050 : 0x00E02020);
	puts_(after == 0xDEADBEEF ? "WRITE STUCK\n" : "WRITE LOST\n");
	fb_clean();
	*sdio_base = before;   /* restore, for tidiness -- harmless either way given the result above */
#endif
#if STAGE == 25
	/* Read-only: is the "dynamic clock" register at the same index (16) as the device tree's
	 * SDIO clock-gate still what clock_setup() configured at boot (plat-s5l8920/clock.c:159-199,
	 * expected ocfg=0x718 for clock_setup_table[16]=0x318, div=24 selector=3)? DYNAMIC_CLOCK_BASE
	 * (0xBF100010) and CLOCK_GATE_BASE (0xBF100078) are exactly 25 words apart -- index 16 exists
	 * in both tables, a real structural coincidence worth checking, not a label. No write: this
	 * stage only reads. */
	at(4, 116, 0x00F0C040); puts_("STEP 25 DYN CLOCK16\n");
	u32 dyn16 = *(volatile u32 *)(0xBF100010 + 16 * 4);
	kv("DYN16   ", dyn16);
	kv("EXPECTED", 0x718);
	at(4, 170, dyn16 == 0x718 ? 0x0000B050 : 0x00E02020);
	puts_(dyn16 == 0x718 ? "MATCHES BOOT CFG\n" : "DIFFERENT\n");
	fb_clean();
#endif
#if STAGE == 28
	/* iop_probe, step 2 (read-only part): the device tree's "iop" node has its own
	 * clock-gates = 0x27 (39). AppleARM7M::start()'s own disassembly (read in full,
	 * 2026-10-16) has no call enabling it and no call to "function-device_reset" either --
	 * both are candidate explanations for STEP 26's all-zero read, but neither was found as
	 * something this specific driver does itself. Check the gate's current state directly,
	 * read-only, before guessing any further. */
	at(4, 116, 0x00F0C040); puts_("STEP 28 IOP CLKGATE\n");
	u32 gate = *(volatile u32 *)SOC_CLOCK_GATE_27;
	kv("GATE+27 ", gate);
	result_hex("gate27", gate);
	fb_clean();
#endif
#if STAGE == 27
	/* iop_probe, step 1: close the mapping question before touching the IOP register window
	 * again -- read the MMU's own L1 descriptor for VA SOC_IOP_REG_BASE (0x86300000) instead
	 * of assuming identity mapping or a "set the high bit" rule. The device tree's own
	 * "ranges" property on /device-tree/arm-io (0x0,0x80000000,0x40000000 then
	 * 0x40000000,0x40000000,0x40000000 -- i.e. "+0x80000000" only for child addresses below
	 * 0x40000000, identity above) already confirms the translation used to reach
	 * 0x86300000/0xbf300000 from the reg property's 0x06300000/0x3f300000; this step checks
	 * the OTHER half, the CPU's own page table, read-only, no write at all. */
	at(4, 116, 0x00F0C040); puts_("STEP 27 IOP MMU\n");
	u32 ttbr0; asm volatile("mrc p15, 0, %0, c2, c0, 0" : "=r"(ttbr0));
	u32 va = SOC_IOP_REG_BASE;
	u32 l1_addr = (ttbr0 & 0xFFFFC000) + ((va >> 20) * 4);
	u32 l1 = *(volatile u32 *)l1_addr;
	u32 type = l1 & 0x3;                 /* 0=fault 1=page table 2=section 3=reserved */
	u32 sect_base = l1 & 0xFFF00000;     /* valid only if type == 2 */
	u32 domain = (l1 >> 5) & 0xF;
	u32 ap     = (l1 >> 10) & 0x3;
	u32 tex    = (l1 >> 12) & 0x7;
	u32 c_bit  = (l1 >> 3) & 1;
	u32 b_bit  = (l1 >> 2) & 1;
	kv("TTBR0   ", ttbr0);
	kv("L1ADDR  ", l1_addr);
	kv("L1DESC  ", l1);
	kv("TYPE    ", type);
	kv("SECTBASE", sect_base);
	kv("DOMAIN  ", domain);
	kv("AP:TEXCB", (ap << 8) | (tex << 4) | (c_bit << 1) | b_bit);
	at(4, 350, type == 2 && sect_base == (va & 0xFFF00000) ? 0x0000B050 : 0x00E02020);
	puts_(type == 2 && sect_base == (va & 0xFFF00000) ? "IDENTITY SECTION\n" : "NOT WHAT WE ASSUMED\n");
	result_hex("ttbr0", ttbr0);
	result_hex("l1desc", l1);
	result_hex("l1type", type);
	result_hex("l1base", sect_base);
	result_hex("l1dom", domain);
	result_hex("l1aptex", (ap << 8) | (tex << 4) | (c_bit << 1) | b_bit);
	fb_clean();
#endif
#if STAGE == 26
	/* First look at the IOP's own control/boot register window (SOC_IOP_REG_BASE =
	 * 0x86300000), read-only, no write at all: does the main CPU even see this address as
	 * mapped/live, and if so, what does it currently hold? This is the milestone-1 dynamic
	 * check from docs/research/2026-10-16-wifi-iop-architecture.md -- offsets +0x100
	 * (control/status), +0x110 (firmware pointer), +0x114 (size), +0x118 (self-address) are
	 * what AppleARM7M::startCPU writes on the host's own boot path; reading them here does
	 * not tell us whether the IOP core itself is executing, only whether this window is
	 * reachable and what state it is in right now, under our own tethered DFU/iBSS path
	 * (which skips the normal iOS boot that would normally run AppleARM7M::start()). */
	at(4, 116, 0x00F0C040); puts_("STEP 26 IOP REGS\n");
	volatile u32 *iop = (volatile u32 *)SOC_IOP_REG_BASE;
	u32 r000 = iop[0x000 / 4];
	u32 r100 = iop[0x100 / 4];
	u32 r110 = iop[0x110 / 4];
	u32 r114 = iop[0x114 / 4];
	u32 r118 = iop[0x118 / 4];
	kv("IOP+000 ", r000);
	kv("IOP+100 ", r100);
	kv("IOP+110 ", r110);
	kv("IOP+114 ", r114);
	kv("IOP+118 ", r118);
	result_hex("iop000", r000);
	result_hex("iop100", r100);
	result_hex("iop110", r110);
	result_hex("iop114", r114);
	result_hex("iop118", r118);
	fb_clean();
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
#elif STAGE == 13
	ENV_SET("trognon", "nand-1", 1);
#elif STAGE == 14
	ENV_SET("trognon", "nand-sweep-1", 1);
#elif STAGE == 15
	ENV_SET("trognon", "wifi-gpio-1", 1);
#elif STAGE == 16
	ENV_SET("trognon", "wifi-reset-1", 1);
#elif STAGE == 17
	ENV_SET("trognon", "sdio-dump-1", 1);
#elif STAGE == 18
	ENV_SET("trognon", "sdio-dump-2", 1);
#elif STAGE == 19
	ENV_SET("trognon", "sdio-cmd0-1", 1);
#elif STAGE == 20
	ENV_SET("trognon", "which-timer-1", 1);
#elif STAGE == 21
	ENV_SET("trognon", "which-timer-2", 1);
#elif STAGE == 22
	ENV_SET("trognon", "sdio-init-1", 1);
#elif STAGE == 23
	ENV_SET("trognon", "gate-write-1", 1);
#elif STAGE == 24
	ENV_SET("trognon", "write-test-1", 1);
#elif STAGE == 25
	ENV_SET("trognon", "dyn-clock16-1", 1);
#elif STAGE == 26
	ENV_SET("trognon", "iop-regs-1", 1);
#elif STAGE == 27
	ENV_SET("trognon", "iop-mmu-1", 1);
#elif STAGE == 28
	ENV_SET("trognon", "iop-clkgate-1", 1);
#elif STAGE == 5
	ENV_SET("trognon", "regs-1", 1);
#elif STAGE == 6
	ENV_SET("trognon", "vic-1", 1);
#else
	ENV_SET("trognon", "trace-1", 1);
#endif
}
