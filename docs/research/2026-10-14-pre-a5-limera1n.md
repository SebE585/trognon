# Pre-A5 devices (iPhone 3GS, iPhone 4): a different, older family

Research date: 2026-10-14. Triggered by actual devices in hand: an iPhone 4
GSM (A1332) bought 2026-10-13, several more iPhone 4 units bought for parts
(3.6-5 EUR each), and an iPhone 3GS found in a drawer.

## Correction, 2026-10-14 (later same day)

This doc originally said limera1n applies to both S5L8920 and A4
(S5L8930). That's wrong. Checked directly in `axi0mX/ipwndfu`'s own
source (its `-p` flag's CPID dispatch table, fetched from
`github.com/axi0mX/ipwndfu`):

- `CPID:8920` / `CPID:8922` (S5L8920/S5L8922, iPhone 3GS) → **limera1n**
- `CPID:8930` (S5L8930, iPhone 4/A4) → **SHAtter**, a different exploit

Both are host-direct, both are "the bootrom-era bug family", but they are
not the same exploit — the earlier text in this doc conflated them. Any
future tooling must pick the right one per CPID, same as ipwndfu itself
does; see `device_db.c`'s `unlock_tool_hint` per entry.

## Bottom line

Neither the iPhone 3GS nor the iPhone 4 is covered by checkm8 — both predate
the A5-A11 range checkm8 targets. Both share the same unpatchable-bootrom-era
bug family, but exploited by two *different* tools — **limera1n** (3GS) and
**SHAtter** (A4), see correction above — both host-direct (no helper board,
unlike A5), but no modern Linux/BSD port effort targets either SoC. Treat
them as boot-chain test devices, not realistic OS port targets today.

## SoC and scope

- iPhone 4 (A1332, GSM) = **iPhone3,1**, SoC **A4 (S5L8930)**.
- iPhone 3GS = **iPhone2,1**, SoC **S5L8920** — predates Apple's "A" naming
  entirely (that started with the A4 in the iPhone 4).
- checkm8 (CVE-2019-8900, 2019) covers A5 through A11 only. Both S5L8920 and
  A4 are explicitly outside its scope.
  https://securityaffairs.com/91795/hacking/checkm8-jailbreak.html
- Between limera1n (2010-10-09) and checkm8 (2019-09-27), the A4 was the
  last Apple SoC with a publicly known bootrom vulnerability.
  https://theapplewiki.com/wiki/GeekGrade
- limera1n applies to S5L8920 too: it is the same unpatchable bootrom bug.
  Some S5L8920 bootrom revisions (359.3.2) additionally need **alloc8**
  instead of the original 0x24000 segment-overflow path, but both chains are
  host-direct. https://theapplewiki.com/wiki/IBoot-359.3.2

## limera1n

- Released 2010-10-09 by George Hotz (geohot); a bootrom/iBoot exploit
  running unsigned code on A4 devices (iPod touch 3, iPhone 3GS, and all A4
  devices up to and including the iPhone 4 — the last device it covers).
  https://theiphonewiki.com/wiki/Limera1n_Exploit
- Shipped inside **redsn0w** (Dev Team), ran on Windows/macOS.
  https://liliputing.com/redsn0w-jailbreak-tool-for-ios-4-1-now-available/
- **Host-direct, no helper board**: the iPhone Wiki's source excerpt shows
  the host driving the exploit over USB control transfers
  (`irecv_control_transfer`) — unlike checkm8-a5, which needs a
  microcontroller because a normal OS's USB stack sends standard requests
  (e.g. `SET_ADDRESS`) that break the exploit before user code can act.
  This host-direct behavior for limera1n is inferred from the source
  excerpt, not independently confirmed; re-check against the original
  geohot/libpois0n source before relying on it.
- Tethered, like checkm8: must be re-run from a host at every boot.

## Why a Raspberry Pi Zero still doesn't help here either way

Moot for limera1n specifically (it may not need a helper board at all), but
worth recording: even where a helper board IS required (A5's checkm8-a5), a
Pi Zero running Linux can't substitute for a Pico/Arduino, for the same
reason any normal OS can't — its USB stack still sends the standard requests
that break the exploit. The constraint is "bare-metal USB control", not "is
it a full computer".

## Implication for Trognon

- Added as two SoC families in `device_db.c` (`TROGNON_SOC_S5L8920`,
  `TROGNON_SOC_A4`, both `TROGNON_UNLOCK_HOST_DIRECT`).
- No modern OS port target: last related work (OpeniBoot/iDroid) targeted
  the A4's predecessor generation and died in 2012 — see
  `docs/research/2026-10-13-a5-a6-32bit.md`. Don't invest kernel-port effort
  here; it's useful as a cheap device to validate the boot-chain tooling
  (identification, orchestration) end to end.

## Not verified

- Whether limera1n truly needs no helper board, or whether that changed in
  later re-implementations (libpois0n, Legacy iOS Kit's A4 support). Check
  the original geohot source or Legacy iOS Kit's A4 docs before building
  tooling on this assumption.
- CDMA iPhone 4 (A1349, iPhone3,3): not yet in the catalog, same A4 SoC
  expected.
- `ipwndfu -p`'s exact behavior and output once run against real hardware —
  confirmed only by reading its source, not by running it yet. See
  `docs/upstream-tools.md`.
