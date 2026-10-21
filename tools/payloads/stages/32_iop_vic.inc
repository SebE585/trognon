/* STAGE 32. Built as: trace_iop_vic.bin.
 * Included by trace.c inside payload_main(): it shares that function's locals, so it is a block, not yet a unit of its own.
 * To change it, edit it here; `make -C tools/payloads sums` must leave every other stage byte-identical. */
#if STAGE == 32
	/* Second IOP register window, SOC_IOP_VIC_BASE = 0xBF300000 (see soc/s5l8920.h): decoded
	 * from the "iop" device tree node's own "reg" property, cross-checked against
	 * AppleARM7M::start()'s own "_vicRegisterMap" field name. Read-only, no write -- but
	 * "read-only" is not the same claim as "no effect": some hardware registers clear-on-read or
	 * otherwise change state just by being read, and this window's real behaviour is unknown.
	 *
	 * +0x18 is now a DRIVER-CONFIRMED offset, not a blind guess: AppleARM7M::start() caches this
	 * window's virtual address at this+0x60 (the exact mirror of this+0x58 for the already-known
	 * window 0 -- both addresses are logged together in the same "mapped I/O registers" call, as
	 * decisive as static analysis gets), and a separate method (code address 0x80713510, a real
	 * vtable entry, not startCPU) writes a 32-bit word to [this+0x60 + 0x18]: either 8 or 4
	 * depending on a 0/1 argument -- a two-way enable/select pattern, offset and width confirmed,
	 * exact semantics not. The rest of the 8-word sweep (+0x00 to +0x1C) stays blind, kept for
	 * context around the one confirmed offset, not elevated to the same confidence.
	 *
	 * Captured ONCE into a RAM array before any display or export, so the on-screen dump and the
	 * USB-exported values are guaranteed to be the same read, not two independent reads of a
	 * register that might answer differently each time (a real risk this review caught: the
	 * previous version of this stage read the screen copy and the result_hex() copy separately).
	 *
	 * Interpret the result against four outcomes, not two: all-zero (like index 0); a stable,
	 * structured-looking value (re-run at least once, same pattern as tools/tick_rate.py's
	 * two-separate-invocations convention, to tell "stable" from "coincidence"); a value that
	 * changes between runs (suggests something live, not proof of IOP firmware execution either
	 * way); or a data abort / USB link drop (informative on its own, same as the GPIOIC
	 * incident). None of these, including a recognisable value at +0x18 matching 4 or 8, would by
	 * itself prove the IOP core is executing its firmware -- only that this offset behaves the
	 * way the host driver's own write would suggest it should. */
	at(4, 116, 0x00F0C040); puts_("STEP 32 IOP VIC?\n");
	volatile u32 *iopvic = (volatile u32 *)SOC_IOP_VIC_BASE;
	u32 snapshot[8];
	for (u32 i = 0; i < 8; i++) snapshot[i] = iopvic[i];

	u32 y32 = 134;
	for (u32 i = 0; i < 8; i++) {
		at(4, y32, i == 6 ? 0x00B050A0 : 0x00A0B0C0);   /* +0x18 (i=6) highlighted: driver-confirmed */
		puts_("+"); hex(i * 4); puts_(" "); hex(snapshot[i]);
		putc_('\n'); fb_clean();
		y32 += 18;
	}
	result_hex("vic00", snapshot[0]);
	result_hex("vic04", snapshot[1]);
	result_hex("vic08", snapshot[2]);
	result_hex("vic0c", snapshot[3]);
	result_hex("vic10", snapshot[4]);
	result_hex("vic14", snapshot[5]);
	result_hex("vic18", snapshot[6]);   /* the driver-confirmed offset */
	result_hex("vic1c", snapshot[7]);
#endif
