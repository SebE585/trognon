/* STAGE 33. Built as: trace_touch_gpio.bin.
 * Included by trace.c inside payload_main(): it shares that function's locals, so it is a block, not yet a unit of its own.
 * To change it, edit it here; `make -C tools/payloads sums` must leave every other stage byte-identical. */
#if STAGE == 33
	/* Touch controller reset (14.01) and chip-select enable (13.00), READ-ONLY. The pins'
	 * configuration is not changed and no value is written to them: each GPIO register is read once, and both the raw
	 * word and its bit 0 are reported. The Bluetooth reset line, already read on the device
	 * (STAGE 30: level 1), is read in the same run as a control, so a run that reads it
	 * differently is flagged as not comparable. A level says nothing about polarity (active
	 * high or low), nor about who drives the pin, nor that the controller works. The
	 * attention line is an interrupt on the GPIO interrupt controller and is deliberately
	 * not touched. */
	at(4, 116, 0x00F0C040); puts_("STEP 33 TOUCH GPIO\n");
	volatile u32 *gp = gpio_regs();
	u32 t_rst_raw = gp[SOC_TOUCH_RESET_IDX];
	u32 t_cs_raw  = gp[SOC_TOUCH_CS_EN_IDX];
	u32 bt_ctl    = gp[SOC_BT_RESET_IDX];
	kv("RST_14.1 raw", t_rst_raw);
	kv("CSEN_13.0 raw", t_cs_raw);
	kv("BT_RST ctl raw", bt_ctl);
	result_hex("touch_reset_raw", t_rst_raw);
	result_hex("touch_reset", t_rst_raw & 1);
	result_hex("touch_cs_en_raw", t_cs_raw);
	result_hex("touch_cs_en", t_cs_raw & 1);
	result_hex("bt_reset_control", bt_ctl & 1);
	fb_clean();
#endif
