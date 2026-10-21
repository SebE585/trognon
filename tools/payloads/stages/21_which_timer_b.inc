/* STAGE 21. Built as: trace_which_timer_b.bin.
 * Included by trace.c inside payload_main(): it shares that function's locals, so it is a block, not yet a unit of its own.
 * To change it, edit it here; `make -C tools/payloads sums` must leave every other stage byte-identical. */
#if STAGE == 21
	/* STAGE 20 sampled only CFG/STATE/COUNT_BUFFER (offsets +0/+4/+8) for timers 0-3 and found
	 * nothing moving over 0.5 s. Before concluding none of them is live: sample the two fields
	 * not checked yet, COUNT_BUFFER2 (+0xC) and PRESCALER (+0x10) -- the live down-counter, if
	 * there is one, may live there instead of in COUNT_BUFFER (a "reload value" register often
	 * isn't the live count itself). Same read-only, two-snapshot technique. */
	at(4, 116, 0x00F0C040); puts_("STEP 21 WHICH TIMER B\n");
	volatile u32 *tick = (volatile u32 *)(SOC_TIMER_BASE + 0x200);
	u32 b0[4], p0[4];
	for (u32 n = 0; n < 4; n++) {
		volatile u32 *t = (volatile u32 *)(SOC_TIMER_BASE + n * 0x20);
		b0[n] = t[3]; p0[n] = t[4];
	}
	{ u32 t0 = *tick; while (*tick - t0 < 24000000u / 2) ; }   /* 0.5 s */
	u32 y = 134;
	for (u32 n = 0; n < 4; n++) {
		volatile u32 *t = (volatile u32 *)(SOC_TIMER_BASE + n * 0x20);
		u32 b1 = t[3], p1 = t[4];
		u32 changed = (b1 != b0[n]) || (p1 != p0[n]);
		at(4, y, changed ? 0x0000B050 : 0x00A0B0C0);
		puts_("T"); hex(n); puts_(" B2 "); hex(b0[n]); puts_("->"); hex(b1);
		putc_('\n'); fb_clean(); y += 18;
		at(4, y, changed ? 0x0000B050 : 0x00FFFFFF);
		puts_("   PR "); hex(p0[n]); puts_("->"); hex(p1);
		putc_('\n'); fb_clean(); y += 18;
	}
#endif
