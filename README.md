# Trognon

[![CI](https://github.com/SebE585/trognon/actions/workflows/ci.yml/badge.svg)](https://github.com/SebE585/trognon/actions/workflows/ci.yml)
[![Docs](https://github.com/SebE585/trognon/actions/workflows/pages.yml/badge.svg)](https://github.com/SebE585/trognon/actions/workflows/pages.yml)
[![License: mixed](https://img.shields.io/badge/license-GPL--2.0--only%20%2F%20MIT%20%2F%20CC--BY--SA--4.0-blue)](LICENSE)

Documentation site: **[sebe585.github.io/trognon](https://sebe585.github.io/trognon/)**. English and French, with diagrams and an evidence level on every claim.

STATUT: ACTIF, 2026-10-21 (status: active, early. The iPhone 3GS runs our code; no operating system yet. See "What works today")

## Goal

Open hardware enablement for the old, unsupported iPhones **before the A5**, the iPhone 3GS and the
iPhone 4, so that devices left in drawers can run code, and in time a free operating system, of
their owner's choosing. Work goes **chip by chip**: the iPhone 3GS first (code already runs on it),
then the iPhone 4, reusing what carries over. From the A5 on, others already do the work (Project
Cascadia, Hoolock, Asahi): Trognon follows them and contributes there instead of repeating it.

**The value is reuse.** Trognon turns an unsupported phone into a base to
build on: a GPS logger, an emulator, a kiosk, a Linux-like pocket computer, whatever its owner wants to make of it.
Not necessarily a phone again: a platform instead of e-waste.

> The core is what's left when you've finished the apple, and it grows again.

## What works today

Every line below is backed by an evidence bundle under [`evidence/`](evidence/) (how to read one:
[`docs/en/08-evidence.md`](docs/en/08-evidence.md)). Evidence tags: **DEVICE** observed on a real phone, **SOURCE**
read in OpenIBoot or in public notes, **HYPOTHESIS** a plan.

- **Our own code runs on an iPhone 3GS** (**DEVICE**): from DFU, through limera1n and a patched iBSS held in RAM,
  to a payload we wrote. A run counts only if the payload hands back a fresh random nonce (`NONCE CONFIRMED`).
- **On-screen console and exception handlers** (**DEVICE**): undefined instruction, data abort, prefetch abort and
  IRQ are caught and printed on the phone's screen. FIQ and SWI are not tested yet.
- **Sensors and SoC blocks read, read-only** (**DEVICE**): the accelerometer answers over bit-banged I2C and its
  axes and scale are measured; the SoC clock gates are surveyed (the full table is in the evidence bundles).

## What does not work yet

- **No operating system, no application platform.** A booted phone shows a console, nothing more.
- **Touch, Wi-Fi, Bluetooth, GPS, flash storage and audio are not usable.** Each is an open question with a
  known next step, tracked in the [bestiary](docs/en/bestiary.md).
- **Tethered boot.** The bootrom exploit does not persist: every cold boot needs a host (or a dongle) to re-run it.
- **Host tooling is developed on macOS (Apple silicon).** Other hosts are untested.
- **One chip only.** The iPhone 4 (A4) is not tried yet.

## Try it

Without a phone (needs a C compiler; `arm-none-eabi-gcc` for the payloads):

```sh
make test test-sanitize                  # host tool tests
make -C tools/payloads all-stages        # build every payload
make -C tools/payloads sums              # one sha256 per binary, to diff before and after a change
```

With an iPhone 3GS in DFU mode (and no other Apple device in DFU: the tools refuse to guess), after setting up the
pinned ipwndfu ([`tools/ipwndfu/README.md`](tools/ipwndfu/README.md)):

```sh
tools/go.sh tools/payloads/trace.bin     # pwn, boot, send, check the nonce, write an evidence bundle
```

If the exploit fails, `go.sh` retries a few times, then asks you to put the phone back in DFU.

## Contribute without a phone

- **Check a claim.** Pick a bundle in [`evidence/`](evidence/) and run
  `python3 tools/rebuild_bundle.py evidence/<bundle>`: it rebuilds the payload from the recorded commit and compares
  its SHA-256 with the manifest (needs `arm-none-eabi-gcc`, never touches a phone). A mismatch is a result worth
  reporting. Results so far: [Verifications](docs/en/verifications.md); how bundles work:
  [`docs/en/08-evidence.md`](docs/en/08-evidence.md).
- **Read and challenge the research.** Notes in [`docs/research/`](docs/research/) are dated and carry sources; a
  refutation with its source is as useful as a confirmation.
- **Pick a question.** The [bestiary](docs/en/bestiary.md) and the [missions](docs/en/missions.md) list the open
  hardware questions with what each would need.
- Rules for contributions: [`CONTRIBUTING.md`](CONTRIBUTING.md).

## Where each chip stands

Detail and overlaps: [`docs/en/06-chips.md`](docs/en/06-chips.md).

| Family | Boot | Our status | Overlap with the previous one |
|---|---|---|---|
| **iPhone 3GS** (S5L8920) | limera1n, host-direct | **DEVICE**: code runs, on-screen console, exception handlers, accelerometer read | the reference |
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
- **32-bit only.** The 3GS (S5L8920) and the iPhone 4 (A4) are 32-bit ARMv7 and share our code. Later chips
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
| S5L8920 (3GS) / A4 (4), 32-bit | limera1n (2010), SHAtter (A4), host-direct, no helper board. No modern OS port. | Our two targets: the boot chain and hardware enablement, then an OS-agnostic base. |
| A5 and later (4S to X) | Project Cascadia (A5/A6, 32-bit): mainline 6.12 on iPad mini 1 (S5L8942) with touch and Wi-Fi, since 2026. Hoolock, Asahi tree, postmarketOS (A7 to A11). | Out of scope: follow and contribute upstream. |
| Host-free boot | Prior art on later chips only: checkm8-a5 on a Raspberry Pi Pico (A5), Pico DFU automation (A11). Nothing for limera1n or SHAtter; no unattended dongle published. | An always-attached boot dongle, the central open problem. |

Details, dates and sources: [`docs/research/`](docs/research/).

## Firmware

Trognon never hosts Apple firmware. Blobs (e.g. Wi-Fi) are extracted at install
time from the owner's own device or IPSW, as Asahi Linux does.

## Resuming work (Reprise)

- **Run:** `make test test-sanitize` for the host tool; payloads in `tools/payloads/` (`make all-stages`);
  on a device, `tools/go.sh tools/payloads/trace.bin` (pinned ipwndfu, see
  [`tools/ipwndfu/README.md`](tools/ipwndfu/README.md) and [`docs/en/01-boot-chain.md`](docs/en/01-boot-chain.md)).
  `tools/boot.sh` then `tools/send.sh PAYLOAD --evidence evidence` are the same steps one at a time; without
  `--evidence`, `send.sh` checks the nonce but writes no bundle. Diagrams: `make diagrams` (maintainer-side, SVGs are committed).
- **Breaks first:** the exploit (`ipwndfu -p`) sometimes needs several tries, and a device idle in Recovery for a few
  minutes drops out. A run counts only if the tools print `NONCE CONFIRMED`; `go.sh` then leaves an evidence
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
