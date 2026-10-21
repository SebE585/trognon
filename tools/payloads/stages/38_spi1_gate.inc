/* STAGE 38. Built as: trace_spi1_gate.bin.
 * Included by trace.c inside payload_main(): it shares that function's locals, so it is a block, not yet a unit of its own.
 * To change it, edit it here; `make -C tools/payloads sums` must leave every other stage byte-identical. */
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
	volatile u32 *gate38 = clock_gate_reg(SOC_SPI1_GATE_DT);
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
