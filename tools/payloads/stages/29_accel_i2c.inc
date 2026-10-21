/* STAGE 29. Built as: trace_accel_i2c.bin.
 * Included by trace.c inside payload_main(): it shares that function's locals, so it is a block, not yet a unit of its own.
 * To change it, edit it here; `make -C tools/payloads sums` must leave every other stage byte-identical. */
#if STAGE == 29
	at(4, 116, 0x00F0C040); puts_("STEP 29 ACCEL I2C\n");

	volatile u32 *gp29  = gpio_regs();
	volatile u32 *scl29 = &gp29[SOC_I2C0_SCL_IDX];
	volatile u32 *sda29 = &gp29[SOC_I2C0_SDA_IDX];

	i2c_release(scl29); i2c_release(sda29); i2c_delay(20);
	u32 scl_idle = i2c_level(scl29), sda_idle = i2c_level(sda29);
	kv("SCL_IDLE", scl_idle);
	kv("SDA_IDLE", sda_idle);
	result_hex("scl_idle", scl_idle);
	result_hex("sda_idle", sda_idle);

	u32 who = 0, ack_aw = 0, ack_reg = 0, ack_ar = 0;
	if (scl_idle && sda_idle) {   /* a pulled-up idle bus; anything else means this is not wired as expected */
		i2c_start(scl29, sda29);
		ack_aw  = i2c_wbyte(scl29, sda29, (SOC_ACCEL_I2C_ADDR << 1) | 0);
		ack_reg = i2c_wbyte(scl29, sda29, SOC_ACCEL_WHO_AM_I_REG);
		i2c_start(scl29, sda29);   /* repeated START */
		ack_ar  = i2c_wbyte(scl29, sda29, (SOC_ACCEL_I2C_ADDR << 1) | 1);
		who = i2c_rbyte(scl29, sda29);
		i2c_stop(scl29, sda29);
	} else {
		puts_("BUS NOT IDLE HIGH, SKIPPED\n");
	}
	kv("ACK_ADDRW", ack_aw);
	kv("ACK_REG  ", ack_reg);
	kv("ACK_ADDRR", ack_ar);
	kv("WHO_AM_I ", who);
	result_hex("ack_addrw", ack_aw);
	result_hex("ack_reg", ack_reg);
	result_hex("ack_addrr", ack_ar);
	result_hex("who_am_i", who);
	fb_clean();
#endif
