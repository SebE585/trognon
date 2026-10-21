/* STAGE 3. Built as: trace.bin.
 * Included by trace.c inside payload_main(): it shares that function's locals, so it is a block, not yet a unit of its own.
 * To change it, edit it here; `make -C tools/payloads sums` must leave every other stage byte-identical. */
#if STAGE == 3
	/* Test 2: deliberate data abort = ALIGNMENT fault. The live page table has no translation-fault
	 * entry and every section is in domain 0 (DACR=3, manager), read on the device 2026-10-15, so
	 * neither a fault entry nor a domain fault is available. Instead set SCTLR.A (bit 1) around one
	 * unaligned load on mapped RAM: the fault is raised before the bus access, nothing is touched.
	 * exc_c() clears A on entry, so the handler never runs with strict alignment. */
	u32 va = (u32)&SC->hit + 1;
	at(4, 252, 0x00A0B0C0); kv("UNALIGNED VA", va);
	at(4, 288, 0x00F0C040); puts_("STEP 3 DATA ABT\n");
	u32 before = SC->hit, sctlr_a = sctlr | 2;
	SC->expected = 1;
	asm volatile("adr.w r1, 1f\n\tstr r1, [%1]\n\t"
		     "mcr p15, 0, %2, c1, c0, 0\n\tisb sy\n\t"
		     "ldr r0, [%0]\n1:\n\t"
		     "mcr p15, 0, %3, c1, c0, 0\n\tisb sy"
		     :: "r"(va), "r"(&SC->resume), "r"(sctlr_a), "r"(sctlr) : "r0", "r1", "memory");
	SC->expected = 0;
	at(4, 432, SC->hit == before ? 0x00E02020 : 0x0000B050);
	puts_(SC->hit == before ? "NO ABORT\n" : "ABT RESUMED\n");
#endif
