/* STAGE 48. Built as: trace_gate_snap.bin.
 * Included by trace.c inside payload_main(): it shares that function's locals, so it is a block, not yet a unit of its own.
 * To change it, edit it here; `make -C tools/payloads sums` must leave every other stage byte-identical. */
#if STAGE == 48
	/* READ-ONLY. Reports the clock-gate snapshots that the iBSS hook (tools/ipwndfu/gate_snapshot.py, boot with
	 * TROGNON_GATE_SNAP=1) copied to SOC_GATE_STASH at each call of the iBSS clock-gate wrapper. The wrapper runs
	 * twice per boot, so a valid run has calls = 2: snapshot A is the state before the first clock initialisation,
	 * snapshot B the state before the second. For each, the gates whose word is 0xF in the low nibble (on) and the
	 * gates whose word is not zero (nz), as 64-bit masks (bit i = gate i). The stash magic is cleared afterwards so
	 * that a later boot without the hook cannot be mistaken for this one. */
	at(4, 116, 0x00F0C040); puts_("STEP 48 GATE SNAPSHOT\n");
	volatile u32 *st48 = (volatile u32 *)SOC_GATE_STASH;
	u32 magic48 = (st48[0] == 0x50534E47);
	u32 calls48 = magic48 ? st48[1] : 0;
	kv("MAGIC", magic48); kv("CALLS", calls48);
	result_hex("magic", magic48); result_hex("calls", calls48);
	for (u32 k = 0; k < 2; k++) {
		u32 on_lo = 0, on_hi = 0, nz_lo = 0, nz_hi = 0, n_on = 0;
		for (u32 i = 0; i < CLOCK_NGATES; i++) {
			u32 v = st48[4 + k * 0x34 + i];
			if ((v & 0xF) == 0xF) { if (i < 32) on_lo |= 1u << i; else on_hi |= 1u << (i - 32); n_on++; }
			if (v) { if (i < 32) nz_lo |= 1u << i; else nz_hi |= 1u << (i - 32); }
		}
		if (k == 0) { result_hex("a_on_lo", on_lo); result_hex("a_on_hi", on_hi); result_hex("a_nz_lo", nz_lo); result_hex("a_nz_hi", nz_hi); result_hex("a_n_on", n_on); kv("A ON", n_on); }
		else        { result_hex("b_on_lo", on_lo); result_hex("b_on_hi", on_hi); result_hex("b_nz_lo", nz_lo); result_hex("b_nz_hi", nz_hi); result_hex("b_n_on", n_on); kv("B ON", n_on); }
	}
	st48[0] = 0;
	fb_clean();
#endif
