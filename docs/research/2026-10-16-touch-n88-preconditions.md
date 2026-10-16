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

## Preconditions, with their state

| Precondition | Source | State |
|---|---|---|
| Power | `function-power_ldo` through the PMU (`pmu,d1755`, I2C `0x74`) | the PMU **acknowledges its address** on `i2c0` (bus scan, `20261016T152515Z`); its register protocol is unknown. The PMU kext (`AppleD1755`, with `AppleD1755PMULDOFunction` and `AppleD1755PMUGPIOFunction`) holds it and was located, not read |
| Clock enable | `function-clock_enable` through the PMU, pin 0 | device tree only |
| Reset | GPIO `14.01` | level read `0` once; polarity not established; the personality has a `reset-deassert-high-z` flag whose effect was not read |
| Chip select | GPIO `13.00` (`function-enable_cs`) plus the SPI controller's own select | level read `0` once; role not established |
| SPI controller | `spi1` at CPU address `0x82100000` (`reg 0x2100000` plus the `0x80000000` offset of `arm-io`) | not read; its clock gate was not read either |
| Attention | interrupt `0xb4` on the GPIO interrupt controller | not read, the controller is treated as dangerous |
| Firmware | a bootload path named in the driver's strings | open: whether it is needed here is not established; source in the IPSW not looked up |

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
