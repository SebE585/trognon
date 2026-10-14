# 7. Modular code layout

**Status: being reworked.** The payloads started as one file, `tools/payloads/trace.c`, with the
3GS addresses written inline. That does not survive a second chip, so the code is being split. This page
describes the **target** layout; check the tree before relying on a path.

![Code layers, from hardware to Mods](../img/code-layers.svg)

## The layers

| Layer | Path | Holds | Does not hold |
|---|---|---|---|
| **HAL** | `tools/payloads/hal/` | console (text on screen), `exc` (exception handlers), `vic` (interrupts). Portable ARMv7 / Cortex-A8 code. | **any address** |
| **SoC constants** | `tools/payloads/soc/s5l8920.h` | the 3GS addresses, sizes and masks, each line carrying its evidence tag | logic |
| | `tools/payloads/soc/s5l8930.h` | the A4 constants, **SOURCE only** (read in OpenIBoot, never on a phone) | logic |
| **Selector** | `tools/payloads/soc.h` | picks the header from `-DSOC=...` | |
| **Payload** | `tools/payloads/trace.c` | the bring-up image: assembles HAL and SoC constants | chip-specific numbers |

The HAL asks `soc.h` for things like "where is the framebuffer" and "where is the interrupt controller".
It never writes a number itself, so the same HAL code builds for any chip that has a constants file.

## The rules

1. **A constant enters `soc/<chip>.h` with an evidence tag.** `DEVICE` only if it was read on a phone
   (see the [index](README.md)); otherwise `SOURCE` or `HYPOTHESIS`.
2. **An unverified header does not build by default.** `s5l8930.h` holds only **SOURCE** values, so
   building for it is refused unless you pass `ALLOW_UNVERIFIED=1`. A silent success on an untested
   chip is the failure mode this project fears most
   ([field notes](05-field-notes.md)).
3. **A new chip means a new `soc/<chip>.h`, re-verified constant by constant on the device. It does not
   mean a rewrite.** If the HAL needs a chip-specific `if`, that is a missing constant, not a new code path.
4. **Anything that is not an address but still differs per chip** (the iBSS patch offsets, the `env_set`
   address) belongs in the constants file too, not in the HAL.

## What this buys

- The [iPhone 4 bench](06-chips.md) becomes "write `s5l8930.h`, build with `ALLOW_UNVERIFIED=1`, read
  the screen". If a console appears with nothing else changed, the split is validated.
- [Mods](04-mods.md) stay portable: they call the HAL and never see an address, so one Mod can run on
  any chip that has a verified constants file.
- Reviewing a chip port is reading one small header.

## Build

```sh
cd tools/payloads
make trace.bin                           # default chip: the 3GS
make trace.bin SOC=s5l8930 ALLOW_UNVERIFIED=1   # target form, A4: unverified, expect to fix constants
```

The exact variable names are those of the target layout and may change while the split lands.
