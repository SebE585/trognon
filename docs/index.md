---
title: Trognon
hide:
  - navigation
  - toc
---

<div class="hero" markdown>

# Trognon

**Open reverse engineering of the iPhone 3GS, so a phone left in a drawer can run code we wrote.**
*Rétro-ingénierie ouverte de l'iPhone 3GS : un téléphone oublié dans un tiroir exécute le code que
nous écrivons.*

[English documentation](en/README.md){ .md-button .md-button--primary }
[Documentation française](fr/README.md){ .md-button }
[Source code](https://github.com/SebE585/trognon){ .md-button }

</div>

## Where we are / Où on en est

The first code runs on a real iPhone 3GS (S5L8920, Cortex-A8): it draws on the screen, and it
catches and survives three kinds of CPU exceptions. Everything below was observed on the device.

Le premier code tourne sur un vrai iPhone 3GS : il dessine à l'écran et attrape trois types
d'exceptions CPU sans planter. Tout ce qui suit a été observé sur l'appareil.

![Capability ladder](img/capability-ladder.svg){ width="260" align=right }

| Rung | Status |
|---|---|
| Run our code from DFU, nothing written to the phone | <span class="tag tag-device">DEVICE</span> |
| Text and shapes on the screen | <span class="tag tag-device">DEVICE</span> |
| UNDEF, data abort, prefetch abort with a readable dump | <span class="tag tag-device">DEVICE</span> |
| Interrupts and timer | <span class="tag tag-source">SOURCE</span> next |
| Buttons, content over USB, storage, touch | <span class="tag tag-hyp">HYPOTHESIS</span> |

## Read it / Le lire

<div class="grid cards" markdown>

- **1. Boot chain** · *Chaîne de démarrage*

    From DFU to our code in RAM. [EN](en/01-boot-chain.md) · [FR](fr/01-boot-chain.md)

- **2. Exception console** · *Console d'exceptions*

    How a crash becomes a readable screen. [EN](en/02-exception-console.md) · [FR](fr/02-exception-console.md)

- **3. Hardware map** · *Carte matérielle*

    Which registers are verified, which are not. [EN](en/03-hardware-map.md) · [FR](fr/03-hardware-map.md)

- **4. Mods** · *Mods*

    Build the OS one use case at a time. [EN](en/04-mods.md) · [FR](fr/04-mods.md)

- **5. Field notes** · *Notes de terrain*

    What fails and how to tell. [EN](en/05-field-notes.md) · [FR](fr/05-field-notes.md)

- **Research** · *Recherche*

    Dated surveys of A5 to A11, the legal frame, untethered boot. [Index](research/2026-10-14-pre-a5-limera1n.md)

</div>

## How to read the evidence / Lire les preuves

<span class="tag tag-device">DEVICE</span> observed on a real phone ·
<span class="tag tag-emu">EMU</span> only in our offline emulation ·
<span class="tag tag-source">SOURCE</span> read in OpenIBoot or a disassembly, not yet checked ·
<span class="tag tag-hyp">HYPOTHESIS</span> a plan or a guess.

No Apple firmware is stored here. Independent project, not affiliated with or endorsed by Apple Inc.
