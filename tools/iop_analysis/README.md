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

### MAJOR CORRECTION (2026-10-16, caught by a third review): this IS real SDHCI hardware, one indirection away

Everything below describing `[dev+0x2f]`, `[dev+0x34]`, `[dev+0x36]`, etc. as
"software state" was **wrong by one level of pointer indirection**. A third
review spotted that the exact offsets and bit masks match the *real,
standard SDHCI register layout* (the same one Linux's own `sdhci.h` uses)
far too precisely to be coincidence, and asked the decisive question: is
`[r0+0x2f]` really using `dev` itself, or a pointer *loaded from* a field of
`dev`? Re-read instruction by instruction (not re-summarised from memory):
in `sdio_op4_reset` (`0x19c4`), the very first thing it does is
`r4 = [r0]` — **it dereferences `message+0x0c` once**, and passes **that
dereferenced value** (not the address `message+0x0c` itself) into
`generic_poll_retry_100x`, `reset_set_bits_0x34_0x36_a` and
`reset_set_bits_0x34_0x36_b`. Same in `sdio_op6` (`0x190c`): `r0 = [r0]`
before tail-calling `sdiodrv_sendCommand`.

**Fixed notation, used consistently from here on** (a previous version of
this document conflated these three distinct things, which a reviewer
flagged):
- `message+0x0c` — a *location*: the byte offset inside the message where a
  pointer is stored. This is RAM, inside the firmware's own 64-byte message
  slot (see "Where the message buffer lives" below).
- `base = *(message+0x0c)` — the *value* stored at that location: a pointer,
  null-checked before use like any driver checks its mapped register
  window. **This is the field previously and wrongly called `[dev+0]`/the
  "configured" flag.**
- `base+N` — real SDHCI registers, for the `N` values in the table below.
  **Not `message+0x0c+N`** — that was the error. (This also resolves a
  consistency check a reviewer asked for: message slots are 64 bytes, and
  `message+0x0c+0x36` would be offset `0x42` — past the end of a slot. Under
  the corrected reading, `0x36` is an offset from `base`, a completely
  separate address space, so there is no overflow.)

With that correction, every offset lines up exactly with real SDHCI:

| `base+N` | What we saw | Real SDHCI register (Linux `sdhci.h`) |
|---|---|---|
| `+0x08` | read/logged as `arg` in `sendCommand` | `SDHCI_ARGUMENT` |
| `+0x0e` | 16-bit, logged as `cmd` | `SDHCI_COMMAND` |
| `+0x10` | `SDHCI_RESPONSE` (0-3) |
| `+0x24` | `&1` checked in a retry loop, error string "Command inhibited!" | `SDHCI_PRESENT_STATE`, bit 0 = `CMD_INHIBIT` — exact match |
| `+0x2f` | byte access, mask `7` | `SDHCI_SOFTWARE_RESET` (bit0=all, bit1=CMD, bit2=DAT — mask 7 is exactly "reset everything") |
| `+0x30`, `+0x32` | read/logged as `normalInt`/`errorInt` | `SDHCI_INT_STATUS` (normal/error) |
| `+0x34`, `+0x36` | bits set by the reset path | `SDHCI_INT_ENABLE` (normal/error) |

This is not a coincidence pattern-match: it is the standard SDHCI layout,
offset for offset. **The reset path is very plausibly writing a real
hardware self-clearing reset register directly** (`SOFTWARE_RESET`'s bits
clear themselves once the reset completes, which would make the
"deferred/background worker" theory from an earlier version of this
document unnecessary — a live hypothesis, not yet proven).

**The real open question**: not "is this software or hardware" (it is real
SDHCI hardware, convincingly) but **what numeric value is `base`, and who
writes it to `message+0x0c`**. Neither `sdio_op2_alloc` nor any opcode
handler disassembled so far *sets* this field — every one of them only
reads and null-checks it.

### Where the message buffer lives, and how it becomes dereferenceable

A static task-creation table at `0xe848` (one 16-byte record per task:
name pointer, entry point, stack size, priority) lists, in priority order:
`"iop"` (priority 0, entry `0xafc`), `"fmi0"`/`"fmi1"` (NAND, priority 5/6,
both entry `0x1114`), `"sdio"` (priority 3, entry `0x1730` =
`sdio_message_loop`) — confirming, from the firmware's own data, that NAND
and Wi-Fi are peer tasks of a real small OS.

The raw value `sdio_message_loop` receives from the generic queue
(`fcn.00000eb0`) is **not** used directly as `message` — it is passed
through `fcn.00008528` → `fcn.000084a0` first. Disassembled in full: this
is a **generic physical-address ↔ IOP-local-address translator** (a table
at `0xf07c`, walked linearly, each 16-byte record holding two candidate
base addresses to match a range against, plus a continuation flag), called
from **27 places across the firmware** — not SDIO-specific at all, the same
helper NAND and every other subsystem uses. This explains how `message`
becomes a real, dereferenceable local pointer starting from whatever raw
(physical) address the sender supplied — but it does **not** supply any
SDIO/SDHC-specific value; it is pure plumbing.

### Checked and ruled out (2026-10-16): the host does not supply `base`

Natural hypothesis: the host CPU knows the real SDHC physical address
(`0x80000000`, from the device tree) and writes it into the message at
`+0x0c` before calling `sendIOPCommand`. **Checked directly in the host
kext (`AppleIOPSDIO.kext`, Thumb-2 code — an earlier plain-ARM disassembly
of this kext was wrong, redone correctly with `asm.bits=16`) and ruled
out**:

- `sendIOPCommand` (`0x8057c424`) has exactly **one caller** in the kext.
  That caller passes, as the real argument to the underlying `send()` call
  (`vtable+0x84` of the `IOSlaveEndpoint`-like object), the **physical
  address of the command buffer itself** — the same value logged as `paddr`
  in `"Sending command @ vaddr=%p, paddr=0x%lX, opcode=%lu"`. This is the
  standard IOKit shared-memory IPC pattern (hand the other processor the
  physical address of a buffer both sides can see), not a separate
  hardware-register address.
- `sendIOPCommand` also checks that the command object is *exactly* one
  single, fixed, pre-allocated buffer (`this->0x14`, compared by pointer
  identity) — there is one reused command buffer, not one per call.
- The kext contains **zero** strings or calls resembling
  `getDeviceMemoryWithIndex`/`mapDeviceMemory` — the standard IOKit calls
  that would resolve the device tree's `"reg"` property (the real
  `0x80000000` SDHC base) into a host-usable address. **The host CPU very
  plausibly never maps or touches the SDHC registers at all.**

Conclusion: `base` is not something the host computes and hands down
through the message. It must come from somewhere entirely on the IOP side
— most likely a boot-time constant baked in before this `0x1b000`-byte
driver blob even starts running (a separate, smaller IOP boot stage that
has not been extracted/examined), or built via split `movw`/`movt`-style
instruction sequences rather than a single 32-bit literal (the earlier
exhaustive literal search only caught whole-word matches, not split
constructions) -- not yet checked.

### Checked (2026-10-16): does sdio's own task entry produce `base`?

Compared `sdio_message_loop`'s entry (`0x1730`) instruction-by-instruction
against `fmi0`/`fmi1`'s shared entry (`0x1114`), as requested. Real
structural difference: `fmi0`/`fmi1` allocate a `0x1b8`-byte instance
struct, zero it, and **register it into a global array at `0xe8c4`-ish
(index table `0x10ec0`/`0x10ec4`)** — the same array the two-phase
boot-init dispatcher (`fcn.000015fc`) iterates. `sdio`'s entry does **none
of this**; it goes straight to creating its `"sdio message"` queue via the
generic `fcn.00000db4` (shared by all four tasks). So SDIO's task entry
has no device-registration step at all, unlike NAND's.

Traced `fcn.00000db4` (generic queue creation) fully: for every slot in a
new ring, it writes the literal value `1` into `[slot+0]` (almost
certainly the opcode byte — a safe default matching real opcode 1 = ping)
and does **not** touch `[slot+0x0c]`. Since `.bss` should be zero at load,
`base` should start at `0` for a freshly created channel — which should
fail every opcode handler's null-check. Whatever writes a real value into
`message+0x0c` for the first time is still not found; it is neither the
generic queue-creation code nor the host (see above).

### Methodology follow-up: searching for split/computed address constants

Grepped a full linear disassembly of the firmware for every `mov r_,
0x8......` immediate (the concern that `base` might be built across
instructions rather than stored as a single literal word). Found several
real `0x80000000`/`0x81000000`-prefixed immediates — but every one
inspected turned out to be a **status/error code** built right before a
function return (e.g. inside `h2fmi_tx_bootpage_pio`, a NAND function:
`mov r0,0x80000000; ...; addne r5,r0,0x200000; ...; mov r0,r5; pop{...}`),
not a hardware address. **None fall inside any SDIO-related function**
(`sdio_message_loop`, the opcode handlers, `sdiodrv_sendCommand`/
`performDMA`, the reset functions). So `base` is not constructed from a
single-instruction or `mov`+`add` immediate anywhere in the SDIO code path
either — it must be a genuine load (`ldr`) from somewhere, most likely the
task's own `arg1` parameter struct, whose own producer (the real
task-creation call site — the static task table at `0xe848` has no
visible field for it) has not yet been located.

### Lower-priority lead (2026-10-16): the "iop control" master task's "install" command

The `"iop"` task (`0xafc`, priority 0) is a dispatcher for commands the
*host* sends on a **separate** channel ("iop control", not "sdio
message"). Its tags (4-byte values from the literal pool) are `"musr"`,
`"inst"`, `" pon"`, `"dnps"`, `"nitt"`, `"pels"`. The `"inst"` case
(`0x696e7374`, matched at `0xbb8`→`0xc64`) builds a per-client
configuration struct, copying five 8-byte field-pairs from the received
command plus one field (`+0x34`) that comes from the "iop control" task's
own third creation-time parameter, not from the host's message.

Flagged as **lower priority** than the host/firmware checks above: nothing
connects this specific struct or field to the "sdio message" channel's own
slots, and the data flow for `base` is more direct (it is consumed right
where `sdio_op4_reset`/`sdio_op6` dereference `message+0x0c`) than this
"install"-command path would require. Kept here as a secondary candidate
only if the boot-stage/split-constant leads above don't pan out.

### Earlier correction (2026-10-16, caught by a second review): "dev" is not a separate struct

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
