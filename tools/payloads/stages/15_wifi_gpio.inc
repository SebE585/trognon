/* STAGE 15. Built as: trace_wifi_gpio.bin.
 * Included by trace.c inside payload_main(): it shares that function's locals, so it is a block, not yet a unit of its own.
 * To change it, edit it here; `make -C tools/payloads sums` must leave every other stage byte-identical. */
#if STAGE == 15
	/* Wi-Fi/baseband reset + SDIO wake GPIO, read-only. Pins decoded tonight from the device
	 * tree's own "function-device_reset"/"function-device_wake" properties (OIPG format),
	 * cross-checked against TheAppleWiki's independently decoded values for this exact device
	 * (both matched exactly): gpio_baseband_wlan_reset = 0x1605, gpio_sdio_device_wake = 0x1404.
	 * Same port.pin encoding as the button sweep (STAGE 12): port = high byte, pin = low byte,
	 * array index = 8*port+pin. No write, no SDIO command: just the current level of each line. */
	volatile u32 *gp = gpio_regs();
	at(4, 116, 0x00F0C040); puts_("STEP 15 WIFI GPIO\n");

	u32 reset_idx = 8 * 0x16 + 0x05;   /* gpio_baseband_wlan_reset, 16.5 */
	u32 wake_idx  = 8 * 0x14 + 0x04;   /* gpio_sdio_device_wake,    14.4 */
	u32 reset_level = gp[reset_idx] & 1;
	u32 wake_level  = gp[wake_idx] & 1;

	at(4, 134, 0x00FFFFFF); puts_("RESET 16.5 = "); hex(reset_level); putc_('\n'); fb_clean();
	at(4, 152, 0x00FFFFFF); puts_("WAKE  14.4 = "); hex(wake_level); putc_('\n'); fb_clean();
	kv("RESET_LVL", reset_level);
	kv("WAKE_LVL ", wake_level);
#endif
