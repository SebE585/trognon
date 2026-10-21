/* STAGE 19. Built as: trace_sdio_cmd0.bin.
 * Included by trace.c inside payload_main(): it shares that function's locals, so it is a block, not yet a unit of its own.
 * To change it, edit it here; `make -C tools/payloads sums` must leave every other stage byte-identical. */
#if STAGE == 19
	/* First real SDIO bus command: CMD0 (GO_IDLE_STATE), the one SD command with no response
	 * and whose sole purpose is "reset the bus state" -- designed to be safe to send blind.
	 * Register layout hypothesis: plat-s5l8900's simple CTRL/CMD/ARGU/STATE/DSTA layout (SOURCE,
	 * not confirmed for s5l8920 -- the generation gap to A4's SDHCI-standard layout is assumed
	 * to be a later redesign, reasoned not proven). Sequence copied from h2fmi.c's own
	 * sdio_reset(), a block the ORIGINAL author left disabled behind "#if 0" (we don't know why
	 * they disabled it; citing it as the documented command format, not as proof it works):
	 *   ARGU=0; CMD=0 (index 0); wait DSTA bit0 ("ready"); CMD|=1<<31 ("execute");
	 *   wait DSTA bit2 ("transmitted"). All waits bounded (50 ms) -- a timeout here is itself
	 * the answer (bus never leaves idle), not a hang. */
	#define SDIO_CTRL  (SOC_SDIO0_BASE + 0x0)
	#define SDIO_CMD   (SOC_SDIO0_BASE + 0x8)
	#define SDIO_ARGU  (SOC_SDIO0_BASE + 0xC)
	#define SDIO_STATE (SOC_SDIO0_BASE + 0x10)
	#define SDIO_DSTA  (SOC_SDIO0_BASE + 0x18)
	volatile u32 *tick = (volatile u32 *)(SOC_TIMER_BASE + 0x200);
	#define WAIT_MS(ms, cond) do { \
		u32 _t0 = *tick; \
		while (!(cond) && (*tick - _t0) < (24000u * (ms))) ; \
	} while (0)

	at(4, 116, 0x00F0C040); puts_("STEP 19 SDIO CMD0\n");
	kv("STATE_B4", *(volatile u32 *)SDIO_STATE);
	kv("DSTA_B4 ", *(volatile u32 *)SDIO_DSTA);

	*(volatile u32 *)SDIO_ARGU = 0;
	*(volatile u32 *)SDIO_CMD = 0;

	WAIT_MS(50, *(volatile u32 *)SDIO_DSTA & 1);
	u32 dsta_ready = *(volatile u32 *)SDIO_DSTA;
	kv("DSTA_RDY", dsta_ready);

	if (dsta_ready & 1) {
		*(volatile u32 *)SDIO_CMD = *(volatile u32 *)SDIO_CMD | (1u << 31);
		WAIT_MS(50, (*(volatile u32 *)SDIO_DSTA >> 2) & 1);
	} else {
		/* Bus never left idle: that IS the result, not a reason to force the execute bit. */
	}
	kv("STATE_AF", *(volatile u32 *)SDIO_STATE);
	kv("DSTA_AF ", *(volatile u32 *)SDIO_DSTA);
	kv("CMD_AF  ", *(volatile u32 *)SDIO_CMD);
#endif
