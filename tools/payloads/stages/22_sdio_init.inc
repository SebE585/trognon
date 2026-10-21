/* STAGE 22. Built as: trace_sdio_init.bin.
 * Included by trace.c inside payload_main(): it shares that function's locals, so it is a block, not yet a unit of its own.
 * To change it, edit it here; `make -C tools/payloads sums` must leave every other stage byte-identical. */
#if STAGE == 22
	/* Full SDIO activation, sourced from a REAL working driver found tonight:
	 * iDroid-Project/iDroid-kernel, drivers/mmc/host/iphone-sdio.c, sdio_init() (lines 197-227).
	 * Everything in STAGE 17-19 only gated the clock and tried CMD0 directly -- this driver's
	 * own init does four more steps we never did: set SDCLK divider (CLKDIV), reset the FIFO
	 * (DCTRL), explicitly ENABLE the block (CTRL=1), and clear status/IRQ latches (STAC/IRQ).
	 * Register offsets (CTRL/CMD/ARGU/STATE/DSTA/CLKDIV/DCTRL/STAC/IRQ) match exactly what
	 * plat-s5l8900's own header already gave us -- same IP family, now confirmed by a driver
	 * that actually runs, not dead/disabled source. Clock gate: this device's own device-tree
	 * value (0x10), the most authoritative one we have (the driver's own SDIO_CLOCKGATE=0xB is
	 * for a different SoC generation, kept only as a documented alternative, not used here). */
	#define SDIO_CTRL   (SOC_SDIO0_BASE + 0x00)
	#define SDIO_DCTRL  (SOC_SDIO0_BASE + 0x04)
	#define SDIO_CMD2   (SOC_SDIO0_BASE + 0x08)
	#define SDIO_ARGU2  (SOC_SDIO0_BASE + 0x0C)
	#define SDIO_STATE2 (SOC_SDIO0_BASE + 0x10)
	#define SDIO_STAC   (SOC_SDIO0_BASE + 0x14)
	#define SDIO_DSTA2  (SOC_SDIO0_BASE + 0x18)
	#define SDIO_CLKDIV (SOC_SDIO0_BASE + 0x30)
	#define SDIO_IRQREG (SOC_SDIO0_BASE + 0x38)
	volatile u32 *tick2 = (volatile u32 *)(SOC_TIMER_BASE + 0x200);

	at(4, 116, 0x00F0C040); puts_("STEP 22 SDIO INIT\n");
	*(volatile u32 *)SOC_CLOCK_GATE_10 |= 0xF;                 /* clock gate ON (device tree value) */
	*(volatile u32 *)SDIO_CLKDIV = 1u << 7;                    /* SDCLK = PCLK/128 */
	*(volatile u32 *)SDIO_DCTRL = 0x3;                         /* reset FIFO */
	*(volatile u32 *)SDIO_DCTRL = 0x0;
	*(volatile u32 *)SDIO_CTRL = 0x1;                          /* enable SDIO, 1-bit bus */
	*(volatile u32 *)SDIO_STAC = 0xFFFFFFFF;                   /* clear status */
	*(volatile u32 *)SDIO_IRQREG = 0xFFFFFFFF;                 /* clear IRQ */

	kv("CTRL    ", *(volatile u32 *)SDIO_CTRL);
	kv("CLKDIV  ", *(volatile u32 *)SDIO_CLKDIV);
	kv("STATE   ", *(volatile u32 *)SDIO_STATE2);
	kv("DSTA    ", *(volatile u32 *)SDIO_DSTA2);

	/* Now the same CMD0 attempt as STAGE 19, with the block actually enabled this time. */
	*(volatile u32 *)SDIO_ARGU2 = 0;
	*(volatile u32 *)SDIO_CMD2 = 0;
	{ u32 t0 = *tick2; while (!(*(volatile u32 *)SDIO_DSTA2 & 1) && (*tick2 - t0) < 24000u * 50) ; }
	u32 dsta_rdy = *(volatile u32 *)SDIO_DSTA2;
	kv("DSTA_RDY", dsta_rdy);
	if (dsta_rdy & 1) {
		*(volatile u32 *)SDIO_CMD2 = *(volatile u32 *)SDIO_CMD2 | (1u << 31);
		{ u32 t0 = *tick2; while (!((*(volatile u32 *)SDIO_DSTA2 >> 2) & 1) && (*tick2 - t0) < 24000u * 50) ; }
	}
	kv("DSTA_AF ", *(volatile u32 *)SDIO_DSTA2);
	kv("STATE_AF", *(volatile u32 *)SDIO_STATE2);
#endif
