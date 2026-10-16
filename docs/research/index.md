# Research index

Dated reports: each one is a snapshot of what was known on its own date, not a living
reference. A report can be superseded by a later one on the same topic without being
deleted — check the date before relying on a specific number or address.

| Date | Report | What it covers | Status |
|---|---|---|---|
| 2026-10-13 | [A5/A6, 32-bit](2026-10-13-a5-a6-32bit.md) | Which exploit and tooling apply to the A5 (4S) and A6 (5, 5c) generations | active |
| 2026-10-13 | [A7 to A11, mainline](2026-10-13-a7-a11-mainline.md) | State of the art for Linux on 64-bit iPhones (5s to X) | active |
| 2026-10-13 | [Legal frame](2026-10-13-legal-frame.md) | EU/France and US framing — not legal advice | active |
| 2026-10-13 | [Untethered boot](2026-10-13-untethered-boot.md) | Whether a persistent (non-tethered) boot is realistic here | active |
| 2026-10-14 | [Pre-A5 and limera1n](2026-10-14-pre-a5-limera1n.md) | Which bootrom exploit actually applies to the 3GS and iPhone 4 | active |
| 2026-10-14 | [Microkernel on the 3GS](2026-10-14-microkernel-3gs.md) | Feasibility study for a microkernel + touch driver | active |
| 2026-10-15 | [Touch, Wi-Fi, NAND recon](2026-10-15-touch-wifi-nand-recon.md) | First reconnaissance pass before writing code for these three | active — the Wi-Fi/SDIO part (chip hypothesis, generic SDIO host code) is refined by the 2026-10-16 report below; NAND and touch findings stand |
| 2026-10-15 | [OS port comparison](2026-10-15-os-port-comparison.md) | NuttX vs. our own kernel vs. Linux ARM vs. Zephyr | active |
| 2026-10-16 | [Wi-Fi/IOP architecture](2026-10-16-wifi-iop-architecture.md) | The second, embedded ARM core and its role in SDIO/Wi-Fi — the longest-running thread, several corrections | active — consolidated in place (the report's own description), supersedes earlier Wi-Fi/IOP notes |
| 2026-10-16 | [IOP start-up to first sign of life](2026-10-16-iop-boot-to-life.md) | Thread from the driver's `start`/`startCPU` to a hardware ladder, open links with the proof each needs | active — `startCPU` read in full, no observation attributable to the IOP yet |
| 2026-10-16 | [Bring-up dependency map](2026-10-16-bringup-dependency-map.md) | Touch and GPS as dependency paths over a shared base; what is acquired, what blocks, what would settle it | active — outline, steps to be amended with evidence |

"Status" is a coarse read of whether a report is still the best current word on its topic,
not a replacement for checking its date: "active" with no further note just means no later
report has corrected it yet, not that it has been re-verified recently.

Supporting material for the Wi-Fi/IOP report, kept separately because it is a working
reference rather than a dated narrative:
[`tools/iop_analysis/`](https://github.com/SebE585/trognon/tree/main/tools/iop_analysis).
