/* STAGE 35. Built as: trace_i2c_scan.bin.
 * Included by trace.c inside payload_main(): it shares that function's locals, so it is a block, not yet a unit of its own.
 * To change it, edit it here; `make -C tools/payloads sums` must leave every other stage byte-identical. */
#if STAGE == 35
	/* (1) Address scan of i2c0 (7-bit addresses 0x08..0x77): for each, START, address+write, STOP,
	 * recording only whether anything acknowledged. No data byte is sent to any device. Shows what
	 * answers on this bus without assuming the device tree is complete; a part that is unpowered
	 * or held in reset does not answer, so silence is not absence. (2) Random reads of the AK8973's
	 * read-only registers at 0x1e (datasheet MS0561-E-01: ST 0xC0, TMPS 0xC1, H1X/H1Y/H1Z
	 * 0xC2..0xC4) and of MS1 0xE0, whose reset value is 0x03 (power-down). The AK8973 has no ID
	 * register, so this is a consistency check by reset state, not an identification. No data byte is
	 * written to any device register; the register-address byte of each random read is sent, and the
	 * I2C routines drive the two bus pins (GPIO) to produce the transactions. */
	at(4, 116, 0x00F0C040); puts_("STEP 35 I2C SCAN\n");
	volatile u32 *gp35  = gpio_regs();
	volatile u32 *scl35 = &gp35[SOC_I2C0_SCL_IDX];
	volatile u32 *sda35 = &gp35[SOC_I2C0_SDA_IDX];
	i2c_release(scl35); i2c_release(sda35); i2c_delay(20);
	u32 scl_idle = i2c_level(scl35), sda_idle = i2c_level(sda35);
	result_hex("scl_idle", scl_idle);
	result_hex("sda_idle", sda_idle);
	volatile u32 scan[4];   /* explicit stores: a brace initialiser makes GCC call memset, absent here */
	scan[0] = 0; scan[1] = 0; scan[2] = 0; scan[3] = 0;
	u32 nack = 0;
	if (scl_idle && sda_idle) {
		for (u32 a = 0x08; a <= 0x77; a++) {
			i2c_start(scl35, sda35);
			u32 ok = i2c_wbyte(scl35, sda35, (a << 1) | 0);
			i2c_stop(scl35, sda35);
			i2c_delay(100);
			if (ok) { scan[a >> 5] |= 1u << (a & 31); nack++; kv("ACK addr", a); }
		}
		result_hex("scan_w0", scan[0]);
		result_hex("scan_w1", scan[1]);
		result_hex("scan_w2", scan[2]);
		result_hex("scan_w3", scan[3]);
		result_hex("scan_count", nack);
		static const u32 regs35[12] = { 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6 };
		static const char *const nm35[12] = { "r_c0", "r_c1", "r_c2", "r_c3", "r_c4", "r_e0", "r_e1", "r_e2", "r_e3", "r_e4", "r_e5", "r_e6" };
		static const char *const ok35[12] = { "ok_c0", "ok_c1", "ok_c2", "ok_c3", "ok_c4", "ok_e0", "ok_e1", "ok_e2", "ok_e3", "ok_e4", "ok_e5", "ok_e6" };
		if (scan[SOC_COMPASS_I2C_ADDR0 >> 5] & (1u << (SOC_COMPASS_I2C_ADDR0 & 31))) {
			for (int k = 0; k < 12; k++) {
				i2c_start(scl35, sda35);
				u32 a1 = i2c_wbyte(scl35, sda35, (SOC_COMPASS_I2C_ADDR0 << 1) | 0);
				u32 a2 = i2c_wbyte(scl35, sda35, regs35[k]);
				i2c_start(scl35, sda35);
				u32 a3 = i2c_wbyte(scl35, sda35, (SOC_COMPASS_I2C_ADDR0 << 1) | 1);
				u32 v = i2c_rbyte(scl35, sda35);
				i2c_stop(scl35, sda35);
				i2c_delay(200);
				kv(nm35[k], v);
				result_hex(nm35[k], v);
				result_hex(ok35[k], a1 & a2 & a3);
			}
		}
	} else {
		puts_("BUS NOT IDLE HIGH, SKIPPED\n");
	}
	fb_clean();
#endif
