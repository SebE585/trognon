/* STAGE 13. Built as: trace_nand.bin.
 * Included by trace.c inside payload_main(): it shares that function's locals, so it is a block, not yet a unit of its own.
 * To change it, edit it here; `make -C tools/payloads sums` must leave every other stage byte-identical. */
#if STAGE == 13
	/* NAND (H2FMI) chip ID probe, bus 0 (fmi0) / CE0 only, read-only. Reproduces by hand, from
	 * OpeniBoot source (plat-s5l8920/h2fmi.c): h2fmi_init_bus() -> h2fmi_nand_reset(chip 0) ->
	 * h2fmi_read_chipid(chip 0). No write to any page, no timing table, no ECC, no AES, no DMA:
	 * just the raw command sequence any NAND chip answers to a reset + READ ID. First real test
	 * of docs/research/2026-10-15-touch-wifi-nand-recon.md's decision to look at NAND before
	 * deciding bare-metal vs Linux. SOURCE for every register offset: soc/s5l8920.h. */
	#define FMI_SEC 24000000u
	volatile u32 *fmi_tick = (volatile u32 *)(SOC_TIMER_BASE + 0x200);
	at(4, 116, 0x00F0C040); puts_("STEP 13 NAND ID\n");

	/* h2fmi_init_bus(fmi0): clock on, timing cache, device_reset, device_reset_814. */
	*(volatile u32 *)SOC_CLOCK_GATE_26 |= 0xF;
	*(volatile u32 *)SOC_FMI_REG1_408 = 0xFFFF;
	*(volatile u32 *)SOC_FMI_REG2_400 = 1;
	*(volatile u32 *)SOC_FMI_REG1_408 = 0xFFFF;
	*(volatile u32 *)SOC_FMI_REG3_814 = 0;

	/* h2fmi_nand_reset(chip 0): enable CE0, reset command, wait for done (bit 0), disable CE0. */
	*(volatile u32 *)SOC_FMI_CHIPMASK_40C = 1;
	*(volatile u32 *)SOC_FMI_REG4_414 = 0xFF;
	*(volatile u32 *)SOC_FMI_REG5_410 = 1;
	u32 t0 = *fmi_tick, reset_ok = 0;
	while (*fmi_tick - t0 < FMI_SEC / 100) {         /* 10 ms, like h2fmi_wait_for_done()'s own timeout */
		if ((*(volatile u32 *)SOC_FMI_REG6_444 & 1) == 1) { reset_ok = 1; break; }
	}
	*(volatile u32 *)SOC_FMI_CHIPMASK_40C &= ~1u;
	kv("RESET_OK", reset_ok);

	u8 chipid[8] = {0};
	u32 id_ok = 0;
	if (reset_ok) {
		/* h2fmi_read_chipid(chip 0, unk=0): enable CE0, READ ID command, wait (bits 0x9), then
		 * h2fmi_read(): 8 bytes, one at a time, each preceded by command 0x50 + a short delay. */
		*(volatile u32 *)SOC_FMI_CHIPMASK_40C = 1;
		*(volatile u32 *)SOC_FMI_REG4_414 = 0x90;
		*(volatile u32 *)SOC_FMI_REG9_418 = 0;
		*(volatile u32 *)SOC_FMI_REG10_420 = 0;
		*(volatile u32 *)SOC_FMI_REG5_410 = 9;
		t0 = *fmi_tick;
		while (*fmi_tick - t0 < FMI_SEC / 100) {
			if ((*(volatile u32 *)SOC_FMI_REG6_444 & 9) == 9) { id_ok = 1; break; }
		}
		if (id_ok) {
			u32 old = *(volatile u32 *)SOC_FMI_REG1_408;
			*(volatile u32 *)SOC_FMI_REG1_408 = 0;
			*(volatile u32 *)SOC_FMI_REG8_424 = 0;
			for (u32 i = 0; i < 8; i++) {
				*(volatile u32 *)SOC_FMI_REG5_410 = 0x50;
				{ u32 d0 = *fmi_tick; while (*fmi_tick - d0 < 24) ; }   /* ~1us at 24MHz, udelay(1) */
				chipid[i] = (u8)(*(volatile u32 *)SOC_FMI_DATA_448 & 0xFF);
				*(volatile u32 *)SOC_FMI_REG5_410 = 0;
			}
			*(volatile u32 *)SOC_FMI_REG1_408 = old;
		}
		*(volatile u32 *)SOC_FMI_CHIPMASK_40C &= ~1u;
	}
	kv("ID_OK   ", id_ok);
	at(4, 152, 0x00FFFFFF); puts_("CHIPID ");
	for (u32 i = 0; i < 8; i++) {
		putc_("0123456789ABCDEF"[chipid[i] >> 4]);
		putc_("0123456789ABCDEF"[chipid[i] & 15]);
		putc_(' ');
	}
	putc_('\n'); fb_clean();
#endif
