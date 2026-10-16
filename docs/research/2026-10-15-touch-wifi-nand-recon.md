# Touch, Wi-Fi, NAND on the 3GS (S5L8920): reconnaissance before code

Research date: 2026-10-15. Source reading only (OpeniBoot/iDroid cloned
locally, `~/tools/openiBoot`, same revision as the 2026-10-14 journal entry)
plus web sources for Wi-Fi. Nothing executed on the device for this study:
this is reconnaissance, not a port.

## Why this document

After the button mapping proven on 2026-10-15, three subjects were still open
to decide whether the 3GS should stay a bare-metal monitor (current position,
`docs/research/2026-10-14-microkernel-3gs.md`) or eventually become a real
Linux port target. Decision taken: NAND (for a real rootfs) and Wi-Fi are the
two tracks that will settle this, once both have at least been explored in
read-only mode.

## Touch

- **No port for the 3GS in OpeniBoot**: no `multitouch-*` file in
  `plat-s5l8920/` (confirmed by a file search in the local clone).
- The protocol exists, but for an **earlier generation**:
  `plat-s5l8900/multitouch-z1.c` and `plat-s5l8900/multitouch-z2.c`.
- Bus and signals documented in `plat-s5l8900/includes/hardware/multitouch.h`:
  - `MT_SPI` = 1 or 2 depending on generation (line 15/18), `MT_SPI_CS` =
    `GPIO_SPI1_CS0` or `GPIO_SPI2_CS0` — this is **SPI**, not I2C.
  - `MT_GPIO_POWER` (line 7/10) and `MT_ATN_INTERRUPT` (line 8/11): a power
    GPIO line and an interrupt GPIO line (ATN, "attention").
  - Application-level command protocol: `MT_INFO_FAMILYID` = `0xD1` (line
    22), plus `SENSORINFO`/`SENSORREGIONDESC`/`SENSORREGIONPARAM`/
    `SENSORDIM`.
- **Not an off-the-shelf chip (neither Broadcom BCM5974 nor a TI "do-it-all"
  controller)**: this is a proprietary Apple touch ASIC, inherited from the
  2005 FingerWorks acquisition. The confusion with the BCM5974 (MacBook
  trackpad chip) comes from unreliable web sources, dismissed after checking
  the real source code.
- `docs/research/2026-10-14-microkernel-3gs.md` (lines 48-67) already noted a
  component candidate for THIS specific 3GS (TI marking 343S0464), not
  confirmed on the physical unit. This document adds confirmation of the bus
  (SPI) and the high-level protocol by analogy with the z1/z2 generation,
  still not verified against hardware.

## Wi-Fi

- **Chip confirmed by two independent iFixit sources: Broadcom BCM4325**
  (802.11 a/b/g only, no 802.11n — consistent with the 3GS's public specs).
  Corrects an earlier wrong assumption (BCM4329, which is the iPod touch
  3G/iPad 1 component, not the 3GS's).
- **Trap found and avoided**: `plat-s5l8900/wlan.c` exists in OpeniBoot, but
  it is a driver for **Marvell Libertas** chips (`IF_SDIO_MODEL_8385` =
  0x04, `_8686` = 0x0b, `_8688` = 0x10, lines 13-15), the Wi-Fi of the
  iPhone 2G/iPod touch 1G — **not applicable** to the 3GS's Broadcom
  BCM4325. The file's own comment (line 9: "No, I'm not really going to
  write a network stack here") says it only aimed to validate the SDIO I/O
  layer, not a full driver.
- The generic **SDIO host controller** exists for two generations:
  `plat-s5l8900/sdio.c` (987 lines) and `plat-a4/sdio.c` (738 lines), but
  **no version for `plat-s5l8920`**. Given the size gap between the two
  existing ones, there is no guarantee either is directly reusable for the
  3GS without checking against hardware.
- Priority lead if this is picked up again: Linux mainline's `brcmfmac`
  driver (which supports the BCM4325) as a protocol reference, not the
  existing OpeniBoot code (which targets a different chip).

## NAND / storage (the most advanced of the three subjects)

- **The driver already exists and compiles for this exact SoC**:
  `plat-s5l8920/h2fmi.c`, 2187 lines, with hardware AES
  (`aes_835_unwrap_key`, line 1864) and SHA1 — consistent with Apple's
  block-level encryption. The compiled object `iPhone3GS_h2fmi.o` is
  already present in the local clone (built on 2026-10-14).
- `plat-s5l8920/SConscript` wires the 3GS target to: `vfl-vfl` (line 19),
  `vfl-vsvfl` (line 20, two variants of the flash translation layer),
  `ftl-yaftl` (line 21, Apple's proprietary FTL — "Yet Another FTL") and
  `h2fmi.c` itself (line 43).
- `h2fmi.c` declares per-device keys (`DKey[32]`, `EMF[32]`, lines 15-16):
  the **existing** NAND content (the already-installed iOS image) is
  encrypted with keys derived from the SoC's UID, so unreadable without
  them. This does not necessarily block future use: writing a **new**
  filesystem (our own rootfs) does not need to decrypt the existing content,
  only to write/read raw pages via `h2fmi`, bypassing VFL/YAFTL if a direct
  access path is chosen instead of the full Apple stack.
- **A risk category distinct from everything done so far**: current
  payloads run in RAM (a mistake is fixed by reconnecting). NAND holds iOS
  and possibly boot-critical structures — a poorly controlled write can
  render the device unusable. Same caution as `tools/boot.sh` already
  applies to NOR ("*Stopping here deliberately: not calling flash_nor()*"):
  needed for NAND too, probably with an even larger safety margin
  (read-only first, a full dump before any write, an isolated area before
  any real attempt).

## Wi-Fi, continued: real address found in the decrypted device tree

Added 2026-10-15 (night), after the source code (OpeniBoot) turned out to
have no address at all for this block on the S5L8920. Extracted directly
from the owner's own IPSW already in hand
(`iPhone2,1_4.3.5_8L1_Restore.ipsw`, the same one already used on 2026-10-14
to extract the iBSS/iBEC):
`Firmware/all_flash/all_flash.n88ap.production/DeviceTree.n88ap.img3`.

That file is an encrypted IMG3 container (`KBAG` tags present). Decrypted
via the chip's own GID key, with the 3GS back in **pwned DFU** (not
Recovery): `image3.py` (`axi0mX/ipwndfu`, pinned per
`tools/ipwndfu/README.md`), `Image3.getDecryptedPayload()` ->
`dfuexec.PwnedDFUDevice().decrypt_keybag()` (a hardware AES primitive, pure
read, nothing written). Result: 46,384 bytes, binary AppleDeviceTree format
(starts with the `compatible` property), parsed with a purpose-built script
(nodes as `{n_props, n_children}`, properties as `{name[32], length,
value}`).

Node found, `/device-tree/arm-io/sdio`:

| Property | Raw bytes | Value |
|---|---|---|
| `compatible` | `7364696f2c73356c383932307800` | `"sdio,s5l8920x\0"` — confirms this block is for this exact SoC |
| `reg` | `0000000000100000` | offset `0`, size `0x1000`, relative to `arm-io` (base `0x80000000`, parent's `ranges` property) -> **absolute address `0x80000000`** |
| `clock-gates` | `10000000` | `0x10` (16) |
| `interrupts` | `22000000` | `0x22` (34) |
| `interrupt-parent` | `0000000d` | phandle `0x0d` |
| `function-device_reset` | `0000000e4f4950470516000001000100` | present, `OIPG` format — exact pin-number encoding **not decoded this session**, to confirm against a source before use |
| `function-device_wake` | `0000000e4f4950470414000001010000` | same |

Bonus, same chip family (BCM4325 Wi-Fi/Bluetooth combo), node
`/device-tree/arm-io/uart3/bluetooth`: `compatible = "bluetooth,n88\0"`,
`function-bt_reset`/`function-bt_wake` present, same undecoded `OIPG`
format.

**What this changes**: contrary to this document's earlier conclusion ("no
address for this block on the S5L8920 anywhere"), the address does exist —
just not in the OpeniBoot source, but in the real firmware's device tree for
this device. That is a more reliable source (specific to THIS device, not a
guess by analogy).

**Remaining before any hardware payload**: decode the `OIPG` format
precisely (which byte is the GPIO pin number, what the flag bytes mean)
from a verified source, not a guess. Leads exhausted this session without
result: TheiPhoneWiki/TheAppleWiki "DeviceTree" (HTTP 403/502 on both
domains, retry later); OpeniBoot's `plat-a4/sdio.c` references
`function-device_reset` only in a comment and never parses the binary
property (hardcoded GPIO, `SDIO_GPIO_DEVICE_RESET`, not read from the
device tree); the Asahi Linux ADT documentation
(`asahilinux.org/docs/fw/adt/`) only covers the modern (M1-era) format,
nothing on the historical pre-A4 binary format or `function-*` properties.
Next lead to try: `UKERN-Developers/dt` (a small C tool for reading the
AppleDeviceTree format, found in search but not yet read) or dumping the
device tree of an already-owned pre-A4 device (iPod touch, iPhone 4) to
compare two different `OIPG` encodings and deduce the structure by
cross-reference.

## Conclusion and next milestone

All three subjects are further along on the identification side than
expected, but at very different stages:

| Subject | Reference code for the S5L8920 | Next step |
|---|---|---|
| Touch | None; protocol from a close generation (SPI + proprietary ASIC) | Identify the real component on the unit (opening it) |
| Wi-Fi | None directly usable (the driver found targets a different chip) | Study `brcmfmac` (Linux) as the BCM4325 protocol reference |
| NAND | **Complete and compiled** (`h2fmi.c` + VFL + YAFTL) for this exact target | Read-only dump, before any write |

Decision of 2026-10-15 (see `TODO.md`): NAND (read-only) + Wi-Fi are the two
tracks chosen to judge whether the 3GS stays a bare-metal monitor or becomes
a real Linux target. Touch stays in parallel, less urgent for this
decision.

## Not verified

- Everything above is source reading (OpeniBoot) and web research (Wi-Fi);
  nothing re-executed against this project's physical device.
- Exact identification of the touch component on THIS unit: still not
  confirmed (needs opening the device or a direct read).
- Real compatibility of `h2fmi.c`/VFL/YAFTL with Trognon's current entry
  contract (MMU/cache/clock state at the moment our code takes over): not
  verified, to be established before any real dump.
- `plat-a4/sdio.c` as a possible base for an S5L8920 SDIO host controller:
  a hypothesis, not checked against hardware or a datasheet.
