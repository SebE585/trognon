/* STAGE 9. Built as: trace_ticks.bin.
 * Included by trace.c inside payload_main(): it shares that function's locals, so it is a block, not yet a unit of its own.
 * To change it, edit it here; `make -C tools/payloads sums` must leave every other stage byte-identical. */
#if STAGE == 9
	/* Tick-rate probe: read the free-running counter FIRST, publish it for the host, nothing else.
	 * The host runs this twice a measured time apart and divides (tools/tick_rate.py). */
	u32 t_lo = *(volatile u32 *)(SOC_TIMER_BASE + 0x200);
	u32 t_hi = *(volatile u32 *)(SOC_TIMER_BASE + 0x204);
	result_nonce();
	result_hex("ticks", t_lo);
	result_hex("ticks_hi", t_hi);
	ENV_SET("trognon", "ticks-1", 1);
	return;
#endif
