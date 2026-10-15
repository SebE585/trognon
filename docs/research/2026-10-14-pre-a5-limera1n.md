# Pre-A5 devices (iPhone 3GS, iPhone 4): a different, older family

Research date: 2026-10-14. Triggered by actual devices in hand: an iPhone 4
GSM (A1332) bought 2026-10-13, several more iPhone 4 units bought for parts
(3.6-5 EUR each), and an iPhone 3GS found in a drawer.

## Correction, 2026-10-14 (later same day)

This doc originally said limera1n applies to both S5L8920 and A4
(S5L8930). That's wrong. Checked directly in `axi0mX/ipwndfu`'s own
source (its `-p` flag's CPID dispatch table, fetched from
`github.com/axi0mX/ipwndfu`):

- `CPID:8920` / `CPID:8922` (S5L8920/S5L8922, iPhone 3GS): **limera1n**
- `CPID:8930` (S5L8930, iPhone 4/A4): **SHAtter**, a different exploit

Both are host-direct, both are "the bootrom-era bug family", but they are
not the same exploit. The earlier text in this doc conflated them. Any
future tooling must pick the right one per CPID, same as ipwndfu itself
does; see `device_db.c`'s `unlock_tool_hint` per entry.

## Bottom line

Neither the iPhone 3GS nor the iPhone 4 is covered by checkm8: both predate
the A5-A11 range checkm8 targets. Both share the same unpatchable-bootrom-era
bug family, but exploited by two *different* tools: **limera1n** (3GS) and
**SHAtter** (A4), see correction above. Both are host-direct (no helper board,
unlike A5), but no modern Linux/BSD port effort targets either SoC. Treat
them as boot-chain test devices, not realistic OS port targets today.

## SoC and scope

- iPhone 4 (A1332, GSM) = **iPhone3,1**, SoC **A4 (S5L8930)**.
- iPhone 3GS = **iPhone2,1**, SoC **S5L8920**, which predates Apple's "A" naming
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
  devices up to and including the iPhone 4, the last device it covers).
  https://theiphonewiki.com/wiki/Limera1n_Exploit
- Shipped inside **redsn0w** (Dev Team), ran on Windows/macOS.
  https://liliputing.com/redsn0w-jailbreak-tool-for-ios-4-1-now-available/
- **Host-direct, no helper board**: the iPhone Wiki's source excerpt shows
  the host driving the exploit over USB control transfers
  (`irecv_control_transfer`), unlike checkm8-a5, which needs a
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
reason any normal OS can't: its USB stack still sends the standard requests
that break the exploit. The constraint is "bare-metal USB control", not "is
it a full computer".

## Implication for Trognon

- Added as two SoC families in `device_db.c` (`TROGNON_SOC_S5L8920`,
  `TROGNON_SOC_A4`, both `TROGNON_UNLOCK_HOST_DIRECT`).
- **Correction, 2026-10-14 (later same day):** this section originally said
  "no modern OS port target, don't invest kernel-port effort here". Reversed
  (see `docs/research/2026-10-14-microkernel-3gs.md`): OpeniBoot/iDroid has
  more S5L8920 support than assumed (an explicit `iphone3gs` target, with a
  2015 trace of it actually running interactively on real 3GS hardware).
  The 3GS is now a real microkernel/driver target, not just boot-chain test
  hardware. Decision taken 2026-10-14.

## Not verified

- Whether limera1n truly needs no helper board, or whether that changed in
  later re-implementations (libpois0n, Legacy iOS Kit's A4 support). Check
  the original geohot source or Legacy iOS Kit's A4 docs before building
  tooling on this assumption.
- CDMA iPhone 4 (A1349, iPhone3,3): not yet in the catalog, same A4 SoC
  expected.
- `ipwndfu -p`'s exact behavior and output once run against real hardware,
  now confirmed: it works, 2026-10-14, against a real iPhone 3GS
  (S5L8920, 359.3.2 bootrom), succeeding on the 2nd-3rd attempt each time
  (matches the README's own "repeat if it fails" warning). See
  [`docs/en/05-field-notes.md`](../en/05-field-notes.md).
- `ipwndfu --dump-rom` after a successful `-p`: initially failed at the
  first post-pwn "exec" command (`dfuexec.py`'s `PwnedDFUDevice.execute()`)
  with a USB timeout, 10+ times in a row. Resolved, same day: a granular
  diagnostic (reading DFU GET_STATUS between every single control
  transfer) showed the full protocol sequence succeeding cleanly on a
  later attempt, with the same inherent flakiness as `-p` itself (the tool's own
  README already warns about this for `-p`), not a Python 3 port bug.
  Confirmed by actually dumping SecureROM and checking its SHA256 against
  the known-good hash in `dfuexec.py`'s config (`0e6feb11...` for
  359.3.2), matched exactly. Lesson: for this exploit family, a failure
  at any step means "retry", not "investigate a protocol bug", unless the
  failure is 100% reproducible across a full fresh pwn cycle.
- `ipwndfu`'s `PwnedDFUDevice.boot_ibss()`: also now confirmed working,
  2026-10-14. Sends a patched iBSS (extracted from the official
  `iPhone2,1_4.3.5_8L1_Restore.ipsw`, SHA256-verified) and reaches
  **Recovery mode** (USB product 0x1281), the furthest point in the
  boot chain reached by this project so far. First attempt appeared to
  fail (blank screen, no USB device at all); a second attempt with the
  same granular GET_STATUS diagnostic succeeded cleanly, with the same inherent
  flakiness as `-p`/`--dump-rom`, not a bug. `image3.py` needed several
  Python 3 porting fixes beyond plain 2to3 (byte-string tag magic
  comparisons that silently never matched, a `bytes`-shadowing local
  variable) before it would even decrypt correctly.
