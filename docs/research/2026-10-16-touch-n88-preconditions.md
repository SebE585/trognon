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

## Preconditions, with their state

| Precondition | Source | State |
|---|---|---|
| Power | `function-power_ldo` through the PMU (`pmu,d1755`, I2C `0x74`) | the PMU **acknowledges its address** on `i2c0` (bus scan, `20261016T152515Z`). Static reading: LDO index 11, enable = bit `0x40` of PMU register `0x11`; not observed on the device, nothing written |
| Clock enable | `function-clock_enable` through the PMU, pin 0 | device tree and a partial reading of the PMU GPIO code; bit meanings of registers `0x50`..`0x57` not decoded |
| Reset | GPIO `14.01` | level read `0` once; polarity not established; the personality has a `reset-deassert-high-z` flag whose effect was not read |
| Chip select | GPIO `13.00` (`function-enable_cs`) plus the SPI controller's own select | level read `0` once; role not established |
| SPI controller | `spi1` at CPU address `0x82100000` (`reg 0x2100000` plus the `0x80000000` offset of `arm-io`) | not read; its clock gate was not read either |
| Attention | interrupt `0xb4` on the GPIO interrupt controller | not read, the controller is treated as dangerous |
| Firmware | a bootload path named in the driver's strings | open: whether it is needed here is not established; source in the IPSW not looked up |

## What would move this forward

In increasing order of risk, each step read-only until the previous one gave a result:

1. (Done for the LDO) read the PMU driver for the register its LDO function writes and the
   meaning of `0x0708020b`; still to do for the PMU GPIO that gates the clock. Static, no risk.
1b. Read the PMU registers `0x10`, `0x11`, `0x17`..`0x22` and `0x50`..`0x57` over I2C
   (`STAGE 36`, prepared, not run) to see their present values, which would show whether LDO 11
   is currently off. Read-only transactions on `i2c0`.
2. Read the clock gate of `spi1` and the controller's idle registers at `0x82100000`. This is the
   same kind of read as the IOP and GPIO probes.
3. Only then consider a PMU write to power the controller, which changes a power rail and
   deserves its own explicit go.
4. A first SPI exchange, with the attention line read through a GPIO level if its pin can be
   identified, never through the interrupt controller.

Until step 3, nothing can say whether the controller answers at all.
