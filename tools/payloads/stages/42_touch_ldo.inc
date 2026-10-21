/* STAGE 42. Built as: trace_touch_ldo.bin.
 * Included by trace.c inside payload_main(): it shares that function's locals, so it is a block, not yet a unit of its own.
 * To change it, edit it here; `make -C tools/payloads sums` must leave every other stage byte-identical. */
#if STAGE == 42
	/* LDO ONLY, then reset release, to isolate which supply of STAGE 40 raised the reset line. Writes, all
	 * restored: PMU 0x11 0xBE -> 0xFE (touch LDO bit 0x40 by static reading), wait 20 ms, then the touch reset
	 * GPIO register (0x83000284) 0x212 -> 0x210, wait 15 ms, read the reset and chip-select registers, then
	 * reset back to 0x212 and PMU 0x11 back to its value. NOT written: the SPI clock gate and PMU 0x50 (the clock
	 * enable) stay as found. No SPI byte is sent. GUARD: nothing is written unless PMU 0x11 reads 0xBE, PMU 0x50
	 * reads 0x11, the gate reads 0 and the reset register reads 0x212. */
	at(4, 116, 0x00F0C040); puts_("STEP 42 LDO ONLY + RESET\n");
	volatile u32 *gp42  = gpio_regs();
	volatile u32 *scl42 = &gp42[SOC_I2C0_SCL_IDX];
	volatile u32 *sda42 = &gp42[SOC_I2C0_SDA_IDX];
	volatile u32 *rst42 = &gp42[SOC_TOUCH_RESET_IDX];
	volatile u32 *cs42  = &gp42[SOC_TOUCH_CS_EN_IDX];
	volatile u32 *gate42 = clock_gate_reg(SOC_SPI1_GATE_DT);
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
