# IOP firmware reverse-engineering notes (reusable reference)

This directory stores **our own analysis** (addresses, function roles, call
graph) of the S5L8920's embedded IOP firmware — never the firmware binary
itself, which is Apple's and is never committed here (see the project's own
rule against hosting Apple firmware). Re-extract it yourself from the owner's
IPSW if you need to re-run anything here; the method is documented in
[`docs/research/2026-10-16-wifi-iop-architecture.md`](../../docs/research/2026-10-16-wifi-iop-architecture.md).

Point of this directory: the next session (us, or anyone else) should not
have to re-derive function addresses from scratch by re-reading strings and
re-tracing xrefs. Load `fwFirmwareImage.r2` into `radare2` and the naming
below is applied automatically (see that file's own header for the exact
command).

## What the firmware is

A ~0x1b000-byte raw ARM32 binary (no Mach-O header of its own — it is a
section called `fwFirmwareImage` inside a small ARMv7 Mach-O *wrapper* kext,
`IOP_s5l8920x_firmware`, which only exists to carry it and tell the host
where to load it). Starts with a genuine ARM exception vector table. Runs on
a second, embedded ARM core (Apple calls it an "IOP"), with its own
scheduler, heap, exception handlers and interrupt controller — a real small
OS, not a thin relay. Full writeup, evidence levels and what is still a
hypothesis: `docs/research/2026-10-16-wifi-iop-architecture.md`.

## Function map (file offset = r2 vaddr, no base)

### SDIO message loop and its 7 opcodes

| Address | Name | Role |
|---|---|---|
| `0x1730` | `sdio_message_loop` | blocking receive on queue `"sdio message"`, reads opcode, dispatches via a 7-entry jump table at `0x17c4` (`r3 = opcode - 1`, so **real opcodes are 1-7**, not 0-6 — an off-by-one caught and fixed on 2026-10-16) |
| `0x1844` | `sdio_op1_ping` (**real opcode 1**) | logs `"SDHC: Ping received..."`, writes success status, returns. **No register access at all.** |
| `0x1a58` | `sdio_op2_alloc` (opcode 2) | allocates a 16-byte buffer, stores it at `[dev+4]` |
| `0x1a24` | `sdio_op3_free` (opcode 3) | frees `[dev+4]` — mirrors opcode 2 |
| `0x19c4` | `sdio_op4_reset` (opcode 4) | the one that reaches `sdiodrv_resetSDHC` — see below |
| `0x192c` | `sdio_op5_set_bus_params` (opcode 5) | compound dispatcher over message fields `+4/+8/+0x10/+0xc`; matches the `setClockRate`/`setBusWidth`/`setBusSpeedMode` family named in the firmware's own strings |
| `0x190c` | `sdio_op6_send_command_maybe` (opcode 6) | thin tail-call into `0x67f0` with 2 pointers; **not disassembled further**, name is a guess from signature only |
| `0x18bc` | `sdio_op7_transfer_data_maybe` (opcode 7) | calls `0x6898` with 4 sub-pointers; **not disassembled further**, name is a guess from signature only |

Every handler starts with the same guard: read `[message+0]` (the device's
"configured" state), bail with generic error `0x401` if zero. Not
opcode-specific.

### Correction (2026-10-16, caught by a second review): "dev" is not a separate struct

Earlier notes called the first argument of opcode handlers 2-7 "dev", as if
it were a distinct device struct. It is not: the switch's own dispatch code
passes **`(message+0xc, message+0x14)`** to every one of the six non-ping
handlers (confirmed identical in all six cases). There is one buffer, not
two — every `[dev+N]` below really means `[message+0xc+N]`.

### Where that buffer actually comes from (traced, 2026-10-16)

The review asked for this explicitly, rather than accepting the offset-only
description. `fcn.00000eb0` (the receive call inside the message loop)
indexes a **fixed table at literal address `0x10da4`**: one 32-byte
channel-descriptor per channel (`channel_id * 0x20`), each holding a ring
buffer of 64-byte message slots (`index << 6`, masked to 64-byte alignment).
**This is RAM, part of the firmware's own static data** — not an MMIO
window, and not a raw hardware pointer handed in by the host. This rules out
one specific concern the review raised ("dev could itself be a pointer into
MMIO space"), though it does not yet rule out that these fields *mirror*
real hardware state updated elsewhere (see below).

### The reset and sendCommand/performDMA paths, traced

```
sdio_op4_reset (0x19c4)
 +-- reset_set_bits_0x34_0x36_a (0x6c24)   -- builds a bitmask in [msg+0xc+0x34]/[+0x36]
 +-- reset_set_bits_0x34_0x36_b (0x6d04)   -- same, different bits
 +-- generic_poll_retry_100x (0x64b0)       -- shared by op4/op6/op7; retries up to 100x;
       |                                       this is also `sdiodrv_resetSDHC` by its own
       |                                       log-tag string, but it is generic, not
       |                                       reset-specific code
       +-- state_set_bits_0x2f (0x6e30)    -- [msg+0xc+0x2f] |= (arg & 7)
       +-- state_check_bits_0x2f (0x6e44)  -- ([msg+0xc+0x2f] & 7), same struct

sdio_op6 -> sdiodrv_sendCommand (0x67f0, confirmed by its own log-tag string,
            not a guess anymore) -- reads/writes more fields of the SAME
            struct: [+0x8], [+0xe], [+0x10], [+0x24], [+0x30], [+0x32].
            Calls command_inhibited_check (0x6ac8): `[+0x24] & 1`, polled
            in a classic retry loop -- still the same RAM struct, no
            literal address.
sdio_op7 -> sdiodrv_performDMA / sdiodrv_transferData (0x6898, same: named
            by its own log-tag strings, not guessed) -- same struct again.
```

**No literal MMIO address found anywhere in any of these paths**, and the
buffer they all operate on is now proven to live in the firmware's own RAM
table, not in peripheral address space. Confirmed separately by an
exhaustive byte-level search of the whole firmware image for `0x80000000`
(SDIO) and `0x81200000`/`0x81300000` (NAND H2FMI0/1): zero matches anywhere,
in either the raw firmware or its wrapper kext.

**What this does and does not prove** (the review's point, still open): this
rules out the buffer itself being a hardware window. It does **not** yet
prove these fields are pure software bookkeeping rather than a
software-maintained *mirror* of real hardware state, kept in sync by an
interrupt handler we have not found. The real register access, if this
firmware does it anywhere outside an ISR, is most likely in one of the
sub-calls below, none of which have been opened yet.

### Known struct offsets, corrected (all relative to `message+0xc`)

| Offset from `message` | Meaning |
|---|---|
| `+0x00` | opcode byte (dispatched 1-7) |
| `+0x08` | response/status field — same offset the host's `sendIOPCommand` writes `pending=1` into before sending |
| `+0xc+0x00` | "configured" flag, checked by every opcode handler |
| `+0xc+0x04` | allocated buffer pointer (opcode 2 sets it, opcode 3 clears it) |
| `+0xc+0x08`, `+0xc+0xe`, `+0xc+0x10` | cmd/arg-style fields, read and logged by `sendCommand` |
| `+0xc+0x24` | a status word, `&1` checked by `command_inhibited_check` — hardware mirror or pure software, not distinguished yet |
| `+0xc+0x2f` | a one-byte state flag, bits 0-2 |
| `+0xc+0x30`, `+0xc+0x32` | more fields read by `sendCommand`/`performDMA` |
| `+0xc+0x34`, `+0xc+0x36` | two 16-bit "desired register" bitmasks built by the reset path |

### Not disassembled yet (real next targets)

- `0x6580`, `0x63d0`, `0x638c` — opcode 5's three sub-calls.
- `0x6c70`, `0x6c90`, `0x6cf0`, `0x6c00`, `0x6f18` — `sendCommand`'s own
  sub-calls; `0x6804`, `0x6d50`, `0xa7d8`, `0x8dfc` — `performDMA`'s. These
  are the best remaining candidates for an actual register access, now that
  every level checked so far has turned out to be the same RAM struct.
- The NAND (`h2fmi_iop_*`) and CDMA driver functions, named in the firmware's
  own strings but not traced at all — NAND is this firmware's dominant
  tenant by far (39 debug strings vs. 6 for SDIO; see the architecture
  report), so its own message loop is the better place to find a real MMIO
  address if one exists anywhere in this firmware.
- `IOSlaveEndpoint`, `IOSlaveCPU`, `IOSlaveFirmware` (in the separate,
  generic `IOSlaveProcessor.kext`) — the channel class and the firmware
  load/verify/start path, respectively. Named, not yet opened.

## Files in this directory

- `fwFirmwareImage.r2` — the r2 naming script described above.
- This `README.md`.

Nothing else: no extracted binaries, no decrypted firmware, consistent with
this project's rule against ever hosting Apple firmware.
