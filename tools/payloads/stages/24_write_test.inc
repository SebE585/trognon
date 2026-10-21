/* STAGE 24. Built as: trace_write_test.bin.
 * Included by trace.c inside payload_main(): it shares that function's locals, so it is a block, not yet a unit of its own.
 * To change it, edit it here; `make -C tools/payloads sums` must leave every other stage byte-identical. */
#if STAGE == 24
	/* STAGE 23's clock-gate test was flawed (the gate was already 0xF from earlier runs this
	 * session, so the before/after comparison couldn't tell write-from-no-op). Proper test this
	 * time: write a recognisable, non-trivial value to the SDIO base itself and read it straight
	 * back. If THIS doesn't stick, the 0x80000000 region genuinely does not accept writes at this
	 * boot stage -- a mapping/bus-fabric fact, not a sequence mistake. */
	at(4, 116, 0x00F0C040); puts_("STEP 24 WRITE TEST\n");
	volatile u32 *sdio_base = (volatile u32 *)SOC_SDIO0_BASE;
	u32 before = *sdio_base;
	*sdio_base = 0xDEADBEEF;
	u32 after = *sdio_base;
	kv("BEFORE  ", before);
	kv("WROTE   ", 0xDEADBEEF);
	kv("READBACK", after);
	at(4, 188, after == 0xDEADBEEF ? 0x0000B050 : 0x00E02020);
	puts_(after == 0xDEADBEEF ? "WRITE STUCK\n" : "WRITE LOST\n");
	fb_clean();
	*sdio_base = before;   /* restore, for tidiness -- harmless either way given the result above */
#endif
