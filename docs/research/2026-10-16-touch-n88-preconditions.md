# Touch controller of the iPhone 3GS: what is known before any transaction

Status as of 2026-10-16. Companion to the
[bring-up dependency map](2026-10-16-bringup-dependency-map.md), which listed the touch path
as a working outline; this page replaces its guesses about the driver family with what the
kernelcache and the device tree show. Nothing here comes from a transaction with the
controller: the only measurement so far is two GPIO levels (`03-hardware-map.md`).

Reproduction (the kernelcache is the owner's, extracted from their IPSW, never committed):

```
python3 -I tools/dt_dump.py /tmp/trognon_dt/devicetree_decrypted.bin multi-touch
python3 -I tools/iop_analysis/extract_kext.py /tmp/trognon_dt/kernelcache_decrypted.bin com.apple.driver.AppleMultitouchSPI /tmp/trognon_dt/AppleMultitouchSPI.kext.bin
strings -a /tmp/trognon_dt/AppleMultitouchSPI.kext.bin
```

## An attribution, not a family

Earlier notes took the protocol from the pre-3GS generation (OpeniBoot's `multitouch-z1`/`z2`
files, and the iDroid Linux driver `zephyr2.c`). The kernelcache of this device names a
different **software personality**: `AppleMultitouchSPI - N1,N88` has
`IOClass = AppleMultitouchN1SPI`, matches the node name `multi-touch,n88` and sits on an SPI
provider. The same kext also contains a `Z2` class (`AppleMultitouchZ2SPI`) and a `z1` match
string. That correspondence is enough to choose which driver to analyse for this phone. It does
not establish that the controller is a different hardware family, nor that the Zephyr protocol
is incompatible with it. The iDroid driver stays **an analogy, not a specification**.

## What the personality configures (kernelcache property values)

The values are the driver's configuration as shipped; what the code does with each one was not
read.

| Property | Value | What it configures, as far as the name says |
|---|---|---|
| `IONameMatch` | `multi-touch,n88` | the device-tree node this personality binds to |
| `IOProviderClass` | `AppleARMSPIDevice` | an SPI provider (`spi1` in the tree) |
| `reset-deassert-delay` | `0xf` (15) | a delay after the reset line is released; the unit is not stated, and the iDroid driver's 15 ms at the same point does not demonstrate it |
| `reset-deassert-high-z` | true | a flag about how the reset line is released; presumably high impedance, which the code would have to confirm |
| `ResetWhenExitingUILock`, `DisablePowerForUILock` | true | flags that name reset and power in a lock-screen context |
| `AutomaticallySetOperatingMode` | true | a flag naming an operating mode that the driver may set |
| `mt-merge-personality` | `N1F54,1` | a reference to the `AppleMultitouchSPIN1F54` plug-in personality |

## Mechanisms named by the driver's strings (candidates, not a sequence)

The strings of the driver binary are an inventory of mechanisms it mentions. Their order in the
binary is not an execution order, and no function was read, so none of them is shown to run, to
run in this order, or to run on this device:

- a power function (the device-tree `function-power_ldo`; one message says the driver "fakes" it
  when it is missing) and a delay before power-on;
- a bootloader check (a `CHECK` packet) and a bootload path: messages "Device has no firmware -
  will attempt to bootload", a retry count, "Cannot load firmware. No bootloader, or firmware is
  unavailable", "A-Speed Firmware", "PreconstructedBootloadPacketType";
- an interface-version query and an operating-mode register (`rOPERATING_MODE`);
- host-interrupt handling with frame reads, and messages about checksum mismatches on control
  reads, control writes, report info and frames.

Whether this controller needs firmware loaded at each power-up is therefore **open**. The strings
name a bootload path; settling the question needs the bootload code read, or a test on the
device. If firmware is needed it is Apple's, and Trognon never ships it: it would be extracted
from the owner's IPSW at install time. Where it sits in the IPSW was not looked up.

## The PMU side (static reading of the `AppleD1755` driver, 2026-10-16)

The touch node's `function-power_ldo` is `(PMU phandle, tag, 0x0708020b)`. The PMU driver
(`com.apple.driver.AppleD1755`, extracted with `extract_kext.py`, SHA-256 in the journal of the
catalog work) registers one function class per tag, among them `AppleD1755PMULDOFunction` and
`AppleD1755PMUGPIOFunction`. What a read of those functions shows, without any run on the device:

- **Register access**: the PMU is reached through an I2C nub with an 8-bit register address and a
  combined transfer (write the register number, then read). Reads and writes go through two small
  helpers whose failure messages say `_readRegs`/`_writeRegs`. The 7-bit address `0x74` from the
  device tree is the one that acknowledged in the bus scan.
- **The LDO argument** `0x0708020b` is read as little-endian bytes `0b 02 08 07`: byte 0
  (`0x0b`) is the LDO index, byte 1 (`0x02`) carries an on/off bit in its bit 0, and the half-word
  at bytes 2 and 3 is `0x0708` (1800), whose top bit is a second flag and whose low 15 bits are a
  voltage in millivolts if the LDO has a voltage table. The driver's own failure message prints
  the index plus one (`ldo12`).
- **A table of 12 LDOs** (8 bytes each, at `0x80764174` of the extracted kext) gives for each a
  minimum voltage, a step, a count, a mask, a secondary enable mask and an enable register with its
  bit mask. LDOs 0 to 10 have a voltage table; **LDO index 11 has none** (min, step and count are
  zero), so the 1800 in the argument is not used to program a voltage for it by this code path. Its
  enable is **bit `0x40` of PMU register `0x11`** (registers `0x10` and `0x11` hold the enable bits
  of the 12 LDOs). Setting the bit turns the LDO on in the code that was read (the update is
  `new = (old & ~mask) | (on ? mask : 0)`).
- **Default state**: the function's initialiser applies the descriptor once with the on/off bit
  taken from byte 1 of the argument, which is 0 here. That reads as "this LDO is switched off
  when the PMU driver starts", which is consistent with a touch controller that is unpowered at
  boot; it was not observed.
- **PMU GPIOs** (used for `function-clock_enable`, pin 0): eight PMU pins, each with a
  configuration register at `0x50` plus the pin number. The code that configures them mixes
  function bits, an input/output distinction and polarity; its bit meanings were not decoded, so
  the clock-enable step is **not** recoverable from this reading yet.

So powering the touch controller would involve at least a read-modify-write of PMU register
`0x11` (bit `0x40`) and some configuration of PMU GPIO 0, plus the SoC reset line, the SPI
controller and its clock. Only the first is decoded. Nothing was written to the PMU.

## PMU registers as read on the device (STAGE 36, bundle `20261016T164348Z-trace_pmu_read`)

A read-only run read 22 PMU registers at `0x74` (random reads: write the register number,
repeated start, read one byte). All 22 transactions were fully acknowledged, the bytes differ from
one register to the next, and the nonce was confirmed: the register-pointer protocol read from the
driver works as read. State of the phone at this point of the boot (iBSS patched, kernel never
started):

| Register | Value | Binary |
|---|---|---|
| `0x10` | `0xEB` | `11101011` |
| `0x11` | `0xBE` | `10111110` |
| `0x17`..`0x22` | `8A 06 8A B8 6A F0 4F B6 44 BA D7 C0` | not decoded |
| `0x50`..`0x57` | `11 D8 09 1A D8 98 D9 E0` | not decoded |

**What this says about the touch LDO**: bit `0x40` of register `0x11` is **0**. Under the static
reading (set means on), LDO index 11 is off, which is what the initialiser's default suggested.
That is one reading of one register at one moment, interpreted with a rule taken from code that
was not run; it does not show that the touch controller is the load of that LDO (that comes from
the device tree), nor that clearing is "off" electrically.

Applying the table's enable bits to the two bytes (same caveat) gives LDOs 0, 2, 3, 4, 6, 7, 8,
9 and 10 on, and LDOs 1, 5 and 11 off. The voltage registers `0x17`..`0x21` were not converted
to millivolts: the conversion routine was not read, so any figure would be a guess. The PMU GPIO
configuration bytes (`0x50`..`0x57`) were read raw for the same reason.

## The SPI controller and its clock (STAGE 37, bundle `20261016T164825Z-trace_spi1_pre`)

The `spi1` node gives `reg 0x2100000` (CPU `0x82100000`), `clock-gates = 0x16`, interrupt `0x1c`,
two DMA channels, `function-spi_cs0` = GPIO `13.00` (the same pin word as the touch node's
`function-enable_cs`), and the compatible list `spi,s5l8920x`, `spi,s5l8720x`, `spi,s5l8900x`; the
last one is the match of `AppleS5L8900XSPIController`, the controller class named in the
kernelcache. A read-only run read the clock-gate registers (`0xBF100078 + 4 * gate`, the layout
already used for the IOP gate `0x27`, which read `0x0000000F`):

| Gate | Source | Value read |
|---|---|---|
| `0x15`, `0x16`, `0x17` | device tree: `spi0`, `spi1`, `spi2` | `0`, `0`, `0` |
| `0x09`..`0x0D` | OpeniBoot `plat-s5l8920` header: SPI0..SPI4 | `0xF` each |
| `0x18`, `0x19` | next gates in the device tree order (`0x1a` is `uart0`) | `0xF` each |

The device tree numbers its gates in the order of the blocks (`sdio` `0x10`, `sha1` `0x11`,
`ceata` `0x12`, `flash-controller0` `0x13` and `0x14`, then the three SPI blocks, then the UARTs),
and the same formula read the IOP gate correctly. The OpeniBoot header for this SoC family gives
different numbers (SPI1 = `0xA`). The device tree is taken as the better-supported source for this
phone, which makes the three SPI gates **off** at this point of the boot; this is a reading of
two sources that disagree, not a test of which one names the SPI clock. The probe skips the
`spi1` register reads when gate `0x16` is not enabled, because a read of an unclocked block may
stall the bus (this project lost its USB link once to such a read); it skipped them, so **no
`spi1` register was read**.

Everything needed for a first exchange is therefore still off or unread: the SPI clock gate
(`0`), the touch LDO (bit `0x40` of PMU register `0x11` is `0`), the PMU clock-enable GPIO (not
decoded), the reset line (level `0`, polarity unknown). Each of these takes a write. None was made.

## Preconditions, with their state

| Precondition | Source | State |
|---|---|---|
| Power | `function-power_ldo` through the PMU (`pmu,d1755`, I2C `0x74`) | the PMU **acknowledges its address** on `i2c0` (bus scan, `20261016T152515Z`). Static reading: LDO index 11, enable = bit `0x40` of PMU register `0x11`; not observed on the device, nothing written |
| Clock enable | `function-clock_enable` through the PMU, pin 0 | device tree and a partial reading of the PMU GPIO code; bit meanings of registers `0x50`..`0x57` not decoded |
| Reset | GPIO `14.01` | level read `0` once; polarity not established; the personality has a `reset-deassert-high-z` flag whose effect was not read |
| Chip select | GPIO `13.00` (`function-enable_cs`) plus the SPI controller's own select | level read `0` once; role not established |
| SPI controller | `spi1` at CPU address `0x82100000` (`reg 0x2100000` plus the `0x80000000` offset of `arm-io`) | clock gate `0x16` reads `0` (off) in STAGE 37; registers not read, deliberately |
| Attention | interrupt `0xb4` on the GPIO interrupt controller | not read, the controller is treated as dangerous |
| Firmware | a bootload path named in the driver's strings | open: whether it is needed here is not established; source in the IPSW not looked up |

## What would move this forward

In increasing order of risk, each step read-only until the previous one gave a result:

1. (Done for the LDO) read the PMU driver for the register its LDO function writes and the
   meaning of `0x0708020b`; still to do for the PMU GPIO that gates the clock. Static, no risk.
1b. Read the PMU registers `0x10`, `0x11`, `0x17`..`0x22` and `0x50`..`0x57` over I2C
   (`STAGE 36`, done: bit `0x40` of `0x11` reads `0`). Read-only transactions on `i2c0`.
2. (Done for the gate) read the clock gate of `spi1`: it is `0`. The controller's idle registers at
   `0x82100000` cannot be read safely until the gate is on.
3. Writes, each deserving its own explicit go and an exact description first: the `spi1` clock gate,
   the PMU register `0x11` bit `0x40` (touch LDO), the PMU clock-enable GPIO once decoded, and the
   reset line.
4. A first SPI exchange, with the attention line read through a GPIO level if its pin can be
   identified, never through the interrupt controller.

Until step 3, nothing can say whether the controller answers at all.
