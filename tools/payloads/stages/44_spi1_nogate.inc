/* STAGE 44. Built as: trace_spi1_nogate.bin.
 * Included by trace.c inside payload_main(): it shares that function's locals, so it is a block, not yet a unit of its own.
 * To change it, edit it here; `make -C tools/payloads sums` must leave every other stage byte-identical. */
#if STAGE == 44
	/* READ-ONLY. Is the spi1 register block readable with NO clock-gate write? The iBSS's own SPI table gives the SPI1 clock gate
	 * as 0x0A (SPI0 0x09), already 0xF in STAGE 37, while the device tree says 0x16 (which read 0). STAGE 38 wrote 0x16 and then
	 * read spi1, so it could not tell the two apart. Here: read the gates 0x0A and 0x16, and, only if gate 0x0A reads with its low
	 * nibble 0xF, the seven idle registers of spi1 (never RXDATA or TXDATA). Nothing is written. */
	at(4, 116, 0x00F0C040); puts_("STEP 44 SPI1 NO GATE WRITE\n");
	volatile u32 *g44 = clock_gates();
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
