/* STAGE 37. Built as: trace_spi1_pre.bin.
 * Included by trace.c inside payload_main(): it shares that function's locals, so it is a block, not yet a unit of its own.
 * To change it, edit it here; `make -C tools/payloads sums` must leave every other stage byte-identical. */
#if STAGE == 37
	/* spi1 preconditions, READ-ONLY. (1) Clock gate registers at 0xBF100078 + 4*g for the SPI gates
	 * named by OpeniBoot (0x9..0xD) and the neighbourhood of the device tree's 0x16 (0x15..0x19);
	 * the gate read in STAGE 28 (0x27) was 0x0000000F. (2) Only if the device tree's gate 0x16 reads
	 * with its low nibble 0xF (the pattern seen on an enabled gate), the idle registers of spi1:
	 * CONTROL, SETUP, STATUS, PIN, CLKDIVIDER, SPCNT, SPIDD. If not, they are skipped: reading a
	 * block whose clock is off may stall the bus, and this project has already lost the USB link to
	 * one such read. RXDATA and TXDATA are never read. */
	at(4, 116, 0x00F0C040); puts_("STEP 37 SPI1 PRECOND\n");
	volatile u32 *gates37 = clock_gates();
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
