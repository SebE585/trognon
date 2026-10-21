/* STAGE 16. Built as: trace_wifi_reset.bin.
 * Included by trace.c inside payload_main(): it shares that function's locals, so it is a block, not yet a unit of its own.
 * To change it, edit it here; `make -C tools/payloads sums` must leave every other stage byte-identical. */
#if STAGE == 16
	/* Wi-Fi/baseband reset line (16.5), first WRITE of the whole project to a live peripheral
	 * pin — everything before this was read-only. Reproduces gpio_custom_io() from OpeniBoot's
	 * plat-s5l8920/gpio.c (modes "clear_output"/"set_output": value/bitmask pairs read straight
	 * from that source, not invented) and the timing from plat-a4/sdio.c's sdio_reset(): assert
	 * (drive low) 12 ms, release (drive high) 150 ms. The line was already read LOW tonight
	 * (STAGE 15): this sequence re-asserts it (a no-op on the current level) then releases it —
	 * the normal "take the chip out of reset" step any boot sequence does, not a novel action on
	 * this pin. Reversible by construction: a reset line is designed to be toggled. No SDIO
	 * command sent, no NAND touched. */
	#define FMI_SEC 24000000u
	volatile u32 *fmi_tick = (volatile u32 *)(SOC_TIMER_BASE + 0x200);
	volatile u32 *gp = gpio_regs();
	u32 idx = 8 * 0x16 + 0x05;   /* gpio_baseband_wlan_reset, 16.5 */
	volatile u32 *pin_reg = &gp[idx];

	at(4, 116, 0x00F0C040); puts_("STEP 16 WIFI RESET\n");
	kv("BEFORE  ", *pin_reg & 1);

	/* clear_output: value=0x212, bitmask=0x27F (gpio.c:242-245) -- assert (drive low) */
	*pin_reg = (*pin_reg & ~0x27Fu) | (0x212u & 0x27Fu);
	{ u32 t0 = *fmi_tick; while (*fmi_tick - t0 < FMI_SEC / 1000 * 12) ; }   /* 12 ms */
	kv("ASSERTED", *pin_reg & 1);

	/* set_output: value=0x213, bitmask=0x27F (gpio.c:247-250) -- release (drive high) */
	*pin_reg = (*pin_reg & ~0x27Fu) | (0x213u & 0x27Fu);
	{ u32 t0 = *fmi_tick; while (*fmi_tick - t0 < FMI_SEC / 1000 * 150) ; }   /* 150 ms */
	kv("RELEASED", *pin_reg & 1);
#endif
