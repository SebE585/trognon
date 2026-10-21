/* STAGE 47. Built as: trace_gate_survey.bin.
 * Included by trace.c inside payload_main(): it shares that function's locals, so it is a block, not yet a unit of its own.
 * To change it, edit it here; `make -C tools/payloads sums` must leave every other stage byte-identical. */
#if STAGE == 47
	/* READ-ONLY survey of all 52 clock-gate words (0xBF100078 + 4 * i, i = 0 to 0x33, the range the decrypted iBSS's clock
	 * initialisation at 0x8400dd04 loops over). Nothing is written. Purpose: test the prediction made from that routine
	 * (catalog iBSS_n88ap_4.3.5/clock_gate_init_0x8400dd58) that gates 0, 0x18, 0x24, 0x2C, 0x2D and 0x31 are the ones
	 * it leaves on, and learn which others were turned on afterwards (already seen on: 0x09 to 0x0D, 0x19, 0x27). */
	at(4, 116, 0x00F0C040); puts_("STEP 47 CLOCK GATE SURVEY\n");
	volatile u32 *cg47 = clock_gates();
	u32 on47 = 0, hi47 = 0, lo47 = 0;
	for (u32 i = 0; i < CLOCK_NGATES; i++) {
		u32 v = cg47[i];
		char nm[4] = { 'g', "0123456789ABCDEF"[(i >> 4) & 0xF], "0123456789ABCDEF"[i & 0xF], 0 };
		result_hex(nm, v);
		if ((v & 0xF) == 0xF) { if (i < 32) lo47 |= 1u << i; else hi47 |= 1u << (i - 32); on47++; }
	}
	kv("ON", on47); kv("MASK LO", lo47); kv("MASK HI", hi47);
	result_hex("n_on", on47); result_hex("mask_lo", lo47); result_hex("mask_hi", hi47);
	/* The kernel's clock controller applies 64-bit gate masks through two registers at offsets 0x48 and 0x4C of the same block
	 * (the message "asserting CGCON0" in AppleS5L8900XClockController). Read-only, fault-tolerant, offsets 0x40 to 0x74. */
	volatile u32 *cc47 = (volatile u32 *)SOC_TIMER_BASE;
	SC->quiet = 1;
	for (u32 o = 0x40; o <= 0x74; o += 4) {
		u32 before = SC->hit, v = 0;
		SC->expected = 1;
		asm volatile("adr.w r1, 1f\n\tstr r1, [%2]\n\tldr %0, [%1]\n1:"
			     : "+r"(v) : "r"(&cc47[o / 4]), "r"(&SC->resume) : "r1", "memory");
		SC->expected = 0;
		char nm[4] = { 'c', "0123456789ABCDEF"[(o >> 4) & 0xF], "0123456789ABCDEF"[o & 0xF], 0 };
		result_hex(nm, (SC->hit == before) ? v : 0xDEAD0000);
	}
	SC->quiet = 0;
	fb_clean();
#endif
