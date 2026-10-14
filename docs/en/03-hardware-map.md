# 3. Hardware map: what is verified

Register addresses come from OpenIBoot (`plat-s5l8920`). They are **SOURCE** until read
on the phone. This page lists what we checked. Reads are read-only; every load is
armed as an expected data abort so an unmapped register prints `ABORT` instead of
resetting the phone.

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
- `SOFTINT` is readable, so the register exists. Reading 0 does not prove that writing it raises an
  interrupt: that is the next test (**HYPOTHESIS**).
- The iBSS handlers sit at `0x8400F9EC` (IRQ vector word `0x38`) and `0x8400FA34` (FIQ vector word `0x3C`).
- Which peripheral each enabled bit is stays unknown.

Timer registers are the least understood part of OpenIBoot (its own comment says they are copied from an older SoC).
