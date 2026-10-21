/* STAGE 26. Built as: trace_iop_regs.bin.
 * Included by trace.c inside payload_main(): it shares that function's locals, so it is a block, not yet a unit of its own.
 * To change it, edit it here; `make -C tools/payloads sums` must leave every other stage byte-identical. */
#if STAGE == 26
	/* First look at the IOP's own control/boot register window (SOC_IOP_REG_BASE =
	 * 0x86300000), read-only, no write at all: does the main CPU even see this address as
	 * mapped/live, and if so, what does it currently hold? This is the milestone-1 dynamic
	 * check from docs/research/2026-10-16-wifi-iop-architecture.md -- offsets +0x100
	 * (control/status), +0x110 (firmware pointer), +0x114 (size), +0x118 (self-address) are
	 * what AppleARM7M::startCPU writes on the host's own boot path; reading them here does
	 * not tell us whether the IOP core itself is executing, only whether this window is
	 * reachable and what state it is in right now, under our own tethered DFU/iBSS path
	 * (which skips the normal iOS boot that would normally run AppleARM7M::start()). */
	at(4, 116, 0x00F0C040); puts_("STEP 26 IOP REGS\n");
	volatile u32 *iop = (volatile u32 *)SOC_IOP_REG_BASE;
	u32 r000 = iop[0x000 / 4];
	u32 r100 = iop[0x100 / 4];
	u32 r110 = iop[0x110 / 4];
	u32 r114 = iop[0x114 / 4];
	u32 r118 = iop[0x118 / 4];
	kv("IOP+000 ", r000);
	kv("IOP+100 ", r100);
	kv("IOP+110 ", r110);
	kv("IOP+114 ", r114);
	kv("IOP+118 ", r118);
	result_hex("iop000", r000);
	result_hex("iop100", r100);
	result_hex("iop110", r110);
	result_hex("iop114", r114);
	result_hex("iop118", r118);
	fb_clean();
#endif
