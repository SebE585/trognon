# 8. Evidence: from claims to reproducible runs

A claim tagged **DEVICE** is only as good as the run behind it. This page ties each one to an **evidence bundle**
that anyone with a 3GS can regenerate, and says plainly which claims have no bundle yet.

--8<-- "docs/snippets/build-commit.en.md"

## What a bundle contains

Each run of `tools/send.sh` or `tools/go.sh` writes a directory under [`evidence/`](https://github.com/SebE585/trognon/tree/main/evidence):

| File | What it proves |
|---|---|
| `manifest.json` | the payload's SHA-256, the git commit it was built from, whether the tree was clean, the nonce sent and read back, the ipwndfu commit, the command line, the device CPID (the ECID is deliberately not recorded) |
| `screen.txt` | the exact text and colour of every console row the phone displayed, **published by the payload itself over USB**: no photo, no OCR |
| `screen.png` | the console re-drawn from `screen.txt`. A reconstruction, not a photo of the phone |
| `vars.txt` | extra values the payload published (`--vars`) |

**Why the nonce matters.** The iBSS keeps its variables between runs, so a stale value can look like a result. A random
32-bit nonce is appended to the binary, the payload republishes it, and a bundle is written only if it matches.
The screen rows are published in full on every run (empty ones too) for the same reason.

## Bundles

--8<-- "docs/snippets/evidence-table.en.md"

Regenerate this table with `make evidence-index`. Re-run the stages with `tools/evidence_all.sh` (device in DFU).
To check a bundle: check out its commit, `make -C tools/payloads <payload>`, compare the SHA-256 with `manifest.json`
(the build is deterministic with the same toolchain), then read `screen.txt` against the claim.

## Claims and their bundle

| Claim (DEVICE) | Bundle | Status |
|---|---|---|
| UNDEF and data abort handled and resumed, `FSR`/`FAR`/`LR`/`SPSR` as documented | `trace` | bundled |
| Prefetch abort (`bkpt`), `IFSR = 2` | `trace_pabt` | bundled |
| Software-triggered IRQ on VIC0 taken once, enables restored | `trace_irq` | bundled |
| CLCD, GPIO and clock-gate register values | `trace_regs` | bundled |
| VIC0/1/2 state left by the iBSS, IRQ/FIQ vector words | `trace_vic` | bundled |
| Timer block state, free-running tick | `trace_timer` | bundled |
| Tick rate 24 MHz against the host clock | `tools/tick_rate.py` | **not bundled yet**: measured and logged in the journal only |
| Code runs from DFU with nothing written to the phone; patched iBSS, `run` | the nonce of every bundle | bundled for execution; the DFU and pwn steps themselves are not |
| Page table has no fault entry, all sections in domain 0, `DACR = 3` | earlier payloads | **not bundled**: an earlier run, not re-run |
| Timer 5 interrupt | none | **not proven** (see the [hardware map](03-hardware-map.md)) |
| Button GPIO mapping, 376 pins read, zero noise | `trace_buttons` | bundled |
| NAND chip ID, CE0/CE1 only (Toshiba) | `trace_nand`, `trace_nand_sweep` x2 | bundled |
| Wi-Fi reset/wake GPIO read at `0` | `trace_wifi_gpio` | bundled |
| Bluetooth reset/wake GPIO read: reset `1`, wake `0` | `trace_bt_gpio` | bundled |
| Accelerometer powered up, `CTRL_REG1` read back `0x47`, axes +11 / -9 / +58 (one sample, phone not moved), then +19 / +47 / +8 after moving it, then +2 / -16 / +59 twice in a row at rest, then +3 / -6 / +59 twice flat screen up, then +2 / -6 / -53 twice screen down, then +57 / -7 / +6 and +58 / -6 / +6 on the left edge, then -55 / -6 / -5 and -54 / -7 / -4 on the right edge, then +3 / -60 / +11 and +4 / -61 / +11 on the top edge | `trace_accel_axes` x14 | bundled |
| Touch LDO only, then reset released: the line reads `0x210` at once and `0x211` 15 ms later, restored | `trace_touch_ldo` | bundled |
| `spi1` registers read with no clock-gate write (gate `0x0a` `0xF`, gate `0x16` `0`): same values as with the write | `trace_spi1_nogate` | bundled |
| Full GPIO configuration survey: 376 pad words read, 21 distinct values, known pads (I2C0, Bluetooth, touch reset and chip select) consistent with the bit layout | `trace_gpio_survey` | bundled |
| Control: touch reset released with no supply reads `0x210` (low), at once and 15 ms later; restored | `trace_touch_rst_ctl` | bundled |
| Dry run of the touch power-up (gate, PMU `0x11` and `0x50`, reset released), no SPI byte: reset register reads `0x211` with the supplies on, everything restored | `trace_touch_power` | bundled |
| PMU register `0x11` written `0xBE` to `0xFE` and register `0x50` `0x11` to `0x13`, both read back, held 20 ms, restored | `trace_pmu_w34` | bundled |
| `spi1` clock gate `0x16` written `0` to `0xF` and back (takes); idle registers of `spi1` read: SETUP `0x8`, PIN `0x3`, CLKDIV `1`, CONTROL/STATUS/SPCNT/SPIDD `0` | `trace_spi1_gate` | bundled |
| Clock gates `0x09`..`0x0D` read `0xF`; `0x15`, `0x16`, `0x17` (device tree SPI gates) read `0`; `spi1` registers deliberately not read | `trace_spi1_pre` | bundled |
| PMU registers `0x10`, `0x11`, `0x17`..`0x22`, `0x50`..`0x57` read over I2C (22 acknowledged reads); bit `0x40` of `0x11` is `0` | `trace_pmu_read` | bundled |
| Bus scan of `i2c0`: `0x1d`, `0x1e`, `0x39`, `0x74` acknowledge, no additional responder detected; AK8973 registers `0xC0`..`0xC4`, `0xE0`..`0xE6` read as the datasheet's reset state | `trace_i2c_scan` | bundled |
| I2C addresses on `i2c0`: `0x1d` and `0x1e` acknowledge, `0x1c` and `0x1f` do not | `trace_compass_i2c` | bundled |
| Touch reset and chip-select enable GPIO levels both `0`, Bluetooth reset control `1` | `trace_touch_gpio` | bundled |
| Wi-Fi reset GPIO toggled low/high, read back `1` | `trace_wifi_reset` | bundled |
| SDIO register dump, two clock-gate hypotheses, all zero | `trace_sdio_dump`, `trace_sdio_dump_b` | bundled |
| SDIO `CMD0` attempt, never leaves "not ready" | `trace_sdio_cmd0` | bundled |
| Legacy `plat-s5l8900`-style SDIO init sequence (not SDHCI-standard), still all zero | `trace_sdio_init` | bundled |
| Clock-gate write persistence check (flawed, superseded below) | `trace_gate_write` | bundled |
| Raw write-then-readback to the SDIO base, write does not land | `trace_write_test` | bundled |
| Dynamic-clock-16 register, read-only, ambiguous | `trace_dyn_clock16` | bundled |
| Timers 0-3, all 5 fields, nothing changes over 0.5 s | `trace_which_timer`, `trace_which_timer_b` | bundled |

A claim without a bundle is documented from an earlier run and should be treated as a lead until re-run.
