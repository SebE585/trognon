/* STAGE 12. Built as: trace_buttons.bin.
 * Included by trace.c inside payload_main(): it shares that function's locals, so it is a block, not yet a unit of its own.
 * To change it, edit it here; `make -C tools/payloads sums` must leave every other stage byte-identical. */
#if STAGE == 12
	/* Button discovery by differences, paced by a human. OpeniBoot's button ids for this SoC look copied from an
	 * older one, so we do not trust them: read bit 0 of every GPIO pin register (GPIO + (8*port+pin)*4, SOURCE
	 * layout), learn the pins that move by themselves (noise), then ask for each button in turn, wait up to 30 s
	 * for a pin to change and list the pins that moved. Read-only: no pin is ever written or reconfigured.
	 * Results stay on the screen (and so in the transcript): no extra env variables, the iBSS env store is small. */
	#define NPIN 376
	volatile u8 *base = (volatile u8 *)SOC_SNAPSHOTS;   /* level when the prompt appeared */
	volatile u8 *noisy = base + NPIN;                     /* moved with nobody touching */
	volatile u8 *mark = base + 2 * NPIN;                  /* moved while a button was handled */
	volatile u8 *readable = base + 3 * NPIN;
	volatile u32 *gp = gpio_regs();
	#define TICK (*(volatile u32 *)(SOC_TIMER_BASE + 0x200))
	#define SEC 24000000u
	at(4, 116, 0x00F0C040); puts_("STEP 12 BUTTONS\n");
	u32 nread = 0;
	SC->quiet = 1;
	for (u32 i = 0; i < NPIN; i++) {                          /* which pin registers answer */
		u32 before = SC->hit, v = 0;
		SC->expected = 1;
		asm volatile("adr.w r1, 1f\n\tstr r1, [%2]\n\tldr %0, [%1]\n1:"
			     : "+r"(v) : "r"(&gp[i]), "r"(&SC->resume) : "r1", "memory");
		SC->expected = 0;
		readable[i] = SC->hit == before;
		base[i] = (u8)(v & 1); noisy[i] = 0; mark[i] = 0;
		nread += readable[i];
	}
	SC->quiet = 0;
	at(4, 134, 0x00FFFFFF);
	kv("READABLE", nread);
	/* 10 s to get ready, then 3 s of noise measurement with hands off. */
	for (u32 s = 10; s > 0; s--) {
		console_clear_row(152);
		at(4, 152, 0x00F0C040); puts_("GET READY ");
		hex(s); fb_clean();
		u32 t0 = TICK;
		while (TICK - t0 < SEC) ;
	}
	console_clear_row(152);
	at(4, 152, 0x00F0C040); puts_("HANDS OFF 3 S"); fb_clean();
	for (u32 i = 0; i < NPIN; i++) base[i] = (u8)(gp[i] & 1);
	{
		u32 t0 = TICK;
		while (TICK - t0 < 3 * SEC)
			for (u32 i = 0; i < NPIN; i++)
				if (readable[i] && (u8)(gp[i] & 1) != base[i])
					noisy[i] = 1;
	}
	u32 nn = 0;
	for (u32 i = 0; i < NPIN; i++) nn += noisy[i];
	console_clear_row(152);
	at(4, 152, 0x00FFFFFF); kv("NOISY   ", nn);
	static const char *const names[5] = { "HOME", "POWER", "VOL UP", "VOL DOWN", "RINGER" };
	static const char *const verbs[5] = { "PRESS ", "PRESS ", "PRESS ", "PRESS ", "FLIP " };
	u32 y = 170;
	for (u32 b = 0; b < 5; b++) {
		for (u32 i = 0; i < NPIN; i++) { base[i] = (u8)(gp[i] & 1); mark[i] = 0; }   /* fresh baseline for this button */
		at(4, y, 0x00F0C040);
		puts_(verbs[b]); puts_(names[b]); puts_(" NOW"); fb_clean();
		u32 t0 = TICK, t_found = 0, last_s = 99;
		for (;;) {
			u32 now = TICK - t0;
			u32 left = 30 - now / SEC;
			if (left != last_s) {                              /* live countdown on the next row */
				last_s = left;
				console_clear_row(y + 18);
				at(4, y + 18, 0x00A0B0C0); puts_("WAITING "); hex(left); fb_clean();
			}
			for (u32 i = 0; i < NPIN; i++)
				if (readable[i] && !noisy[i] && (u8)(gp[i] & 1) != base[i]) {
					mark[i] = 1;
					if (!t_found) t_found = now ? now : 1;
				}
			if (t_found && now - t_found > SEC)                /* a pin moved: 1 s more to catch the others */
				break;
			if (now > 30 * SEC)
				break;
		}
		console_clear_row(y); console_clear_row(y + 18);
		at(4, y, t_found ? 0x0000B050 : 0x00E02020);
		puts_(names[b]);
		char list[64]; u32 n = 0, shown = 0;
		for (u32 i = 0; i < NPIN && shown < 5; i++) {
			if (!mark[i]) continue;
			list[n++] = ' ';
			list[n++] = "0123456789ABCDEF"[(i >> 3) >> 4];     /* port */
			list[n++] = "0123456789ABCDEF"[(i >> 3) & 15];
			list[n++] = '.';
			list[n++] = (char)('0' + (i & 7));                 /* pin */
			shown++;
		}
		list[n] = 0;
		puts_(t_found ? list : " NONE"); putc_('\n'); fb_clean();
		y += 18;
		if (b < 4) {                                           /* let go before the next prompt */
			u32 t1 = TICK;
			while (TICK - t1 < 2 * SEC) ;
		}
	}
	at(4, y + 18, 0x00FFFFFF);
	puts_("DONE\n");
#endif
