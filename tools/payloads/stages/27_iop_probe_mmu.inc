/* STAGE 27. Built as: trace_iop_probe_mmu.bin.
 * Included by trace.c inside payload_main(): it shares that function's locals, so it is a block, not yet a unit of its own.
 * To change it, edit it here; `make -C tools/payloads sums` must leave every other stage byte-identical. */
#if STAGE == 27
	/* iop_probe, step 1: close the mapping question before touching the IOP register window
	 * again -- read the MMU's own L1 descriptor for VA SOC_IOP_REG_BASE (0x86300000) instead
	 * of assuming identity mapping or a "set the high bit" rule. The device tree's own
	 * "ranges" property on /device-tree/arm-io (0x0,0x80000000,0x40000000 then
	 * 0x40000000,0x40000000,0x40000000 -- i.e. "+0x80000000" only for child addresses below
	 * 0x40000000, identity above) already confirms the translation used to reach
	 * 0x86300000/0xbf300000 from the reg property's 0x06300000/0x3f300000; this step checks
	 * the OTHER half, the CPU's own page table, read-only, no write at all. */
	at(4, 116, 0x00F0C040); puts_("STEP 27 IOP MMU\n");
	u32 ttbr0; asm volatile("mrc p15, 0, %0, c2, c0, 0" : "=r"(ttbr0));
	u32 va = SOC_IOP_REG_BASE;
	u32 l1_addr = (ttbr0 & 0xFFFFC000) + ((va >> 20) * 4);
	u32 l1 = *(volatile u32 *)l1_addr;
	u32 type = l1 & 0x3;                 /* 0=fault 1=page table 2=section 3=reserved */
	u32 sect_base = l1 & 0xFFF00000;     /* valid only if type == 2 */
	u32 domain = (l1 >> 5) & 0xF;
	u32 ap     = (l1 >> 10) & 0x3;
	u32 tex    = (l1 >> 12) & 0x7;
	u32 c_bit  = (l1 >> 3) & 1;
	u32 b_bit  = (l1 >> 2) & 1;
	kv("TTBR0   ", ttbr0);
	kv("L1ADDR  ", l1_addr);
	kv("L1DESC  ", l1);
	kv("TYPE    ", type);
	kv("SECTBASE", sect_base);
	kv("DOMAIN  ", domain);
	kv("AP:TEXCB", (ap << 8) | (tex << 4) | (c_bit << 1) | b_bit);
	at(4, 350, type == 2 && sect_base == (va & 0xFFF00000) ? 0x0000B050 : 0x00E02020);
	puts_(type == 2 && sect_base == (va & 0xFFF00000) ? "IDENTITY SECTION\n" : "NOT WHAT WE ASSUMED\n");
	result_hex("ttbr0", ttbr0);
	result_hex("l1desc", l1);
	result_hex("l1type", type);
	result_hex("l1base", sect_base);
	result_hex("l1dom", domain);
	result_hex("l1aptex", (ap << 8) | (tex << 4) | (c_bit << 1) | b_bit);
	fb_clean();
#endif
