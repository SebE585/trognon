# Trognon documentation (English)

Reading guide for the reverse-engineering work on the iPhone 3GS (S5L8920).
Every statement carries an evidence level:

| Tag | Meaning |
|---|---|
| **DEVICE** | Observed on a real iPhone 3GS (read off its screen or over USB). |
| **EMU** | Only seen in our offline emulation (Unicorn). Proves the logic, not the hardware. |
| **SOURCE** | Read in OpenIBoot or in a disassembly, not yet checked on the device. |
| **HYPOTHESIS** | A plan or a guess. Do not build on it without checking. |

State as of 2026-10-15, one device (ECID redacted), iBSS 4.3.5 for `n88ap`
(CPID 0x8920). Anything older than six months must be re-checked.

1. [Boot chain: from DFU to our code](01-boot-chain.md)
2. [The on-screen exception console](02-exception-console.md)
3. [Hardware map: what is verified](03-hardware-map.md)
4. [Mods: building the OS one use case at a time](04-mods.md)
5. [Field notes: what fails and how to tell](05-field-notes.md)
6. [Support by chip: 3GS, 4, 4S, then the rest](06-chips.md)
7. [Modular code layout](07-code-layout.md)
8. [Evidence: from claims to reproducible runs](08-evidence.md)

Français : [`../fr/README.md`](../fr/README.md).
Older working notes (French, mixed with the log): [`../architecture.md`](../architecture.md).

No Apple firmware is stored in this repository. The iBSS used here is
decrypted on the owner's device and patched in RAM.

## Provenance and licensing

Register addresses and boot-flow facts were *read* in OpenIBoot (GPL-3.0, dormant since 2022)
and then checked on the device. Trognon does not copy OpenIBoot code: our payloads are written
from scratch, and only addresses and offsets (facts) are reused, each tagged **SOURCE** until
read on the phone. This matters because OpenIBoot's GPL-3.0 is not compatible with this
project's GPL-2.0-only and MIT components. Keep it that way: no code pasted from OpenIBoot.
