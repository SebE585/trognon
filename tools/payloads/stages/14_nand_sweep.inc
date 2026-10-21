/* STAGE 14. Built as: trace_nand_sweep.bin.
 * Included by trace.c inside payload_main(): it shares that function's locals, so it is a block, not yet a unit of its own.
 * To change it, edit it here; `make -C tools/payloads sums` must leave every other stage byte-identical. */
#if STAGE == 14
	/* NAND (H2FMI) chip ID sweep, CE 0-15, command sequencer on fmi0 throughout — this is
	 * exactly OpeniBoot's own first top-level call (h2fmi_init() -> h2fmi_reset_and_read_chipids
	 * (&fmi0, buff1, 0), h2fmi.c:1926, which itself loops chip 0..15). CE 8-15 route their
	 * enable/disable mask to fmi1's own register (h2fmi_enable_chip()/h2fmi_disable_chip(),
	 * h2fmi.c:338-354) even though the command/status registers stay on fmi0 for this sweep —
	 * reproduced as-is, not our own design. A second sweep with fmi1 as the command sequencer
	 * (OpeniBoot's own second call) is a separate next step, not done here. Read-only. */
	#define FMI_SEC 24000000u
	volatile u32 *fmi_tick = (volatile u32 *)(SOC_TIMER_BASE + 0x200);
	at(4, 116, 0x00F0C040); puts_("STEP 14 NAND SWEEP\n");

	*(volatile u32 *)SOC_CLOCK_GATE_26 |= 0xF;
	*(volatile u32 *)SOC_FMI_REG1_408 = 0xFFFF;
	*(volatile u32 *)SOC_FMI_REG2_400 = 1;
	*(volatile u32 *)SOC_FMI_REG1_408 = 0xFFFF;
	*(volatile u32 *)SOC_FMI_REG3_814 = 0;

	u32 y = 134;
	for (u32 chip = 0; chip < 16; chip++) {
		volatile u32 *mask_reg = (chip >> 3) ? (volatile u32 *)SOC_FMI1_CHIPMASK_40C
						      : (volatile u32 *)SOC_FMI_CHIPMASK_40C;
		u32 bit = 1u << (chip & 7);

		*mask_reg = bit;
		*(volatile u32 *)SOC_FMI_REG4_414 = 0xFF;
		*(volatile u32 *)SOC_FMI_REG5_410 = 1;
		u32 t0 = *fmi_tick, reset_ok = 0;
		while (*fmi_tick - t0 < FMI_SEC / 100)
			if ((*(volatile u32 *)SOC_FMI_REG6_444 & 1) == 1) { reset_ok = 1; break; }
		*mask_reg &= ~bit;

		u8 id[8] = {0};
		u32 id_ok = 0;
		if (reset_ok) {
			*mask_reg = bit;
			*(volatile u32 *)SOC_FMI_REG4_414 = 0x90;
			*(volatile u32 *)SOC_FMI_REG9_418 = 0;
			*(volatile u32 *)SOC_FMI_REG10_420 = 0;
			*(volatile u32 *)SOC_FMI_REG5_410 = 9;
			t0 = *fmi_tick;
			while (*fmi_tick - t0 < FMI_SEC / 100)
				if ((*(volatile u32 *)SOC_FMI_REG6_444 & 9) == 9) { id_ok = 1; break; }
			if (id_ok) {
				u32 old = *(volatile u32 *)SOC_FMI_REG1_408;
				*(volatile u32 *)SOC_FMI_REG1_408 = 0;
				*(volatile u32 *)SOC_FMI_REG8_424 = 0;
				for (u32 i = 0; i < 8; i++) {
					*(volatile u32 *)SOC_FMI_REG5_410 = 0x50;
					{ u32 d0 = *fmi_tick; while (*fmi_tick - d0 < 24) ; }
					id[i] = (u8)(*(volatile u32 *)SOC_FMI_DATA_448 & 0xFF);
					*(volatile u32 *)SOC_FMI_REG5_410 = 0;
				}
				*(volatile u32 *)SOC_FMI_REG1_408 = old;
			}
			*mask_reg &= ~bit;
		}

		at(4, y, reset_ok ? (id_ok ? 0x0000B050 : 0x00E02020) : 0x00808080);
		puts_("CE"); hex(chip); putc_(' ');
		if (reset_ok && id_ok) {
			for (u32 i = 0; i < 4; i++) {
				putc_("0123456789ABCDEF"[id[i] >> 4]);
				putc_("0123456789ABCDEF"[id[i] & 15]);
			}
		} else {
			puts_(reset_ok ? "NO ID" : "NO RESET");
		}
		putc_('\n'); fb_clean();
		y += 18;
	}
#endif
