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

### Full scheduler trace (2026-10-16): confirmed instruction-by-instruction, still negative

A reviewer correctly refused to accept "arg1" at face value and asked for
the exact chain: stack/register recount at entry, the real first-dispatch
instruction and its R0 value, and a search for *indexed* (not just
literal) writes to the context-slot table. All three checked, in order:

1. **Stack/register recount at `sdio_message_loop` (`0x1730`)**: `push
   {r4,r5,r7,lr}` never touches r0; `mov r4,r0` at `0x1740` captures the
   raw incoming r0 immediately, before anything else uses r0. So `ldm
   r4,{r0,r5}` genuinely dereferences the task's real first argument —
   not a mislabelled stack slot.
2. **The real first dispatch, found and confirmed**: `fcn.00009ff4`
   (the "resume task" trampoline, itself installed as the fake initial
   return address by `fcn.00009fa4` at task-creation time) does
   `r3 = *(0xf0a8)` (the global "current task" TCB pointer) then
   `r0 = [r3+0x74]; mov lr,pc; pc = [r3+0x70]` — i.e. it loads R0 from
   the TCB's own `+0x74` field *right before* jumping to the TCB's
   `+0x70` field (the task's entry point). Traced `+0x74` back to its
   write site (`fcn.00008988`, task init: `str r8,[r4,0x74]`, where `r8`
   is the per-priority context-slot address computed by the
   task-creation loop, `0xf028 + priority*8`). This confirms the
   original hypothesis instruction-by-instruction, not just by name:
   **R0 at first entry = the address of the task's own 8-byte slot in
   the `0xf028` table.**
3. **Indexed-write search**: grepped every `str ..., [reg, 0x74]` in the
   whole firmware. Only two exist: the TCB-init write just described
   (storing the slot *address* into the TCB, not writing *through* it),
   and one more, at `0x1be0`, inside `fcn.00001ac4` — called from
   `fmi0`/`fmi1`'s **shared** entry (`0x1114`), i.e. NAND, not SDIO. That
   function builds a similarly-shaped record (`+0x58`/`0x5c`/`0x60`/
   `0x64`/`0x68`/`0x6c`/`0x70`/`0x74`) — a generic
   "register a handler + context" idiom reused by NAND's own init
   (feeding the two-phase boot dispatcher described above), **not**
   something SDIO's entry calls at all. No other indexed write to the
   `0xf028` range exists anywhere.

**Conclusion, now checked from every angle available in this firmware
blob**: the chain from R0-at-entry back to its producer is fully traced,
and at every step the value is either zero (the static slot content) or
simply never set by anything SDIO-specific. This is a genuinely strong
negative result, not an unexplored gap. Combined with NAND having a whole
parallel per-instance registration mechanism (`fcn.00001ac4` and the
0x1b8-byte struct registry) that **SDIO's own task entry never calls at
all**, the most likely explanations left are: (a) SDIO support in this
exact firmware build is incomplete/not fully wired up (consistent with
the original real-hardware observation that every SDHC register reads
0), or (b) the real initialisation happens before this ~0x1b000-byte
blob even starts running, in an earlier IOP boot stage that has not been
extracted.

### BREAKTHROUGH (2026-10-16): the configuration-injection mechanism, found and confirmed

A reviewer pointed at a lead from the original architecture report that
had not been followed up: the wrapper kext (`IOP_s5l8920x_firmware`,
`iop_firmware.bin` — distinct from the raw `fwFirmwareImage.bin` payload)
carries a property dictionary with keys `ConfigurationOffset`,
`HeapBaseOffset`, `HeapSizeOffset`, `HeapRequired`, `PageTableBase`,
`PageTableSize`, `MessageChannelSize`, `PanicString`, `PanicFunction`,
`PanicLog...`, found earlier but never opened.

Disassembled the wrapper's own `__text` (it has real code, not just
metadata): a clean array of `{key_string_ptr, value_location_ptr}` pairs
at `0x804801b4` onward, each value pointer landing inside the wrapper's
own `__data` section. Read the actual stored values directly:

| Key | Value |
|---|---|
| `ConfigurationOffset` | `0xf0a0` |
| `HeapBaseOffset` | `0xf0a4` |
| `HeapSizeOffset` | `0xa000` |
| `HeapRequired` | `0xffffffff` (not required) |
| `PageTableBase` | `0xffffffff` (no MMU on this IOP) |
| `PageTableSize` | `8` |
| `MessageChannelSize` | `0x1a460` |

**These are offsets *into the raw firmware image* (`fwFirmwareImage.bin`),
not addresses in some other space.** Checked directly: at file offset
`0xf0a0` (`ConfigurationOffset`) and `0xf0a4` (`HeapBaseOffset`), the raw
firmware image holds `0xffffffff` at both — exactly the "-1 = not yet
configured" sentinel pattern seen everywhere else in this firmware for an
unset field.

**This confirms, not just hypothesizes, that the host-side loader patches
specific, fixed locations inside the firmware image before releasing the
IOP from reset.** This is the configuration-injection mechanism that was
missing from every other lead chased so far (host message content,
generic queue creation, task creation, the scheduler) — none of those
ever write anything real, because the real write happens *before any of
that code runs*, as part of loading the firmware itself.

**Not yet found**: which kext actually performs this patch. Neither
`AppleIOPSDIO.kext` nor `IOSlaveProcessor.kext` contain the string
`"ConfigurationOffset"` anywhere — so a third kext (a generic IOP
firmware-loader, likely the concrete subclass backing `IOSlaveFirmware`/
`IOSlaveCPU`) exists and has not been extracted yet. Finding and opening
it — to see what it actually writes at `fwFirmwareImage+0xf0a0` and
`+0xf0a4`, and whether that includes or points to the real SDHC base
address — is now the single most direct remaining step.

### Host opcode 0 vs IOP opcode 1: resolved, was never actually a mismatch

An earlier note claimed `sendIOPCommand` has "special handling of opcode
0". Re-read its full body: there is no `cmp` against 0 anywhere in it —
the opcode byte is just stored and used as-is. Its one caller (inside
`runIOPCommandSyncGated`) passes a hardcoded `movs r2, 1` before calling
it — the host sends opcode **1**, matching the IOP's real opcode 1 (the
ping) exactly. No off-by-one, no translation layer; the "opcode 0" claim
was an early, unverified guess that stuck around across several
revisions instead of being checked directly against the disassembly.

### Found: AppleARM7M::startCPU(IOSlaveFirmware*), the real IOP-management driver

Per the reviewer's step-1 guidance (identify the concrete provider via
IOKit matching rather than the generic classes), checked the
`__PRELINK_INFO` plist properly (XML, regex-extracted per entry) for
`AppleARM7M`'s own `IOKitPersonalities`: `IOProviderClass =
AppleARMIODevice`, **`role = "IOP"`**, `IONameMatch = "iop,s5l8920x"` —
this is the concrete driver matched directly against the IOP's own
device-tree node (parallel to `AppleIOPSDIO` matching
`"sdio,s5l8920x"`). Extracted it from the kernelcache
(`_PrelinkExecutableLoadAddr = 0x80712000`, size `0x3000`, SHA-256
`f286d9fb9b05f3802dbe2bccef2a4e329bcddd41ee85748d7fadc0789a7fa2e1`).

Its own strings confirm the role precisely: `mapDeviceMemoryWithIndex(0)`
(`_arm7mRegisterMap`) and `mapDeviceMemoryWithIndex(1)`
(`_vicRegisterMap`), `function-device_reset`, `arm7m-enable-jtag`,
`iop-version`, and — the real target — `"virtual IOReturn
AppleARM7M::startCPU(IOSlaveFirmware*)"`.

Located and disassembled `startCPU` (r2's automatic function detection
missed it, reached only via vtable dispatch, not a direct `bl` — found
instead by a literal-pool byte search for its own error-log string's
address, then linear disassembly from there). It:

1. Takes the `IOSlaveFirmware*` argument, calls its vtable `+0x34c` (a
   memory-descriptor accessor), then that descriptor's `+0x98`
   (`getBytesNoCopy`-shaped: stores the raw firmware bytes pointer at
   `this+0x88`) and `+0x84` (`getLength`-shaped: validates it against a
   computed segment size, `"segLength >= md->getLength()"`).
2. Rounds the length up to the next power of two starting from `0x1000`,
   stores it at `this+0x8c`.
3. Writes that size (negated and masked with `0x3ffff000`, or the size
   directly, depending on a mode flag at `this+0x64`) into
   `[this+0x58]+0x114`.
4. **Writes the literal `0x86300000` into `[this+0x58]+0x118`** — the
   exact same hardware address found hardcoded in the IOP firmware's own
   generic log function (`fcn.000074d4`/`0x74e8`, see the main
   architecture report's earlier methodology note). Not a coincidence: a
   real, host-side confirmation of the same peripheral address the
   firmware itself uses.
5. Writes `[this+0x88]` (the firmware bytes pointer) into
   `[this+0x58]+0x110`.

`[this+0x58]` is very plausibly the actual argument structure handed to
whatever kernel primitive maps memory for and starts the second ARM
core — this is the load-path code "Open questions" #6 asked for, now
located and partially read.

**Checked and still not found**: `0xf0a0`/`0xf0a4`/`0xa000`/`0x1a460`
(the `ConfigurationOffset`/`HeapBaseOffset`/`HeapSizeOffset`/
`MessageChannelSize` values) do **not** appear as literals anywhere in
`AppleARM7M.kext` either, and neither does the string
`"ConfigurationOffset"` or `"getProperty"`. The property-dictionary
values must be consumed through the same opaque vtable call on the
`IOSlaveFirmware` object (`+0x34c` or a neighbouring slot not yet
individually identified) — a generic accessor whose real implementation
was not found in any of the three kexts checked so far
(`AppleIOPSDIO`, `IOSlaveProcessor`, `AppleARM7M`). It is very plausibly
compiled directly into the kernel's own platform-expert/IOResources
support rather than any separate kext.

### A real structural finding for "is this the only host entry point?"

Checked directly, per the reviewer's concern that the single known
`sendIOPCommand` caller sending only the ping might not be the whole
picture: searched `AppleIOPSDIO.kext` exhaustively for `sendIOPCommand`'s
own address stored as a function pointer anywhere (a vtable slot, not
just a direct `bl`) — none found, with or without the Thumb bit set.
`sendIOPCommand` really does have exactly one call site in this kext.

That one call site is inside a function that logs itself as
`runIOPCommandSyncGated` under a specific error condition — a
generically-named wrapper, but the opcode it passes is a hardcoded `1`,
not a forwarded parameter. The *only* other interesting string in this
whole kext is `"setPowerStateGated"` — a power-management callback, not
a generic command dispatcher, and no `externalMethod`/`IOUserClient`-style
entry point was found either. **This is a real structural finding, not
just an absence**: as extracted, this driver may genuinely only ever
ping the IOP (as part of power-state bring-up) and never issue the
higher-numbered opcodes (reset, bus-params, sendCommand, performDMA)
from the host side at all — those may be driven entirely by the IOP's
own internal logic, or by a separate path (e.g. `AppleBCMWLAN`, the
actual Wi-Fi chip driver sitting above this one) not present in this
kext. Confirming this needs either checking `AppleBCMWLAN.kext` (not
extracted) or accepting that this specific kext slice's outbound command
surface is limited to the ping.

### ConfigurationOffset's consumer: checked the full kernelcache, still not found

Searched the entire decompressed kernelcache (10 MB, not just
`AppleIOPSDIO.kext`/`IOSlaveProcessor.kext`) for the literal string
`"ConfigurationOffset"` and for a literal reference to its known vaddr
(`0x804804b0`): both appear exactly once more, right next to the
wrapper's own `{key,value}` table already disassembled — the
kernelcache's prelinked copy of the *same* wrapper kext, not a new
consumer. Disassembled `IOSlaveProcessor.kext`'s full `__text` (small
enough, ~3 KB): metaclass-registration boilerplate and a generic
allocate+`init` factory only, no property-by-name lookup. The real
consumer must read these properties generically (iterating existing
dictionary keys, not looking each one up by a literal string in its own
code) — which a string/address search cannot find. Needs either a full
kernel-text disassembly pass (not completed this session — file too
large for `radare2`'s `aaa` to finish in reasonable time) or dynamic
tracing on real hardware.

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
