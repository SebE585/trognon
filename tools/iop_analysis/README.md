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

## `catalog/`: structured per-function fiches (pilot, 2026-10-16)

`catalog/<Binary>/*.yaml` holds one structured fiche per examined function
(confidence, exact bytes read, claims with their own evidence, hardware
tests) for `AppleARM7M` and `IOSlaveProcessor` — a small pilot scope, not
yet the whole IOP firmware. `gen_coverage.py` (`make iop-coverage`) reads
the catalog, validates it (SHA-256 agreement with each binary's own
`tool_coverage.json`, sane byte ranges, no duplicate claim ids, known status
values), and publishes a coverage table plus an approved-claims list into
`docs/snippets/iop-coverage.en.md`, included from the research report.

**Every `*.yaml` file here is committed to this public repository and is as
visible on GitHub as any other file in it.** A fiche's own
`publication.status` (`approved` / `pending_review` / `excluded`) controls
only what `gen_coverage.py` prints into the generated table — it is an
editorial decision about what reaches the docs site, not an access
restriction and not a legal certification. Never put anything in a
committed fiche (a long verbatim excerpt, anything closer to a reproduction
of Apple's code than a stated fact) that should not be public from the
moment it's committed. See `gen_coverage.py`'s own module docstring for the
full reasoning (ties to Article L122-6-1 on decompilation for
interoperability, covered in
[`docs/research/2026-10-13-legal-frame.md`](../../docs/research/2026-10-13-legal-frame.md)).

`review.status` means, precisely (this was being used inconsistently
before it was written down): `partial` = some bytes of the function were
read but control flow has NOT been traced to every exit; `complete` =
every instruction from entry to every exit/return was read -- a claim
about review *coverage*, not about *understanding*. A `complete` fiche
can and often does still carry open questions in `review.unresolved`
about what the code means (register semantics, a method's real name) --
that is normal, not a contradiction.

Both `claims[]` and `review.unresolved[]` entries carry their own stable
id (`<FICHE>-NNN` / `<FICHE>-Q<N>`), validated for uniqueness across the
whole catalog, so a later correction can reference one without depending
on wording or an address staying the same.

`gen_coverage.py` ships regression tests
(`python3 -m unittest tools.iop_analysis.test_gen_coverage`, run in CI
on every push) against a disposable scratch catalog, covering: SHA-256
mismatch, an out-of-section range, overlapping ranges counted once (not
double-counted), a `tool_coverage.json` cache that has drifted from its
own `recognized_ranges`, and a hardware test left `planned` correctly
excluded from the executed count.

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
| `0x1730` | `sdio_message_loop` | blocking receive on queue `"sdio message"`, reads opcode, dispatches via a 7-entry jump table at `0x17c4` (`r3 = opcode - 1`, so **real opcodes are 1-7**, not 0-6) |
| `0x1844` | `sdio_op1_ping` (**real opcode 1**) | logs `"SDHC: Ping received..."`, writes success status, returns. **No register access at all.** |
| `0x1a58` | `sdio_op2_alloc` (opcode 2) | allocates a 16-byte buffer, stores it at `message+0x0c+4` |
| `0x1a24` | `sdio_op3_free` (opcode 3) | frees `message+0x0c+4` — mirrors opcode 2 |
| `0x19c4` | `sdio_op4_reset` (opcode 4) | dereferences `base = *(message+0x0c)` and reaches `sdiodrv_resetSDHC` through it — see below |
| `0x192c` | `sdio_op5_set_bus_params` (opcode 5) | compound dispatcher over message fields `+4/+8/+0x10/+0xc`; matches the `setClockRate`/`setBusWidth`/`setBusSpeedMode` family named in the firmware's own strings |
| `0x190c` | `sdio_op6` | tail-call into `0x67f0`, confirmed by its own log-tag string to be `sdiodrv_sendCommand`/`sdiodrv_completeCommand` — not a guess |
| `0x18bc` | `sdio_op7` | calls `0x6898`, confirmed by its own log-tag string to be `sdiodrv_performDMA`/`sdiodrv_transferData` — not a guess |

Every non-ping handler (opcodes 2-7) receives the identical argument pair
`(message+0x0c, message+0x14)` from the dispatcher, and starts with the
same guard: dereference `base = *(message+0x0c)` once, bail with generic
error `0x401` if it is zero (a real pointer, null-checked like any driver
checks its mapped register window — not a boolean "configured" flag; see
the correction below, which replaces this framing wherever it appears
later in this document).

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

Separately, `fcn.00000eb0` (the receive call itself) indexes a **fixed
table at literal address `0x10da4`**: one 32-byte channel-descriptor per
channel (`channel_id * 0x20`), each holding a ring buffer of 64-byte
message slots (`index << 6`, masked to 64-byte alignment). This confirms
`message` itself — the buffer `base = *(message+0x0c)` is read from — is
ordinary firmware RAM, not an MMIO window and not a raw hardware pointer
handed in directly by the host.

### Checked and ruled out, for this specific path: `sendIOPCommand`'s transport does not supply `base`

Natural hypothesis: the host CPU knows the real SDHC physical address
(`0x80000000`, from the device tree) and writes it into the message at
`+0x0c` before calling `sendIOPCommand`. **Checked directly in the host
kext (`AppleIOPSDIO.kext`, Thumb-2 code — an earlier plain-ARM disassembly
of this kext was wrong, redone correctly with `asm.bits=16`) and ruled
out for this specific call path** — not every conceivable host-side
producer; other command constructors, the channel object's own methods
(`this+0x24`, not itself disassembled), and pre-boot configuration
(see "`HeapBaseOffset`/`HeapSizeOffset`" below, which turned out to be
exactly this kind of mechanism, just for heap setup rather than SDHC)
remain outside this specific check's scope:

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

### CORRECTED (2026-10-16): the key/value table was misread; redone by tracing the actual consuming code

The property-dictionary table below was first read by assuming a uniform
8-byte `{key_ptr, value_ptr}` stride starting at `0x804801b4`. **That
assumption was wrong, and a reviewer was right to insist it be checked
against the code rather than trusted from the data layout alone.** The
compiler's literal pool is not laid out as clean adjacent pairs: an extra
value slot (`0x804801dc`, holding `0x804809a8`) is inserted early for a
value (`PanicLogBytesMax`) that is actually *consumed* much later in the
function, via a register (`r8`) held live across many intervening calls.
This shifts every property from `ConfigurationOffset` through
`PageTableBase` by one slot relative to the naive reading.

Redone by disassembling the wrapper's `__text` in full (`asm.bits=16`)
and tracing, for every one of the ~14 `setProperty`-shaped calls, which
literal-pool address feeds `r2` (the value, always dereferenced once)
and which key string loads into `r1` immediately before the matching
`blx`. Confirmed, call by call:

| Key | Value address | Value |
|---|---|---|
| `ConfigurationOffset` | `0x80480988` | **`0xf018`** |
| `HeapBaseOffset` | `0x8048098c` | **`0xf0a0`** |
| `HeapSizeOffset` | `0x80480990` | **`0xf0a4`** |
| `HeapRequired` | `0x804809b0` | **`0xa000`** |
| `PageTableBase` | `0x80480994` | `0xffffffff` |
| `PageTableSize` | `0x80480998` | **`0xffffffff`** |
| `MessageChannelSize` | `0x804809b4` | **`0x8`** |
| `PanicString` | `0x804809a0` | `0x1a464` |
| `PanicFunction` | `0x804809a4` | `0x12460` |
| `PanicLogBytesMax` (via `r8`) | `0x804809a8` | `0x1245c` |

Every value changed except `PanicString`/`PanicFunction` (which the naive
reading happened to get right) and `PageTableBase` (which kept the same
numeric value by coincidence of position). **Do not reuse the old
table** (`ConfigurationOffset = 0xf0a0`, etc.) from any earlier version
of this document or its git history — it was wrong.

### `ConfigurationOffset` (`0xf018`) traced in full: a real, checkable config struct, just not an SDHC one

Searched the firmware for every reference to the literal address
`0xf018`. Exactly three exist. Two of them (inside `fcn.0000080c` and
`iop_control_task`'s own two-phase boot-init dispatcher, `fcn.00000afc`)
use `0xf018` purely as the **upper bound** of the 2-entry callback-pointer
loop at `0xf010`/`0xf014` (already traced: entry 1 → `0x664`, a generic
per-priority lock callback used for channel 3/"sdio"; entry 2 → `0x15fc`,
NAND's own two-phase device-list dispatcher). Taken alone, that made
`0xf018` look like a mere boundary, not something to dereference — a
conclusion a reviewer correctly refused to accept without checking what
*else* reads it.

The third reference, inside `fcn.0000080c` right after the main
task-creation loop, is different and more interesting:

```
0x890  ldr r4, [lit]       ; r4 = 0xf018
0x894  ldr r3, [r4, 4]     ; r3 = *(0xf01c)
0x898  tst r3, 1
0x89c  beq 0x8b8           ; skip unless bit 0 is set
       ... create an extra task named "slopsucker" ...
0x8b8  ldr r0, [r4, 0xc]   ; r0 = *(0xf024)
0x8bc  cmp r0, 0
0x8c0  blne fcn.000085f8   ; call it with r0, if non-zero
```

`fcn.000085f8` is a two-instruction function: it just stores its argument
into a global at `0x118e0` (read elsewhere by `fcn.00008ef0`, a generic
dispatcher — not examined further). **This confirms `0xf018` is genuinely
the start of a small, host-checkable configuration/flags struct** — the
boundary interpretation and the struct interpretation are both correct
and compatible, because the 2-entry callback table happens to end exactly
where this struct begins in the firmware's packed static layout. Both
fields (`+4` bit 0, `+0xc`) read as zero in the shipped image, so neither
optional behaviour (an extra debug task, a global-variable override)
fires as shipped.

**This does not restore any SDHC link** — the two real fields found here
(an extra-task flag, a generic global-variable seed) are unrelated to
SDHC/SDHCI in anything traced so far — but it does mean `ConfigurationOffset`
should not be dismissed as "just a boundary" the way an earlier version of
this note put it. It is a real configuration mechanism; it is simply for
something else.

### `HeapBaseOffset`/`HeapSizeOffset` (`0xf0a0`/`0xf0a4`): the real, confirmed injection point

These are the ones that actually matter, and unlike `ConfigurationOffset`
this is now **fully confirmed with its real consumer found**, not just a
sentinel-pattern inference. Searched for literal references to `0xf0a0`
and `0xf0a4`: exactly one each, both inside `fcn.00008254`, called from
`fcn.0000080c` (the firmware's own early boot/task-setup routine) at its
very first call (`0x81c`):

```
0x0000825c  ldr r0, [0x8280]   ; r0 = 0xf0a0 (HeapBaseOffset's own address)
0x00008260  ldr r1, [0x8284]   ; r1 = 0xf0a4 (HeapSizeOffset's own address)
0x00008264  ldr r0, [r0]       ; r0 = *(0xf0a0)  -- the actual heap base VALUE
0x00008268  ldr r1, [r1]       ; r1 = *(0xf0a4)  -- the actual heap size VALUE
0x0000826c  bl fcn.00007794    ; the GENERIC ALLOCATOR this whole firmware uses everywhere
```

**This is the firmware's own heap-allocator initialisation, seeded
directly from `*(0xf0a0)`/`*(0xf0a4)`.** Both currently hold `0xffffffff`
in the static file. **Precision, corrected after an earlier overclaim**:
this confirms the *consumer* side only — the firmware reads and
dereferences both offsets at boot and feeds the result to its generic
allocator. The producer (what writes `0xf0a0`/`0xf0a4` before this boot
code runs) has not been found or traced; a producer existing is implied,
not shown. `fcn.00008254` also
calls `fcn.00008b14` and `fcn.00009304` right after the allocator setup
(not yet opened) and tail-calls `fcn.00008098`, which does a second,
separate `fcn.0000ae40(0x1000, 0x116a0)` call and stores its result at a
fixed address (`0x116b0`) — plausibly a second, fixed-size pool, not yet
connected to anything SDIO-specific either.

**Not yet found**: which kext/code patches `fwFirmwareImage+0xf0a0`/
`+0xf0a4` before boot. Neither `AppleIOPSDIO.kext` nor
`IOSlaveProcessor.kext` contain these offsets or the string
`"HeapBaseOffset"` — the patcher remains unlocated (see "ConfigurationOffset's
consumer" below, which applies equally here). `AppleARM7M::startCPU`
(found separately, see below) computes a rounded-up size and a hardware
address (`0x86300000`) into its own companion structure; whether that
structure's values ultimately get written into these two firmware
offsets has not been confirmed — a plausible, not yet proven, connection.

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
core — this is the load-path code the main report's "Open questions"
(question 6) asked for, now located and partially read.

**Checked and still not found**: `0xf018`/`0xf0a0`/`0xf0a4`/`0xa000`
(the corrected `ConfigurationOffset`/`HeapBaseOffset`/`HeapSizeOffset`/
`HeapRequired` values — re-checked with the corrected table above after
the earlier key/value misreading) do **not** appear as literals anywhere
in `AppleARM7M.kext` either, and neither does the string
`"ConfigurationOffset"`, `"HeapBaseOffset"`, or `"getProperty"`. The property-dictionary
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

### The host-side patcher (for `HeapBaseOffset`/`HeapSizeOffset`, the real target): checked the full kernelcache, still not found

(Originally searched for `ConfigurationOffset`'s own patcher before that
property turned out to be a loop boundary, not a host-injection point —
the search method below applies equally to the real target,
`HeapBaseOffset`/`HeapSizeOffset`, since neither property's *name* is
baked into any consumer's own code either way.)

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

### Reframing (2026-10-16): the queue slot holds a POINTER to the message, not the message itself

Per a reviewer's request to distinguish precisely the queue slot, the
value it carries, and the translated buffer the handler finally uses —
re-traced `fcn.00000eb0` (the raw receive call) instruction by
instruction rather than trusting the earlier summary. The actual return
path:

```
r4 = (channel_index << 5) + 0x10da4      ; the channel descriptor
r1 = [r4+0xc]                             ; read index; -1 = empty queue
r5 = [r4+0x14]                            ; ring base
slot = (read_index << 6) + r5             ; the 64-byte slot address
r1  = [slot]                              ; *** the slot's own FIRST WORD ***
...
r1  = r1 & ~3
str r1, [r8]                              ; <-- THIS is the function's return value
```

**The value `sdio_message_loop` receives, and passes into the
physical→local translator (`fcn.00008528`), is `*(slot) & ~3` — the
*content* of the slot's first word — not the slot's own address.** This
reframes the whole chain: the 64-byte ring slot is a **pointer-holder**,
not the message. Consistent with this, `fcn.00000db4` (generic queue
creation) initialises every slot's first word to the literal `1` — a
placeholder/"empty" sentinel, not a valid pointer — which some other
sender (plausibly the host, writing its own command buffer's physical
address into the slot as part of "posting" a message) is expected to
overwrite before a real message can be received.

Put together with everything already known about the host side: the
host's `sendIOPCommand` hands the IOP the **physical address of its own
command buffer** via the transport call. The most direct reading of this
new trace is that *that very physical address* is what ends up written
into the ring slot, and **"message" (and therefore `message+0x0c`) is
the translated view of the host's own shared command buffer — not an
IOP-internal structure.** This does not, by itself, relocate `base`'s
real producer (nothing here shows the host writing to `command_buffer+0x0c`
specifically; a grep-based search for that pattern in `AppleIOPSDIO.kext`
did not resolve cleanly — large, unaligned Thumb regions produced too
much noise to trust without per-function disassembly), but it does mean
the earlier framing ("message lives in firmware RAM, ruling out host
involvement") was describing the *slot*, not the *message* — the message
itself is, very plausibly, host memory all along. This reopens, rather
than closes, the question of whether the host writes `base`, through a
narrower and more specific question: does anything write to
`command_buffer+0x0c` in the host's own code, at buffer-allocation time
or afterward? Not yet confirmed either way.

### Milestone 1 done: `startCPU`'s hardware sequence, `this+0x58` identified, `0x86300000` cross-confirmed via the device tree

Per the plan to finish the `startCPU` load path before moving to dynamic
testing: traced `AppleARM7M::start(IOService*)` (the big function at
`0x807130f0`, not just `startCPU` at `0x80713664`) to find exactly what
`this+0x58` is.

```
r0 = [this+0x50]            ; _provider (AppleARMIODevice)
... bl vtable+0x208         ; mapDeviceMemoryWithIndex(0)
str r0, [this+0x54]         ; the IOMemoryMap OBJECT -- not yet the raw pointer
... (same call with index=1, result -> [this+0x5c])
r0 = [this+0x54]
... bl vtable+0x4c          ; getVirtualAddress() on the memory map object
str r0, [this+0x58]         ; <-- the RAW VIRTUAL ADDRESS, cached here
... (same for the second map -> [this+0x60])
```

**`this+0x58` = `_arm7mRegisterMap`'s raw virtual address — a real,
mapped hardware register window, not a C++ object.** `startCPU` writes
directly into it (`+0x100`, `+0x110`, `+0x114`, `+0x118`) with no
intervening virtual call, consistent with this.

**Cross-confirmed independently via the device tree**, not just asserted
from the host driver's own code: the `iop,s5l8920x` node's `reg`
property (parsed directly, not eyeballed) gives two regions:

| Index | Physical base | Size | CPU-visible (high bit set) |
|---|---|---|---|
| 0 (`_arm7mRegisterMap`) | `0x06300000` | `0x1000` | **`0x86300000`** |
| 1 (`_vicRegisterMap`) | `0x3f300000` | `0x1000` | `0xbf300000` |

**`0x86300000` is index 0's real, device-tree-confirmed physical
address** — the same value found independently hardcoded in the IOP
firmware's own generic log function (`fcn.000074d4`). Two separate
sources (host driver code, device tree) now agree; this is as solid as
static analysis gets without running it.

The real hardware sequence `startCPU` performs through this window:

1. `base+0x100 = 0x10` (a control/mode write), then re-read and tested
   for bit 1 — if clear, logs `"ARM7M not stopped for some reason"` but
   **continues anyway** (non-fatal; reads as a sanity check against a
   stale previous session, not a hard precondition for this boot).
2. `base+0x118 = 0x86300000` — the window's own base address, written
   back into itself as data. Role not fully resolved: plausibly a
   self-identification/notify field, or an address the IOP's own side
   needs told to it explicitly rather than assuming it.
3. `base+0x110 = ` the firmware's raw **host-virtual** bytes pointer
   (from the `IOSlaveFirmware` object's memory descriptor) — notably
   not obviously translated to a physical/DMA-coherent address first;
   whether that translation happens elsewhere (inside one of the two
   `vtable+0x360`/`+0x364` provider calls made just before, not
   individually resolved) is open.
4. `base+0x114 = ` the firmware length, rounded up to the next power of
   two from `0x1000`, then either negated-and-masked with `0x3ffff000`
   or used directly, depending on a mode ivar (`this+0x64`). **Resolved
   (2026-10-16)**: `this+0x64` is not a runtime condition — it is the
   device tree's own `"iop-version"` property value, read via
   `getBytesNoCopy`-then-dereference inside `start()`. On this device
   `iop-version = 1`, so the negate-and-mask branch is always taken.
5. At the very end, after a few more provider/VIC-related virtual calls:
   `base+0x100 = 1` (a second, different control write — the "go" signal
   after "reset"/"configure") immediately followed by `this+0x80 = 1`
   (marks the CPU object as started) and a provider `registerService`-style
   call.

**Confirmation mechanism**: `this+0x80` (checked at `startCPU`'s own
entry, to refuse double-start) is the host's own bookkeeping flag that
the sequence completed without an early error return — not, by itself,
proof the IOP is actually executing. No readback step was found in this
function that verifies the IOP core itself reached a running state
(e.g. polling a "core alive" bit) — if one exists, it is outside
`startCPU`'s own body.

**What this gives for a dynamic test**: the physical address to read
from the main CPU to inspect this register window directly is now known
(`0x86300000`, size `0x1000`) — a real, reproducible target for a
Trognon payload, independent of reconstructing the message transport.

### First dynamic result (2026-10-16): the window reads zero, no fault

Ran it: `tools/payloads/trace.c` stage 26 (`trace_iop_regs`), a read-only
dump of `+0x000`/`+0x100`/`+0x110`/`+0x114`/`+0x118` relative to
`0x86300000`, sent via `tools/go.sh` to the 3GS already in pwned DFU.
Nonce-confirmed execution (the harness's own proof that this exact
binary ran this exact time, not a stale result):

```
tools/go.sh tools/payloads/trace_iop_regs.bin --vars iop000,iop100,iop110,iop114,iop118
```

All five offsets read `0x00000000`, with **no data abort** — the window
is reachable from the main CPU (no MMU fault), but its content is inert.
Consistent with the hypothesis that `AppleARM7M::startCPU` never runs
under Trognon's own tethered DFU/iBSS path (which skips the normal iOS
boot where it would execute) — not, by itself, proof of that; zero could
also mean "mapped but never written for an unrelated reason." Evidence
bundle: `evidence/20261016T093436Z-trace_iop_regs/` (payload SHA-256
`ac28b3f7cb024a3581bff0fc418bf8511b75e54c11d278d0275c5e53e6d4d5b1`,
nonce `401E6B27`).

### `iop_probe` steps 1-2 (2026-10-16): mapping and clock both check out; no missing prerequisite found

Per a reviewer's plan to close the mapping question and extract
`start()`'s own prerequisites *before* touching the register window's
control bits again — read-only throughout, no write attempted.

**Step 1, mapping.** Checked statically first: the device tree's
`/device-tree/arm-io` (the `iop` node's own parent) has a real `ranges`
property, `(0x0 → 0x80000000, size 0x40000000)` then `(0x40000000 →
0x40000000, size 0x40000000)` — i.e. "add `0x80000000`" only for child
addresses below `0x40000000`, identity above. Both this project's
addresses (`0x06300000`, `0x3f300000`) fall in the first range, so
`0x86300000`/`0xbf300000` are confirmed by the real rule, not by
treating "set the high bit" as universal. Then checked the CPU's own
side: a new stage (`trace_iop_probe_mmu`, stage 27) reads the live L1
MMU descriptor for `SOC_IOP_REG_BASE` via `TTBR0`. Nonce-confirmed
result: identity section (`VA = PA = 0x86300000`), domain 0 (matches the
already-documented `DACR`), memory type Strongly-Ordered
(`TEX=0,C=0,B=0` — no caching, no buffering), `AP=3` (full access).
**Closes the mapping question**: the earlier all-zero read was not a
translation error, a stale cache line, or a silent permission failure.

**Step 2, `start()`'s prerequisites.** Re-read `AppleARM7M::start()` in
full, start to end (it finishes with a `registerService()`-shaped call).
**No clock-enable call and no reset-deassert call anywhere in it** — the
`"function-device_reset"` reference found earlier in this kext turned
out to be past `start()`'s own end, i.e. inside `stop()` (asserting
reset when stopping, not deasserting it when starting). Checked the
live clock gate anyway (`trace_iop_clkgate`, stage 28): the device
tree's own `clock-gates = 0x27` for the `iop` node reads `0x0000000F`
on the device — the same "fully enabled" pattern used elsewhere in this
project, already on independently of this driver ever running.

**Resolved along the way**: `startCPU`'s mode ivar (`this+0x64`) is the
device tree's own `"iop-version"` property value (read via
`getBytesNoCopy`-then-dereference inside `start()`), not a runtime
condition — `iop-version = 1` on this device, so the negate-and-mask
branch for `base+0x114` is always taken here.

**Where this leaves things**: mapping correct, memory type correct,
clock already enabled. No missing prerequisite was found that would
explain the all-zero read from the previous step — it most likely
reflects the real idle state of a core that has never been started
under this tethered path, not a configuration artifact. Evidence:
`evidence/20261016T094340Z-trace_iop_probe_mmu/`,
`evidence/20261016T094945Z-trace_iop_clkgate/`.

**Correction (2026-10-16, later): the clock-gate reading above is withdrawn.** The decrypted iBSS names physical gates `0x26` and `0x27` as the NAND flash
controllers 0 and 1 (`WMR_CLOCK_RESET_DEVICE`, `iBSS_n88ap_4.3.5/clock_gate_init_0x8400dd58.yaml`, `IBSS-CLK-006`), as OpeniBoot does. The `0xF` read at `0x27`
is the NAND controller 1's clock, which the iBSS turns on to load the next stage; it says nothing about the IOP. The device tree's `clock-gates = 0x27` for the `iop` node
does not tell which physical gate clocks the IOP. The IOP's clock state is unknown, and "clock already enabled" in the paragraph above is not established.

### Host side, traced further: `this+0x20` is found by waiting for a `role="IOP"` service, not a simple cast

Continued static analysis of `vtable+0x84` (the real send, per
`sendIOPCommand`). Re-read `AppleIOPSDIO`'s channel-setup code
(`fcn.8057c6e0`) instruction by instruction:

1. Reads a property from its own provider (likely `"EmbeddedIOP-role"`,
   compared against the string `"IOP"` sitting right next to it).
2. Enters a retry loop with a real timeout (`0x3b9aca00` = one second in
   nanoseconds), logging `"Failed to get IOP after %llu sec."` on
   failure — it actively **waits for a matching service to appear**,
   not reading an already-attached object.
3. Casts the result via a generic kernel safe-cast helper against a
   metaclass pointer stored in `IOSlaveProcessor.kext`'s own data
   (`*(0x80399bfc)`), failing with `"...service is not an instance of
   IOSlaveProcessor"` otherwise. The result becomes `this+0x20`.

**This strongly suggests `this+0x20` ends up being the `AppleARM7M`
instance itself** — `AppleARM7M` is the concrete driver matched against
`role = "IOP"` in the device tree (see above), and this
wait-for-a-role-IOP-service pattern is exactly how one driver finds a
sibling through the IORegistry.

The factory call itself, read precisely: `this+0x20`'s vtable slot
`0x350` is called with **three real arguments** — the `AppleIOPSDIO`
instance, a small callback object built via a local helper
(`fcn.8057c354`, the same wrapper used for `handleIOProcCmdResp`), and
the literal string `"sdio"`. **A wrong hypothesis was considered and
correctly rejected before being written down anywhere**: slot `0x350`
is *not* the generic, well-known `IOService::getDeviceMemoryWithIndex(unsigned int)`
(a real kernel symbol, confirmed by disassembling it directly in the
kernelcache at a completely different address) — that takes one integer
argument, not three, and the call site's own argument count doesn't
match. The three-argument shape is consistent with a genuine "open or
create a named channel, with a completion callback" factory, matching
the original reading. Its result becomes `this+0x24`; `vtable+0x84` on
*that* object is the real send.

**Practical limit of static analysis reached here**: pinning down slot
`0x350`/`0x84`'s own concrete code requires correctly reconstructing
`IOSlaveProcessor`'s (or `AppleARM7M`'s) real C++ vtable layout. A blind
offset-into-`__const` read was tried and its numbers didn't match this
call site's own argument count — caught and discarded before being
reported as a finding, rather than published as one. `IOSlaveProcessor.kext`'s
own code remains almost entirely generic IOKit boilerplate with no
method bodies distinctive enough to assign to a specific named class
with confidence. Reading the live object's actual vtable pointer and
slots from a running system is the more reliable next step here —
deferred to hardware testing.

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
