# 6. Support by chip

Trognon covers the 32-bit iPhones **before the A5**: the iPhone 3GS first, then the iPhone 4. It is built
**bottom-up from the 3GS**. This page says, per chip, what is known, how it boots, and what carries over
from the neighbour. Evidence tags as in the [index](README.md). Only the 3GS has been touched by us:
everything about the iPhone 4 is **SOURCE** (read in our research notes) or **HYPOTHESIS**.

## Where the line is

The line is the A5, because that is where an upstream starts.

| | **Inside Trognon: pre-A5** (3GS, iPhone 4) | **Beyond the A4** (A5 and later) |
|---|---|---|
| Boot exploit | limera1n (3GS), SHAtter (A4, per ipwndfu): bootrom bugs from 2010, **before** checkm8 (**SOURCE**) | checkm8 (2019), A5 to A11 (**SOURCE**) |
| Helper board | none, host-direct | the A5 needs a Pico (**SOURCE**); later chips do not |
| Existing OS work | none worth building on today: OpeniBoot, dormant (**SOURCE**) | Project Cascadia (A5), Hoolock, Asahi tree, postmarketOS (**SOURCE**) |
| What we do there | **bare-metal**: our own boot payloads, drivers and [Mods](04-mods.md) | we follow the upstream projects and contribute there; see [Beyond the A4](#beyond-the-a4-a5-and-later) |

No upstream is active on the 3GS or the iPhone 4 (OpeniBoot is dormant, **SOURCE**), so everything here is proven here first. From the A5 on, others
already do the work, and Trognon does not repeat it. What carries over from one side to the other is the
method and the habits (evidence levels, the on-screen console idea), not the addresses. The two phones
inside Trognon differ in their exploit tool (limera1n versus SHAtter), and some older texts say limera1n
for both: the [research note](../research/2026-10-14-pre-a5-limera1n.md) records the correction.


## Timeline, chip by chip

One card per chip generation, in release order. Colour of the left edge: inside Trognon or beyond. Green fill: where our code runs. Red outline: the next bench.

--8<-- "docs/snippets/timeline.en.html"

## Overlap at a glance

| | 3GS (S5L8920) | iPhone 4 (A4, S5L8930) |
|---|---|---|
| Our status | **DEVICE**: code runs | not tried |
| CPU core | Cortex-A8 r2p1 (**DEVICE**, `MIDR`) | Cortex-A8 class (**HYPOTHESIS**, read `MIDR` first) |
| Instruction set | 32-bit ARMv7 | 32-bit ARMv7 |
| Boot exploit | limera1n (**DEVICE**) | SHAtter per ipwndfu (**SOURCE**) |
| Helper board | none | none |
| Boot method reusable? | the reference | yes: same shape (pwned DFU, patched iBSS, run) |
| CPU code, exception console | the reference | reusable (**HYPOTHESIS**) |
| Device addresses, display, iBSS | verified | **to redo** |
| Existing OS work | OpeniBoot/iDroid, dormant (**SOURCE**) | partial in OpeniBoot, dormant (**SOURCE**) |

What carries over is the *method* and the *CPU-level code*. What never carries over is every
address, the display path and the iBSS patch offsets: they are re-read on each chip (see
[code layout](07-code-layout.md)).

## iPhone 3GS, S5L8920: the reference (DEVICE)

- Pwned DFU with limera1n, patched iBSS, Recovery `run`, payload at `0x41000000`:
  [boot chain](01-boot-chain.md). Registers, display, GPIO and interrupt controllers:
  [hardware map](03-hardware-map.md). Exceptions: [console](02-exception-console.md).
- 320 x 480 screen, framebuffer `0x4fd00000`.
- Next rung of the [ladder](04-mods.md): a timer (the software-triggered IRQ is proven).
- OpeniBoot/iDroid is the code we read for addresses, and it is dormant since 2022
  (**SOURCE**, [research](../research/2026-10-14-microkernel-3gs.md)).
- **NAND**: identified, geometry and capacity confirmed (Toshiba, 32 GB, two dies on CE0/CE1) —
  [hardware map](03-hardware-map.md#nand).
- **Wi-Fi/SDIO** (BCM4325): address, clock-gate, IRQ and the reset/wake GPIO lines are known
  and the reset line has been toggled, but every register in the block still reads back `0`.
  The leading explanation is architectural, not a wrong offset: a second, embedded processor
  (Apple's "IOP") owns this block, running its own small OS that also drives NAND and an
  AES-capable DMA engine — see the
  [Wi-Fi/IOP architecture report](../research/2026-10-16-wifi-iop-architecture.md).
- **Touch**: no OpeniBoot port for this SoC; the protocol (SPI, a proprietary Apple ASIC, not an
  off-the-shelf chip) is known only from the previous generation
  ([recon](../research/2026-10-15-touch-wifi-nand-recon.md)).

## iPhone 4, A4 (S5L8930): the second bench (SOURCE, not tried)

- Same CPU family, so the CPU-level code should port with small changes (**HYPOTHESIS**).
- Exploit: our research found that `ipwndfu` dispatches **CPID 8930 to SHAtter**, not limera1n
  (**SOURCE**, [correction](../research/2026-10-14-pre-a5-limera1n.md)). Older write-ups list limera1n
  for A4 devices as well, so the two sources disagree: `ipwndfu -p` is the arbiter.
  Host-direct either way, no helper board.
- Addresses read in OpenIBoot's `plat-a4` (**SOURCE**, none checked on a phone): framebuffer
  `0x4F700000` or `0x5F700000` (two variants in its `clcd.h`), GPIO `0xBFA00000`,
  four interrupt controllers VIC0 to VIC3 at `0xBF200000`, `0xBF210000`, `0xBF220000`, `0xBF230000`.
  The screen is Retina, 640 x 960 (**HYPOTHESIS**, to confirm): the console font must scale.
- **Success criterion:** a
  console on the screen with only `soc/s5l8930.h` written, nothing else changed. If it works, the
  per-chip split of the code is validated.
- `env_set`, the `run` mapping and every iBSS offset are specific to the 3GS image and must be found
  again in the A4 iBSS (**HYPOTHESIS**).

## Beyond the A4: A5 and later

Trognon stops before the A5 (decision of 2026-10-17). From here on there is already an upstream, and the
rule is to **contribute there, not to fork** ([research](../research/2026-10-13-a7-a11-mainline.md)).
Everything below is **SOURCE**: read in our research notes, not tried by us.

- **A5 (iPhone 4S, iPad 2).** Boot needs a Pico running checkm8-a5 (RP2040 with a per-SoC UF2 and an OTG
  Y-cable; RP2350 does not work). Base: **Project Cascadia**, mainline Linux 6.12 on the iPad mini 1
  (S5L8942) with both cores, multitouch and Wi-Fi. No port exists for a phone yet
  ([research](../research/2026-10-13-a5-a6-32bit.md)).
- **A6 (iPhone 5, 5c).** checkm8 runs natively in `ipwndfu` over plain USB. The iPad 4 (A6X) is Cascadia's
  stated roadmap. Nothing known for the iPhone 5 or 5c.
- **A7 to A11 (iPhone 5s to X).** 64-bit, a different track. Device trees for all of them are in mainline
  Linux since 6.13; no real display driver, Wi-Fi, audio, camera, GPU or touch yet. Boot: checkm8, then
  pongoOS, then m1n1, then Linux. Downstream: Hoolock Linux and postmarketOS `apple-idevice`. Our 32-bit code
  does not apply; what carries over is the documentation method and the idea of per-device evidence.

One A5 device (an iPad 2) was identified over USB, read-only, on 2026-10-15; that work is not part of this
repository.

## Order of work

1. 3GS: the timer, then buttons.
2. iPhone 4: prove the per-chip split with a second bench.
3. Beyond the A4: follow the upstream projects already working there.

## Scope decision

Decision (2026-10-17): Trognon covers the 32-bit iPhones **before the A5** and stays one repository. It
replaces the decision of 2026-10-15 (one repository, two tracks: bare metal, then OS enablement on the A5 and
later). The reason: Project Cascadia starts at the A5, so the 3GS and the iPhone 4 are the devices with no
active upstream, and repeating the Cascadia path in a public repository would add nothing. The 3GS and the iPhone 4
stay together: they share a method and a CPU core, not their addresses, so moving from one to the other is a
second `soc/` header, not a second project.
