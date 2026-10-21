/* STAGE 31. Built as: trace_accel_axes.bin.
 * Included by trace.c inside payload_main(): it shares that function's locals, so it is a block, not yet a unit of its own.
 * To change it, edit it here; `make -C tools/payloads sums` must leave every other stage byte-identical. */
#if STAGE == 31
	/* Accelerometer axes: first real write to this device (CTRL_REG1, power it up and enable
	 * X/Y/Z at +-2g/100Hz), then read the three 8-bit signed axis registers. Same safety pattern
	 * as STAGE 29: re-read WHO_AM_I first and only proceed if it still matches 0x3B, so a
	 * different device on the bus (or a mis-wired guess) cannot look like real axis data. */
	at(4, 116, 0x00F0C040); puts_("STEP 31 ACCEL AXES\n");

	volatile u32 *gp31  = gpio_regs();
	volatile u32 *scl31 = &gp31[SOC_I2C0_SCL_IDX];
	volatile u32 *sda31 = &gp31[SOC_I2C0_SDA_IDX];

	i2c_release(scl31); i2c_release(sda31); i2c_delay(20);
	u32 scl_idle31 = i2c_level(scl31), sda_idle31 = i2c_level(sda31);
	result_hex("scl_idle", scl_idle31);
	result_hex("sda_idle", sda_idle31);

	u32 who31 = 0, ctrl1_rb = 0, status = 0, out_x = 0, out_y = 0, out_z = 0;
	u32 ack_who = 0, ack_ctrl_w = 0, ack_ctrl_v = 0, ack_status = 0, ack_x = 0, ack_y = 0, ack_z = 0;
	if (scl_idle31 && sda_idle31) {
		/* WHO_AM_I, safety check */
		i2c_start(scl31, sda31);
		ack_who = i2c_wbyte(scl31, sda31, (SOC_ACCEL_I2C_ADDR << 1) | 0)
		        & i2c_wbyte(scl31, sda31, SOC_ACCEL_WHO_AM_I_REG);
		i2c_start(scl31, sda31);
		ack_who &= i2c_wbyte(scl31, sda31, (SOC_ACCEL_I2C_ADDR << 1) | 1);
		who31 = i2c_rbyte(scl31, sda31);
		i2c_stop(scl31, sda31);

		if (who31 == 0x3B) {
			/* CTRL_REG1 write: power up, enable X/Y/Z, +-2g, 100 Hz (first real write to
			 * this device). */
			i2c_start(scl31, sda31);
			ack_ctrl_w = i2c_wbyte(scl31, sda31, (SOC_ACCEL_I2C_ADDR << 1) | 0)
			           & i2c_wbyte(scl31, sda31, SOC_ACCEL_CTRL_REG1)
			           & i2c_wbyte(scl31, sda31, SOC_ACCEL_CTRL1_ACTIVE_XYZ_2G_100HZ);
			i2c_stop(scl31, sda31);

			/* Turn-on delay before trusting the data (datasheet: a few ms, be generous). */
			i2c_delay(10000);

			/* Read CTRL_REG1 back, to confirm the write actually landed (not assumed). */
			i2c_start(scl31, sda31);
			ack_ctrl_v = i2c_wbyte(scl31, sda31, (SOC_ACCEL_I2C_ADDR << 1) | 0)
			           & i2c_wbyte(scl31, sda31, SOC_ACCEL_CTRL_REG1);
			i2c_start(scl31, sda31);
			ack_ctrl_v &= i2c_wbyte(scl31, sda31, (SOC_ACCEL_I2C_ADDR << 1) | 1);
			ctrl1_rb = i2c_rbyte(scl31, sda31);
			i2c_stop(scl31, sda31);

			/* STATUS_REG, then the three axis bytes -- one single-byte read transaction each,
			 * same pattern as WHO_AM_I, nothing new attempted here. */
			i2c_start(scl31, sda31);
			ack_status = i2c_wbyte(scl31, sda31, (SOC_ACCEL_I2C_ADDR << 1) | 0)
			           & i2c_wbyte(scl31, sda31, SOC_ACCEL_STATUS_REG);
			i2c_start(scl31, sda31);
			ack_status &= i2c_wbyte(scl31, sda31, (SOC_ACCEL_I2C_ADDR << 1) | 1);
			status = i2c_rbyte(scl31, sda31);
			i2c_stop(scl31, sda31);

			i2c_start(scl31, sda31);
			ack_x = i2c_wbyte(scl31, sda31, (SOC_ACCEL_I2C_ADDR << 1) | 0)
			      & i2c_wbyte(scl31, sda31, SOC_ACCEL_OUT_X);
			i2c_start(scl31, sda31);
			ack_x &= i2c_wbyte(scl31, sda31, (SOC_ACCEL_I2C_ADDR << 1) | 1);
			out_x = i2c_rbyte(scl31, sda31);
			i2c_stop(scl31, sda31);

			i2c_start(scl31, sda31);
			ack_y = i2c_wbyte(scl31, sda31, (SOC_ACCEL_I2C_ADDR << 1) | 0)
			      & i2c_wbyte(scl31, sda31, SOC_ACCEL_OUT_Y);
			i2c_start(scl31, sda31);
			ack_y &= i2c_wbyte(scl31, sda31, (SOC_ACCEL_I2C_ADDR << 1) | 1);
			out_y = i2c_rbyte(scl31, sda31);
			i2c_stop(scl31, sda31);

			i2c_start(scl31, sda31);
			ack_z = i2c_wbyte(scl31, sda31, (SOC_ACCEL_I2C_ADDR << 1) | 0)
			      & i2c_wbyte(scl31, sda31, SOC_ACCEL_OUT_Z);
			i2c_start(scl31, sda31);
			ack_z &= i2c_wbyte(scl31, sda31, (SOC_ACCEL_I2C_ADDR << 1) | 1);
			out_z = i2c_rbyte(scl31, sda31);
			i2c_stop(scl31, sda31);
		} else {
			puts_("WHO_AM_I MISMATCH, SKIPPED\n");
		}
	} else {
		puts_("BUS NOT IDLE HIGH, SKIPPED\n");
	}
	kv("WHO_AM_I ", who31);
	kv("CTRL1_RB ", ctrl1_rb);
	kv("STATUS   ", status);
	kv("OUT_X    ", out_x);
	kv("OUT_Y    ", out_y);
	kv("OUT_Z    ", out_z);
	result_hex("who_am_i", who31);
	result_hex("ack_who", ack_who);
	result_hex("ack_ctrl_w", ack_ctrl_w);
	result_hex("ack_ctrl_v", ack_ctrl_v);
	result_hex("ctrl1_rb", ctrl1_rb);
	result_hex("ack_status", ack_status);
	result_hex("status", status);
	result_hex("ack_x", ack_x);
	result_hex("out_x", out_x);
	result_hex("ack_y", ack_y);
	result_hex("out_y", out_y);
	result_hex("ack_z", ack_z);
	result_hex("out_z", out_z);
	fb_clean();
#endif
