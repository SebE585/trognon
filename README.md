# Trognon

**📖 Documentation site: [sebe585.github.io/trognon](https://sebe585.github.io/trognon/)** · English / Français, with diagrams and an evidence level on every claim.

STATUT: ACTIF — 2026-10-15 (first code running on an iPhone 3GS: on-screen console and exception handlers; see Documentation)

Open hardware enablement for unsupported Apple phones, A5 to A11
(iPhone 4S → iPhone X), so that devices left in drawers can run a free
operating system of their owner's choosing.

**The value is reuse.** Trognon turns an unsupported phone into a base to
build on: a GPS logger, an emulator, a kiosk, a Linux-like pocket computer —
whatever its owner wants to make of it. Not necessarily a phone again; a
platform, instead of e-waste.

> The core is what's left when you've finished the apple — and it grows again.

## Documentation

Same content as the [documentation site](https://sebe585.github.io/trognon/): reverse-engineering notes with an evidence level on every claim (observed on the device,
emulated, read in source, or hypothesis), with diagrams:
[English](docs/en/README.md) · [Français](docs/fr/README.md).
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

1. **Boot chain** — getting a device from DFU to a payload we control
   (checkm8 bootrom exploit, A5–A11), reproducibly, from open tooling.
2. **Hardware enablement** — per-SoC, per-device descriptions and drivers
   (display, touch, storage, USB, Wi-Fi, battery, camera…), upstreamed
   wherever an upstream exists (mainline Linux, BSDs).
3. **Support matrix** — a generated, dated table of what works on which
   device, so nobody re-discovers it.
4. **Targets** — Linux first because that is where prior work is; BSD and
   other systems welcome on equal terms.

Open innovation is the method: everything public, everything reproducible,
upstream first, no fork kept alive for its own sake.

## Hard constraints (known before writing a line)

- **Tethered boot.** checkm8 lives in the bootrom and does not persist across
  reboots. Every cold boot needs a host (or a dongle) to re-run it. Turning
  that into something a non-expert can live with is the central problem.
- **32-bit vs 64-bit.** A5/A6 (4S, 5, 5c) are 32-bit ARMv7 with 512 MB–1 GB
  RAM; A7–A11 (5s → X) are arm64. Two very different tracks.
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
| A7–A11 (5s → X), arm64 | Device trees in mainline since 6.13; Hoolock Linux downstream; postmarketOS `apple-idevice`. Framebuffer, buttons, brightness, NVMe on A11. | Contribute upstream, don't fork. Nobody works on touch, Wi-Fi, audio, charging. |
| A5/A6 (4S, 5, 5c), 32-bit | Project Cascadia: mainline 6.12 on iPad mini 1 (S5L8942) with touch and Wi-Fi, since 2026. Nothing for phones. | Port to S5L8940 (iPhone 4S, iPad 2), then A6. |
| Host-free boot | checkm8-a5 on a Raspberry Pi Pico (A5); Pico DFU automation (A11). No unattended dongle published. | An always-attached boot dongle — the central open problem. |

Details, dates and sources: [`docs/research/`](docs/research/).

## Firmware

Trognon never hosts Apple firmware. Blobs (e.g. Wi-Fi) are extracted at install
time from the owner's own device or IPSW, as Asahi Linux does.

## Reprise

- **Run:** payloads in `tools/payloads/` (`make trace.bin`) and `tools/run_payload.py`; the boot
  recipe is in [`docs/en/01-boot-chain.md`](docs/en/01-boot-chain.md). Diagrams: `make diagrams`
  (maintainer-side, SVGs are committed).
- **Breaks first:** USB sends fail about one time in three and a stale `getenv` marker looks like
  success: read [`docs/en/05-field-notes.md`](docs/en/05-field-notes.md) first.
- **Know before touching:** the research notes are dated; anything older than
  six months on boot chains or mainline support must be re-checked.

## License

Per component, see [`LICENSE`](LICENSE): GPL-2.0-only for kernel-facing code,
MIT for tooling and dongle firmware, CC-BY-SA-4.0 for documentation.

Trognon is an independent project, not affiliated with or endorsed by Apple
Inc. "iPhone" is a trademark of Apple Inc., used here only to identify
compatible hardware.
