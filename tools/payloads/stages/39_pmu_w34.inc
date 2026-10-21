/* STAGE 39. Built as: trace_pmu_w34.bin.
 * Included by trace.c inside payload_main(): it shares that function's locals, so it is a block, not yet a unit of its own.
 * To change it, edit it here; `make -C tools/payloads sums` must leave every other stage byte-identical. */
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
	volatile u32 *gp39  = gpio_regs();
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
