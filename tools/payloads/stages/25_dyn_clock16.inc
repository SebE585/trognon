/* STAGE 25. Built as: trace_dyn_clock16.bin.
 * Included by trace.c inside payload_main(): it shares that function's locals, so it is a block, not yet a unit of its own.
 * To change it, edit it here; `make -C tools/payloads sums` must leave every other stage byte-identical. */
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
