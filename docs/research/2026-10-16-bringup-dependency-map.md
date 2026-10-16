# Bring-up dependency map: touch and GPS

Status as of 2026-10-16. A map of what is acquired, what blocks, and which
observation would settle each step, for two candidate paths. It assumes no
step is already necessary or known: the steps are a working outline, to be
amended as evidence arrives. It does not say these components will be easy to
activate.

Each row carries one of: **acquired** (with evidence), **unread** (a source
exists, not read), **unknown** (no source identified), **blocked** (a
precondition is unresolved). A row only becomes *acquired* with a pointer to
evidence that can be re-run.

Reproduction for every device-tree fact below (decrypted tree extracted from
the owner's IPSW, never committed):

```
python3 -I tools/dt_dump.py /tmp/trognon_dt/devicetree_decrypted.bin multi-touch baseband pmu
```

## Two levels of work

- **Shared base**: power, clocks, GPIO, interrupts, buses, addressing,
  measurement tools. One finding here can unblock several components.
- **Per-device protocol**: initialisation, possible firmware, message format,
  calibration. Each device needs its own analysis.

## Shared base: what each item would unblock

| Item | State | Evidence or gap | Branches that depend on it |
|---|---|---|---|
| GPIO block and `port.pin` decoding | acquired | pins read on the device (`trace_bt_gpio`, STAGE 30) | touch reset and chip select, baseband reset and radio, Bluetooth, buttons |
| I2C bus 0 (bit-banged over GPIO) | acquired for the accelerometer transaction code, **device test pending** (STAGE 31) | `docs/spec/soc-s5l8920.md` | accelerometer, compass, **PMU** |
| PMU (`pmu,d1755`, on `i2c0`) | unread: node known, never probed | device tree only | touch power (`function-power_ldo`), baseband power (`function-bb_on`), touch clock enable |
| GPIO interrupt controller (`0x39A00000`) | blocked: read once dropped the USB link | documented as dangerous | touch attention line, baseband interrupt, any interrupt-driven device |
| SPI controllers (`spi0`–`spi4` nodes) | unread | device tree names only | touch (`spi1`), baseband (`spi,s5l8920x,baseband`) |
| Clock gates | register array readable and writable (a write to physical gate `0x16` took); **the numbering is unsettled**: the device tree, the iBSS and OpeniBoot disagree (see the touch preconditions page, "Correction: which clock-gate numbers are physical") | gate 0x27 read for the IOP (`trace_iop_clkgate`), which is the NAND controller 1's gate, not the IOP's (`IBSS-CLK-006`): the IOP's clock state is unknown | every block that needs its clock enabled |
| Measurement tools (nonce-confirmed payload runs, evidence bundles) | acquired | `tools/run_payload.py`, `evidence/` | all |

The PMU row stands out: the device tree routes the touch controller's power
and the baseband's power through it, and it sits on the same bus the
accelerometer test already exercises. That is a hypothesis about leverage
(one probe, several branches), not a finding; the PMU register protocol is
not known.

## Touch path

**Update, same day**: the driver family and the firmware question are answered in more detail, from the kernelcache, in [Touch controller preconditions](2026-10-16-touch-n88-preconditions.md): this phone's kernelcache names the `N1` software personality, which replaces the Zephyr driver the earlier notes borrowed from older devices as the one to analyse (it does not prove a different hardware family), and the driver's strings name a firmware bootload path whose use here is still open. The table below keeps its original wording; read it with that page.

Controller node: `/device-tree/arm-io/spi1/multi-touch`, compatible
`multi-touch,n88`. Decoded properties (project convention for `function-*`
words: provider phandle, tag, pin word):

- reset: GPIO controller, pin word `0x1401` (14.01; both components are hexadecimal digits of the word, port 0x14 and pin 0x01)
- chip-select enable: GPIO controller, pin word `0x1300` (13.00, same notation). The electrical function of these pins (polarity, who drives them) is untested
- power: `function-power_ldo` through the PMU phandle
- clock enable: `function-clock_enable` through the PMU phandle, pin 0
- attention: `interrupts = 0xb4, 0x0`, parent the GPIO node
- `multi-touch-calibration` (256 bytes) and `prox-calibration` (128 bytes):
  all zero in the IPSW tree, so per-unit calibration is not in this file.

| Step | State | Evidence or gap | Observation that would settle it |
|---|---|---|---|
| 1. Identify controller and driver | partly acquired | the bus (`spi1`) and the node come from the device tree; the 343S0464 marking is unconfirmed on this unit; no OpeniBoot port for this SoC (`2026-10-15-touch-wifi-nand-recon.md`) | opening the unit, or a driver symbol in the kernelcache |
| 2. Power, reset, transport | unread | the pins and the PMU routes are named above; their meaning is by convention and untested; the PMU protocol is unknown | read the reset and chip-select levels (read-only, as done for Bluetooth); then PMU probe |
| 3. Does firmware need loading | unknown | earlier text says "probably" (`TODO.md`); no evidence either way | a driver string or request in the kernelcache; a response from the controller before any load |
| 4. Controller responds | blocked on step 2 | | any valid answer to a minimal SPI transaction, with chip-select and attention line observed |
| 5. Raw contact | blocked on 4 | | a raw frame that changes with a finger, with the touch-free frame as baseline |
| 6. Coordinates and calibration | blocked on 5 | calibration blobs not in the tree | frames at known positions |

Minimal first observation: a raw contact frame, attributed to the controller
(chip-select and attention behaviour consistent with it), before any driver.

## GPS path

No node with `gps` appears anywhere in the device tree (string search over
the decrypted tree). The `baseband` node (`baseband,n88`) exposes power
(`function-bb_on`), reset (`function-bb_rst`, pin word `0x1407`), radio
(`function-radio_on`, `0x1405`), UART lines (`uart2_tx`, `uart3_tx`,
`bb_usart0_rxd_ctrl`, `umts_rxd_ctrl`), a reset-detect line and one
interrupt. A separate `spi,s5l8920x,baseband` compatible exists. Whether the
GPS is reached through the baseband, or by some other route, is not
established; the earlier remark in `TODO.md` ("probably behind the baseband")
was a guess and stays one.

| Step | State | Evidence or gap | Observation that would settle it |
|---|---|---|---|
| 1. Identify receiver and driver | unknown | no GPS node | kernelcache strings and kext names; whether any kext references the baseband node for navigation data |
| 2. Power, reset, transport | partly unread | baseband power, reset and UART lines named in the tree; which one carries navigation data unknown | read the named levels (read-only); identify which UART the receiver uses |
| 3. Initialisation and protocol | unknown | depends on step 1 | a captured byte stream, or the driver's strings |
| 4. Valid message | blocked on 2, 3 | | one message passing its own checksum, decoded |
| 5. Navigation state | blocked on 4 | | a status field in a decoded message |
| 6. Position and quality | blocked on 5 | depends on reception conditions | repeat outdoors with a reference position; record the date and conditions |

Minimal first observation: one correctly decoded message from the receiver.
A valid position additionally depends on reception, so its absence indoors
settles nothing.

## Choosing the next research

Rank by branches unblocked and by whether the step can be settled by a
read-only observation, not by how attractive the thread is.

| Candidate | Branches affected | Risk | Note |
|---|---|---|---|
| Run STAGE 31 (accelerometer) | proves the I2C path that the PMU probe would reuse | low (reads) | already waiting on the device in DFU |
| Probe the PMU | touch power, baseband power, touch clock | writes may change power rails; start read-only | blocked until the I2C path is proven |
| Resolve the GPIO interrupt controller hazard | touch, baseband, any interrupt-driven device | high (USB link was dropped once) | needs a safe read plan first |
| Read the touch reset and chip-select levels | touch step 2 | low (reads) | same pattern as STAGE 30 |
| IOP preamble (`Q8`, `Q9`) and buffer producer (`Q15`) | IOP, NAND, SDIO only | none (static) | does not help touch or GPS |

A retrieval layer over the catalog could help find these links later; each
retrieved link would still need to be checked against its evidence.
