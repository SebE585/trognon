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
#include "hal/gpio.h"
#include "hal/clock.h"
#include "hal/i2c_gpio.h"
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

#include "stages/09_ticks.inc"
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
#include "stages/03_trace.inc"
#if STAGE == 5 || STAGE == 6 || STAGE == 8
	/* Read-only register survey, shown on screen. Each load is armed as an expected data abort, so
	 * an unmapped register prints ABORT instead of resetting the phone. GPIOIC (0x39A00000) is NOT
	 * read: it dropped the USB link once (docs/en/03-hardware-map.md). Offsets from OpeniBoot
	 * (plat-s5l8920/clcd.c, gpio.c, includes/hardware/gpio.h): to be checked against the answer. */
#include "stages/05_regs.inc"
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
#include "stages/07_irq.inc"
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
#include "stages/12_buttons.inc"
#include "stages/13_nand.inc"
#include "stages/14_nand_sweep.inc"
#include "stages/15_wifi_gpio.inc"
#include "stages/16_wifi_reset.inc"
#include "stages/17_sdio_dump.inc"
#include "stages/18_sdio_dump_b.inc"
#include "stages/19_sdio_cmd0.inc"
#include "stages/20_which_timer.inc"
#include "stages/21_which_timer_b.inc"
#include "stages/22_sdio_init.inc"
#include "stages/23_gate_write.inc"
#include "stages/24_write_test.inc"
#include "stages/25_dyn_clock16.inc"
#include "stages/28_iop_clkgate.inc"
#include "stages/29_accel_i2c.inc"
#include "stages/30_bt_gpio.inc"
#include "stages/33_touch_gpio.inc"
#include "stages/34_compass_i2c.inc"
#include "stages/35_i2c_scan.inc"
#include "stages/36_pmu_read.inc"
#include "stages/37_spi1_pre.inc"
#include "stages/38_spi1_gate.inc"
#include "stages/39_pmu_w34.inc"
#include "stages/40_touch_power.inc"
#include "stages/41_touch_rst_ctl.inc"
#include "stages/42_touch_ldo.inc"
#include "stages/48_gate_snap.inc"
#include "stages/47_gate_survey.inc"
#include "stages/43_gpio_survey.inc"
#include "stages/44_spi1_nogate.inc"
#include "stages/45_spi1_selftest.inc"
#include "stages/46_spi1_selftest2.inc"
#include "stages/31_accel_axes.inc"
#include "stages/27_iop_probe_mmu.inc"
#include "stages/26_iop_regs.inc"
#include "stages/32_iop_vic.inc"
#include "stages/04_pabt.inc"
#if STAGE >= 1
	SC->expected = 0;
	hook(0);
	at(4, 450, 0x0000B050);
	puts_("UNHOOKED\n");
#endif
	result_screen();
	result_nonce();
#include "stages/00_offline.inc"
}
