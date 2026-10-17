# Trognon

[![CI](https://github.com/SebE585/trognon/actions/workflows/ci.yml/badge.svg)](https://github.com/SebE585/trognon/actions/workflows/ci.yml)
[![Docs](https://github.com/SebE585/trognon/actions/workflows/pages.yml/badge.svg)](https://github.com/SebE585/trognon/actions/workflows/pages.yml)
[![License: mixed](https://img.shields.io/badge/license-GPL--2.0--only%20%2F%20MIT%20%2F%20CC--BY--SA--4.0-blue)](LICENSE)

Documentation site: **[sebe585.github.io/trognon](https://sebe585.github.io/trognon/)**. English and French, with diagrams and an evidence level on every claim.

STATUT: ACTIF, 2026-10-17 (scope: the 32-bit iPhones before the A5; first code running on an iPhone 3GS: on-screen console and exception handlers; see Documentation)

Open hardware enablement for the old, unsupported iPhones **before the A5**, the iPhone 3GS and the
iPhone 4, so that devices left in drawers can run code, and in time a free operating system, of
their owner's choosing. Work goes **chip by chip**: the iPhone 3GS first (code already runs on it),
then the iPhone 4, reusing what carries over. From the A5 on, others already do the work (Project
Cascadia, Hoolock, Asahi): Trognon follows them and contributes there instead of repeating it.

**The value is reuse.** Trognon turns an unsupported phone into a base to
build on: a GPS logger, an emulator, a kiosk, a Linux-like pocket computer, whatever its owner wants to make of it.
Not necessarily a phone again: a platform instead of e-waste.

> The core is what's left when you've finished the apple, and it grows again.

## Where each chip stands

Evidence tags: **DEVICE** observed on a real phone, **SOURCE** read in our notes or in OpenIBoot,
**HYPOTHESIS** a plan. Detail and overlaps: [`docs/en/06-chips.md`](docs/en/06-chips.md).

| Family | Boot | Our status | Overlap with the previous one |
|---|---|---|---|
| **iPhone 3GS** (S5L8920) | limera1n, host-direct | **DEVICE**: code runs, on-screen console, three exception types handled | the reference |
| **iPhone 4** (A4, S5L8930) | SHAtter per ipwndfu, host-direct (**SOURCE**) | not tried | same CPU family: method and CPU-level code should carry over (**HYPOTHESIS**) |
| A5 and later (iPhone 4S to X) | checkm8 (**SOURCE**) | followed, not tried | out of scope: an upstream exists (Cascadia, Hoolock, Asahi), see [Beyond the A4](docs/en/06-chips.md#beyond-the-a4-a5-and-later) |

What carries over between chips is the boot *method* and the CPU-level code. Addresses, the display
and the iBSS offsets are re-read on each chip, so the code is split to keep them in one small file per
chip: [`docs/en/07-code-layout.md`](docs/en/07-code-layout.md).

## Documentation

Same content as the [documentation site](https://sebe585.github.io/trognon/): reverse-engineering notes
with an evidence level on every claim (observed on the device, emulated, read in source, or hypothesis),
with diagrams: [English](docs/en/README.md), [Français](docs/fr/README.md).
Direction for what comes next (small self-contained *Mods* that grow a shared library instead
of writing an OS up front): [`docs/en/04-mods.md`](docs/en/04-mods.md).

## Origin

It started with an iPhone 4S turned into a 10 Hz GPS + IMU data logger, still
on iOS 9: [Ressusciter un iPhone 4S en 2025](https://roadsimulator3.fr/iphone-4s-datalogger-gps-imu-10hz/)
(in French). Trognon is the next step: the device's own OS should not be the
end of its life.

## Scope

Trognon is **OS-agnostic**. It is not "a Linux distro for iPhones". It is the
layer every free OS needs and none should have to rebuild:

1. **Boot chain**: getting a device from DFU to a payload we control
   (limera1n on the 3GS, SHAtter on the A4), reproducibly,
   from open tooling.
2. **Hardware enablement**: per-SoC, per-device descriptions and drivers
   (display, touch, storage, USB, Wi-Fi, battery, camera, and so on), upstreamed
   wherever an upstream exists (mainline Linux, BSDs).
3. **Support matrix**: a generated, dated table of what works on which
   device, so nobody re-discovers it.
4. **Targets**: Linux first because that is where prior work is; BSD and
   other systems welcome on equal terms.

Open innovation is the method: everything public, everything reproducible,
upstream first, no fork kept alive for its own sake.

## Hard constraints (known before writing a line)

- **Tethered boot.** The bootrom exploits (limera1n, SHAtter) do not persist across
  reboots. Every cold boot needs a host (or a dongle) to re-run it. Turning
  that into something a non-expert can live with is the central problem.
- **32-bit only.** The 3GS and the A4 (3GS, 4) are 32-bit ARMv7 and share our code. Later chips
  (A7 to A11, arm64) do not, and are followed upstream rather than ported here.
- **Batteries.** A ten-year-old Li-ion pack is often dead or swollen. Refurbishing
  is part of the project, not an afterthought.

## Device ownership

Trognon does not bypass, remove or interact with Activation Lock, and keeps no
registry of devices or owners. Replacing the OS does not make a locked device
usable as an iPhone again, so it adds no resale value to a stolen phone.

## Where Trognon fits (state of the art, 2026-10-13)

| Track | Existing work | Gap Trognon takes on |
|---|---|---|
| S5L8920 (3GS) / A4 (4), 32-bit | limera1n (2010), host-direct, no helper board. No modern OS port. | Boot-chain test devices only, not a port target. |
| A5 and later (4S to X) | Project Cascadia (A5/A6, 32-bit): mainline 6.12 on iPad mini 1 (S5L8942) with touch and Wi-Fi, since 2026. Hoolock, Asahi tree, postmarketOS (A7 to A11). | Out of scope: follow and contribute upstream. |
| Host-free boot | Prior art on later chips only: checkm8-a5 on a Raspberry Pi Pico (A5), Pico DFU automation (A11). Nothing for limera1n or SHAtter; no unattended dongle published. | An always-attached boot dongle, the central open problem. |

Details, dates and sources: [`docs/research/`](docs/research/).

## Firmware

Trognon never hosts Apple firmware. Blobs (e.g. Wi-Fi) are extracted at install
time from the owner's own device or IPSW, as Asahi Linux does.

## Reprise

- **Run:** `make test test-sanitize` for the host tool; payloads in `tools/payloads/` (`make trace.bin`);
  on a device, `tools/boot.sh` then `tools/send.sh tools/payloads/trace.bin` (pinned ipwndfu, see
  [`tools/ipwndfu/README.md`](tools/ipwndfu/README.md) and [`docs/en/01-boot-chain.md`](docs/en/01-boot-chain.md)).
  Diagrams: `make diagrams` (maintainer-side, SVGs are committed).
- **Breaks first:** the exploit (`ipwndfu -p`) sometimes needs several tries, and a device idle in Recovery for a few
  minutes drops out. A run counts only if `tools/send.sh` prints `NONCE CONFIRMED`, and it then leaves an evidence
  bundle under `evidence/` ([how to read it](docs/en/08-evidence.md)). See
  [`docs/en/05-field-notes.md`](docs/en/05-field-notes.md).
- **Know before touching:** the research notes are dated; anything older than
  six months on boot chains or mainline support must be re-checked.

## License

Per component, see [`LICENSE`](LICENSE): GPL-2.0-only for kernel-facing code,
MIT for tooling and dongle firmware, CC-BY-SA-4.0 for documentation.

Trognon is an independent project, not affiliated with or endorsed by Apple
Inc. "iPhone" is a trademark of Apple Inc., used here only to identify
compatible hardware.
