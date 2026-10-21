/* STAGE 41. Built as: trace_touch_rst_ctl.bin.
 * Included by trace.c inside payload_main(): it shares that function's locals, so it is a block, not yet a unit of its own.
 * To change it, edit it here; `make -C tools/payloads sums` must leave every other stage byte-identical. */
#if STAGE == 41
	/* CONTROL for STAGE 40: release the touch reset line with NO supply applied. The only write is the touch
	 * reset GPIO register (14.01, 0x83000284): 0x212 -> 0x210 (released to input), then back to 0x212. The clock
	 * gate, the PMU registers and every other GPIO are only READ (PMU through I2C reads, whose only writes are the
	 * register-number byte and the bus pins). GUARD: nothing is written unless the PMU registers read 0xBE and 0x11
	 * (supplies in the state of STAGE 36), the clock gate reads 0 and the reset register reads 0x212. The reset
	 * register is read right after the release and again 15 ms later. If it reads 1 here too, a pull-up holds
	 * the line independently of the controller. */
	at(4, 116, 0x00F0C040); puts_("STEP 41 RESET CONTROL\n");
	volatile u32 *gp41  = gpio_regs();
	volatile u32 *scl41 = &gp41[SOC_I2C0_SCL_IDX];
	volatile u32 *sda41 = &gp41[SOC_I2C0_SDA_IDX];
	volatile u32 *rst41 = &gp41[SOC_TOUCH_RESET_IDX];
	volatile u32 *gate41 = clock_gate_reg(SOC_SPI1_GATE_DT);
	i2c_release(scl41); i2c_release(sda41); i2c_delay(20);
	u32 scl_idle = i2c_level(scl41), sda_idle = i2c_level(sda41);
	u32 ok11 = 0, ok50 = 0;
	u32 p11_0 = 0, p50_0 = 0;
	if (scl_idle && sda_idle) {
		p11_0 = pmu_read(scl41, sda41, 0x11, &ok11);
		p50_0 = pmu_read(scl41, sda41, 0x50, &ok50);
	}
	u32 g_0 = *gate41, rst_0 = *rst41;
	u32 go = (ok11 && ok50 && p11_0 == 0xBE && p50_0 == 0x11 && g_0 == 0 && rst_0 == 0x212) ? 1 : 0;
	kv("P11", p11_0); kv("P50", p50_0); kv("GATE", g_0); kv("RST", rst_0); kv("GUARD", go);
	result_hex("p11_before", p11_0); result_hex("p50_before", p50_0); result_hex("gate_before", g_0);
	result_hex("rst_before", rst_0); result_hex("guard_ok", go);
	if (go) {
		i2c_release(rst41);                                                    /* the only change */
		u32 rst_a = *rst41;
		i2c_delay(15000);
		u32 rst_b = *rst41;
		kv("RST rel0", rst_a); kv("RST rel15", rst_b);
		result_hex("rst_rel_now", rst_a); result_hex("rst_rel_15ms", rst_b);
		i2c_low(rst41);                                                        /* restore: driven low */
		i2c_delay(2000);
		u32 rst_e = *rst41, g_e = *gate41;
		u32 okr;
		u32 p11_e = pmu_read(scl41, sda41, 0x11, &okr);
		u32 p50_e = pmu_read(scl41, sda41, 0x50, &okr);
		kv("RST end", rst_e); kv("GATE end", g_e); kv("P11 end", p11_e); kv("P50 end", p50_e);
		result_hex("rst_end", rst_e); result_hex("gate_end", g_e);
		result_hex("p11_end", p11_e); result_hex("p50_end", p50_e);
	} else {
		puts_("GUARD FAILED, NOTHING WRITTEN\n");
	}
	fb_clean();
#endif
