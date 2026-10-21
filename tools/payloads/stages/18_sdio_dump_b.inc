/* STAGE 18. Built as: trace_sdio_dump_b.bin.
 * Included by trace.c inside payload_main(): it shares that function's locals, so it is a block, not yet a unit of its own.
 * To change it, edit it here; `make -C tools/payloads sums` must leave every other stage byte-identical. */
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
