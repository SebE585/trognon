/* STAGE 7. Built as: trace_irq.bin.
 * Included by trace.c inside payload_main(): it shares that function's locals, so it is a block, not yet a unit of its own.
 * To change it, edit it here; `make -C tools/payloads sums` must leave every other stage byte-identical. */
#if STAGE == 7
	/* Test 4: software-triggered IRQ on VIC0. IRQs masked while we set up; every VIC0 source the iBSS
	 * left enabled is disabled (and restored after) so only ours can fire. */
	at(4, 116, 0x00F0C040); puts_("STEP 7 SOFT IRQ\n");
	at(4, 134, 0x00FFFFFF);
	u32 cpsr0, en0 = VICREG(SOC_VIC0, 0x010);
	asm volatile("mrs %0, cpsr" : "=r"(cpsr0));
	u32 bit = ~en0 & -(~en0);                           /* lowest source not enabled by the iBSS */
	SC->irq = 0; SC->stray = 0; SC->irq_bit = bit; SC->irq_status = 0; SC->irq_vec = 0;
	kv("CPSR  ", cpsr0);
	kv("EN0   ", en0);
	kv("BIT   ", bit);
	asm volatile("cpsid i" ::: "memory");
	VICREG(SOC_VIC0, 0x014) = 0xFFFFFFFF;               /* INTENCLEAR: all of VIC0 off */
	u32 en_off = VICREG(SOC_VIC0, 0x010);
	hook_irq(1);
	VICREG(SOC_VIC0, 0x010) = bit;                      /* INTENABLE: ours only */
	VICREG(SOC_VIC0, 0x018) = bit;                      /* SOFTINT: raise it */
	asm volatile("dsb sy" ::: "memory");
	u32 raw = VICREG(SOC_VIC0, 0x008), stat = VICREG(SOC_VIC0, 0x000);
	kv("EN OFF", en_off);
	kv("RAW   ", raw);
	kv("STAT  ", stat);
	at(4, 134 + 6 * 18, 0x00F0C040); puts_("UNMASKING\n");
	asm volatile("cpsie i" ::: "memory");
	for (volatile u32 w = 0; w < 0x4000000 && !SC->irq && !SC->stray; w++) ;
	asm volatile("cpsid i" ::: "memory");
	hook_irq(0);                                         /* vector back BEFORE the iBSS sources return */
	u32 raw2 = VICREG(SOC_VIC0, 0x008), stat2 = VICREG(SOC_VIC0, 0x000);
	VICREG(SOC_VIC0, 0x014) = 0xFFFFFFFF;
	VICREG(SOC_VIC0, 0x01C) = bit;                      /* SOFTINTCLEAR, in case nothing took it */
	VICREG(SOC_VIC0, 0x010) = en0;                      /* restore the iBSS's enables */
	u32 en_back = VICREG(SOC_VIC0, 0x010);
	if (!(cpsr0 & 0x80))
		asm volatile("cpsie i" ::: "memory");
	at(4, 134 + 7 * 18, SC->irq ? 0x0000B050 : 0x00E02020);
	puts_(SC->irq ? "IRQ TAKEN\n" : "NO IRQ\n");
	at(4, 134 + 8 * 18, 0x00FFFFFF);
	kv("HITS  ", SC->irq);
	kv("STRAY ", SC->stray);
	kv("H STAT", SC->irq_status);
	kv("H VEC ", SC->irq_vec);
	kv("RAW2  ", raw2);
	kv("STAT2 ", stat2);
	kv("EN BCK", en_back);
#endif
