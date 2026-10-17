---
title: Trognon
hide:
  - navigation
  - toc
---

<div class="home-hero" markdown>

<div class="home-hero__text" markdown>

# Trognon

**A phone left in a drawer, running code we wrote.** Open reverse engineering of the 32-bit iPhones before the A5 (the iPhone 3GS first, then the iPhone 4),
documented to the register, with every claim marked by how we know it: observed on the phone, read in code, or guessed.

**Two ways in.** Follow the story of the phone from a powered-off drawer to the first pixel, or take a problem: a block we have not finished understanding, and a code to find.

[Follow the story](en/journey/start.md){ .md-button .md-button--primary }
[Take a problem](en/bestiary.md){ .md-button .md-button--primary }
[Read the reference](en/README.md){ .md-button }

[Source code](https://github.com/SebE585/trognon)

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

    Our payloads are built never to write to the phone's storage (by design, not yet checked by comparing the storage before and after).

    <span class="tag tag-device">DEVICE</span> [Boot chain](en/01-boot-chain.md){ .card-link }

- **It draws and survives crashes**

    Text on the screen; three kinds of CPU exception caught and dumped readably.

    <span class="tag tag-device">DEVICE</span> [Exception console](en/02-exception-console.md){ .card-link }

- **Interrupts, a clock and buttons**

    A software interrupt taken, a 24 MHz tick, five buttons mapped by GPIO.

    <span class="tag tag-device">DEVICE</span> [Hardware map](en/03-hardware-map.md#interrupt-controllers){ .card-link }

- **An accelerometer that answers**

    Its control register reads back what was written; axes read in several positions, about 56 counts per g.

    <span class="tag tag-device">DEVICE</span> [Accelerometer](en/03-hardware-map.md#accelerometer){ .card-link }

- **A NAND chip identified**

    Toshiba, read on two chip-enable lines; no page access yet.

    <span class="tag tag-device">DEVICE</span> [NAND](en/03-hardware-map.md#nand){ .card-link }

- **Not yet: touch, Wi-Fi, storage**

    Being worked out, see below.

    <span class="tag tag-hyp">HYPOTHESIS</span> [Research](research/index.md){ .card-link }

</div>

## Where to start

<div class="grid cards" markdown>

- ![](img/icons/understand.svg){ .card-icon } **Understand**

    From DFU to our code in RAM, and what each stage is.

    [Boot chain](en/01-boot-chain.md){ .card-link }

- ![](img/icons/explore.svg){ .card-icon } **Explore the components**

    Which registers and chips are verified, which are not.

    [Hardware map](en/03-hardware-map.md){ .card-link }

- ![](img/icons/reproduce.svg){ .card-icon } **Reproduce**

    The exact command, commit and raw output behind each claim.

    [Evidence](en/08-evidence.md){ .card-link }

</div>

## Reproduce a run

With the one-time setup of [Boot chain](en/01-boot-chain.md#reproduce) done and the phone in DFU mode:

```sh
make -C tools/payloads trace.bin        # build the exception-console payload (arm-none-eabi-gcc)
tools/go.sh tools/payloads/trace.bin    # pwn, send it, read its variables, write an evidence bundle
```

The screen shows the console above, and `evidence/` gains a dated folder with the payload's hash, the commit and the raw
output. Payloads that **write** to hardware registers are described, with their guards, on the
[hardware map](en/03-hardware-map.md); read a payload before you run it.

## Take part

You do not need a phone to help. Pick the way in that suits you.

<div class="grid cards four" markdown>

- **Take a mission**

    An open question of the analysis, with what you need and what a good answer looks like. Or pick a creature of the [Bestiary](en/bestiary.md): a block of the phone still to be understood.

    [Missions](en/missions.md){ .card-link }

- **Check a published run**

    Rebuild a payload and compare its hash with the bundle's manifest. No phone.

    [Verifications](en/verifications.md){ .card-link }

- **Report an observation**

    Something you saw or concluded, with the method and its limits. Negative results are welcome.

    [Open the form](https://github.com/SebE585/trognon/issues/new?template=observation.yml){ .card-link }

- **Suggest a correction**

    A page is wrong, misleading or unclear. Quote it and say what it should say.

    [Open the form](https://github.com/SebE585/trognon/issues/new?template=correction.yml){ .card-link }

</div>

## Research in progress

- **Touch controller.** Its reset line and power sequence have been observed on the device; no byte has been exchanged with it yet. The transfer recipe of the SPI controller is being established.
- **Wi-Fi and the coprocessor.** The start-up sequence of the coprocessor involved in the SDIO and Wi-Fi path has been read in the firmware; we have not started that coprocessor and do not know whether it runs under the boot stage; its role in the Wi-Fi chain is not established.

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
