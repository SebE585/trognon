/* STAGE 28. Built as: trace_iop_clkgate.bin.
 * Included by trace.c inside payload_main(): it shares that function's locals, so it is a block, not yet a unit of its own.
 * To change it, edit it here; `make -C tools/payloads sums` must leave every other stage byte-identical. */
#if STAGE == 28
	/* iop_probe, step 2 (read-only part): the device tree's "iop" node has its own
	 * clock-gates = 0x27 (39). AppleARM7M::start()'s own disassembly (read in full,
	 * 2026-10-16) has no call enabling it and no call to "function-device_reset" either --
	 * both are candidate explanations for STEP 26's all-zero read, but neither was found as
	 * something this specific driver does itself. Check the gate's current state directly,
	 * read-only, before guessing any further. */
	at(4, 116, 0x00F0C040); puts_("STEP 28 IOP CLKGATE\n");
	u32 gate = *(volatile u32 *)SOC_CLOCK_GATE_27;
	kv("GATE+27 ", gate);
	result_hex("gate27", gate);
	fb_clean();
#endif
