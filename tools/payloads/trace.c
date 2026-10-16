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

#if STAGE == 29 || STAGE == 31 || STAGE == 34 || STAGE == 35 || STAGE == 36 || STAGE == 39 || STAGE == 40 || STAGE == 41 || STAGE == 42
/* Accelerometer probe over bit-banged i2c0 (decoded 2026-10-16 from the device tree, see
 * soc/s5l8920.h for the full chain). This is the FIRST bit-bang on these two pins: STAGE 29
 * reads their idle level with both held as inputs (no write at all) before doing anything
 * else -- if there is no pull-up on this bus, that read-only step already tells us (both
 * read 0 and stay 0), without risking a short. Only if that looks sane does it go on to a
 * real I2C transaction (START, address+write, register address, repeated START,
 * address+read, one byte, STOP) asking for the accelerometer's WHO_AM_I (register 0x0F).
 * GPIO mode encoding is OpeniBoot's own gpio_custom_io() (plat-s5l8920/gpio.c):
 * use_as_input = value 0x210 / mask 0x27E (releases the pin, no level forced -- this is what
 * makes the open-drain emulation safe: we only ever drive LOW or let go, never drive HIGH),
 * clear_output = value 0x212 / mask 0x27F (drive low). Bit timing is generous (~20 us/half-bit,
 * well under I2C "standard mode" 100 kHz) using the already-confirmed 24.001 MHz free-running
 * tick. Reversible by construction: an I2C bus is designed to be idle (both lines released)
 * between transactions, which is exactly the state this leaves. Plain file-scope functions,
 * not nested ones: a nested function under GCC needs a stack trampoline, which has no defined
 * behaviour on this bare-metal, MMU-mapped, cacheable payload section. */
static volatile u32 *i2c_tick29 = (volatile u32 *)(SOC_TIMER_BASE + 0x200);

static void i2c_mode(volatile u32 *reg, u32 value, u32 mask) { *reg = (*reg & ~mask) | (value & mask); }
static void i2c_release(volatile u32 *reg) { i2c_mode(reg, 0x210, 0x27E); }
static void i2c_low(volatile u32 *reg)     { i2c_mode(reg, 0x212, 0x27F); }
static u32  i2c_level(volatile u32 *reg)   { return *reg & 1; }
static void i2c_delay(u32 us) { u32 t0 = *i2c_tick29; while (*i2c_tick29 - t0 < 24u * us) ; }

static void i2c_start(volatile u32 *scl, volatile u32 *sda) {
	i2c_release(sda); i2c_release(scl); i2c_delay(20);
	i2c_low(sda); i2c_delay(20);
	i2c_low(scl); i2c_delay(20);
}
static void i2c_stop(volatile u32 *scl, volatile u32 *sda) {
	i2c_low(sda); i2c_delay(20);
	i2c_release(scl); i2c_delay(20);
	i2c_release(sda); i2c_delay(20);
}
static void i2c_wbit(volatile u32 *scl, volatile u32 *sda, u32 bit) {
	if (bit) i2c_release(sda); else i2c_low(sda);
	i2c_delay(20);
	i2c_release(scl); i2c_delay(20);
	i2c_low(scl); i2c_delay(20);
}
static u32 i2c_rbit(volatile u32 *scl, volatile u32 *sda) {
	i2c_release(sda); i2c_delay(10);
	i2c_release(scl); i2c_delay(20);
	u32 b = i2c_level(sda);
	i2c_low(scl); i2c_delay(20);
	return b;
}
static u32 i2c_wbyte(volatile u32 *scl, volatile u32 *sda, u32 byte) {
	for (int i = 7; i >= 0; i--) i2c_wbit(scl, sda, (byte >> i) & 1);
	return !i2c_rbit(scl, sda);   /* ACK = slave pulls SDA low */
}
static u32 i2c_rbyte(volatile u32 *scl, volatile u32 *sda) {
	u32 byte = 0;
	for (int i = 0; i < 8; i++) byte = (byte << 1) | i2c_rbit(scl, sda);
	i2c_wbit(scl, sda, 1);   /* master NACK: single-byte read, no more wanted */
	return byte;
}
#if STAGE == 39 || STAGE == 40 || STAGE == 41 || STAGE == 42
/* PMU (i2c0 0x74) one-byte register read and write. Read: write the register number, repeated start,
 * read one byte (same sequence as STAGE 36). Write: START, address+write, register number, data byte, STOP;
 * returns 1 only if all three bytes were acknowledged. */
static u32 pmu_read(volatile u32 *scl, volatile u32 *sda, u32 reg, u32 *ok) {
	i2c_start(scl, sda);
	u32 a1 = i2c_wbyte(scl, sda, (0x74 << 1) | 0);
	u32 a2 = i2c_wbyte(scl, sda, reg);
	i2c_start(scl, sda);
	u32 a3 = i2c_wbyte(scl, sda, (0x74 << 1) | 1);
	u32 v = i2c_rbyte(scl, sda);
	i2c_stop(scl, sda);
	i2c_delay(200);
	*ok = a1 & a2 & a3;
	return v;
}
static u32 pmu_write(volatile u32 *scl, volatile u32 *sda, u32 reg, u32 val) {
	i2c_start(scl, sda);
	u32 a1 = i2c_wbyte(scl, sda, (0x74 << 1) | 0);
	u32 a2 = i2c_wbyte(scl, sda, reg);
	u32 a3 = i2c_wbyte(scl, sda, val);
	i2c_stop(scl, sda);
	i2c_delay(200);
	return a1 & a2 & a3;
}
#endif
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
#if STAGE == 29
	at(4, 116, 0x00F0C040); puts_("STEP 29 ACCEL I2C\n");

	volatile u32 *gp29  = (volatile u32 *)SOC_GPIO_BASE;
	volatile u32 *scl29 = &gp29[SOC_I2C0_SCL_IDX];
	volatile u32 *sda29 = &gp29[SOC_I2C0_SDA_IDX];

	i2c_release(scl29); i2c_release(sda29); i2c_delay(20);
	u32 scl_idle = i2c_level(scl29), sda_idle = i2c_level(sda29);
	kv("SCL_IDLE", scl_idle);
	kv("SDA_IDLE", sda_idle);
	result_hex("scl_idle", scl_idle);
	result_hex("sda_idle", sda_idle);

	u32 who = 0, ack_aw = 0, ack_reg = 0, ack_ar = 0;
	if (scl_idle && sda_idle) {   /* a pulled-up idle bus; anything else means this is not wired as expected */
		i2c_start(scl29, sda29);
		ack_aw  = i2c_wbyte(scl29, sda29, (SOC_ACCEL_I2C_ADDR << 1) | 0);
		ack_reg = i2c_wbyte(scl29, sda29, SOC_ACCEL_WHO_AM_I_REG);
		i2c_start(scl29, sda29);   /* repeated START */
		ack_ar  = i2c_wbyte(scl29, sda29, (SOC_ACCEL_I2C_ADDR << 1) | 1);
		who = i2c_rbyte(scl29, sda29);
		i2c_stop(scl29, sda29);
	} else {
		puts_("BUS NOT IDLE HIGH, SKIPPED\n");
	}
	kv("ACK_ADDRW", ack_aw);
	kv("ACK_REG  ", ack_reg);
	kv("ACK_ADDRR", ack_ar);
	kv("WHO_AM_I ", who);
	result_hex("ack_addrw", ack_aw);
	result_hex("ack_reg", ack_reg);
	result_hex("ack_addrr", ack_ar);
	result_hex("who_am_i", who);
	fb_clean();
#endif
#if STAGE == 30
	/* Bluetooth GPIO levels, read-only (same pattern as STAGE 15 for Wi-Fi reset/wake): just the
	 * current level of the reset (15.06) and wake (16.02) lines, decoded tonight from the device
	 * tree's "function-bt_reset"/"function-bt_wake" properties. No write, no UART touched. */
	at(4, 116, 0x00F0C040); puts_("STEP 30 BT GPIO\n");
	volatile u32 *gp = (volatile u32 *)SOC_GPIO_BASE;
	u32 bt_reset = gp[SOC_BT_RESET_IDX] & 1;
	u32 bt_wake  = gp[SOC_BT_WAKE_IDX] & 1;
	kv("RESET_15.6", bt_reset);
	kv("WAKE_16.2 ", bt_wake);
	result_hex("bt_reset", bt_reset);
	result_hex("bt_wake", bt_wake);
	fb_clean();
#endif
#if STAGE == 33
	/* Touch controller reset (14.01) and chip-select enable (13.00), READ-ONLY. The pins'
	 * configuration is not changed and no value is written to them: each GPIO register is read once, and both the raw
	 * word and its bit 0 are reported. The Bluetooth reset line, already read on the device
	 * (STAGE 30: level 1), is read in the same run as a control, so a run that reads it
	 * differently is flagged as not comparable. A level says nothing about polarity (active
	 * high or low), nor about who drives the pin, nor that the controller works. The
	 * attention line is an interrupt on the GPIO interrupt controller and is deliberately
	 * not touched. */
	at(4, 116, 0x00F0C040); puts_("STEP 33 TOUCH GPIO\n");
	volatile u32 *gp = (volatile u32 *)SOC_GPIO_BASE;
	u32 t_rst_raw = gp[SOC_TOUCH_RESET_IDX];
	u32 t_cs_raw  = gp[SOC_TOUCH_CS_EN_IDX];
	u32 bt_ctl    = gp[SOC_BT_RESET_IDX];
	kv("RST_14.1 raw", t_rst_raw);
	kv("CSEN_13.0 raw", t_cs_raw);
	kv("BT_RST ctl raw", bt_ctl);
	result_hex("touch_reset_raw", t_rst_raw);
	result_hex("touch_reset", t_rst_raw & 1);
	result_hex("touch_cs_en_raw", t_cs_raw);
	result_hex("touch_cs_en", t_cs_raw & 1);
	result_hex("bt_reset_control", bt_ctl & 1);
	fb_clean();
#endif
#if STAGE == 34
	/* Magnetometer presence probe over the same bit-banged i2c0, no data byte written to any device register. For each of
	 * four addresses it makes two minimal transactions: (A) START, address+write, STOP, which only
	 * asks "does anything acknowledge this address"; (B) only if A was acknowledged, START,
	 * address+read, ONE byte, STOP -- a current-address read, so the chip's register pointer is
	 * never set by us. 0x1d is the accelerometer, which answered in STAGE 29/31 (positive
	 * control); 0x1c is expected to be silent (negative control); 0x1e and 0x1f are the two
	 * "compass" device-tree nodes (AKM8973S). The byte read from 0x1e/0x1f is whatever the chip
	 * returns from its current pointer and is NOT interpreted here: no register map has been
	 * checked against a datasheet. */
	at(4, 116, 0x00F0C040); puts_("STEP 34 COMPASS I2C\n");
	volatile u32 *gp34  = (volatile u32 *)SOC_GPIO_BASE;
	volatile u32 *scl34 = &gp34[SOC_I2C0_SCL_IDX];
	volatile u32 *sda34 = &gp34[SOC_I2C0_SDA_IDX];
	i2c_release(scl34); i2c_release(sda34); i2c_delay(20);
	u32 scl_idle = i2c_level(scl34), sda_idle = i2c_level(sda34);
	kv("SCL_IDLE", scl_idle);
	kv("SDA_IDLE", sda_idle);
	result_hex("scl_idle", scl_idle);
	result_hex("sda_idle", sda_idle);
	static const u32 addrs34[4] = { 0x1c, 0x1d, 0x1e, 0x1f };
	static const char *const nm_aw[4] = { "aw_1c", "aw_1d", "aw_1e", "aw_1f" };
	static const char *const nm_ar[4] = { "ar_1c", "ar_1d", "ar_1e", "ar_1f" };
	static const char *const nm_b[4]  = { "b_1c",  "b_1d",  "b_1e",  "b_1f"  };
	if (scl_idle && sda_idle) {
		for (int k = 0; k < 4; k++) {
			u32 aw = 0, ar = 0, b = 0;
			i2c_start(scl34, sda34);
			aw = i2c_wbyte(scl34, sda34, (addrs34[k] << 1) | 0);
			i2c_stop(scl34, sda34);
			i2c_delay(200);
			if (aw) {
				i2c_start(scl34, sda34);
				ar = i2c_wbyte(scl34, sda34, (addrs34[k] << 1) | 1);
				if (ar) b = i2c_rbyte(scl34, sda34);
				i2c_stop(scl34, sda34);
				i2c_delay(200);
			}
			kv(nm_aw[k], aw);
			kv(nm_b[k], b);
			result_hex(nm_aw[k], aw);
			result_hex(nm_ar[k], ar);
			result_hex(nm_b[k], b);
		}
	} else {
		puts_("BUS NOT IDLE HIGH, SKIPPED\n");
	}
	fb_clean();
#endif
#if STAGE == 35
	/* (1) Address scan of i2c0 (7-bit addresses 0x08..0x77): for each, START, address+write, STOP,
	 * recording only whether anything acknowledged. No data byte is sent to any device. Shows what
	 * answers on this bus without assuming the device tree is complete; a part that is unpowered
	 * or held in reset does not answer, so silence is not absence. (2) Random reads of the AK8973's
	 * read-only registers at 0x1e (datasheet MS0561-E-01: ST 0xC0, TMPS 0xC1, H1X/H1Y/H1Z
	 * 0xC2..0xC4) and of MS1 0xE0, whose reset value is 0x03 (power-down). The AK8973 has no ID
	 * register, so this is a consistency check by reset state, not an identification. No data byte is
	 * written to any device register; the register-address byte of each random read is sent, and the
	 * I2C routines drive the two bus pins (GPIO) to produce the transactions. */
	at(4, 116, 0x00F0C040); puts_("STEP 35 I2C SCAN\n");
	volatile u32 *gp35  = (volatile u32 *)SOC_GPIO_BASE;
	volatile u32 *scl35 = &gp35[SOC_I2C0_SCL_IDX];
	volatile u32 *sda35 = &gp35[SOC_I2C0_SDA_IDX];
	i2c_release(scl35); i2c_release(sda35); i2c_delay(20);
	u32 scl_idle = i2c_level(scl35), sda_idle = i2c_level(sda35);
	result_hex("scl_idle", scl_idle);
	result_hex("sda_idle", sda_idle);
	volatile u32 scan[4];   /* explicit stores: a brace initialiser makes GCC call memset, absent here */
	scan[0] = 0; scan[1] = 0; scan[2] = 0; scan[3] = 0;
	u32 nack = 0;
	if (scl_idle && sda_idle) {
		for (u32 a = 0x08; a <= 0x77; a++) {
			i2c_start(scl35, sda35);
			u32 ok = i2c_wbyte(scl35, sda35, (a << 1) | 0);
			i2c_stop(scl35, sda35);
			i2c_delay(100);
			if (ok) { scan[a >> 5] |= 1u << (a & 31); nack++; kv("ACK addr", a); }
		}
		result_hex("scan_w0", scan[0]);
		result_hex("scan_w1", scan[1]);
		result_hex("scan_w2", scan[2]);
		result_hex("scan_w3", scan[3]);
		result_hex("scan_count", nack);
		static const u32 regs35[12] = { 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6 };
		static const char *const nm35[12] = { "r_c0", "r_c1", "r_c2", "r_c3", "r_c4", "r_e0", "r_e1", "r_e2", "r_e3", "r_e4", "r_e5", "r_e6" };
		static const char *const ok35[12] = { "ok_c0", "ok_c1", "ok_c2", "ok_c3", "ok_c4", "ok_e0", "ok_e1", "ok_e2", "ok_e3", "ok_e4", "ok_e5", "ok_e6" };
		if (scan[SOC_COMPASS_I2C_ADDR0 >> 5] & (1u << (SOC_COMPASS_I2C_ADDR0 & 31))) {
			for (int k = 0; k < 12; k++) {
				i2c_start(scl35, sda35);
				u32 a1 = i2c_wbyte(scl35, sda35, (SOC_COMPASS_I2C_ADDR0 << 1) | 0);
				u32 a2 = i2c_wbyte(scl35, sda35, regs35[k]);
				i2c_start(scl35, sda35);
				u32 a3 = i2c_wbyte(scl35, sda35, (SOC_COMPASS_I2C_ADDR0 << 1) | 1);
				u32 v = i2c_rbyte(scl35, sda35);
				i2c_stop(scl35, sda35);
				i2c_delay(200);
				kv(nm35[k], v);
				result_hex(nm35[k], v);
				result_hex(ok35[k], a1 & a2 & a3);
			}
		}
	} else {
		puts_("BUS NOT IDLE HIGH, SKIPPED\n");
	}
	fb_clean();
#endif
#if STAGE == 36
	/* PMU register READS at i2c0 0x74 (device tree "pmu,d1755"). Registers chosen from a static
	 * reading of the AppleD1755 driver (docs/research/2026-10-16-touch-n88-preconditions.md):
	 * 0x10 and 0x11 (LDO on/off bits; the touch LDO is bit 0x40 of 0x11), 0x17..0x22 (LDO
	 * voltage/auxiliary registers) and 0x50..0x57 (per-GPIO configuration). Each is a random read:
	 * write the register address, repeated start, read one byte. No data byte is written to any PMU
	 * register; the I2C routines drive the two bus pins. The meaning of the bytes is NOT decided
	 * here; the run only records what the PMU holds at this point of the boot. */
	at(4, 116, 0x00F0C040); puts_("STEP 36 PMU READ\n");
	volatile u32 *gp36  = (volatile u32 *)SOC_GPIO_BASE;
	volatile u32 *scl36 = &gp36[SOC_I2C0_SCL_IDX];
	volatile u32 *sda36 = &gp36[SOC_I2C0_SDA_IDX];
	i2c_release(scl36); i2c_release(sda36); i2c_delay(20);
	u32 scl_idle = i2c_level(scl36), sda_idle = i2c_level(sda36);
	result_hex("scl_idle", scl_idle);
	result_hex("sda_idle", sda_idle);
	static const u32 pmu_regs[22] = { 0x10, 0x11, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E,
	                                  0x1F, 0x20, 0x21, 0x22, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55,
	                                  0x56, 0x57 };
	static const char *const pmu_val[22] = { "p10","p11","p17","p18","p19","p1a","p1b","p1c","p1d","p1e",
	                                         "p1f","p20","p21","p22","p50","p51","p52","p53","p54","p55",
	                                         "p56","p57" };
	static const char *const pmu_ok[22]  = { "k10","k11","k17","k18","k19","k1a","k1b","k1c","k1d","k1e",
	                                         "k1f","k20","k21","k22","k50","k51","k52","k53","k54","k55",
	                                         "k56","k57" };
	if (scl_idle && sda_idle) {
		for (int k = 0; k < 22; k++) {
			i2c_start(scl36, sda36);
			u32 a1 = i2c_wbyte(scl36, sda36, (0x74 << 1) | 0);
			u32 a2 = i2c_wbyte(scl36, sda36, pmu_regs[k]);
			i2c_start(scl36, sda36);
			u32 a3 = i2c_wbyte(scl36, sda36, (0x74 << 1) | 1);
			u32 v = i2c_rbyte(scl36, sda36);
			i2c_stop(scl36, sda36);
			i2c_delay(200);
			kv(pmu_val[k], v);
			result_hex(pmu_val[k], v);
			result_hex(pmu_ok[k], a1 & a2 & a3);
		}
	} else {
		puts_("BUS NOT IDLE HIGH, SKIPPED\n");
	}
	fb_clean();
#endif
#if STAGE == 37
	/* spi1 preconditions, READ-ONLY. (1) Clock gate registers at 0xBF100078 + 4*g for the SPI gates
	 * named by OpeniBoot (0x9..0xD) and the neighbourhood of the device tree's 0x16 (0x15..0x19);
	 * the gate read in STAGE 28 (0x27) was 0x0000000F. (2) Only if the device tree's gate 0x16 reads
	 * with its low nibble 0xF (the pattern seen on an enabled gate), the idle registers of spi1:
	 * CONTROL, SETUP, STATUS, PIN, CLKDIVIDER, SPCNT, SPIDD. If not, they are skipped: reading a
	 * block whose clock is off may stall the bus, and this project has already lost the USB link to
	 * one such read. RXDATA and TXDATA are never read. */
	at(4, 116, 0x00F0C040); puts_("STEP 37 SPI1 PRECOND\n");
	volatile u32 *gates37 = (volatile u32 *)SOC_CLOCK_GATE_BASE;
	static const u32 gl37[10] = { 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x15, 0x16, 0x17, 0x18, 0x19 };
	static const char *const gn37[10] = { "g09","g0a","g0b","g0c","g0d","g15","g16","g17","g18","g19" };
	volatile u32 gv37[10];
	for (int k = 0; k < 10; k++) {
		gv37[k] = gates37[gl37[k]];
		kv(gn37[k], gv37[k]);
		result_hex(gn37[k], gv37[k]);
	}
	u32 gate16 = gates37[SOC_SPI1_GATE_DT];
	u32 spi_read = ((gate16 & 0xF) == 0xF) ? 1 : 0;
	result_hex("spi_read", spi_read);
	if (spi_read) {
		volatile u32 *sp = (volatile u32 *)SOC_SPI1_BASE;
		u32 v_ctrl = sp[0x00 / 4], v_setup = sp[0x04 / 4], v_status = sp[0x08 / 4], v_pin = sp[0x0C / 4];
		u32 v_div = sp[0x30 / 4], v_cnt = sp[0x34 / 4], v_idd = sp[0x38 / 4];
		kv("CTRL ", v_ctrl); kv("SETUP", v_setup); kv("STATUS", v_status); kv("PIN  ", v_pin);
		kv("CLKDIV", v_div); kv("SPCNT", v_cnt); kv("SPIDD", v_idd);
		result_hex("s_ctrl", v_ctrl); result_hex("s_setup", v_setup); result_hex("s_status", v_status);
		result_hex("s_pin", v_pin); result_hex("s_clkdiv", v_div); result_hex("s_cnt", v_cnt);
		result_hex("s_idd", v_idd);
	} else {
		puts_("GATE 0x16 NOT ENABLED, SPI REGS SKIPPED\n");
	}
	fb_clean();
#endif
#if STAGE == 38
	/* W1 + W2 of the touch write plan (docs/research/2026-10-16-touch-n88-preconditions.md).
	 * THE ONLY WRITES OF THIS PAYLOAD are to the spi1 clock-gate register, address
	 * SOC_CLOCK_GATE_BASE + 4 * 0x16 = 0xBF1000D0: (1) gate |= 0xF, (2) at the end, the register is
	 * written back with the value read before (0x0 expected). Nothing else is written: no PMU, no
	 * GPIO, no SPI register. Steps: read the gate; write gate | 0xF; wait; read it back; only if the
	 * low nibble reads 0xF, read the spi1 idle registers (CONTROL, SETUP, STATUS, PIN, CLKDIVIDER,
	 * SPCNT, SPIDD; never RXDATA or TXDATA, a FIFO read may pop); restore; read back again. If the
	 * gate does not take the write, the register reads are skipped and the restore is still done. */
	at(4, 116, 0x00F0C040); puts_("STEP 38 SPI1 GATE W1+W2\n");
	volatile u32 *gate38 = (volatile u32 *)(SOC_CLOCK_GATE_BASE + 4 * SOC_SPI1_GATE_DT);
	u32 g_before = *gate38;
	kv("GATE before", g_before);
	result_hex("g_before", g_before);
	*gate38 = g_before | 0xF;                               /* W1: the only write */
	for (volatile u32 d = 0; d < 0x10000; d++) ;
	u32 g_after = *gate38;
	kv("GATE after W", g_after);
	result_hex("g_after_write", g_after);
	u32 took = ((g_after & 0xF) == 0xF) ? 1 : 0;
	result_hex("gate_took", took);
	if (took) {
		volatile u32 *sp = (volatile u32 *)SOC_SPI1_BASE;
		u32 v_ctrl = sp[0x00 / 4], v_setup = sp[0x04 / 4], v_status = sp[0x08 / 4], v_pin = sp[0x0C / 4];
		u32 v_div = sp[0x30 / 4], v_cnt = sp[0x34 / 4], v_idd = sp[0x38 / 4];
		kv("CTRL ", v_ctrl); kv("SETUP", v_setup); kv("STATUS", v_status); kv("PIN  ", v_pin);
		kv("CLKDIV", v_div); kv("SPCNT", v_cnt); kv("SPIDD", v_idd);
		result_hex("s_ctrl", v_ctrl); result_hex("s_setup", v_setup); result_hex("s_status", v_status);
		result_hex("s_pin", v_pin); result_hex("s_clkdiv", v_div); result_hex("s_cnt", v_cnt);
		result_hex("s_idd", v_idd);
	} else {
		puts_("GATE DID NOT TAKE THE WRITE, SPI REGS SKIPPED\n");
	}
	*gate38 = g_before;                                     /* restore: the second and last write */
	for (volatile u32 d = 0; d < 0x10000; d++) ;
	u32 g_restored = *gate38;
	kv("GATE restored", g_restored);
	result_hex("g_restored", g_restored);
	fb_clean();
#endif
#if STAGE == 39
	/* W3 + W4 of the touch write plan (docs/research/2026-10-16-touch-n88-preconditions.md).
	 * THE ONLY WRITES OF THIS PAYLOAD are two PMU registers at i2c0 0x74, each written once to change and
	 * once to restore: register 0x11 (bit 0x40: the touch LDO's enable bit by the static reading) and register
	 * 0x50 (PMU GPIO 0, the touch clock enable by the static reading). Guard: the registers are read first and
	 * NOTHING is written unless 0x11 reads 0xBE and 0x50 reads 0x11 (the values seen in STAGE 36); any other
	 * value, or a missing acknowledge, aborts without a write. Sequence: write 0x11 = old | 0x40; read back;
	 * write 0x50 = (old & 0x19) | 0x02; read back; wait; read both again; restore 0x50, then 0x11, to the
	 * values read at the start; read both again. No SoC GPIO, clock gate or SPI register is touched. */
	at(4, 116, 0x00F0C040); puts_("STEP 39 PMU W3+W4\n");
	volatile u32 *gp39  = (volatile u32 *)SOC_GPIO_BASE;
	volatile u32 *scl39 = &gp39[SOC_I2C0_SCL_IDX];
	volatile u32 *sda39 = &gp39[SOC_I2C0_SDA_IDX];
	i2c_release(scl39); i2c_release(sda39); i2c_delay(20);
	u32 scl_idle = i2c_level(scl39), sda_idle = i2c_level(sda39);
	result_hex("scl_idle", scl_idle);
	result_hex("sda_idle", sda_idle);
	u32 ok11 = 0, ok50 = 0, go = 0;
	u32 p11_0 = 0, p50_0 = 0;
	if (scl_idle && sda_idle) {
		p11_0 = pmu_read(scl39, sda39, 0x11, &ok11);
		p50_0 = pmu_read(scl39, sda39, 0x50, &ok50);
		go = (ok11 && ok50 && p11_0 == 0xBE && p50_0 == 0x11) ? 1 : 0;
	}
	kv("P11 before", p11_0); kv("P50 before", p50_0); kv("GUARD", go);
	result_hex("p11_before", p11_0); result_hex("p50_before", p50_0);
	result_hex("guard_ok", go);
	if (go) {
		u32 okw, okr;
		okw = pmu_write(scl39, sda39, 0x11, p11_0 | 0x40);             /* W4 */
		u32 p11_w = pmu_read(scl39, sda39, 0x11, &okr);
		result_hex("w4_ack", okw); result_hex("p11_after_w4", p11_w);
		okw = pmu_write(scl39, sda39, 0x50, (p50_0 & 0x19) | 0x02);   /* W3 */
		u32 p50_w = pmu_read(scl39, sda39, 0x50, &okr);
		result_hex("w3_ack", okw); result_hex("p50_after_w3", p50_w);
		kv("P11 after W4", p11_w); kv("P50 after W3", p50_w);
		i2c_delay(20000);                                             /* 20 ms, then look again */
		u32 p11_h = pmu_read(scl39, sda39, 0x11, &okr);
		u32 p50_h = pmu_read(scl39, sda39, 0x50, &okr);
		result_hex("p11_after_wait", p11_h); result_hex("p50_after_wait", p50_h);
		okw = pmu_write(scl39, sda39, 0x50, p50_0);                   /* restore, reverse order */
		result_hex("r50_ack", okw);
		okw = pmu_write(scl39, sda39, 0x11, p11_0);
		result_hex("r11_ack", okw);
		u32 p11_r = pmu_read(scl39, sda39, 0x11, &okr);
		u32 p50_r = pmu_read(scl39, sda39, 0x50, &okr);
		kv("P11 restored", p11_r); kv("P50 restored", p50_r);
		result_hex("p11_restored", p11_r); result_hex("p50_restored", p50_r);
	} else {
		puts_("GUARD FAILED, NOTHING WRITTEN\n");
	}
	fb_clean();
#endif
#if STAGE == 40
	/* DRY RUN of the touch power-up sequence: W1 + W4 + W3 + W5, NO SPI TRANSACTION. Nothing is sent to the
	 * touch controller; the point is to see whether the lines move once it is powered, clocked and released,
	 * and to leave everything as found. Writes, all restored at the end in reverse order:
	 *   spi1 clock gate  0xBF1000D0   0 -> 0xF           (W1)
	 *   PMU reg 0x11                  0xBE -> 0xFE        (W4, touch LDO bit 0x40 by static reading)
	 *   PMU reg 0x50                  0x11 -> 0x13        (W3, touch clock enable by static reading)
	 *   SoC GPIO 14.01 (touch reset)  0x212 -> 0x210      (W5: released to input / high impedance, as the
	 *                                                      personality flag reset-deassert-high-z suggests)
	 * GUARD: nothing is written unless PMU 0x11 reads 0xBE, PMU 0x50 reads 0x11, the gate reads 0 and the reset
	 * GPIO register reads 0x212 (the values of STAGES 33, 36 and 37). After the writes: wait 20 ms, then 15 ms
	 * after the reset release (the unit of the driver's value 0xf is unknown; 15 ms is the iDroid driver's
	 * delay), then READ ONLY: the reset GPIO register, the chip-select 13.00 register, the SPI1 idle registers
	 * (STATUS, PIN, SETUP, CLKDIVIDER). Restore order: reset GPIO back to driven low (0x212), PMU 0x50, PMU 0x11,
	 * clock gate. The clock gate read-back and every register are reported before and after. */
	at(4, 116, 0x00F0C040); puts_("STEP 40 TOUCH POWER DRY\n");
	volatile u32 *gp40  = (volatile u32 *)SOC_GPIO_BASE;
	volatile u32 *scl40 = &gp40[SOC_I2C0_SCL_IDX];
	volatile u32 *sda40 = &gp40[SOC_I2C0_SDA_IDX];
	volatile u32 *rst40 = &gp40[SOC_TOUCH_RESET_IDX];
	volatile u32 *cs40  = &gp40[SOC_TOUCH_CS_EN_IDX];
	volatile u32 *gate40 = (volatile u32 *)(SOC_CLOCK_GATE_BASE + 4 * SOC_SPI1_GATE_DT);
	i2c_release(scl40); i2c_release(sda40); i2c_delay(20);
	u32 scl_idle = i2c_level(scl40), sda_idle = i2c_level(sda40);
	u32 ok11 = 0, ok50 = 0;
	u32 p11_0 = 0, p50_0 = 0, g_0 = 0, rst_0 = 0, cs_0 = 0;
	if (scl_idle && sda_idle) {
		p11_0 = pmu_read(scl40, sda40, 0x11, &ok11);
		p50_0 = pmu_read(scl40, sda40, 0x50, &ok50);
	}
	g_0 = *gate40; rst_0 = *rst40; cs_0 = *cs40;
	u32 go = (ok11 && ok50 && p11_0 == 0xBE && p50_0 == 0x11 && g_0 == 0 && rst_0 == 0x212) ? 1 : 0;
	kv("P11", p11_0); kv("P50", p50_0); kv("GATE", g_0); kv("RST", rst_0); kv("CS ", cs_0); kv("GUARD", go);
	result_hex("p11_before", p11_0); result_hex("p50_before", p50_0); result_hex("gate_before", g_0);
	result_hex("rst_before", rst_0); result_hex("cs_before", cs_0); result_hex("guard_ok", go);
	if (go) {
		u32 okr;
		*gate40 = g_0 | 0xF;                                                   /* W1 */
		u32 w4 = pmu_write(scl40, sda40, 0x11, p11_0 | 0x40);                  /* W4 */
		u32 w3 = pmu_write(scl40, sda40, 0x50, (p50_0 & 0x19) | 0x02);         /* W3 */
		u32 g_1 = *gate40;
		u32 p11_1 = pmu_read(scl40, sda40, 0x11, &okr);
		u32 p50_1 = pmu_read(scl40, sda40, 0x50, &okr);
		result_hex("w4_ack", w4); result_hex("w3_ack", w3);
		result_hex("gate_on", g_1); result_hex("p11_on", p11_1); result_hex("p50_on", p50_1);
		i2c_delay(20000);                                                      /* settle, 20 ms */
		i2c_release(rst40);                                                    /* W5: reset line released */
		i2c_delay(15000);                                                      /* 15 ms */
		u32 rst_1 = *rst40, cs_1 = *cs40;
		volatile u32 *sp = (volatile u32 *)SOC_SPI1_BASE;
		u32 s_status = sp[0x08 / 4], s_pin = sp[0x0C / 4], s_setup = sp[0x04 / 4], s_div = sp[0x30 / 4];
		kv("RST rel", rst_1); kv("CS  rel", cs_1); kv("STATUS", s_status); kv("PIN", s_pin);
		kv("SETUP", s_setup); kv("CLKDIV", s_div);
		result_hex("rst_released", rst_1); result_hex("cs_released", cs_1);
		result_hex("s_status", s_status); result_hex("s_pin", s_pin);
		result_hex("s_setup", s_setup); result_hex("s_clkdiv", s_div);
		i2c_low(rst40);                                                        /* restore reset: driven low */
		u32 r50 = pmu_write(scl40, sda40, 0x50, p50_0);
		u32 r11 = pmu_write(scl40, sda40, 0x11, p11_0);
		*gate40 = g_0;
		i2c_delay(2000);
		u32 g_2 = *gate40, rst_2 = *rst40;
		u32 p11_2 = pmu_read(scl40, sda40, 0x11, &okr);
		u32 p50_2 = pmu_read(scl40, sda40, 0x50, &okr);
		kv("GATE end", g_2); kv("RST end", rst_2); kv("P11 end", p11_2); kv("P50 end", p50_2);
		result_hex("r50_ack", r50); result_hex("r11_ack", r11);
		result_hex("gate_end", g_2); result_hex("rst_end", rst_2);
		result_hex("p11_end", p11_2); result_hex("p50_end", p50_2);
	} else {
		puts_("GUARD FAILED, NOTHING WRITTEN\n");
	}
	fb_clean();
#endif
#if STAGE == 41
	/* CONTROL for STAGE 40: release the touch reset line with NO supply applied. The only write is the touch
	 * reset GPIO register (14.01, 0x83000284): 0x212 -> 0x210 (released to input), then back to 0x212. The clock
	 * gate, the PMU registers and every other GPIO are only READ (PMU through I2C reads, whose only writes are the
	 * register-number byte and the bus pins). GUARD: nothing is written unless the PMU registers read 0xBE and 0x11
	 * (supplies in the state of STAGE 36), the clock gate reads 0 and the reset register reads 0x212. The reset
	 * register is read right after the release and again 15 ms later. If it reads 1 here too, a pull-up holds
	 * the line independently of the controller. */
	at(4, 116, 0x00F0C040); puts_("STEP 41 RESET CONTROL\n");
	volatile u32 *gp41  = (volatile u32 *)SOC_GPIO_BASE;
	volatile u32 *scl41 = &gp41[SOC_I2C0_SCL_IDX];
	volatile u32 *sda41 = &gp41[SOC_I2C0_SDA_IDX];
	volatile u32 *rst41 = &gp41[SOC_TOUCH_RESET_IDX];
	volatile u32 *gate41 = (volatile u32 *)(SOC_CLOCK_GATE_BASE + 4 * SOC_SPI1_GATE_DT);
	i2c_release(scl41); i2c_release(sda41); i2c_delay(20);
	u32 scl_idle = i2c_level(scl41), sda_idle = i2c_level(sda41);
	u32 ok11 = 0, ok50 = 0;
	u32 p11_0 = 0, p50_0 = 0;
	if (scl_idle && sda_idle) {
		p11_0 = pmu_read(scl41, sda41, 0x11, &ok11);
		p50_0 = pmu_read(scl41, sda41, 0x50, &ok50);
	}
	u32 g_0 = *gate41, rst_0 = *rst41;
	u32 go = (ok11 && ok50 && p11_0 == 0xBE && p50_0 == 0x11 && g_0 == 0 && rst_0 == 0x212) ? 1 : 0;
	kv("P11", p11_0); kv("P50", p50_0); kv("GATE", g_0); kv("RST", rst_0); kv("GUARD", go);
	result_hex("p11_before", p11_0); result_hex("p50_before", p50_0); result_hex("gate_before", g_0);
	result_hex("rst_before", rst_0); result_hex("guard_ok", go);
	if (go) {
		i2c_release(rst41);                                                    /* the only change */
		u32 rst_a = *rst41;
		i2c_delay(15000);
		u32 rst_b = *rst41;
		kv("RST rel0", rst_a); kv("RST rel15", rst_b);
		result_hex("rst_rel_now", rst_a); result_hex("rst_rel_15ms", rst_b);
		i2c_low(rst41);                                                        /* restore: driven low */
		i2c_delay(2000);
		u32 rst_e = *rst41, g_e = *gate41;
		u32 okr;
		u32 p11_e = pmu_read(scl41, sda41, 0x11, &okr);
		u32 p50_e = pmu_read(scl41, sda41, 0x50, &okr);
		kv("RST end", rst_e); kv("GATE end", g_e); kv("P11 end", p11_e); kv("P50 end", p50_e);
		result_hex("rst_end", rst_e); result_hex("gate_end", g_e);
		result_hex("p11_end", p11_e); result_hex("p50_end", p50_e);
	} else {
		puts_("GUARD FAILED, NOTHING WRITTEN\n");
	}
	fb_clean();
#endif
#if STAGE == 42
	/* LDO ONLY, then reset release, to isolate which supply of STAGE 40 raised the reset line. Writes, all
	 * restored: PMU 0x11 0xBE -> 0xFE (touch LDO bit 0x40 by static reading), wait 20 ms, then the touch reset
	 * GPIO register (0x83000284) 0x212 -> 0x210, wait 15 ms, read the reset and chip-select registers, then
	 * reset back to 0x212 and PMU 0x11 back to its value. NOT written: the SPI clock gate and PMU 0x50 (the clock
	 * enable) stay as found. No SPI byte is sent. GUARD: nothing is written unless PMU 0x11 reads 0xBE, PMU 0x50
	 * reads 0x11, the gate reads 0 and the reset register reads 0x212. */
	at(4, 116, 0x00F0C040); puts_("STEP 42 LDO ONLY + RESET\n");
	volatile u32 *gp42  = (volatile u32 *)SOC_GPIO_BASE;
	volatile u32 *scl42 = &gp42[SOC_I2C0_SCL_IDX];
	volatile u32 *sda42 = &gp42[SOC_I2C0_SDA_IDX];
	volatile u32 *rst42 = &gp42[SOC_TOUCH_RESET_IDX];
	volatile u32 *cs42  = &gp42[SOC_TOUCH_CS_EN_IDX];
	volatile u32 *gate42 = (volatile u32 *)(SOC_CLOCK_GATE_BASE + 4 * SOC_SPI1_GATE_DT);
	i2c_release(scl42); i2c_release(sda42); i2c_delay(20);
	u32 scl_idle = i2c_level(scl42), sda_idle = i2c_level(sda42);
	u32 ok11 = 0, ok50 = 0;
	u32 p11_0 = 0, p50_0 = 0;
	if (scl_idle && sda_idle) {
		p11_0 = pmu_read(scl42, sda42, 0x11, &ok11);
		p50_0 = pmu_read(scl42, sda42, 0x50, &ok50);
	}
	u32 g_0 = *gate42, rst_0 = *rst42, cs_0 = *cs42;
	u32 go = (ok11 && ok50 && p11_0 == 0xBE && p50_0 == 0x11 && g_0 == 0 && rst_0 == 0x212) ? 1 : 0;
	kv("P11", p11_0); kv("P50", p50_0); kv("GATE", g_0); kv("RST", rst_0); kv("GUARD", go);
	result_hex("p11_before", p11_0); result_hex("p50_before", p50_0); result_hex("gate_before", g_0);
	result_hex("rst_before", rst_0); result_hex("cs_before", cs_0); result_hex("guard_ok", go);
	if (go) {
		u32 okr;
		u32 w4 = pmu_write(scl42, sda42, 0x11, p11_0 | 0x40);                  /* LDO on */
		u32 p11_on = pmu_read(scl42, sda42, 0x11, &okr);
		result_hex("w4_ack", w4); result_hex("p11_on", p11_on);
		i2c_delay(20000);                                                      /* 20 ms */
		u32 rst_pre = *rst42;
		i2c_release(rst42);                                                    /* reset released */
		u32 rst_a = *rst42;
		i2c_delay(15000);
		u32 rst_b = *rst42, cs_b = *cs42;
		kv("RST pre", rst_pre); kv("RST rel0", rst_a); kv("RST rel15", rst_b); kv("CS rel15", cs_b);
		result_hex("rst_pre", rst_pre); result_hex("rst_rel_now", rst_a); result_hex("rst_rel_15ms", rst_b);
		result_hex("cs_rel_15ms", cs_b);
		i2c_low(rst42);                                                        /* restore reset: driven low */
		u32 r11 = pmu_write(scl42, sda42, 0x11, p11_0);                        /* LDO restored */
		i2c_delay(2000);
		u32 rst_e = *rst42, g_e = *gate42;
		u32 p11_e = pmu_read(scl42, sda42, 0x11, &okr);
		u32 p50_e = pmu_read(scl42, sda42, 0x50, &okr);
		kv("RST end", rst_e); kv("GATE end", g_e); kv("P11 end", p11_e); kv("P50 end", p50_e);
		result_hex("r11_ack", r11); result_hex("rst_end", rst_e); result_hex("gate_end", g_e);
		result_hex("p11_end", p11_e); result_hex("p50_end", p50_e);
	} else {
		puts_("GUARD FAILED, NOTHING WRITTEN\n");
	}
	fb_clean();
#endif
#if STAGE == 43
	/* READ-ONLY survey of every GPIO pin register (376 words at SOC_GPIO_BASE + 4 * (8 * port + pin)), the full 32-bit
	 * word of each, with the same fault-tolerant read as STAGE 12 (an access that aborts is recorded, not fatal).
	 * Nothing is written. The point is to find pads whose configuration differs from the common default, which may
	 * include the SPI1 clock and data pads. Reported: how many answered, the distinct values with their counts
	 * (the 8 most common), and up to 48 pins whose value is not the most common one. The GPIO interrupt controller
	 * (GPIOIC) is not touched. */
	#define NPIN43 376
	volatile u32 *raw43 = (volatile u32 *)SOC_SNAPSHOTS;
	volatile u8 *ok43 = (volatile u8 *)(SOC_SNAPSHOTS + 0x800);
	volatile u32 *gp43 = (volatile u32 *)SOC_GPIO_BASE;
	at(4, 116, 0x00F0C040); puts_("STEP 43 GPIO SURVEY\n");
	u32 nread43 = 0;
	SC->quiet = 1;
	for (u32 i = 0; i < NPIN43; i++) {
		u32 before = SC->hit, v = 0;
		SC->expected = 1;
		asm volatile("adr.w r1, 1f\n\tstr r1, [%2]\n\tldr %0, [%1]\n1:"
			     : "+r"(v) : "r"(&gp43[i]), "r"(&SC->resume) : "r1", "memory");
		SC->expected = 0;
		ok43[i] = (SC->hit == before);
		raw43[i] = ok43[i] ? v : 0;
		nread43 += ok43[i];
	}
	SC->quiet = 0;
	volatile u32 dval[24], dcnt[24];
	u32 nd = 0;
	for (u32 i = 0; i < NPIN43; i++) {
		if (!ok43[i]) continue;
		u32 k = 0;
		while (k < nd && dval[k] != raw43[i]) k++;
		if (k == nd && nd < 24) { dval[nd] = raw43[i]; dcnt[nd] = 0; nd++; }
		if (k < nd) dcnt[k]++;
	}
	kv("READABLE", nread43); kv("DISTINCT", nd);
	result_hex("n_readable", nread43); result_hex("n_distinct", nd);
	u32 top = 0;
	for (u32 r = 0; r < 8 && r < nd; r++) {          /* the 8 most common, by selection */
		u32 best = 0xFFFFFFFF, bc = 0;
		for (u32 k = 0; k < nd; k++)
			if (dcnt[k] > bc) { bc = dcnt[k]; best = k; }
		if (best == 0xFFFFFFFF) break;
		char nv[4] = { 'v', (char)('0' + r), 0, 0 }, nc[4] = { 'c', (char)('0' + r), 0, 0 };
		result_hex(nv, dval[best]); result_hex(nc, dcnt[best]);
		if (r == 0) top = dval[best];
		dcnt[best] = 0;                                 /* consumed (counts no longer needed) */
	}
	u32 shown = 0, nrare = 0;
	for (u32 i = 0; i < NPIN43; i++) {
		if (!ok43[i] || raw43[i] == top) continue;
		nrare++;
		if (shown < 48) {
			char nm[5] = { 'g', "0123456789ABCDEF"[(i >> 8) & 15], "0123456789ABCDEF"[(i >> 4) & 15],
				       "0123456789ABCDEF"[i & 15], 0 };
			result_hex(nm, raw43[i]);
			shown++;
		}
	}
	kv("NOT TOP", nrare); kv("TOP VAL", top);
	result_hex("n_not_top", nrare); result_hex("top_value", top);
	/* Full table: the distinct values (dt00..) as in first-seen order, and one 8-character string per port
	 * (pt00..pt2e) holding, for its 8 pins, the index of each pin's value in that table, in the alphabet
	 * 0-9 then A-V ('-' for a pin that did not answer). */
	volatile u32 dvals[32];
	u32 ndv = 0;
	for (u32 i = 0; i < NPIN43; i++) {
		if (!ok43[i]) continue;
		u32 k = 0;
		while (k < ndv && dvals[k] != raw43[i]) k++;
		if (k == ndv && ndv < 32) dvals[ndv++] = raw43[i];
	}
	for (u32 k = 0; k < ndv; k++) {
		char nm[5] = { 'd', 't', "0123456789ABCDEF"[(k >> 4) & 15], "0123456789ABCDEF"[k & 15], 0 };
		result_hex(nm, dvals[k]);
	}
	for (u32 port = 0; port < NPIN43 / 8; port++) {
		char nm[5] = { 'p', 't', "0123456789ABCDEF"[(port >> 4) & 15], "0123456789ABCDEF"[port & 15], 0 };
		char val[9];
		for (u32 pin = 0; pin < 8; pin++) {
			u32 i = port * 8 + pin, k = 0;
			if (!ok43[i]) { val[pin] = '-'; continue; }
			while (k < ndv && dvals[k] != raw43[i]) k++;
			val[pin] = "0123456789ABCDEFGHIJKLMNOPQRSTUV"[k & 31];
		}
		val[8] = 0;
		result_str(nm, val);
	}
	fb_clean();
#endif
#if STAGE == 44
	/* READ-ONLY. Is the spi1 register block readable with NO clock-gate write? The iBSS's own SPI table gives the SPI1 clock gate
	 * as 0x0A (SPI0 0x09), already 0xF in STAGE 37, while the device tree says 0x16 (which read 0). STAGE 38 wrote 0x16 and then
	 * read spi1, so it could not tell the two apart. Here: read the gates 0x0A and 0x16, and, only if gate 0x0A reads with its low
	 * nibble 0xF, the seven idle registers of spi1 (never RXDATA or TXDATA). Nothing is written. */
	at(4, 116, 0x00F0C040); puts_("STEP 44 SPI1 NO GATE WRITE\n");
	volatile u32 *g44 = (volatile u32 *)SOC_CLOCK_GATE_BASE;
	u32 ga = g44[0x0A], gd = g44[SOC_SPI1_GATE_DT];
	kv("GATE 0A", ga); kv("GATE 16", gd);
	result_hex("gate_0a", ga); result_hex("gate_16", gd);
	if ((ga & 0xF) == 0xF) {
		volatile u32 *sp = (volatile u32 *)SOC_SPI1_BASE;
		u32 v_ctrl = sp[0x00 / 4], v_setup = sp[0x04 / 4], v_status = sp[0x08 / 4], v_pin = sp[0x0C / 4];
		u32 v_div = sp[0x30 / 4], v_cnt = sp[0x34 / 4], v_idd = sp[0x38 / 4];
		kv("CTRL ", v_ctrl); kv("SETUP", v_setup); kv("STATUS", v_status); kv("PIN  ", v_pin);
		kv("CLKDIV", v_div); kv("SPCNT", v_cnt); kv("SPIDD", v_idd);
		result_hex("s_ctrl", v_ctrl); result_hex("s_setup", v_setup); result_hex("s_status", v_status);
		result_hex("s_pin", v_pin); result_hex("s_clkdiv", v_div); result_hex("s_cnt", v_cnt); result_hex("s_idd", v_idd);
		result_hex("read_done", 1);
	} else {
		puts_("GATE 0A NOT ENABLED, SPI REGS SKIPPED\n");
		result_hex("read_done", 0);
	}
	fb_clean();
#endif
#if STAGE == 45
	/* SELF-TEST of the SPI1 controller with NO peripheral involved: the kernel's recipe for a 4-byte full-duplex exchange
	 * (the shape of the touch CHECK exchange), driven from here. The ONLY writes are to the spi1 register block
	 * (0x82100000): CLKDIVIDER 0x30, STATUS 0x08, SETUP 0x04, CONTROL 0x00, SPCNT 0x34, register 0x4c, TXDATA 0x10, and a
	 * restore of SETUP, CLKDIVIDER, SPCNT, 0x4c and STATUS at the end. NOT touched: any clock gate (the SPI1 gate 0x0A reads
	 * 0xF), the chip-select GPIO (13.00 stays driven low, untouched), the PMU, the touch LDO. The controller clocks 4 bytes
	 * (05 00 00 06) at a deliberately slow divider (240), about 100 kHz if the source clock is 24 MHz. Interrupts are masked
	 * in CPSR for the whole test and the controller's interrupt enable bits (0x180) are cleared before they are unmasked,
	 * because the iBSS left VIC0 bit 28 (the SPI1 interrupt, 0x1c) enabled. GUARD: nothing is written unless gate 0x0A reads
	 * 0xF and the seven idle registers read CONTROL 0, SETUP 8, STATUS 0, PIN 3, CLKDIV 1, SPCNT 0, SPIDD 0.
	 * Observed: STATUS at several instants, the RX FIFO bytes (whatever MISO carries), and the registers afterwards. */
	at(4, 116, 0x00F0C040); puts_("STEP 45 SPI1 SELF-TEST\n");
	u32 cpsr0;
	asm volatile("mrs %0, cpsr" : "=r"(cpsr0));
	asm volatile("cpsid i" ::: "memory");
	volatile u32 *sp = (volatile u32 *)SOC_SPI1_BASE;
	volatile u32 *tick = (volatile u32 *)(SOC_TIMER_BASE + 0x200);
	u32 ga = ((volatile u32 *)SOC_CLOCK_GATE_BASE)[0x0A];
	u32 i_ctrl = sp[0x00 / 4], i_setup = sp[0x04 / 4], i_status = sp[0x08 / 4], i_pin = sp[0x0C / 4];
	u32 i_div = sp[0x30 / 4], i_cnt = sp[0x34 / 4], i_idd = sp[0x38 / 4], i_4c = sp[0x4C / 4];
	u32 vic0 = *(volatile u32 *)SOC_VIC0;
	u32 go = ((ga & 0xF) == 0xF && i_ctrl == 0 && i_setup == 8 && i_status == 0 && i_pin == 3 && i_div == 1 &&
		  i_cnt == 0 && i_idd == 0) ? 1 : 0;
	kv("GATE0A", ga); kv("SETUP", i_setup); kv("R4C", i_4c); kv("VIC0", vic0); kv("GUARD", go);
	result_hex("gate_0a", ga); result_hex("i_ctrl", i_ctrl); result_hex("i_setup", i_setup); result_hex("i_status", i_status);
	result_hex("i_pin", i_pin); result_hex("i_div", i_div); result_hex("i_cnt", i_cnt); result_hex("i_idd", i_idd);
	result_hex("i_4c", i_4c); result_hex("vic0_before", vic0); result_hex("guard_ok", go);
	if (go) {
		sp[0x30 / 4] = 240;                       /* CLKDIVIDER, slow */
		sp[0x08 / 4] = 0x40000f;                  /* STATUS: the value the kernel writes before a transfer */
		sp[0x04 / 4] = 0x4018;                    /* SETUP base: clock source 0x4000 + master 0x18, 8-bit words */
		sp[0x00 / 4] = 0;                         /* CONTROL, as the kernel (a field nothing in the part read assigns) */
		sp[0x34 / 4] = 4;                         /* SPCNT: max(T, R) = 4 */
		sp[0x4C / 4] = 4;                         /* register 0x4c (spi-version 1) */
		sp[0x10 / 4] = 0x05; sp[0x10 / 4] = 0x00; sp[0x10 / 4] = 0x00; sp[0x10 / 4] = 0x06;   /* TXDATA: 05 00 00 06 */
		u32 st_pre = sp[0x08 / 4];
		u32 t0 = *tick;
		sp[0x04 / 4] = 0x4018 | 0x180 | 0x200000; /* start, as the kernel's start bits */
		u32 st1 = sp[0x08 / 4];
		u32 st2 = 0, st3 = 0, st4 = 0, done_us = 0xFFFFFFFF;
		u32 got2 = 0, got3 = 0, got4 = 0;
		for (;;) {
			u32 el = *tick - t0;
			u32 st = sp[0x08 / 4];
			if (!got2 && el >= 24u * 50) { st2 = st; got2 = 1; }
			if (!got3 && el >= 24u * 1000) { st3 = st; got3 = 1; }
			if (!got4 && el >= 24u * 5000) { st4 = st; got4 = 1; }
			if (((st >> 11) & 0x1f) >= 4 && done_us == 0xFFFFFFFF) { done_us = el / 24; }
			if (el >= 24u * 20000 || (done_us != 0xFFFFFFFF && got4)) break;
			if (done_us != 0xFFFFFFFF && el >= 24u * 6000) break;
		}
		u32 st_end = sp[0x08 / 4];
		u32 nrx = (st_end >> 11) & 0x1f, rx[4];
		rx[0] = 0; rx[1] = 0; rx[2] = 0; rx[3] = 0;
		for (u32 k = 0; k < 4 && k < nrx; k++) rx[k] = sp[0x20 / 4] & 0xFF;     /* RXDATA: only what the FIFO holds */
		sp[0x08 / 4] = st_end;                    /* write the value read back to STATUS (clears) */
		sp[0x04 / 4] = 0x4018;                    /* start and interrupt bits off */
		u32 p_status = sp[0x08 / 4], p_cnt = sp[0x34 / 4], p_4c = sp[0x4C / 4], p_setup = sp[0x04 / 4];
		sp[0x04 / 4] = i_setup; sp[0x30 / 4] = i_div; sp[0x34 / 4] = i_cnt; sp[0x4C / 4] = i_4c;   /* restore */
		sp[0x08 / 4] = 0x40000f;
		u32 r_setup = sp[0x04 / 4], r_div = sp[0x30 / 4], r_cnt = sp[0x34 / 4], r_status = sp[0x08 / 4], r_4c = sp[0x4C / 4];
		kv("ST pre", st_pre); kv("ST +0", st1); kv("ST 50us", st2); kv("ST 1ms", st3); kv("ST 5ms", st4); kv("ST end", st_end);
		kv("RX0", rx[0]); kv("RX1", rx[1]); kv("RX2", rx[2]); kv("RX3", rx[3]); kv("DONE us", done_us);
		result_hex("st_pre", st_pre); result_hex("st_0", st1); result_hex("st_50us", st2); result_hex("st_1ms", st3);
		result_hex("st_5ms", st4); result_hex("st_end", st_end); result_hex("done_us", done_us); result_hex("nrx", nrx);
		result_hex("rx0", rx[0]); result_hex("rx1", rx[1]); result_hex("rx2", rx[2]); result_hex("rx3", rx[3]);
		result_hex("p_status", p_status); result_hex("p_cnt", p_cnt); result_hex("p_4c", p_4c); result_hex("p_setup", p_setup);
		result_hex("r_setup", r_setup); result_hex("r_div", r_div); result_hex("r_cnt", r_cnt); result_hex("r_status", r_status);
		result_hex("r_4c", r_4c);
	} else {
		puts_("GUARD FAILED, NOTHING WRITTEN\n");
	}
	u32 vic1 = *(volatile u32 *)SOC_VIC0;
	result_hex("vic0_after", vic1);
	if (!(cpsr0 & 0x80)) asm volatile("cpsie i" ::: "memory");
	fb_clean();
#endif
#if STAGE == 46
	/* SELF-TEST 2 of the SPI1 controller, no peripheral: OpeniBoot's recipe (plat-s5l8920/spi.c, spi_set_baud then spi_tx) instead of
	 * the kernel's, because STAGE 45 (kernel recipe, CONTROL left 0) moved no FIFO counter. Writes ONLY to the spi1 block
	 * (0x82100000): CONTROL 0x00, SETUP 0x04, PIN 0x0C, TXDATA 0x10, CLKDIV 0x30, SPCNT 0x34, register 0x4C, STATUS 0x08, all
	 * restored at the end. Same guard as STAGE 45, same untouched list (no gate, no GPIO, no PMU), interrupts masked in CPSR. */
	at(4, 116, 0x00F0C040); puts_("STEP 46 SPI1 SELF-TEST\n");
	u32 cpsr0;
	asm volatile("mrs %0, cpsr" : "=r"(cpsr0));
	asm volatile("cpsid i" ::: "memory");
	volatile u32 *sp = (volatile u32 *)SOC_SPI1_BASE;
	volatile u32 *tick = (volatile u32 *)(SOC_TIMER_BASE + 0x200);
	u32 ga = ((volatile u32 *)SOC_CLOCK_GATE_BASE)[0x0A];
	u32 i_ctrl = sp[0x00 / 4], i_setup = sp[0x04 / 4], i_status = sp[0x08 / 4], i_pin = sp[0x0C / 4];
	u32 i_div = sp[0x30 / 4], i_cnt = sp[0x34 / 4], i_idd = sp[0x38 / 4], i_4c = sp[0x4C / 4];
	u32 vic0 = *(volatile u32 *)SOC_VIC0;
	u32 go = ((ga & 0xF) == 0xF && i_ctrl == 0 && i_setup == 8 && i_status == 0 && i_pin == 3 && i_div == 1 &&
		  i_cnt == 0 && i_idd == 0) ? 1 : 0;
	kv("GATE0A", ga); kv("SETUP", i_setup); kv("R4C", i_4c); kv("VIC0", vic0); kv("GUARD", go);
	result_hex("gate_0a", ga); result_hex("i_ctrl", i_ctrl); result_hex("i_setup", i_setup); result_hex("i_status", i_status);
	result_hex("i_pin", i_pin); result_hex("i_div", i_div); result_hex("i_cnt", i_cnt); result_hex("i_idd", i_idd);
	result_hex("i_4c", i_4c); result_hex("vic0_before", vic0); result_hex("guard_ok", go);
	if (go) {
		sp[0x00 / 4] = 0;                         /* spi_set_baud: control = 0 */
		sp[0x30 / 4] = 240;                       /* clkDiv (slow, ~100 kHz if the source is 24 MHz) */
		sp[0x04 / 4] = 0x18 | 0x20 | 0x4000;      /* config: master | 0x20 | clock source, 8-bit words */
		sp[0x0C / 4] = 2;                         /* pin = 2 */
		sp[0x00 / 4] = 1;                         /* control = 1 */
		u32 c_set = sp[0x00 / 4], s_set = sp[0x04 / 4], p_set = sp[0x0C / 4];
		sp[0x00 / 4] = sp[0x00 / 4] | 4;          /* spi_tx: control |= 1<<2 */
		sp[0x00 / 4] = sp[0x00 / 4] | 8;          /* control |= 1<<3 */
		u32 c_tx = sp[0x00 / 4];
		sp[0x34 / 4] = 0;                         /* rxCount = 0 */
		sp[0x10 / 4] = 0x05; sp[0x10 / 4] = 0x00; sp[0x10 / 4] = 0x00; sp[0x10 / 4] = 0x06;   /* TXDATA: 05 00 00 06 */
		u32 st_pre = sp[0x08 / 4];
		u32 t0 = *tick;
		sp[0x4C / 4] = 4;                         /* txCount = 4 */
		sp[0x04 / 4] = (0x18 | 0x20 | 0x4000) | 0x200100;
		sp[0x00 / 4] = 1;                         /* control = 1: start */
		u32 st1 = sp[0x08 / 4];
		u32 st2 = 0, st3 = 0, st4 = 0, done_us = 0xFFFFFFFF;
		u32 got2 = 0, got3 = 0, got4 = 0;
		for (;;) {
			u32 el = *tick - t0;
			u32 st = sp[0x08 / 4];
			if (!got2 && el >= 24u * 50) { st2 = st; got2 = 1; }
			if (!got3 && el >= 24u * 1000) { st3 = st; got3 = 1; }
			if (!got4 && el >= 24u * 5000) { st4 = st; got4 = 1; }
			if (((st >> 6) & 0x1f) == 0 && st != 0 && done_us == 0xFFFFFFFF) { done_us = el / 24; }
			if (el >= 24u * 20000) break;
		}
		u32 st_end = sp[0x08 / 4];
		u32 nrx = (st_end >> 11) & 0x1f, rx[4];
		rx[0] = 0; rx[1] = 0; rx[2] = 0; rx[3] = 0;
		for (u32 k = 0; k < 4 && k < nrx; k++) rx[k] = sp[0x20 / 4] & 0xFF;     /* RXDATA: only what the FIFO holds */
		sp[0x08 / 4] = st_end;                    /* write the value read back to STATUS (clears) */
		sp[0x00 / 4] = 0;                         /* control off first */
		u32 p_ctrl = sp[0x00 / 4], p_pin = sp[0x0C / 4], p_status = sp[0x08 / 4], p_cnt = sp[0x34 / 4], p_4c = sp[0x4C / 4], p_setup = sp[0x04 / 4];
		sp[0x0C / 4] = i_pin; sp[0x04 / 4] = i_setup; sp[0x30 / 4] = i_div; sp[0x34 / 4] = i_cnt; sp[0x4C / 4] = i_4c;   /* restore */
		sp[0x08 / 4] = 0x40000f;
		u32 r_ctrl = sp[0x00 / 4], r_pin = sp[0x0C / 4], r_setup = sp[0x04 / 4], r_div = sp[0x30 / 4], r_cnt = sp[0x34 / 4], r_status = sp[0x08 / 4], r_4c = sp[0x4C / 4];
		kv("ST pre", st_pre); kv("ST +0", st1); kv("ST 50us", st2); kv("ST 1ms", st3); kv("ST 5ms", st4); kv("ST end", st_end);
		kv("RX0", rx[0]); kv("RX1", rx[1]); kv("RX2", rx[2]); kv("RX3", rx[3]); kv("DONE us", done_us);
		result_hex("st_pre", st_pre); result_hex("st_0", st1); result_hex("st_50us", st2); result_hex("st_1ms", st3);
		result_hex("st_5ms", st4); result_hex("st_end", st_end); result_hex("done_us", done_us); result_hex("nrx", nrx);
		result_hex("rx0", rx[0]); result_hex("rx1", rx[1]); result_hex("rx2", rx[2]); result_hex("rx3", rx[3]);
		result_hex("p_status", p_status); result_hex("p_cnt", p_cnt); result_hex("p_4c", p_4c); result_hex("p_setup", p_setup);
		result_hex("r_setup", r_setup); result_hex("r_div", r_div); result_hex("r_cnt", r_cnt); result_hex("r_status", r_status);
		result_hex("r_4c", r_4c);
		result_hex("c_set", c_set); result_hex("s_set", s_set); result_hex("p_set", p_set); result_hex("c_tx", c_tx);
		result_hex("p_ctrl", p_ctrl); result_hex("p_pin", p_pin); result_hex("r_ctrl", r_ctrl); result_hex("r_pin", r_pin);
	} else {
		puts_("GUARD FAILED, NOTHING WRITTEN\n");
	}
	u32 vic1 = *(volatile u32 *)SOC_VIC0;
	result_hex("vic0_after", vic1);
	if (!(cpsr0 & 0x80)) asm volatile("cpsie i" ::: "memory");
	fb_clean();
#endif
#if STAGE == 31
	/* Accelerometer axes: first real write to this device (CTRL_REG1, power it up and enable
	 * X/Y/Z at +-2g/100Hz), then read the three 8-bit signed axis registers. Same safety pattern
	 * as STAGE 29: re-read WHO_AM_I first and only proceed if it still matches 0x3B, so a
	 * different device on the bus (or a mis-wired guess) cannot look like real axis data. */
	at(4, 116, 0x00F0C040); puts_("STEP 31 ACCEL AXES\n");

	volatile u32 *gp31  = (volatile u32 *)SOC_GPIO_BASE;
	volatile u32 *scl31 = &gp31[SOC_I2C0_SCL_IDX];
	volatile u32 *sda31 = &gp31[SOC_I2C0_SDA_IDX];

	i2c_release(scl31); i2c_release(sda31); i2c_delay(20);
	u32 scl_idle31 = i2c_level(scl31), sda_idle31 = i2c_level(sda31);
	result_hex("scl_idle", scl_idle31);
	result_hex("sda_idle", sda_idle31);

	u32 who31 = 0, ctrl1_rb = 0, status = 0, out_x = 0, out_y = 0, out_z = 0;
	u32 ack_who = 0, ack_ctrl_w = 0, ack_ctrl_v = 0, ack_status = 0, ack_x = 0, ack_y = 0, ack_z = 0;
	if (scl_idle31 && sda_idle31) {
		/* WHO_AM_I, safety check */
		i2c_start(scl31, sda31);
		ack_who = i2c_wbyte(scl31, sda31, (SOC_ACCEL_I2C_ADDR << 1) | 0)
		        & i2c_wbyte(scl31, sda31, SOC_ACCEL_WHO_AM_I_REG);
		i2c_start(scl31, sda31);
		ack_who &= i2c_wbyte(scl31, sda31, (SOC_ACCEL_I2C_ADDR << 1) | 1);
		who31 = i2c_rbyte(scl31, sda31);
		i2c_stop(scl31, sda31);

		if (who31 == 0x3B) {
			/* CTRL_REG1 write: power up, enable X/Y/Z, +-2g, 100 Hz (first real write to
			 * this device). */
			i2c_start(scl31, sda31);
			ack_ctrl_w = i2c_wbyte(scl31, sda31, (SOC_ACCEL_I2C_ADDR << 1) | 0)
			           & i2c_wbyte(scl31, sda31, SOC_ACCEL_CTRL_REG1)
			           & i2c_wbyte(scl31, sda31, SOC_ACCEL_CTRL1_ACTIVE_XYZ_2G_100HZ);
			i2c_stop(scl31, sda31);

			/* Turn-on delay before trusting the data (datasheet: a few ms, be generous). */
			i2c_delay(10000);

			/* Read CTRL_REG1 back, to confirm the write actually landed (not assumed). */
			i2c_start(scl31, sda31);
			ack_ctrl_v = i2c_wbyte(scl31, sda31, (SOC_ACCEL_I2C_ADDR << 1) | 0)
			           & i2c_wbyte(scl31, sda31, SOC_ACCEL_CTRL_REG1);
			i2c_start(scl31, sda31);
			ack_ctrl_v &= i2c_wbyte(scl31, sda31, (SOC_ACCEL_I2C_ADDR << 1) | 1);
			ctrl1_rb = i2c_rbyte(scl31, sda31);
			i2c_stop(scl31, sda31);

			/* STATUS_REG, then the three axis bytes -- one single-byte read transaction each,
			 * same pattern as WHO_AM_I, nothing new attempted here. */
			i2c_start(scl31, sda31);
			ack_status = i2c_wbyte(scl31, sda31, (SOC_ACCEL_I2C_ADDR << 1) | 0)
			           & i2c_wbyte(scl31, sda31, SOC_ACCEL_STATUS_REG);
			i2c_start(scl31, sda31);
			ack_status &= i2c_wbyte(scl31, sda31, (SOC_ACCEL_I2C_ADDR << 1) | 1);
			status = i2c_rbyte(scl31, sda31);
			i2c_stop(scl31, sda31);

			i2c_start(scl31, sda31);
			ack_x = i2c_wbyte(scl31, sda31, (SOC_ACCEL_I2C_ADDR << 1) | 0)
			      & i2c_wbyte(scl31, sda31, SOC_ACCEL_OUT_X);
			i2c_start(scl31, sda31);
			ack_x &= i2c_wbyte(scl31, sda31, (SOC_ACCEL_I2C_ADDR << 1) | 1);
			out_x = i2c_rbyte(scl31, sda31);
			i2c_stop(scl31, sda31);

			i2c_start(scl31, sda31);
			ack_y = i2c_wbyte(scl31, sda31, (SOC_ACCEL_I2C_ADDR << 1) | 0)
			      & i2c_wbyte(scl31, sda31, SOC_ACCEL_OUT_Y);
			i2c_start(scl31, sda31);
			ack_y &= i2c_wbyte(scl31, sda31, (SOC_ACCEL_I2C_ADDR << 1) | 1);
			out_y = i2c_rbyte(scl31, sda31);
			i2c_stop(scl31, sda31);

			i2c_start(scl31, sda31);
			ack_z = i2c_wbyte(scl31, sda31, (SOC_ACCEL_I2C_ADDR << 1) | 0)
			      & i2c_wbyte(scl31, sda31, SOC_ACCEL_OUT_Z);
			i2c_start(scl31, sda31);
			ack_z &= i2c_wbyte(scl31, sda31, (SOC_ACCEL_I2C_ADDR << 1) | 1);
			out_z = i2c_rbyte(scl31, sda31);
			i2c_stop(scl31, sda31);
		} else {
			puts_("WHO_AM_I MISMATCH, SKIPPED\n");
		}
	} else {
		puts_("BUS NOT IDLE HIGH, SKIPPED\n");
	}
	kv("WHO_AM_I ", who31);
	kv("CTRL1_RB ", ctrl1_rb);
	kv("STATUS   ", status);
	kv("OUT_X    ", out_x);
	kv("OUT_Y    ", out_y);
	kv("OUT_Z    ", out_z);
	result_hex("who_am_i", who31);
	result_hex("ack_who", ack_who);
	result_hex("ack_ctrl_w", ack_ctrl_w);
	result_hex("ack_ctrl_v", ack_ctrl_v);
	result_hex("ctrl1_rb", ctrl1_rb);
	result_hex("ack_status", ack_status);
	result_hex("status", status);
	result_hex("ack_x", ack_x);
	result_hex("out_x", out_x);
	result_hex("ack_y", ack_y);
	result_hex("out_y", out_y);
	result_hex("ack_z", ack_z);
	result_hex("out_z", out_z);
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
#if STAGE == 32
	/* Second IOP register window, SOC_IOP_VIC_BASE = 0xBF300000 (see soc/s5l8920.h): decoded
	 * from the "iop" device tree node's own "reg" property, cross-checked against
	 * AppleARM7M::start()'s own "_vicRegisterMap" field name. Read-only, no write -- but
	 * "read-only" is not the same claim as "no effect": some hardware registers clear-on-read or
	 * otherwise change state just by being read, and this window's real behaviour is unknown.
	 *
	 * +0x18 is now a DRIVER-CONFIRMED offset, not a blind guess: AppleARM7M::start() caches this
	 * window's virtual address at this+0x60 (the exact mirror of this+0x58 for the already-known
	 * window 0 -- both addresses are logged together in the same "mapped I/O registers" call, as
	 * decisive as static analysis gets), and a separate method (code address 0x80713510, a real
	 * vtable entry, not startCPU) writes a 32-bit word to [this+0x60 + 0x18]: either 8 or 4
	 * depending on a 0/1 argument -- a two-way enable/select pattern, offset and width confirmed,
	 * exact semantics not. The rest of the 8-word sweep (+0x00 to +0x1C) stays blind, kept for
	 * context around the one confirmed offset, not elevated to the same confidence.
	 *
	 * Captured ONCE into a RAM array before any display or export, so the on-screen dump and the
	 * USB-exported values are guaranteed to be the same read, not two independent reads of a
	 * register that might answer differently each time (a real risk this review caught: the
	 * previous version of this stage read the screen copy and the result_hex() copy separately).
	 *
	 * Interpret the result against four outcomes, not two: all-zero (like index 0); a stable,
	 * structured-looking value (re-run at least once, same pattern as tools/tick_rate.py's
	 * two-separate-invocations convention, to tell "stable" from "coincidence"); a value that
	 * changes between runs (suggests something live, not proof of IOP firmware execution either
	 * way); or a data abort / USB link drop (informative on its own, same as the GPIOIC
	 * incident). None of these, including a recognisable value at +0x18 matching 4 or 8, would by
	 * itself prove the IOP core is executing its firmware -- only that this offset behaves the
	 * way the host driver's own write would suggest it should. */
	at(4, 116, 0x00F0C040); puts_("STEP 32 IOP VIC?\n");
	volatile u32 *iopvic = (volatile u32 *)SOC_IOP_VIC_BASE;
	u32 snapshot[8];
	for (u32 i = 0; i < 8; i++) snapshot[i] = iopvic[i];

	u32 y32 = 134;
	for (u32 i = 0; i < 8; i++) {
		at(4, y32, i == 6 ? 0x00B050A0 : 0x00A0B0C0);   /* +0x18 (i=6) highlighted: driver-confirmed */
		puts_("+"); hex(i * 4); puts_(" "); hex(snapshot[i]);
		putc_('\n'); fb_clean();
		y32 += 18;
	}
	result_hex("vic00", snapshot[0]);
	result_hex("vic04", snapshot[1]);
	result_hex("vic08", snapshot[2]);
	result_hex("vic0c", snapshot[3]);
	result_hex("vic10", snapshot[4]);
	result_hex("vic14", snapshot[5]);
	result_hex("vic18", snapshot[6]);   /* the driver-confirmed offset */
	result_hex("vic1c", snapshot[7]);
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
#elif STAGE == 29
	ENV_SET("trognon", "accel-i2c-1", 1);
#elif STAGE == 30
	ENV_SET("trognon", "bt-gpio-1", 1);
#elif STAGE == 31
	ENV_SET("trognon", "accel-axes-1", 1);
#elif STAGE == 32
	ENV_SET("trognon", "iop-vic-1", 1);
#elif STAGE == 33
	ENV_SET("trognon", "touch-gpio-1", 1);
#elif STAGE == 34
	ENV_SET("trognon", "compass-i2c-1", 1);
#elif STAGE == 35
	ENV_SET("trognon", "i2c-scan-1", 1);
#elif STAGE == 36
	ENV_SET("trognon", "pmu-read-1", 1);
#elif STAGE == 37
	ENV_SET("trognon", "spi1-pre-1", 1);
#elif STAGE == 38
	ENV_SET("trognon", "spi1-gate-1", 1);
#elif STAGE == 39
	ENV_SET("trognon", "pmu-w34-1", 1);
#elif STAGE == 40
	ENV_SET("trognon", "touch-power-dry-1", 1);
#elif STAGE == 41
	ENV_SET("trognon", "touch-rst-ctl-1", 1);
#elif STAGE == 42
	ENV_SET("trognon", "touch-ldo-only-1", 1);
#elif STAGE == 43
	ENV_SET("trognon", "gpio-survey-1", 1);
#elif STAGE == 44
	ENV_SET("trognon", "spi1-nogate-1", 1);
#elif STAGE == 45
	ENV_SET("trognon", "spi1-selftest-1", 1);
#elif STAGE == 46
	ENV_SET("trognon", "spi1-selftest-2", 1);
#elif STAGE == 5
	ENV_SET("trognon", "regs-1", 1);
#elif STAGE == 6
	ENV_SET("trognon", "vic-1", 1);
#else
	ENV_SET("trognon", "trace-1", 1);
#endif
}
