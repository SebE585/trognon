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

## A correction

Earlier notes took the protocol from the pre-3GS generation (OpeniBoot's `multitouch-z1`/`z2`
files, and the iDroid Linux driver `zephyr2.c`). The kernelcache of this device names a
different driver class: the personality `AppleMultitouchSPI - N1,N88` has
`IOClass = AppleMultitouchN1SPI`, matches the node name `multi-touch,n88` and sits on an SPI
provider. The same kext also contains a `Z2` class (`AppleMultitouchZ2SPI`) and a `z1`
match string, so the family is shared and this phone uses the N1 variant. The iDroid driver
is therefore **an analogy, not a specification** for this device.

## What the personality says (kernelcache, property values)

| Property | Value | Reading |
|---|---|---|
| `IONameMatch` | `multi-touch,n88` | the device-tree node of the touch controller |
| `IOProviderClass` | `AppleARMSPIDevice` | reached over SPI (`spi1` in the tree) |
| `reset-deassert-delay` | `0xf` (15) | a delay after releasing reset; the unit is not stated in the property, the iDroid driver waits 15 ms at the same point, which is consistent but not a proof |
| `reset-deassert-high-z` | true | releasing reset means letting the line float (high impedance), not driving it high |
| `ResetWhenExitingUILock`, `DisablePowerForUILock` | true | power and reset are used as controls at runtime |
| `AutomaticallySetOperatingMode` | true | the driver sets an operating-mode register itself |
| `mt-merge-personality` | `N1F54,1` | links to the `AppleMultitouchSPIN1F54` plug-in personality |

## What the driver's strings suggest about the sequence

Strings only; the code was not read. They describe, in order of appearance in the driver's
messages, a sequence that has to be confirmed by disassembly:

1. power the device through a power function (the device-tree `function-power_ldo`, which the
   driver can "fake" if it is missing), with an optional delay before power-on;
2. check whether the device is in a bootloader (a `CHECK` packet), and whether firmware is
   already running ("Device has no firmware - will attempt to bootload", retried a fixed number
   of times, "Cannot load firmware. No bootloader, or firmware is unavailable");
3. bootload a firmware image ("A-Speed Firmware", "PreconstructedBootloadPacketType");
4. read an interface version, set the operating mode register (`rOPERATING_MODE`);
5. enable the interrupt and, on each host interrupt, read a frame; every control read, control
   write and frame is checked against a checksum.

So the controller very likely needs firmware loaded at each power-up, which answers the open
question of the dependency map in the affirmative **by the driver's behaviour, not by a test on
the device**. That firmware is Apple's; Trognon never ships it. It would be extracted from the
owner's IPSW at install time, like the other firmware. Where it sits in the IPSW was not looked up.

## Preconditions, with their state

| Precondition | Source | State |
|---|---|---|
| Power | `function-power_ldo` through the PMU (`pmu,d1755`, I2C `0x74`) | the PMU **acknowledges its address** on `i2c0` (bus scan, `20261016T152515Z`); its register protocol is unknown. The PMU kext (`AppleD1755`, with `AppleD1755PMULDOFunction` and `AppleD1755PMUGPIOFunction`) holds it and was located, not read |
| Clock enable | `function-clock_enable` through the PMU, pin 0 | device tree only |
| Reset | GPIO `14.01` | level read `0` once; polarity not established, and the driver releases it to high impedance |
| Chip select | GPIO `13.00` (`function-enable_cs`) plus the SPI controller's own select | level read `0` once; role not established |
| SPI controller | `spi1` at CPU address `0x82100000` (`reg 0x2100000` plus the `0x80000000` offset of `arm-io`) | not read; its clock gate was not read either |
| Attention | interrupt `0xb4` on the GPIO interrupt controller | not read, the controller is treated as dangerous |
| Firmware | the driver's bootload path | needed by the driver's own logic; source in the IPSW not looked up |

## What would move this forward

In increasing order of risk, each step read-only until the previous one gave a result:

1. Read the PMU driver (`AppleD1755`) for the register that its LDO function writes and the
   meaning of the argument `0x0708020b` in `function-power_ldo`. Static, no risk.
2. Read the clock gate of `spi1` and the controller's idle registers at `0x82100000`. This is the
   same kind of read as the IOP and GPIO probes.
3. Only then consider a PMU write to power the controller, which changes a power rail and
   deserves its own explicit go.
4. A first SPI exchange, with the attention line read through a GPIO level if its pin can be
   identified, never through the interrupt controller.

Until step 3, nothing can say whether the controller answers at all.
