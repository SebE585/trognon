/* STAGE 4. Built as: trace_pabt.bin.
 * Included by trace.c inside payload_main(): it shares that function's locals, so it is a block, not yet a unit of its own.
 * To change it, edit it here; `make -C tools/payloads sums` must leave every other stage byte-identical. */
#if STAGE == 4
	/* Test 3: deliberate PREFETCH ABORT. A Thumb BKPT with the debug unit not in halt mode raises
	 * a prefetch abort (IFSR = 0x2, "debug event"), with no memory fetch involved. Resume address
	 * computed in asm like for the UDF. */
	at(4, 116, 0x00F0C040); puts_("STEP 4 BKPT\n");
	u32 before = SC->hit;
	SC->expected = 1;
	asm volatile("adr.w r0, 1f\n\tstr r0, [%0]\n\tbkpt #0\n1:" :: "r"(&SC->resume) : "r0", "memory");
	SC->expected = 0;
	at(4, 270, SC->hit == before ? 0x00E02020 : 0x0000B050);
	puts_(SC->hit == before ? "NO ABORT\n" : "PABT RESUMED\n");
#endif
