/* STAGE 36. Built as: trace_pmu_read.bin.
 * Included by trace.c inside payload_main(): it shares that function's locals, so it is a block, not yet a unit of its own.
 * To change it, edit it here; `make -C tools/payloads sums` must leave every other stage byte-identical. */
#if STAGE == 36
	/* PMU register READS at i2c0 0x74 (device tree "pmu,d1755"). Registers chosen from a static
	 * reading of the AppleD1755 driver (docs/research/2026-10-16-touch-n88-preconditions.md):
	 * 0x10 and 0x11 (LDO on/off bits; the touch LDO is bit 0x40 of 0x11), 0x17..0x22 (LDO
	 * voltage/auxiliary registers) and 0x50..0x57 (per-GPIO configuration). Each is a random read:
	 * write the register address, repeated start, read one byte. No data byte is written to any PMU
	 * register; the I2C routines drive the two bus pins. The meaning of the bytes is NOT decided
	 * here; the run only records what the PMU holds at this point of the boot. */
	at(4, 116, 0x00F0C040); puts_("STEP 36 PMU READ\n");
	volatile u32 *gp36  = gpio_regs();
	volatile u32 *scl36 = &gp36[SOC_I2C0_SCL_IDX];
	volatile u32 *sda36 = &gp36[SOC_I2C0_SDA_IDX];
	i2c_release(scl36); i2c_release(sda36); i2c_delay(20);
	u32 scl_idle = i2c_level(scl36), sda_idle = i2c_level(sda36);
	result_hex("scl_idle", scl_idle);
	result_hex("sda_idle", sda_idle);
	static const u32 pmu_regs[22] = { 0x10, 0x11, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E,
	                                  0x1F, 0x20, 0x21, 0x22, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55,
	                                  0x56, 0x57 };
	static const char *const pmu_val[22] = { "p10","p11","p17","p18","p19","p1a","p1b","p1c","p1d","p1e",
	                                         "p1f","p20","p21","p22","p50","p51","p52","p53","p54","p55",
	                                         "p56","p57" };
	static const char *const pmu_ok[22]  = { "k10","k11","k17","k18","k19","k1a","k1b","k1c","k1d","k1e",
	                                         "k1f","k20","k21","k22","k50","k51","k52","k53","k54","k55",
	                                         "k56","k57" };
	if (scl_idle && sda_idle) {
		for (int k = 0; k < 22; k++) {
			i2c_start(scl36, sda36);
			u32 a1 = i2c_wbyte(scl36, sda36, (0x74 << 1) | 0);
			u32 a2 = i2c_wbyte(scl36, sda36, pmu_regs[k]);
			i2c_start(scl36, sda36);
			u32 a3 = i2c_wbyte(scl36, sda36, (0x74 << 1) | 1);
			u32 v = i2c_rbyte(scl36, sda36);
			i2c_stop(scl36, sda36);
			i2c_delay(200);
			kv(pmu_val[k], v);
			result_hex(pmu_val[k], v);
			result_hex(pmu_ok[k], a1 & a2 & a3);
		}
	} else {
		puts_("BUS NOT IDLE HIGH, SKIPPED\n");
	}
	fb_clean();
#endif
