# Use case: an old iPhone in a LoRa field probe

Status as of 2026-10-21: **a proposal, nothing of it is built or proven.** This page records a
reuse scenario so that its limits are written down before anyone relies on it. It claims no
capability: every function below is either not available under Trognon today, or untested on the
iPhone 4 (A4).

## The idea

Reuse a pre-A5 iPhone as the local interface and logbook of a small battery-powered probe that
sends LoRa frames from known positions to a fixed receiving station, so that radio coverage and
the variability of the channel can be recorded at surveyed points and analysed offline.

## What the phone is not

- It has **no LoRa radio**. Attaching an antenna adds nothing: a radio board, a microcontroller
  (MCU) to drive it, power and a wired antenna are all needed.
- Its GPS is not an RTK receiver. RTK corrections must reach a receiver built for them, and that
  receiver's antenna position must be related to the radio's.

## What Trognon gives today, and what it does not

See the [README](https://github.com/SebE585/trognon#readme) for the evidence behind each line.

| Function a probe would use | State under Trognon |
|---|---|
| Code running, serial console, exception handling | DEVICE on the 3GS; the iPhone 4 boot chain is not tried |
| Accelerometer | DEVICE on the 3GS; nothing transfers automatically to the A4 |
| Wi-Fi, GPS, Bluetooth, flash storage, touch | not available |
| Restart without a host | not available (captive boot) |

A proof obtained on the 3GS is not a proof on the iPhone 4.

## Architecture that does not depend on the phone

The MCU owns the radio and a minimal log, and keeps working if the phone hangs. The phone is an
optional layer, qualified separately, so the same module can be compared with and without it.
This also means the probe is first a **plain MCU probe**: it only counts as a reuse of the phone
once the phone's contribution is verified on the device.

Open links to settle, each with a minimal proof: the 30-pin dock to the MCU (pinout, host and
device roles, voltage levels, driver: a charging cable is not enough), Bluetooth 2.1 + EDR (no
direct BLE peripheral), and export of the log after a controlled restart.

## Acceptance of one unit

A unit is accepted when one can identify its software, receive a fresh frame, find that frame in
the exported file, check order and gaps, and re-read the file after a controlled reboot. A
screen counter or an LED is not proof.

## Stages

1. Triage each phone with `trognon probe` (CPID `8930` = A4), record model, screen, battery,
   lock state; count the usable units.
2. Bring up the MCU probe without a phone.
3. Port the boot payload to the A4 with a proof bundle on a real iPhone 4.
4. Qualify one phone-to-module link and an export.
5. Measure energy and endurance with and without the phone.

If stages 3 or 4 fail, the radio work keeps its value and the phone's role stays unproven. If the
battery, storage or restart behaviour cost more than the phone brings, keep a single phone as a
Trognon demonstrator and use plain modules for the field work.

## Field and radio constraints

Radiated power, band and duty cycle must be checked for the place and date of use. Check batteries
before charging old units. Obtain the agreement of anyone hosting a probe.
