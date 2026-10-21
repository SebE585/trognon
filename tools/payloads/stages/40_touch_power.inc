/* STAGE 40. Built as: trace_touch_power.bin.
 * Included by trace.c inside payload_main(): it shares that function's locals, so it is a block, not yet a unit of its own.
 * To change it, edit it here; `make -C tools/payloads sums` must leave every other stage byte-identical. */
#if STAGE == 40
	/* DRY RUN of the touch power-up sequence: W1 + W4 + W3 + W5, NO SPI TRANSACTION. Nothing is sent to the
	 * touch controller; the point is to see whether the lines move once it is powered, clocked and released,
	 * and to leave everything as found. Writes, all restored at the end in reverse order:
	 *   spi1 clock gate  0xBF1000D0   0 -> 0xF           (W1)
	 *   PMU reg 0x11                  0xBE -> 0xFE        (W4, touch LDO bit 0x40 by static reading)
	 *   PMU reg 0x50                  0x11 -> 0x13        (W3, touch clock enable by static reading)
	 *   SoC GPIO 14.01 (touch reset)  0x212 -> 0x210      (W5: released to input / high impedance, as the
	 *                                                      personality flag reset-deassert-high-z suggests)
	 * GUARD: nothing is written unless PMU 0x11 reads 0xBE, PMU 0x50 reads 0x11, the gate reads 0 and the reset
	 * GPIO register reads 0x212 (the values of STAGES 33, 36 and 37). After the writes: wait 20 ms, then 15 ms
	 * after the reset release (the unit of the driver's value 0xf is unknown; 15 ms is the iDroid driver's
	 * delay), then READ ONLY: the reset GPIO register, the chip-select 13.00 register, the SPI1 idle registers
	 * (STATUS, PIN, SETUP, CLKDIVIDER). Restore order: reset GPIO back to driven low (0x212), PMU 0x50, PMU 0x11,
	 * clock gate. The clock gate read-back and every register are reported before and after. */
	at(4, 116, 0x00F0C040); puts_("STEP 40 TOUCH POWER DRY\n");
	volatile u32 *gp40  = gpio_regs();
	volatile u32 *scl40 = &gp40[SOC_I2C0_SCL_IDX];
	volatile u32 *sda40 = &gp40[SOC_I2C0_SDA_IDX];
	volatile u32 *rst40 = &gp40[SOC_TOUCH_RESET_IDX];
	volatile u32 *cs40  = &gp40[SOC_TOUCH_CS_EN_IDX];
	volatile u32 *gate40 = clock_gate_reg(SOC_SPI1_GATE_DT);
	i2c_release(scl40); i2c_release(sda40); i2c_delay(20);
	u32 scl_idle = i2c_level(scl40), sda_idle = i2c_level(sda40);
	u32 ok11 = 0, ok50 = 0;
	u32 p11_0 = 0, p50_0 = 0, g_0 = 0, rst_0 = 0, cs_0 = 0;
	if (scl_idle && sda_idle) {
		p11_0 = pmu_read(scl40, sda40, 0x11, &ok11);
		p50_0 = pmu_read(scl40, sda40, 0x50, &ok50);
	}
	g_0 = *gate40; rst_0 = *rst40; cs_0 = *cs40;
	u32 go = (ok11 && ok50 && p11_0 == 0xBE && p50_0 == 0x11 && g_0 == 0 && rst_0 == 0x212) ? 1 : 0;
	kv("P11", p11_0); kv("P50", p50_0); kv("GATE", g_0); kv("RST", rst_0); kv("CS ", cs_0); kv("GUARD", go);
	result_hex("p11_before", p11_0); result_hex("p50_before", p50_0); result_hex("gate_before", g_0);
	result_hex("rst_before", rst_0); result_hex("cs_before", cs_0); result_hex("guard_ok", go);
	if (go) {
		u32 okr;
		*gate40 = g_0 | 0xF;                                                   /* W1 */
		u32 w4 = pmu_write(scl40, sda40, 0x11, p11_0 | 0x40);                  /* W4 */
		u32 w3 = pmu_write(scl40, sda40, 0x50, (p50_0 & 0x19) | 0x02);         /* W3 */
		u32 g_1 = *gate40;
		u32 p11_1 = pmu_read(scl40, sda40, 0x11, &okr);
		u32 p50_1 = pmu_read(scl40, sda40, 0x50, &okr);
		result_hex("w4_ack", w4); result_hex("w3_ack", w3);
		result_hex("gate_on", g_1); result_hex("p11_on", p11_1); result_hex("p50_on", p50_1);
		i2c_delay(20000);                                                      /* settle, 20 ms */
		i2c_release(rst40);                                                    /* W5: reset line released */
		i2c_delay(15000);                                                      /* 15 ms */
		u32 rst_1 = *rst40, cs_1 = *cs40;
		volatile u32 *sp = (volatile u32 *)SOC_SPI1_BASE;
		u32 s_status = sp[0x08 / 4], s_pin = sp[0x0C / 4], s_setup = sp[0x04 / 4], s_div = sp[0x30 / 4];
		kv("RST rel", rst_1); kv("CS  rel", cs_1); kv("STATUS", s_status); kv("PIN", s_pin);
		kv("SETUP", s_setup); kv("CLKDIV", s_div);
		result_hex("rst_released", rst_1); result_hex("cs_released", cs_1);
		result_hex("s_status", s_status); result_hex("s_pin", s_pin);
		result_hex("s_setup", s_setup); result_hex("s_clkdiv", s_div);
		i2c_low(rst40);                                                        /* restore reset: driven low */
		u32 r50 = pmu_write(scl40, sda40, 0x50, p50_0);
		u32 r11 = pmu_write(scl40, sda40, 0x11, p11_0);
		*gate40 = g_0;
		i2c_delay(2000);
		u32 g_2 = *gate40, rst_2 = *rst40;
		u32 p11_2 = pmu_read(scl40, sda40, 0x11, &okr);
		u32 p50_2 = pmu_read(scl40, sda40, 0x50, &okr);
		kv("GATE end", g_2); kv("RST end", rst_2); kv("P11 end", p11_2); kv("P50 end", p50_2);
		result_hex("r50_ack", r50); result_hex("r11_ack", r11);
		result_hex("gate_end", g_2); result_hex("rst_end", rst_2);
		result_hex("p11_end", p11_2); result_hex("p50_end", p50_2);
	} else {
		puts_("GUARD FAILED, NOTHING WRITTEN\n");
	}
	fb_clean();
#endif
