---
title: Trognon
hide:
  - navigation
  - toc
---

<div class="home-hero" markdown>

<div class="home-hero__text" markdown>

# Trognon

**A phone left in a drawer, running code we wrote.** Open reverse engineering of the iPhone 3GS,
documented to the register, with every claim tied to a run you can repeat.

[Understand the project](en/01-boot-chain.md){ .md-button .md-button--primary }
[Explore the hardware](en/03-hardware-map.md){ .md-button }

[Documentation française](fr/README.md) · [Source code](https://github.com/SebE585/trognon)

</div>

<figure class="phone" markdown>
![Console shown on the 3GS screen](img/console-exceptions.png)
<figcaption>The exception console, as the phone's own screen shows it. Rebuilt from the payload's
console buffer, <strong>not a photograph</strong>.</figcaption>
</figure>

</div>

## What works today

Everything here was observed on a real iPhone 3GS (S5L8920, Cortex-A8); each item points to its page, and [Evidence](en/08-evidence.md) ties every claim to a run.

<div class="grid cards" markdown>

- **Our code runs from DFU**

    Nothing is written to the phone. [Boot chain](en/01-boot-chain.md) <span class="tag tag-device">DEVICE</span>

- **It draws and survives crashes**

    Text on the screen; three kinds of CPU exception caught and dumped readably. [Exception console](en/02-exception-console.md) <span class="tag tag-device">DEVICE</span>

- **Interrupts, a clock and buttons**

    A software interrupt taken, a 24 MHz tick, five buttons mapped by GPIO. [Hardware map](en/03-hardware-map.md) <span class="tag tag-device">DEVICE</span>

- **An accelerometer that answers**

    Powered up, axes read in several positions, about 56 counts per g. [Hardware map](en/03-hardware-map.md) <span class="tag tag-device">DEVICE</span>

- **A NAND chip identified**

    Toshiba, read on two chip-enable lines; no page access yet. [Hardware map](en/03-hardware-map.md) <span class="tag tag-device">DEVICE</span>

- **Not yet: touch, Wi-Fi, storage**

    Being worked out, see below. [Research](research/index.md) <span class="tag tag-hyp">HYPOTHESIS</span>

</div>

## Where to start

<div class="grid cards" markdown>

- **Understand**

    From DFU to our code in RAM, and what each stage is. [Boot chain](en/01-boot-chain.md)

- **Explore the components**

    Which registers and chips are verified, which are not. [Hardware map](en/03-hardware-map.md)

- **Reproduce**

    The exact command, commit and raw output behind each claim. [Evidence](en/08-evidence.md)

</div>

## Research in progress

- **Touch controller.** Its reset line and power sequence have been observed on the device; no byte has been exchanged with it yet. The transfer recipe of the SPI controller is being established.
- **Wi-Fi and the coprocessor.** The start-up sequence of the coprocessor that runs the Wi-Fi stack has been read in the firmware; the coprocessor is not running yet.

The dated notes are in the [research index](research/index.md).

## Read the rest

[Boot chain](en/01-boot-chain.md) · [Exception console](en/02-exception-console.md) · [Hardware map](en/03-hardware-map.md) · [Mods](en/04-mods.md) · [Field notes](en/05-field-notes.md) · [Support by chip](en/06-chips.md) · [Modular code](en/07-code-layout.md) · [Evidence](en/08-evidence.md) · [Research](research/index.md)

## How to read the evidence

| Tag | Meaning |
|---|---|
| <span class="tag tag-device">DEVICE</span> | observed on a real phone |
| <span class="tag tag-emu">EMU</span> | seen only in our offline emulation |
| <span class="tag tag-source">SOURCE</span> | read in OpenIBoot or a disassembly, not yet checked |
| <span class="tag tag-hyp">HYPOTHESIS</span> | a plan or a guess |

No Apple firmware is stored here. Independent project, not affiliated with or endorsed by Apple Inc.
