/* STAGE 20. Built as: trace_which_timer.bin.
 * Included by trace.c inside payload_main(): it shares that function's locals, so it is a block, not yet a unit of its own.
 * To change it, edit it here; `make -C tools/payloads sums` must leave every other stage byte-identical. */
#if STAGE == 20
	/* Which timer actually causes the periodic VIC0 source-6 IRQ seen on 2026-10-15 (taken even
	 * when our own timer 5 was never started)? Timers 0-3 showed structured (non-zero) values
	 * that day -- already in active use by the iBSS, unlike timer 5. Read-only: sample each
	 * timer's CFG/STATE/COUNT_BUFFER twice, ~0.5 s apart (longer than the shortest period seen,
	 * 0.188 s), and show what changed. No IRQ taken here, no handler touched -- just two
	 * snapshots, same style as the button noise measurement (STAGE 12). Offsets: timer N base =
	 * N*0x20 for N<4 (STATE offsets 0x04/0x24/0x44/0x64 match the 2026-10-15 survey); CFG=+0,
	 * STATE=+4, COUNT_BUFFER=+8 (same layout already used for timer 5 at SOC_TIMER5_BASE). */
	at(4, 116, 0x00F0C040); puts_("STEP 20 WHICH TIMER\n");
	volatile u32 *tick = (volatile u32 *)(SOC_TIMER_BASE + 0x200);
	u32 cfg0[4], state0[4], cnt0[4];
	for (u32 n = 0; n < 4; n++) {
		volatile u32 *t = (volatile u32 *)(SOC_TIMER_BASE + n * 0x20);
		cfg0[n] = t[0]; state0[n] = t[1]; cnt0[n] = t[2];
	}
	{ u32 t0 = *tick; while (*tick - t0 < 24000000u / 2) ; }   /* 0.5 s */
	u32 y = 134;
	for (u32 n = 0; n < 4; n++) {
		volatile u32 *t = (volatile u32 *)(SOC_TIMER_BASE + n * 0x20);
		u32 cfg1 = t[0], state1 = t[1], cnt1 = t[2];
		at(4, y, (state1 != state0[n] || cnt1 != cnt0[n] || cfg1 != cfg0[n]) ? 0x0000B050 : 0x00A0B0C0);
		puts_("T"); hex(n); puts_(" "); fb_clean(); y += 18;
		at(4, y, cfg1 != cfg0[n] ? 0x0000B050 : 0x00FFFFFF);
		puts_("  CFG "); hex(cfg0[n]); puts_(" -> "); hex(cfg1); putc_('\n'); fb_clean(); y += 18;
		at(4, y, cnt1 != cnt0[n] ? 0x0000B050 : 0x00FFFFFF);
		puts_("  CNT "); hex(cnt0[n]); puts_(" -> "); hex(cnt1); putc_('\n'); fb_clean(); y += 18;
		at(4, y, state1 != state0[n] ? 0x0000B050 : 0x00FFFFFF);
		puts_("  STA "); hex(state0[n]); puts_(" -> "); hex(state1); putc_('\n'); fb_clean(); y += 18;
	}
#endif
