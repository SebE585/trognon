# 7. Modular code layout

**Status: done for the 3GS payloads (2026-10-15).** They started as one file, `tools/payloads/trace.c`,
with the 3GS addresses written inline, which does not survive a second chip. The code is now split into
`hal/` (portable), `soc/` (per-chip constants) and `trace.c` (the payload). The regression check was an
emulated render of every stage, identical before and after the split (EMU), then `trace.bin` built from
the split code was run on the phone and showed the same screen as before (**DEVICE**, 2026-10-15). The A4 header exists but is deliberately unusable until verified.

![Code layers, from hardware to Mods](../img/code-layers.svg)

## The layers

| Layer | Path | Holds | Does not hold |
|---|---|---|---|
| **HAL** | `tools/payloads/hal/` | console (text on screen), `exc` (exception handlers), `vic` (interrupts), `result` (values published to the host), `gpio.h` and `clock.h` (pin and clock-gate word accessors), `i2c_gpio` (bit-banged I2C and PMU register access). Portable ARMv7 / Cortex-A8 code. | **any address** |
| **SoC constants** | `tools/payloads/soc/s5l8920.h` | the 3GS addresses, sizes and masks, each line carrying its evidence tag | logic |
| | `tools/payloads/soc/s5l8930.h` | the A4 constants, **SOURCE only** (read in OpenIBoot, never on a phone) | logic |
| **Selector** | `tools/payloads/soc.h` | picks the header from `-DSOC_<NAME>` (`make SOC=s5l8920`) | |
| **Payload** | `tools/payloads/trace.c` (entry and shared steps) and `tools/payloads/stages/NN_name.inc` (one file per experiment) | the bring-up image: assembles HAL and SoC constants | chip-specific numbers |

The HAL asks `soc.h` for things like "where is the framebuffer" and "where is the interrupt controller".
It never writes a number itself, so the same HAL code **builds** for any chip that has a constants file. Building is
not behaving the same: see the next section.

## Four kinds of knowledge, four places

A difference between chips is not always a constant, and a behaviour seen on the 3GS is not a property of the
architecture. The code keeps four kinds of knowledge apart, because they have different lifetimes and different proof:

| Level | What it is | Lives in today | Carries to another chip? |
|---|---|---|---|
| **ARM core** | exceptions and vectors, caches, barriers, CPU identification | `hal/entry.c`, `hal/exc.c` | yes, for the same core family; **proven only on the 3GS's Cortex-A8** |
| **SoC** | controllers and their registers: interrupt controller, GPIO block, clock gates, timer, I2C and SPI controllers | `soc/<chip>.h` and the HAL that only reads `SOC_*` (`gpio.h`, `clock.h`, `vic.c`, `i2c_gpio.c`) | a new header, re-verified; a controller that behaves differently needs code, not only a constant |
| **Board** | wiring and power of one product: which pin is the touch reset, the PMU's I2C address, which gate feeds which part | `soc/<chip>.h` **for now, mixed with the SoC** (to be split when a second board exists) | no: it is a fact about one phone model |
| **Experiment** | one sequence to run and prove: preconditions, reads, writes, restore, evidence | `tools/payloads/stages/NN_name.inc` (one `STAGE`), and the bundle it produces | no: it is dated and tied to its evidence |

Rules that follow:

- **"Portable" means the code builds for another chip, not that the behaviour is confirmed there.** Every SoC and board
  fact keeps its evidence tag and says which device it was seen on. The HAL claims nothing about a chip nobody ran it on.
- **A constant is not always enough.** If two chips differ in how a controller works (not where it is), that is a new driver
  or a clearly named variant, not a hidden `if` and not a number.
- **Do not put board wiring in the HAL.** The PMU address `0x74` and the accelerometer's pins are facts about the iPhone 3GS
  board (N88). They sit in `soc/s5l8920.h` today only because there is one board; the separate board file arrives with the
  second one, so as not to guess a boundary early.

## The rules

1. **A constant enters `soc/<chip>.h` with an evidence tag.** `DEVICE` only if it was read on a phone
   (see the [index](README.md)); otherwise `SOURCE` or `HYPOTHESIS`.
2. **An unverified header does not build by default.** `s5l8930.h` holds only **SOURCE** values, so
   building for it is refused unless you pass `ALLOW_UNVERIFIED=1`. A silent success on an untested
   chip is the failure mode this project fears most
   ([field notes](05-field-notes.md)).
3. **A new chip means a new `soc/<chip>.h`, re-verified constant by constant on the device. It does not
   mean a rewrite.** If the HAL needs a chip-specific `if`, first ask whether it is a missing constant; if the
   controller itself behaves differently, it is a new driver or a named variant (see the four levels above).
4. **Anything that is not an address but still differs per chip** (the iBSS patch offsets, the `env_set`
   address) belongs in the constants file too, not in the HAL.

## What this buys

- The [iPhone 4 bench](06-chips.md) becomes "write `s5l8930.h`, build with `ALLOW_UNVERIFIED=1`, read
  the screen". If a console appears with nothing else changed, the split is validated.
- [Mods](04-mods.md) stay portable at the source level: they call the HAL and never see an address, so one Mod
  **builds** for any chip that has a constants file. That it **behaves** the same is only established for the chips where it was run.
- Reviewing a chip port is reading one small header.

## Build

```sh
cd tools/payloads
make trace.bin                           # default chip: the 3GS
make trace.bin SOC=s5l8930 ALLOW_UNVERIFIED=1   # target form, A4: unverified, expect to fix constants
```

The build of an unverified chip stops with an `#error`; with `ALLOW_UNVERIFIED=1` it then fails on every
constant still to find (`SOC_SCRATCH`, `SOC_ENV_SET`...), which lists what is left to verify.

## Checking a refactor of the HAL

`make -C tools/payloads sums` builds every stage and prints one sha256 per binary. Run it before and after a change
that is meant to keep the behaviour, and diff the two outputs: the stages the change does not reach must stay
byte-identical, and the ones it does reach must be run on the phone. The 2026-10-20 split of the GPIO, clock-gate
and I2C code out of `trace.c` was checked this way (40 of 49 binaries identical; the 9 that use I2C or the PMU
still to be run on a device). **Status: builds, not yet run on the device for those 9.**

## One file per experiment (`stages/`)

Until 2026-10-21 every experiment was a `#if STAGE == N` block inside the one function `payload_main()` of `trace.c`, which made a single
experiment hard to review. Each block now lives in `tools/payloads/stages/NN_name.inc`, named after the binary it builds
(`47_gate_survey.inc` builds `trace_gate_survey.bin`), and `trace.c` includes it at the same place. The first lines of each file give the stage
number and the binaries. The Makefile rebuilds a payload when any `stages/*.inc` changes, so an edited experiment is never built from a stale object.

This step is deliberately mechanical. The compiler sees the same text as before, and `make -C tools/payloads sums` gives the **same 49 sha256**
before and after: the split changed no binary, so nothing needs to be run on a phone because of it. The binary names are unchanged, so the
commands and the evidence bundles that name them still hold.

What it does **not** do yet: the files are still blocks of one function and share its local variables, so they are not independent units. The next
step turns each into a function of its own (preconditions, reads, writes, restore, evidence reference) behind a common entry that initialises the
console, calls it and publishes the result. That step does change the binaries, so it is done one experiment at a time and run on the phone.

