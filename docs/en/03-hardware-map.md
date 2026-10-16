# 3. Hardware map: what is verified

![SoC map: what each block answers](../img/soc-map.svg)

[Open the diagram full-size](../img/soc-map.svg) — addresses and subtitles are small in an
inline render.

Base addresses alone, with no narrative, are kept in the stabilised
[SoC memory map](../spec/soc-s5l8920.md) — look there for a quick address lookup; this
page is where the reasoning and the evidence for each one live.

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

Chip ID read: reset + READ ID command (control writes, but no program and no erase) across
all 16 chip-select lines on bus 0, then its geometry looked up in OpeniBoot's own hardcoded
table.

| Item | Value | Meaning |
|---|---|---|
| CE0, CE1 | `98 DE 95 32 7A 54 0C 00` | Toshiba (JEDEC manufacturer `0x98`); the same ID on both, cross-check |
| CE2 to CE15 | `00 00 00 00` | no exploitable identification response under these conditions |
| Geometry (OpeniBoot table, `nand_chip_info[]`, exact ID match) | `blocks_per_ce=0x2068`, `pages_per_block=0x80`, `bytes_per_page=0x2000`, `banks_per_ce=2` | SOURCE match, not read from the device |
| Capacity, computed | 8296 x 128 x 8192 x 2 banks x 2 CE = 34.8 GB raw | matches the 32 GB label on this unit after the usual ECC/spare margin (~8%) |

Bus 1 (`H2FMI1_BASE = 0x81300000`) not swept yet: same ID-read-only approach, no program and
no erase, this project's NOR-equivalent caution (`tools/boot.sh` never calls `flash_nor()`)
applies here too, with a larger margin, since NAND holds the device's own iOS image.

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

## Accelerometer, i2c0 address `0x1d` (DEVICE, 2026-10-16)

**Status**: DEVICE, first try, exact chip model still open. Everything below came from this
device's own decrypted device tree, then a real bit-banged I2C transaction over GPIO.

**What's confirmed**: the device tree names a bus (`i2c0`, `compatible = "i2c,s5l8920x",
"iic,soft"`: software, bit-banged, not a dedicated hardware controller) with SDA/SCL on
GPIO `04.01`/`04.00`, and an accelerometer on it (`compatible =
"accelerometer,lis331dl","accelerometer,lis302dl"`) at I2C address `0x1d`. A full I2C
transaction (START, address+write, register `0x0F`, repeated START, address+read, one
byte, STOP) got a real answer: all three software ACK flags read `1` (each slave-address
and register byte was acknowledged by the device), and the byte read back was `0x3B`. That
is the documented `WHO_AM_I` value for the LIS331DL — but ST's LIS302DL datasheet (table 16)
also lists `0x3B` as a valid value for some revisions of that part, so the read confirms a
working transaction and an identifier consistent with both compatible strings in the device
tree, not which of the two chips is actually on the board.

| Item | Value | Status |
|---|---|---|
| Bus | `i2c0`, SDA `04.01`, SCL `04.00` | device tree gives the name/pins; DEVICE: both read idle-high before the transaction |
| I2C address | `0x1d` | device tree `reg` property |
| `WHO_AM_I` (register `0x0F`) | `0x3B` | DEVICE: full I2C read, all three software ACK flags `1` |

**What remains hypothesis**: which of the two compatible-string chips (LIS331DL vs LIS302DL)
is actually present, reading actual axis data (the three accelerations), and the
`orientation` device-tree property's exact meaning. A compass (AKM8973S, address `0x1e`)
and a second `compass1` (`0x1f`, no `interrupts` property) share the same bus, device tree
only, not probed at all yet — two device-tree nodes do not by themselves prove two separate
physical chips.

**How to reproduce**: `make trace_accel_i2c.bin` (see `tools/payloads/`, STAGE 29). Reads
the bus idle level first and only attempts the transaction if it looks plausibly
pulled-up, so a mis-wired guess cannot look like a silent success.

**Links to evidence**: [evidence bundle `20261016T101710Z-trace_accel_i2c`](https://github.com/SebE585/trognon/tree/main/evidence/20261016T101710Z-trace_accel_i2c), indexed on the [evidence page](08-evidence.md).

## Wi-Fi/SDIO, `SDIO0_BASE = 0x80000000` (mixed DEVICE/HYPOTHESIS, 2026-10-15/16)

**Status**: mixed DEVICE/HYPOTHESIS. No OpeniBoot source exists for this block on the
S5L8920 at all (unlike NAND). Everything below the address itself came from this device's
own decrypted device tree and kernelcache (owner's IPSW, never redistributed), not from
source code.

**What's confirmed**: the block's base address; that every probed offset reads `0` and that
re-reads never reflect a value written to them; that the reset and wake GPIO lines are real
and one of them is writable. **The internal register layout itself is still open** — two
incompatible candidates exist upstream (`plat-s5l8900`'s simple CTRL/CMD/ARGU/STATE/DSTA
layout, and `plat-a4`'s SDHCI-standard layout), and only the first has actually been tried
on this device; the all-zero/non-sticky result holds either way because the raw
write-then-readback test (STAGE 24) writes straight to the base address and does not depend
on either layout.

| Item | Value | Status |
|---|---|---|
| Base address, size | `0x80000000`, `0x1000` | from this device's own decrypted device tree (owner's IPSW), not a register read |
| `compatible` | `"sdio,s5l8920x"` | device tree, same caveat |
| `clock-gates` | `0x10` | device tree value; DEVICE: enabling it alone did not make any register respond |
| `interrupts` | `0x22` | device tree, same caveat |
| Reset GPIO (`gpio_baseband_wlan_reset`) | `16.5` | device tree gives the name/pin; DEVICE: read at `0`, then toggled low 12 ms / high 150 ms, read back `1` — confirmed write |
| Wake GPIO (`gpio_sdio_device_wake`) | `14.4` | device tree gives the name/pin; DEVICE: read at `0` |
| Any probed register in the block, tried under the legacy `plat-s5l8900`-style layout (STAGE 22) and as a raw write straight to the base address (STAGE 24) — the SDHCI-standard layout was never actually tried | reads `0`, re-reads don't reflect values written | DEVICE, repeatedly confirmed (clock-gate on, legacy-layout init sequence, raw write-then-readback) |

**What remains hypothesis**: *why* writes don't stick, and which register layout (if either
of the two candidates above) actually matches this SoC. The leading explanation for the
non-sticky writes is that a second, embedded ARM processor (Apple's "IOP") owns this block
directly and runs its own real-time OS, with the application CPU only posting messages to
it over a shared command channel — not yet fully proven.

**How to reproduce**: `make trace_sdio_init.bin` (see `tools/payloads/`, STAGE 22) runs the
clock-gate enable and the legacy `plat-s5l8900`-style init sequence — not an SDHCI-standard
sequence. The raw write-then-readback on the base address is a separate build (STAGE 24).

**Links to evidence**: see the
[Wi-Fi/IOP architecture report](../research/2026-10-16-wifi-iop-architecture.md) for the full
chain of evidence and what is confirmed versus still a hypothesis. That report's own firmware
analysis finds NAND the dominant subsystem by debug-string count in the IOP's firmware image —
a scoped, string-count observation, not a claim that the coprocessor was designed for NAND or
that Wi-Fi is secondary to it; no evidence either way on original design intent exists.

## Bluetooth, UART3, reset `15.06` / wake `16.02` (DEVICE, 2026-10-16)

**Status**: DEVICE, read-only GPIO levels. No UART traffic attempted yet.

**What's confirmed**: the device tree names a `bluetooth` node under `uart3`
(`compatible = "bluetooth,n88"`, `product-id = 0x1294`, `vendor-id = 0x5ac`,
`transport-speed = 0x2dc6c0` — 3,000,000 baud, `coex = 1`): this confirms the transport is
UART, not SDIO, and that this device tree expects Wi-Fi/Bluetooth coexistence signalling.
Reading the reset (`15.06`) and wake (`16.02`) lines, decoded the same `8*port+pin` way as
the already-proven Wi-Fi reset/wake pair, gave a real result: reset reads `1` (released, not
held low), wake reads `0`.

| Item | Value | Status |
|---|---|---|
| Transport | UART3, 3,000,000 baud | device tree `transport-speed`, not yet exercised |
| Reset GPIO (`function-bt_reset`) | `15.06` | device tree gives the name/pin (same physical line as the baseband node's own `function-bt_reset`); DEVICE: read `1` |
| Wake GPIO (`function-bt_wake`) | `16.02` | device tree gives the name/pin; DEVICE: read `0` |

**What remains hypothesis**: everything about the UART link itself — whether the chip
answers at all, any HCI handshake, whether toggling reset or wake changes anything. This is
a read-only GPIO level check, nothing more.

**How to reproduce**: `make trace_bt_gpio.bin` (see `tools/payloads/`, STAGE 30). Same
pattern as the Wi-Fi reset/wake read (STAGE 15): plain reads, no write, no UART touched.

**Links to evidence**: [evidence bundle `20261016T124101Z-trace_bt_gpio`](https://github.com/SebE585/trognon/tree/main/evidence/20261016T124101Z-trace_bt_gpio), indexed on the [evidence page](08-evidence.md).
