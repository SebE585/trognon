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

### The reset path (opcode 4), traced 5 levels deep

```
sdio_op4_reset (0x19c4)
 +-- reset_set_bits_0x34_0x36_a (0x6c24)   -- builds a bitmask in [dev+0x34]/[dev+0x36]
 +-- reset_set_bits_0x34_0x36_b (0x6d04)   -- same, different bits
 +-- generic_poll_retry_100x (0x64b0)       -- shared by op4/op6/op7; retries up to 100x;
       |                                       this is also `sdiodrv_resetSDHC` by its own
       |                                       log-tag string, but it is generic, not
       |                                       reset-specific code
       +-- state_set_bits_0x2f (0x6e30)    -- [dev+0x2f] |= (arg & 7)
       +-- state_check_bits_0x2f (0x6e44)  -- ([dev+0x2f] & 7), pure software check
```

**No literal MMIO address found anywhere in this chain.** Confirmed by an
exhaustive byte-level search of the whole firmware image for `0x80000000`
(SDIO) and `0x81200000`/`0x81300000` (NAND H2FMI0/1): zero matches, in either
the raw firmware or its wrapper kext (whose own `__const` turned out to be a
plain kernel-API jump table, not a peripheral address table). The actual
MMIO write is most likely deferred — an interrupt handler or a background
task applying these bitmasks — not in the direct call path from the switch.
**Not yet located.**

### Known struct offsets (software state, not registers)

| Offset | Meaning |
|---|---|
| `[dev+0x00]` | "configured" flag, checked by every opcode handler |
| `[dev+0x04]` | allocated buffer pointer (opcode 2 sets it, opcode 3 clears it) |
| `[dev+0x2f]` | a one-byte state flag, bits 0-2 |
| `[dev+0x34]`, `[dev+0x36]` | two 16-bit "desired register" bitmasks built by the reset path |
| `[message+0x00]` | opcode byte |
| `[message+0x08]` | response/status field — same offset the host's `sendIOPCommand` writes `pending=1` into before sending |

### Not disassembled yet (real next targets)

- `0x6580`, `0x63d0`, `0x638c` — opcode 5's three sub-calls.
- `0x67f0`, `0x6898` — opcode 6 and 7's call targets; the most likely place
  to actually find a register access for "send command" / "transfer data".
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
