# NuttX / our own kernel / Linux ARM / Zephyr: which target for the 3GS?

Research date: 2026-10-15 (night). A reasoning-based analysis, no new source
reading that night: based on the hardware facts already established in this
session and the previous days (see `docs/architecture.md`,
`docs/research/2026-10-15-touch-wifi-nand-recon.md`).

## Hardware known at the time of this analysis

ARMv7 Cortex-A8, MMU present and in use, **VIC** interrupt controller (not
GIC), 256 MB RAM, Toshiba NAND identified (32 GB, CE0+CE1), reverse-engineered
CLCD screen (RGB888/RGB565 framebuffer), GPIO buttons mapped, SDIO block
(Wi-Fi/Bluetooth BCM4325) identified by address/clock-gate/IRQ but without
the reset GPIO line decoded, USB console only (ACM, device mode; host mode
not proven), permanently tethered boot (none of this bootrom-bug family
survives a reboot).

## The four options

### NuttX
Best effort-to-result ratio for this exact hardware. Modular RTOS, BSD
license, existing Cortex-A support (less common than Cortex-M in its
ecosystem, but real). Porting needed: `arch/arm/src/s5l8920`, which directly
reuses what is already reverse-engineered (VIC, timer, clock gates, GPIO,
CLCD) instead of starting from zero. Its MTD/NAND subsystem can build on the
`h2fmi` reads already done. Result: a POSIX-ish shell, a USB console, a
filesystem, without Linux's weight.

### Our own kernel (current trajectory)
Full control, maximum educational value (consistent with the project's
exhaustive-documentation goal). This is the path followed by
`docs/research/2026-10-14-microkernel-3gs.md`. Every building block
(scheduler, IPC, filesystem) is written by hand: the slowest to become
*useful* in the sense of an application ecosystem, the fastest to understand
line by line.

### Linux ARM mainline
The largest effort, the best long-term potential (consistent with "make the
phone a base for anything"). No mainline support for the S5L8920, unlike
the S5L8942 (Project Cascadia). Would require writing, out of tree: an
`irqchip` for the VIC, a `clocksource`, a `pinctrl`, a NAND MTD driver, an
SDIO driver. 256 MB of RAM is not an obstacle (embedded Linux runs with
less); the real cost is the volume of new drivers. Timescale: months, not
weeks.

### Zephyr
Less relevant here than the other three. Zephyr is strong on
microcontrollers without an MMU (Cortex-M, RISC-V); its Cortex-A/MMU support
is thinner than NuttX's for this chip class. Same porting effort as NuttX
(VIC, timer, GPIO, CLCD to write), but with less precedent in its tree for
an application-class Cortex-A8 with 256 MB of RAM.

## Summary table

| | Effort | Result | Reuses the RE already done |
|---|---|---|---|
| NuttX | Medium | Shell + fs + basic networking | Yes, directly |
| Our kernel | Low to start, high to go far | Minimal, 100% understood | Yes, it is the base |
| Linux | Very high | The richest long-term | Yes, but buried under new code |
| Zephyr | Medium to high | Comparable to NuttX, less mature here | Yes, but less precedent |

## Recommendation

Not an exclusive choice right now. Keep going with the current bare-metal
monitor (it provides proof at every step, consistent with the project's
method), but keep **NuttX in mind as a realistic medium-term target** once
the base drivers are stable: it is the path that discards the least
already-done work, unlike Linux, which would be a separate, considerably
longer undertaking for a payoff that would not materialize for a long time
on this specific SoC.

## Not verified

- Real maturity of Cortex-A8 support in NuttX and Zephyr: assessed from
  memory, not checked against their respective repositories during this
  session.
- None of these ports has been started; this is a decision analysis, not a
  progress report.
