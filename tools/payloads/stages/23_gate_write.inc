/* STAGE 23. Built as: trace_gate_write.bin.
 * Included by trace.c inside payload_main(): it shares that function's locals, so it is a block, not yet a unit of its own.
 * To change it, edit it here; `make -C tools/payloads sums` must leave every other stage byte-identical. */
#if STAGE == 23
	/* STAGE 22 wrote CTRL=1 but read it back as 0: the write itself didn't stick, not just "DSTA
	 * never goes ready". Check whether even the CLOCK GATE write persists -- if it doesn't
	 * either, this whole peripheral domain is likely not accessible for writes at this boot
	 * stage at all (gated by something above clock-gating, e.g. a power domain only the real OS
	 * enables later), not a sequence/offset mistake. Read-only except the one clock-gate write
	 * already done safely for NAND earlier. */
	at(4, 116, 0x00F0C040); puts_("STEP 23 GATE WRITE?\n");
	u32 before = *(volatile u32 *)SOC_CLOCK_GATE_10;
	kv("GATE_B4 ", before);
	*(volatile u32 *)SOC_CLOCK_GATE_10 |= 0xF;
	u32 after = *(volatile u32 *)SOC_CLOCK_GATE_10;
	kv("GATE_AF ", after);
	at(4, 170, after != before ? 0x0000B050 : 0x00E02020);
	puts_(after != before ? "WRITE STUCK\n" : "WRITE LOST\n");
	fb_clean();

	/* Same check on the NAND clock gate (0x26), already proven to work tonight -- a control. */
	u32 nb4 = *(volatile u32 *)SOC_CLOCK_GATE_26;
	*(volatile u32 *)SOC_CLOCK_GATE_26 |= 0xF;
	u32 naf = *(volatile u32 *)SOC_CLOCK_GATE_26;
	kv("NAND B4 ", nb4);
	kv("NAND AF ", naf);
#endif
