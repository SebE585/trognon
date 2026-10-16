# 3. Hardware map: what is verified

Register addresses come from OpenIBoot (`plat-s5l8920`). They are **SOURCE** until read
on the phone. This page lists what we checked. Most entries are plain reads, each one
armed as an expected data abort so an unmapped register prints `ABORT` instead of
resetting the phone — but not all: a few sections deliberately write to the hardware
(a GPIO toggle, a software-triggered interrupt, a NAND chip-ID command) and say so
explicitly where they occur. "No persistent write" (nothing stored to flash) is not the
same claim as "no write at all".

## CPU and MMU (DEVICE)

| Item | Value | Meaning |
|---|---|---|
| `MIDR` | `0x412FC081` | ARM, part `0xC08`, variant 2, revision 1: Cortex-A8 r2p1 |
| `SCTLR` | `0x00C5187D` | M=1 (MMU), C=1 (D-cache), I=1 (I-cache), Z=1, A=0 (no alignment check), V=0 (vectors at VA 0) |
| `DACR` | `0x00000003` | domain 0 manager, other domains no access |
| Page table | 1 MiB sections, all in domain 0 | nothing unmapped in indexes 1..0xfff |

## Display controller, `CLCD_BASE = 0x85400000` (DEVICE)

| Offset | Value read | Meaning |
|---|---|---|
| `+0x0` | `0x00020085` | bit 0 set: enabled (OpenIBoot writes `0x20084`) |
| `+0x24` | `0x4fd00000` | framebuffer address, confirmed by a visible fill |
| `+0x1B10` | `0x00000001` | enable |
| `+0x1B14` | varies per read | **live counter**, not configuration: low byte `0xE0` stable, high bits advance. Bits 0-3 are 0, as OpenIBoot writes |
| `+0x1B1C` | `0x00050507` | vertical porches (value stored minus 1) |
| `+0x1B20` | `0x000b0b0f` | horizontal porches (stored minus 1) |
| `+0x1B24` | `0x013f01df` | 320 x 480 (0x13f, 0x1df) |

The panel itself is driven over MIPI DSI (reset line probably GPIO `0x500`): the
controller is already initialised by the iBSS, which is why we can just draw.
Writing our own panel init is a much larger job (**HYPOTHESIS**, see OpenIBoot `pinot_init`).

## GPIO and clock gates (DEVICE, partial)

| Address | Value | Status |
|---|---|---|
| GPIO base `0x83000000` | readable, no abort | mapped, offsets `(8*port+pin)*4` answer |
| `GPIO+0xA0` (pin `0x500`) | bit 0 = 1 | consistent with a released panel reset, **not proven** |
| `GPIO+0x00 / +0x80 / +0x320` | read, role unknown | do not interpret |
| Clock gate `0xBF100128` | `0x0000000F` | read only |

Values read by eye from the screen; field widths were assumed to be 8 hex digits.

## Not read, on purpose

`GPIOIC` (`0x39A00000`): the one read of it dropped the USB link (cause undetermined).
Treat as dangerous until understood.

## Interrupt controllers VIC0/1/2 (DEVICE, 2026-10-15)

Read-only survey. All registers answered, no abort.

| Register | VIC0 `0xBF200000` | VIC1 `0xBF210000` | VIC2 `0xBF220000` |
|---|---|---|---|
| `STATUS` (`+0x00`) | `0` | `0` | `0` |
| `RAW` (`+0x08`) | `0` | `0` | `0` |
| `SELECT` (`+0x0C`, 0 = IRQ) | `0` | `0` | `0` |
| `ENABLE` (`+0x10`) | `0x3FFA4040` | `0` | `0` |
| `SOFTINT` (`+0x18`) | `0` | `0` | `0` |

- The iBSS left **14 sources enabled in VIC0** (bits 6, 14, 17 and 19 to 29), all routed to IRQ,
  none pending, and IRQs are unmasked in the CPU: interrupts are live while our code runs.
- `SOFTINT` works: writing bit 0 made `RAW` and `STATUS` read 1 and raised a real IRQ, taken by our handler
  (see the [IRQ test](02-exception-console.md)). `INTENCLEAR` clears `ENABLE`, `SOFTINTCLEAR` clears the soft
  interrupt, both read back on the device.
- The iBSS handlers sit at `0x8400F9EC` (IRQ vector word `0x38`) and `0x8400FA34` (FIQ vector word `0x3C`).
- Which peripheral each enabled bit is stays unknown.

Timer registers are the least understood part of OpenIBoot (its own comment says they are copied from an older SoC).

## Timer block, `TIMER_BASE = 0xBF100000` (DEVICE, 2026-10-15)

| Register | Offset | Observed |
|---|---|---|
| Free-running tick, low | `+0x200` | advances; **24.001 MHz measured against the host clock** (3 samples, 12.2 s, 0.004 % off 24 MHz) |
| Free-running tick, high | `+0x204` | `0` (the 32-bit low word wraps every ~179 s) |
| Timer 0 `STATE` | `+0x04` | `0x00669601` |
| Timer 1 `STATE` | `+0x24` | `0x00000E02` |
| Timers 2 and 3 `STATE` | `+0x44`, `+0x64` | `0x00000B00` each |
| Timer 5 `STATE` | `+0xC4` | `0` (looks unused) |
| Timer 4 `STATE`, `CFG`, `CNT`, `CNT2`; Timer 6 `STATE`; clock gate `0x25` | `+0xA4, +0xA0, +0xA8, +0xAC, +0xE4, +0x10C` | all exactly `0x0000000F`; Timer 4 `PRES` (`+0xB0`) reads `0` |

- The tick runs at 24 MHz. OpeniBoot says the same (`TicksPerSec = PLLFrequencies[3]`, **SOURCE**), which is why
  the measurement is a confirmation, not a surprise.
- The `STATE` words of timers 0 to 3 look structured (they are probably not just a start bit); we do not decode them.
- The wall of `0x0000000F` on timer 4 and 6 is not explained: it resembles the clock-gate registers (`0xF`
  everywhere), so those registers may be aliased or not implemented. Do not interpret them.
- **A timer interrupt is not proven.** Line 6 of VIC0 (the timer line in OpeniBoot) can be taken by our
  handler, but the source is not ours: with timer 5 never started, the interrupt still arrived 127 ticks
  (5 microseconds) after we enabled the line, because it was already asserted. `IRQSTAT` (`+0x10000`) *was*
  read as part of this test — it reads `0xFFFFFF81` before, during and after, whatever we do, so it is not
  a timer status register, and the acknowledge sequence copied from OpeniBoot (`IRQSTAT`, `IRQLATCH`) clears
  nothing. `IRQLATCH` (`+0xF8`) itself was only ever written as part of that same acknowledge sequence, never
  read in isolation to see what it holds. OpeniBoot's own timer handler for this SoC is commented out. Treat
  both offsets as wrong until proven otherwise.
- Measurement tool: `make trace_ticks.bin` plus `tools/tick_rate.py`. It publishes the counter through an
  environment variable read with `getenv`, so no photo is needed. Two samples taken in two separate
  invocations: one process cannot hold the device across a pause (the second acquire fails).

## NAND, `H2FMI0_BASE = 0x81200000` (DEVICE, 2026-10-15)

Chip ID read (read-only: reset + READ ID command, no page read, no write) across all 16
chip-select lines on bus 0, then its geometry looked up in OpeniBoot's own hardcoded table.

| Item | Value | Meaning |
|---|---|---|
| CE0, CE1 | `98 DE 95 32 7A 54 0C 00` | Toshiba (JEDEC manufacturer `0x98`); the same ID on both, cross-check |
| CE2 to CE15 | `00 00 00 00` | no chip present |
| Geometry (OpeniBoot table, `nand_chip_info[]`, exact ID match) | `blocks_per_ce=0x2068`, `pages_per_block=0x80`, `bytes_per_page=0x2000`, `banks_per_ce=2` | SOURCE match, not read from the device |
| Capacity, computed | 8296 x 128 x 8192 x 2 banks x 2 CE = 34.8 GB raw | matches the 32 GB label on this unit after the usual ECC/spare margin (~8%) |

Bus 1 (`H2FMI1_BASE = 0x81300000`) not swept yet. No page read, no write attempted: this
project's NOR-equivalent caution (`tools/boot.sh` never calls `flash_nor()`) applies here too,
with a larger margin, since NAND holds the device's own iOS image.

## Buttons, same GPIO base as above (DEVICE, 2026-10-15)

Found by difference, not by name: OpeniBoot's own button names for this SoC look copied from
an older one, so every readable GPIO bit (`(8*port+pin)*4` from the base above) was sampled,
noise measured with hands off, then each button asked for in turn.

| Button | Pin (`port.pin`) |
|---|---|
| HOME | `16.6` |
| POWER | `16.7` |
| VOL UP | `16.0` |
| VOL DOWN | `16.1` |
| RINGER (toggle) | `14.3` |

376 pins read, zero noise over a 3-second hands-off window. A long payload (this sweep takes
close to 3 minutes) killed the USB control endpoint afterwards; a host-side USB port reset
(`dev.reset()`, no DFU needed) revived it — see [evidence](08-evidence.md).

## Wi-Fi/SDIO, `SDIO0_BASE = 0x80000000` (mixed DEVICE/HYPOTHESIS, 2026-10-15/16)

No OpeniBoot source exists for this block on the S5L8920 at all (unlike NAND). Everything
below the address itself came from this device's own decrypted device tree and kernelcache
(owner's IPSW, never redistributed), not from source code.

| Item | Value | Status |
|---|---|---|
| Base address, size | `0x80000000`, `0x1000` | from this device's own decrypted device tree (owner's IPSW), not a register read |
| `compatible` | `"sdio,s5l8920x"` | device tree, same caveat |
| `clock-gates` | `0x10` | device tree value; DEVICE: enabling it alone did not make any register respond |
| `interrupts` | `0x22` | device tree, same caveat |
| Reset GPIO (`gpio_baseband_wlan_reset`) | `16.5` | device tree gives the name/pin; DEVICE: read at `0`, then toggled low 12 ms / high 150 ms, read back `1` — confirmed write |
| Wake GPIO (`gpio_sdio_device_wake`) | `14.4` | device tree gives the name/pin; DEVICE: read at `0` |
| Any register in the block (standard SDHCI layout: argument, command, response, present-state, software-reset, interrupt status/enable) | reads `0`, writes do not stick | DEVICE, repeatedly confirmed (clock-gate on, full SDHCI-style init sequence, raw write-then-readback) |

**Why writes don't stick**: a second, embedded ARM processor (Apple's "IOP") owns this block
directly and runs its own real-time OS; the application CPU only posts messages to it over a
shared command channel. This is the leading explanation, not yet a fully proven one — see the
[Wi-Fi/IOP architecture report](../research/2026-10-16-wifi-iop-architecture.md) for the full
chain of evidence and what is confirmed versus still a hypothesis. That report's own firmware
analysis finds NAND the dominant subsystem by debug-string count in the IOP's firmware image —
a scoped, string-count observation, not a claim that the coprocessor was designed for NAND or
that Wi-Fi is secondary to it; no evidence either way on original design intent exists.
