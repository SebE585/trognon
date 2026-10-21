/* STAGE 30. Built as: trace_bt_gpio.bin.
 * Included by trace.c inside payload_main(): it shares that function's locals, so it is a block, not yet a unit of its own.
 * To change it, edit it here; `make -C tools/payloads sums` must leave every other stage byte-identical. */
#if STAGE == 30
	/* Bluetooth GPIO levels, read-only (same pattern as STAGE 15 for Wi-Fi reset/wake): just the
	 * current level of the reset (15.06) and wake (16.02) lines, decoded tonight from the device
	 * tree's "function-bt_reset"/"function-bt_wake" properties. No write, no UART touched. */
	at(4, 116, 0x00F0C040); puts_("STEP 30 BT GPIO\n");
	volatile u32 *gp = gpio_regs();
	u32 bt_reset = gp[SOC_BT_RESET_IDX] & 1;
	u32 bt_wake  = gp[SOC_BT_WAKE_IDX] & 1;
	kv("RESET_15.6", bt_reset);
	kv("WAKE_16.2 ", bt_wake);
	result_hex("bt_reset", bt_reset);
	result_hex("bt_wake", bt_wake);
	fb_clean();
#endif
