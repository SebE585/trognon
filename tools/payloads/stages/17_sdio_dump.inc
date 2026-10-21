/* STAGE 17. Built as: trace_sdio_dump.bin.
 * Included by trace.c inside payload_main(): it shares that function's locals, so it is a block, not yet a unit of its own.
 * To change it, edit it here; `make -C tools/payloads sums` must leave every other stage byte-identical. */
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
