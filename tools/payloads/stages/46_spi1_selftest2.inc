/* STAGE 46. Built as: trace_spi1_selftest2.bin.
 * Included by trace.c inside payload_main(): it shares that function's locals, so it is a block, not yet a unit of its own.
 * To change it, edit it here; `make -C tools/payloads sums` must leave every other stage byte-identical. */
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
	u32 ga = clock_gates()[0x0A];
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
