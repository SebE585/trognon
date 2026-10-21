/* STAGE 43. Built as: trace_gpio_survey.bin.
 * Included by trace.c inside payload_main(): it shares that function's locals, so it is a block, not yet a unit of its own.
 * To change it, edit it here; `make -C tools/payloads sums` must leave every other stage byte-identical. */
#if STAGE == 43
	/* READ-ONLY survey of every GPIO pin register (376 words at SOC_GPIO_BASE + 4 * (8 * port + pin)), the full 32-bit
	 * word of each, with the same fault-tolerant read as STAGE 12 (an access that aborts is recorded, not fatal).
	 * Nothing is written. The point is to find pads whose configuration differs from the common default, which may
	 * include the SPI1 clock and data pads. Reported: how many answered, the distinct values with their counts
	 * (the 8 most common), and up to 48 pins whose value is not the most common one. The GPIO interrupt controller
	 * (GPIOIC) is not touched. */
	#define NPIN43 376
	volatile u32 *raw43 = (volatile u32 *)SOC_SNAPSHOTS;
	volatile u8 *ok43 = (volatile u8 *)(SOC_SNAPSHOTS + 0x800);
	volatile u32 *gp43 = gpio_regs();
	at(4, 116, 0x00F0C040); puts_("STEP 43 GPIO SURVEY\n");
	u32 nread43 = 0;
	SC->quiet = 1;
	for (u32 i = 0; i < NPIN43; i++) {
		u32 before = SC->hit, v = 0;
		SC->expected = 1;
		asm volatile("adr.w r1, 1f\n\tstr r1, [%2]\n\tldr %0, [%1]\n1:"
			     : "+r"(v) : "r"(&gp43[i]), "r"(&SC->resume) : "r1", "memory");
		SC->expected = 0;
		ok43[i] = (SC->hit == before);
		raw43[i] = ok43[i] ? v : 0;
		nread43 += ok43[i];
	}
	SC->quiet = 0;
	volatile u32 dval[24], dcnt[24];
	u32 nd = 0;
	for (u32 i = 0; i < NPIN43; i++) {
		if (!ok43[i]) continue;
		u32 k = 0;
		while (k < nd && dval[k] != raw43[i]) k++;
		if (k == nd && nd < 24) { dval[nd] = raw43[i]; dcnt[nd] = 0; nd++; }
		if (k < nd) dcnt[k]++;
	}
	kv("READABLE", nread43); kv("DISTINCT", nd);
	result_hex("n_readable", nread43); result_hex("n_distinct", nd);
	u32 top = 0;
	for (u32 r = 0; r < 8 && r < nd; r++) {          /* the 8 most common, by selection */
		u32 best = 0xFFFFFFFF, bc = 0;
		for (u32 k = 0; k < nd; k++)
			if (dcnt[k] > bc) { bc = dcnt[k]; best = k; }
		if (best == 0xFFFFFFFF) break;
		char nv[4] = { 'v', (char)('0' + r), 0, 0 }, nc[4] = { 'c', (char)('0' + r), 0, 0 };
		result_hex(nv, dval[best]); result_hex(nc, dcnt[best]);
		if (r == 0) top = dval[best];
		dcnt[best] = 0;                                 /* consumed (counts no longer needed) */
	}
	u32 shown = 0, nrare = 0;
	for (u32 i = 0; i < NPIN43; i++) {
		if (!ok43[i] || raw43[i] == top) continue;
		nrare++;
		if (shown < 48) {
			char nm[5] = { 'g', "0123456789ABCDEF"[(i >> 8) & 15], "0123456789ABCDEF"[(i >> 4) & 15],
				       "0123456789ABCDEF"[i & 15], 0 };
			result_hex(nm, raw43[i]);
			shown++;
		}
	}
	kv("NOT TOP", nrare); kv("TOP VAL", top);
	result_hex("n_not_top", nrare); result_hex("top_value", top);
	/* Full table: the distinct values (dt00..) as in first-seen order, and one 8-character string per port
	 * (pt00..pt2e) holding, for its 8 pins, the index of each pin's value in that table, in the alphabet
	 * 0-9 then A-V ('-' for a pin that did not answer). */
	volatile u32 dvals[32];
	u32 ndv = 0;
	for (u32 i = 0; i < NPIN43; i++) {
		if (!ok43[i]) continue;
		u32 k = 0;
		while (k < ndv && dvals[k] != raw43[i]) k++;
		if (k == ndv && ndv < 32) dvals[ndv++] = raw43[i];
	}
	for (u32 k = 0; k < ndv; k++) {
		char nm[5] = { 'd', 't', "0123456789ABCDEF"[(k >> 4) & 15], "0123456789ABCDEF"[k & 15], 0 };
		result_hex(nm, dvals[k]);
	}
	for (u32 port = 0; port < NPIN43 / 8; port++) {
		char nm[5] = { 'p', 't', "0123456789ABCDEF"[(port >> 4) & 15], "0123456789ABCDEF"[port & 15], 0 };
		char val[9];
		for (u32 pin = 0; pin < 8; pin++) {
			u32 i = port * 8 + pin, k = 0;
			if (!ok43[i]) { val[pin] = '-'; continue; }
			while (k < ndv && dvals[k] != raw43[i]) k++;
			val[pin] = "0123456789ABCDEFGHIJKLMNOPQRSTUV"[k & 31];
		}
		val[8] = 0;
		result_str(nm, val);
	}
	fb_clean();
#endif
