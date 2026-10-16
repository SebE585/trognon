# IOP start-up, from the driver to a first sign of life

Status as of 2026-10-16. Companion to the
[Wi-Fi/IOP architecture report](2026-10-16-wifi-iop-architecture.md); this
page follows one thread only: what the host driver does to start the IOP
core, which links of that thread are still unknown, and which hardware
observation would settle each one.

Every statement below points to a catalog fiche (`tools/iop_analysis/catalog/`),
a claim id, an open-link id, or an evidence bundle (`evidence/`). Nothing
here claims that the IOP has run: no observation so far is attributable to
the IOP itself.

## What "complete" means here

A fiche marked `complete` means every instruction from the function's entry
to its return was read. It is bounded to the reconstructed control-flow
graph: an indirect call (`blx reg`) or an external call is a place where
that graph stops, and each one is listed as an open link with the proof
that would resolve it. The generated table in the main report carries them
([open links](2026-10-16-wifi-iop-architecture.md#open-links-and-hardware-tests)).

## The thread: proven, assumed, related

The earlier version of this page drew one call chain. Only part of it is
established, so the links are separated by what supports them.

**Proven by disassembly** (a call or data reference visible in the code):

| From | To | Evidence |
|---|---|---|
| `startCPU` | window 0 register writes `+0x100`, `+0x118`, `+0x110`, `+0x114`, `+0x100` | `ARM7M-STARTCPU-002` |
| `startCPU` | 11 indirect or external calls (callee not read) | `ARM7M-STARTCPU-Q4`–`Q12` |
| `start()` | window 0 mapped to `this+0x58`, window 1 to `this+0x60` | `ARM7M-START-001` |
| the 0x80713511 method | a write of 8 or 4 to `window1+0x18` | `ARM7M-VIC-001` |
| one data word at `0x807143e8` | `startCPU` (its only reference in this kext) | `ARM7M-STARTCPU-006` |

**Assumed, not shown** (plausible, no evidence that the link exists):

| Assumption | What is missing |
|---|---|
| `startCPU` triggers, or is followed by, the VIC method | the two functions sit in the same pointer table (`0x807143e8`, `0x807143f0`), which shows class membership only; no call between them was found |
| the IOP reaches the ping handler after `startCPU` | no observation of the IOP running (see proof of life below) |
| `startCPU`'s caller copies the image before the call | caller and producer not located (`ARM7M-STARTCPU-Q15`) |

## Who starts the core and prepares the image

Found on 2026-10-16 by following class metadata rather than call sites. All
addresses below are in the kernelcache's own kexts; extraction is
reproducible with `tools/iop_analysis/extract_kext.py` (it reproduces the
SHA-256 of the previously extracted `AppleARM7M.kext.bin`).

- **Class layout of `IOSlaveProcessor.kext`** (`IOSLAVE-TABLES-001`–`003`).
  Facts: the second entry of each pointer table passes a size to a delete-like
  call (8, 0x10, 0x38, 0x50, 0x50, 0x68), and the six sizes equal the six
  instance sizes the module init registers. Candidate attribution: `0x68` is
  unique to `IOSlaveProcessor`, so the table holding the field accessor
  (`IOSLAVE-GETTER-001`) is probably its own; the two 0x50 tables (CPU and
  Firmware) cannot be told apart by size. Not confirmed: no constructor was
  linked to a metaclass registration explicitly. `IOSlaveCPU`,
  `IOSlaveFirmware` and `IOSlaveProcessor` are passed the same superclass
  metaclass object, which makes them siblings.
- **`AppleARM7M` derives from `IOSlaveCPU`** as far as its metaclass
  constructor's arguments show; instance size 0x90 (`ARM7M-META-001`).
- **A concrete firmware class, `IOP_s5l8920x_firmware`**
  (`IOP-CLASS-001`, `-002`), instance size 0x54, is the only concrete subclass
  of `IOSlaveFirmware` found by scanning the kernelcache for its metaclass
  pointer. Its method at `+0x34c`, the slot `startCPU` calls first
  (`ARM7M-STARTCPU-Q4`), is a lazy getter. It calls
  `IOMemoryDescriptor::withAddress(void*, unsigned long, IODirection)` (a
  symbol at `0x801ea698` of the decompressed kernel) with `0x80481000`,
  `0x1b000` and `3`, stores the result at `this+0x50`, and calls the result's
  slot `+0xa0` (role unknown); `0x1b000` = 110592 bytes, the size of the
  extracted `fwFirmwareImage.bin` (`IOP-GETMD-001`). This is a target
  identified **under a condition**: it is the callee only if the object that
  `startCPU` receives is an instance of that class, which is not shown.
- **A concrete `IOSlaveProcessor` subclass probably exists in `EmbeddedIOP`**
  (bundle `com.apple.driver.EmbeddedIOP`, 28672 bytes). Its strings name
  `EmbeddedIOP::start`, `_prepareFirmwareImage()`, a lookup of the firmware
  driver by the property `IOPFirmwareName`, reads of firmware properties
  (message channel size, panic buffer, heap base, size and requirement),
  and three messages: "IOP failed to start", "startup ping failed",
  "failed to allocate IOP-visible memory" (`EMBIOP-001`). No function of this
  kext has been read: everything about it below comes from strings and
  references.

What this changes:

- `ARM7M-STARTCPU-Q4` stays open, with a named target under a condition.
  `Q15` also remains open as worded: the caller is not identified. The only
  reference to `startCPU` found is one data word at `0x807143e8`, which is
  compatible with a virtual call and does not show that every call passes
  through it. `EmbeddedIOP` is a candidate.
- The strings suggest that the host driver checks the core after starting:
  "startup ping failed" at `EmbeddedIOP.cpp:221`. If it is a ping sent to the
  core and answered by its code, it would be the form of evidence the
  proof-of-life section asks for. Neither the message, nor the check, nor the
  answering routine has been read (`EMBIOP-Q3`).
- The strings suggest that `_prepareFirmwareImage` adjusts fields of the image
  (heap, channel, panic) before the start. Whether it writes anything, and
  what, has not been read (`EMBIOP-Q2`). If it does, a T2 that loads the
  unmodified image would not match what the driver does.
- `ARM7M-ADDR-001`: a method of `AppleARM7M` subtracts `0x40000000` (for
  `iop-version` 1) from an address and compares it unsigned with the size
  field that `startCPU` computes; the result is replaced by -1 when the offset
  is *below* the size, which is not an ordinary range check. The decoding is
  confirmed by the bytes; the purpose is open (`ARM7M-ADDR-Q1`), pending its
  callers.

**Correction**: an earlier version of these notes placed the pointer table
holding `startCPU` at `0x807153e8`. The kext's file offset 0x1000 maps to
virtual address `0x80713000`, so the table is at `0x807143e8`; the earlier
scan had used the wrong base. The values read were right, the addresses were
shifted by `0x1000`.

**Related functions** (same subsystem, no dependency claimed): the six
`IOSlaveProcessor` class registrations (`IOSLAVE-INIT-001`), `sendIOPCommand`
and the SDIO ping handler in the IOP firmware (architecture report, prose
only, no fiche yet).

Priority for the next reading: `EMBIOP-Q1` (the caller) and `EMBIOP-Q2` (what
is patched in the image), then `Q8`/`Q9` (they precede the first register
write and frame T1).

## What the register sequence says, and what it does not

`startCPU` writes, in window 0 of the `iop` node (`this+0x58`), in this
order: `+0x100 <- 0x10`, readback of `+0x100` (bit 1 tested), `+0x118 <-
0x86300000`, `+0x110 <- this+0x88`, `+0x114 <- (-size) & 0x3ffff000`, then
`+0x100 <- 1`. The device-tree property `iop-version` is 1 on this device,
which selects the masked form of `+0x114`.

- **`0x86300000` is the window's own address.** It is pair 0 of the `reg`
  property of the `iop` node, i.e. the CPU-visible base of the register block
  that `startCPU` is writing into. It is neither a RAM address nor the image's
  load address (`ARM7M-STARTCPU-Q14`). The earlier probe of `0x86300000` that
  read zero (`20261016T094340Z-trace_iop_probe_mmu`) was a read of this
  register block, not of RAM.
- **The image is linked at base 0.** `fwFirmwareImage.bin` (110592 bytes)
  starts with an ARM vector table whose handler words are small absolute
  addresses (`0x40`, `0x939c`), with no `0x86xxxxxx` literal.
- **Zero at rest is not "idle".** All five probed registers read `0`
  (`20261016T093436Z-trace_iop_regs`). The driver expects bit 1 of `+0x100`
  after writing `0x10`; a zero before any write is compatible with that and
  measures nothing about a stop request.
- **Readbacks prove storage, not role.** Reading back what was written to
  `+0x110` or `+0x118` would not show that the hardware treats them as a
  physical address or as a base. An apparent zero at the firmware's location
  would not exclude an alias or a translation between the IOP's view and the
  host's; that stays open.

## Hardware ladder

Each rung states its prediction before it is run. Whatever is observed goes
back into the fiche named in the last column, including a result that
contradicts the reading.

| Rung | Action | Prediction recorded in advance | Records into | Status |
|---|---|---|---|---|
| T0 | read window 0 at rest (5 offsets) | all zero | `startCPU.yaml`, `start.yaml` | executed, result 0 |
| T0b | read window 1 (`SOC_IOP_VIC_BASE`), highlight `+0x18` (STAGE 32, `trace_iop_vic.bin`) | readable without abort | `start.yaml`, `vic_enable_0x80713510.yaml` | payload ready, **not run**; adds one observation, resolves none of the dependencies |
| T1 | **isolated test**: write `0x10` to `window0+0x100`, read back immediately and again after fixed delays, check the USB link after | `readback & 2 != 0` at some delay | `startCPU.yaml` (`Q13`) | spec only, no payload |
| T2 | place the image in RAM, program `+0x118`, `+0x110`, `+0x114`, write `1` to `+0x100`, bounded wait, observe | none yet: depends on `Q5`, `Q14`, `Q15` | `startCPU.yaml`, new fiche for the caller | **not authorised** |

T1 does not replay the driver's preamble: `Q8` and `Q9` are made before the
first register write and are unread. A positive T1 is informative; a
negative T1 is ambiguous (missing precondition, or the reading of the bit is
wrong) and must be recorded as such, not as a refutation. Reading once
immediately can also miss a delayed transition, hence the delayed readbacks.

T2 starts a second core and points a DMA-capable block at a buffer: a
different class of risk, gated on T1 and on `Q5`, `Q14`, `Q15`.

## Proof of life: indications versus proof

Reading grid for T1 and T2, fixed before the run: (a) register still zero,
(b) stable structured value, (c) value that changes between reads, (d)
abort or loss of the USB link.

Two levels, not to be confused:

- **Indication of activity.** A status bit that changes, a sentinel region
  that gets modified, a VIC register that moves. Each of these can also come
  from a hardware state machine, a DMA engine or a corruption. They justify
  further work and nothing more.
- **Proof of execution.** A response to a host-chosen nonce that was computed
  by identified IOP code, or an explicit write by that code to an agreed
  location, with the code's address known in the firmware image. This
  requires knowing which IOP routine produces it (for example the SDIO ping
  handler once its trigger is understood, or a minimal program of our own, the
  "own firmware" step of Phase 4). Until then the project claims at most an
  indication.

## Reproduction

```
python3 -I tools/iop_analysis/dt_node_reg.py /tmp/trognon_dt/devicetree_decrypted.bin iop   # reg pairs of the iop node
python3 -I tools/iop_analysis/extract_kext.py /tmp/trognon_dt/kernelcache_decrypted.bin com.apple.driver.EmbeddedIOP /tmp/trognon_dt/EmbeddedIOP.kext.bin
python3 -I tools/iop_analysis/extract_kext.py /tmp/trognon_dt/kernelcache_decrypted.bin --decompress-only /tmp/trognon_dt/kernel.bin
r2 -qc 'e asm.bits=16; is~withAddress' /tmp/trognon_dt/kernel.bin   # symbol at 0x801ea698
r2 -e asm.bits=16 -c 'pd 90 @ 0x80713664' /tmp/trognon_dt/AppleARM7M.kext.bin   # startCPU
make iop-coverage                                        # validates and regenerates the table
```

The binaries come from the owner's IPSW and are never committed; the SHA-256
of each is checked against its `tool_coverage.json`.
