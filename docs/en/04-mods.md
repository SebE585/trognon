# 4. Mods: building the OS one use case at a time

**Status: design proposal (HYPOTHESIS). Nothing here is built yet.**

## The idea

Instead of writing an operating system first, write small self-contained programs,
**Mods**, each one useful on its own. Every Mod needs some capabilities (draw text, read a
button, keep time...). Each time a Mod needs one that does not exist, we write that
driver, once, into a shared library. After a few Mods the library *is* the OS, and it
was shaped by real use instead of guessed up front.

This fits the constraints we found:

- Boot is **tethered**: every run already sends a RAM payload over USB. A Mod is
  exactly that unit. No installer; the aim is to write nothing to the phone's storage.
- Every driver is proven on the device before the next one (the method of the
  [exception console](02-exception-console.md)).
- It stays OS-agnostic: Mods are one track; booting Linux or a BSD on A5+ stays
  another, and both sit on the same boot chain and hardware map.

## Capability ladder

![Capability ladder](../img/capability-ladder.svg)

![Mods on a shared library, on the boot chain](../img/mods-layers.svg)

## Candidate Mods

| Mod | Needs | Why it is a good test |
|---|---|---|
| **Gamebook reader** (Markdown, choose-your-own-adventure) | L1, L4, L5 | Lowest bar: text, two buttons, content pushed over USB. First useful Mod. |
| **Barcode-Battler-style game** | L1, L4, L5 | Same stack, adds game logic. One example among others, not the project goal. |
| **Atari ST emulator** | L1, L3 (timing), L4, L5, L6 | Stresses CPU, display and timing. A 600 MHz class core is plausible for an 8 MHz 68000 (**HYPOTHESIS**). |
| **Compass / motion** | L1, L6 (I2C sensors) | Needs the 3GS magnetometer and accelerometer wiring (**to verify**, not read yet). |
| **GPS gadget** | L1, L6 | On this generation the GPS is likely behind the baseband: possibly very hard (**to verify**). |

## Limits

- A Mod is not resident: it is sent each boot. A field gadget (compass in a pocket)
  needs untethered boot, an open question ([research note](../research/2026-10-13-untethered-boot.md)).
- One Mod at a time, no isolation, no multitasking: a unikernel-style library, not a
  general OS. That is fine until a Mod needs more.
- Touch and radios are the expensive rungs. The ladder is ordered so useful Mods
  exist before they are needed.

## Portability between chips

A Mod calls the shared library and never sees an address. Addresses live in one constants file per
chip (`soc/<chip>.h`, see [code layout](07-code-layout.md)), so a Mod runs on any chip that has a
verified file, and nowhere else (**HYPOTHESIS**, nothing proves it yet on a second chip).

## Rule for adding a driver

A driver enters the shared library only with (1) the register addresses read on the
device, (2) a Mod that uses it, (3) the evidence tag from the [index](README.md).
