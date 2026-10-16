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

## The thread

| Step | Function (address in `AppleARM7M.kext.bin`) | Fiche | Established | Still open |
|---|---|---|---|---|
| 1 | `AppleARM7M::start(IOService*)`, read in part (`0x80713190`–`0x8071326c`) | `start.yaml` | provider cached at `this+0x50`; device memory index 0 mapped to `this+0x58`, index 1 to `this+0x60` (`ARM7M-START-001`, `-002`) | entry address, lock allocation, tail after `0x8071326c` (`ARM7M-START-Q1`–`Q3`) |
| 2 | `AppleARM7M::startCPU(IOSlaveFirmware*)` (`0x80713664`) | `startCPU.yaml` | full body read; register sequence in window 0 (`ARM7M-STARTCPU-002`), guard (`-003`), size field (`-004`), start write last (`-005`) | 11 indirect or external calls (`ARM7M-STARTCPU-Q4`–`Q12`), bit meanings (`Q13`), constant `0x86300000` (`Q14`) |
| 3 | method at `0x80713510` | `vic_enable_0x80713510.yaml` | writes 8 or 4 to `[window1 + 0x18]` (`ARM7M-VIC-001`) | register semantics (`ARM7M-VIC-Q2`) |
| 4 | caller of `startCPU`, firmware copy to RAM, the `IOSlaveProcessor` class | `module_init.yaml` covers class sizes only | six class sizes (`IOSLAVE-INIT-001`) | **not read**: who calls `startCPU`, who copies the image, where. No id yet; to be created when the first of them is read |
| 5 | `sendIOPCommand` and the SDIO ping handler in the IOP firmware | not yet in the catalog | described in prose in the architecture report | no fiche; the ping is only reachable once the core runs |

Step 4 is the weakest link: the registers programmed by step 2 describe a
buffer (`+0x110`) and a size mask (`+0x114`), but this thread has not shown
who put the firmware there.

## What the register sequence says, and what it does not

`startCPU` writes, in window 0 of the `iop` node (`this+0x58`), in this
order: `+0x100 <- 0x10`, readback of `+0x100` (bit 1 tested), `+0x118 <-
0x86300000`, `+0x110 <- this+0x88`, `+0x114 <- (-size) & 0x3ffff000`, then
`+0x100 <- 1`. The device-tree property `iop-version` is 1 on this device,
which selects the masked form of `+0x114`.

Two observations that limit what can be inferred:

- The firmware image (`fwFirmwareImage.bin`, 110592 bytes) starts with an
  ARM vector table whose handler words are small absolute addresses (`0x40`,
  `0x939c`, ...). It is linked at base 0 and contains no `0x86xxxxxx`
  literal. `0x86300000` is therefore not the image's address as the IOP
  sees it (`ARM7M-STARTCPU-Q14`).
- At rest all five probed registers of window 0 read `0` (evidence
  `20261016T093436Z-trace_iop_regs`). The driver expects bit 1 of `+0x100`
  to be set after it writes `0x10`. A zero at rest is compatible with that
  and is not a measurement of "idle".

## Hardware ladder

Each rung states its prediction before it is run. Whatever is observed goes
back into the fiche named in the last column, including a result that
contradicts the reading.

| Rung | Action | Prediction recorded in advance | Records into | Status |
|---|---|---|---|---|
| T0 | read window 0 at rest (5 offsets) | all zero | `startCPU.yaml`, `start.yaml` | executed, result 0 |
| T0b | read window 1 (`SOC_IOP_VIC_BASE`), highlight `+0x18` (STAGE 32, `trace_iop_vic.bin`) | readable without abort | `start.yaml`, `vic_enable_0x80713510.yaml` | payload ready, **not run** |
| T1 | write `0x10` to `window0+0x100`, read it back, bounded: one write, one read, USB link checked after | `readback & 2 != 0` | `startCPU.yaml` (`ARM7M-STARTCPU-Q13`) | spec only, no payload yet |
| T2 | place the image in RAM with a sentinel pattern around it, program `+0x118`, `+0x110`, `+0x114`, write `1` to `+0x100`, wait a bounded time, read `+0x100`, window 1, and the sentinel | none yet: depends on `Q5` and `Q14` | `startCPU.yaml`, new fiche for the caller | **not authorised** until T1 is observed and `Q5`, `Q14` are reviewed |

T1 is the same write the driver issues at the start of every `startCPU`.
T2 starts a second core running an image whose first instructions were not
disassembled for this purpose, with a DMA-capable block pointed at a buffer:
it is a different class of risk and is gated.

Reading grid for T1 and T2, to fix before the run: (a) register still zero,
(b) stable structured value, (c) value that changes between reads, (d)
abort or loss of the USB link. A recognisable value would still not prove
that the IOP executes its firmware; proof of life requires an effect that
only the IOP can produce, such as the sentinel region changing, or a
status bit changing after the start write without any further host write.

## Reproduction

```
python3 -I tools/iop_analysis/dt_node_reg.py ...        # reg pairs of the iop node
r2 -e asm.bits=16 -c 'pd 90 @ 0x80713664' /tmp/trognon_dt/AppleARM7M.kext.bin
make iop-coverage                                        # validates and regenerates the table
```

The binaries come from the owner's IPSW and are never committed; the SHA-256
of each is checked against its `tool_coverage.json`.
