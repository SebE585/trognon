/* STAGE 34. Built as: trace_compass_i2c.bin.
 * Included by trace.c inside payload_main(): it shares that function's locals, so it is a block, not yet a unit of its own.
 * To change it, edit it here; `make -C tools/payloads sums` must leave every other stage byte-identical. */
#if STAGE == 34
	/* Magnetometer presence probe over the same bit-banged i2c0, no data byte written to any device register. For each of
	 * four addresses it makes two minimal transactions: (A) START, address+write, STOP, which only
	 * asks "does anything acknowledge this address"; (B) only if A was acknowledged, START,
	 * address+read, ONE byte, STOP -- a current-address read, so the chip's register pointer is
	 * never set by us. 0x1d is the accelerometer, which answered in STAGE 29/31 (positive
	 * control); 0x1c is expected to be silent (negative control); 0x1e and 0x1f are the two
	 * "compass" device-tree nodes (AKM8973S). The byte read from 0x1e/0x1f is whatever the chip
	 * returns from its current pointer and is NOT interpreted here: no register map has been
	 * checked against a datasheet. */
	at(4, 116, 0x00F0C040); puts_("STEP 34 COMPASS I2C\n");
	volatile u32 *gp34  = gpio_regs();
	volatile u32 *scl34 = &gp34[SOC_I2C0_SCL_IDX];
	volatile u32 *sda34 = &gp34[SOC_I2C0_SDA_IDX];
	i2c_release(scl34); i2c_release(sda34); i2c_delay(20);
	u32 scl_idle = i2c_level(scl34), sda_idle = i2c_level(sda34);
	kv("SCL_IDLE", scl_idle);
	kv("SDA_IDLE", sda_idle);
	result_hex("scl_idle", scl_idle);
	result_hex("sda_idle", sda_idle);
	static const u32 addrs34[4] = { 0x1c, 0x1d, 0x1e, 0x1f };
	static const char *const nm_aw[4] = { "aw_1c", "aw_1d", "aw_1e", "aw_1f" };
	static const char *const nm_ar[4] = { "ar_1c", "ar_1d", "ar_1e", "ar_1f" };
	static const char *const nm_b[4]  = { "b_1c",  "b_1d",  "b_1e",  "b_1f"  };
	if (scl_idle && sda_idle) {
		for (int k = 0; k < 4; k++) {
			u32 aw = 0, ar = 0, b = 0;
			i2c_start(scl34, sda34);
			aw = i2c_wbyte(scl34, sda34, (addrs34[k] << 1) | 0);
			i2c_stop(scl34, sda34);
			i2c_delay(200);
			if (aw) {
				i2c_start(scl34, sda34);
				ar = i2c_wbyte(scl34, sda34, (addrs34[k] << 1) | 1);
				if (ar) b = i2c_rbyte(scl34, sda34);
				i2c_stop(scl34, sda34);
				i2c_delay(200);
			}
			kv(nm_aw[k], aw);
			kv(nm_b[k], b);
			result_hex(nm_aw[k], aw);
			result_hex(nm_ar[k], ar);
			result_hex(nm_b[k], b);
		}
	} else {
		puts_("BUS NOT IDLE HIGH, SKIPPED\n");
	}
	fb_clean();
#endif
