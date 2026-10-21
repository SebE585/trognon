/* STAGE 5. Built as: trace_regs.bin.
 * Included by trace.c inside payload_main(): it shares that function's locals, so it is a block, not yet a unit of its own.
 * To change it, edit it here; `make -C tools/payloads sums` must leave every other stage byte-identical. */
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
