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
  the clock-enable step is only partly recoverable from this first reading; see the later section on the decoded GPIO functions.

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

## PMU GPIO and LDO functions, decoded (static, 2026-10-16)

Catalog: `AppleD1755/` (`PMU-LDO-*`, `PMU-LDOINIT-*`, `PMU-GPIOFN-*`, `PMU-GPIOAPPLY-*`,
`PMU-GPIOBITS-*`). Everything here is a reading of code that was not run on the device, except
the register values quoted from the STAGE 36 run.

**How a `function-*` property is laid out** (words after the property's own header): the code keeps a
pointer to `[phandle, tag, word2, word3]`. For the LDO function the arguments are packed in
`word2` (`0x0708020b`: byte 0 = LDO index, bit 0 of byte 1 = initial on/off, bytes 2 and 3 =
a 15-bit value and one flag bit). For the GPIO function `word2` is the PMU pin number and `word3`
is four bytes (`0x02000101` for the touch clock: `01 01 00 02`).

**PMU GPIO configuration registers** (`0x50 + pin`, pins 0 to 7): the function that applies a
configuration reads the register, optionally overlays two bytes (mask, value) from the PMU node's
`gpio-pin-config` property (eight 4-byte entries; on this device only pin 5 is non-zero, so pin 0
has no overlay), keeps bits `0x19`, then sets bit 1 (`0x02`) for function 1 or bit 2 (`0x04`) for
function 2. A non-zero polarity flag then goes straight to the write. When the flag is zero, the code
flips bit 1 if the upper bits `0xE0` are all zero, and otherwise logs that polarity on an input is not
supported; since the mask `0x19` has just cleared those bits, that message looks unreachable in the
reconstructed flow, an observation without an explanation (`PMU-GPIOFN-Q3`). The byte written is rebuilt
(old, with the overlay, masked with `0x19`, plus the function bit); it is not in general the old byte
with one bit added. Another routine switches a pin between
input (it sets `0x20`) and output (upper bits zero). A third updates a register with a mask and a
value and can also update 3-bit per-pin fields packed in registers `0x59` to `0x5b`.

**Reading for the touch clock enable** (`function-clock_enable`: pin 0, bytes `01 01 00 02`): the
type byte (`+0xc`) is 1, so the default application is "function `0x00`, polarity flag `1`". A
later "set" with a value calls the same routine with function `value != 0` and the same flag, which
reads as: **asserting the clock enable makes the setter rebuild PMU register `0x50` with bit 1
(`0x02`) set**, and a non-zero flag means the code does not flip that bit afterwards. That is a byte
transformation and the absence of an inversion in the code; what it does electrically (the polarity of the
pin, whether it is driven, what it gates) was not established. STAGE 36 read register `0x50` as `0x11`
(bit 1 clear, bits `0xE0` clear); no electrical state is concluded from that byte. The rebuilt value
would be `0x13` (`0x11 & 0x19`, plus `0x02`), which equals `old | 0x02` only because the old byte has no
bits outside `0x19`. That the touch driver calls this "set with a value"
path for its clock enable is not established: only the descriptor and the path were read.
This is a reading of about thirty instructions. The jump table (`PMU-GPIOAPPLY-003`) maps the type
byte to: 0 an input direction, 1 an output direction, 2 and 3 a function taken from byte `+0xe` or
`+0xf`; the touch clock's type is 1, which takes the function path when no explicit type is passed.
Nothing was written.

**The LDO function** (`0x8075bf34`): reads the descriptor, then calls the setter once with the
index, the 15-bit value, the flag bit and the on/off bit taken from the descriptor (here `0`), and
logs a failure with the index plus one. The device tree's `ldo-suspend-defaults` is the four bytes
`00 40 00 00`: byte 1 has the bit `0x40`, the same bit as the touch LDO in register `0x11`. What the
property controls (a set to switch off, or to keep on, in suspend) was not read.

**The touch node's `reg`** (`0x0 0x7c 0x8010000 0x1388 0x2710 0x0 0x0 0x0`): not decoded. Two of the
words, 5000 and 10000, resemble the 5 and 10 of the iDroid driver's `{83000, 5, 10}` SPI setting
with the units changed from microseconds to nanoseconds; that is a numerical resemblance, not a
finding, and the first words (`0x7c`, `0x8010000`) have no counterpart there.

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
| Reset | GPIO `14.01` | level read `0` once; the descriptor bytes read as an active-low line asserted by default, consistent with that read (`SOC-GPIOFN-003`); the `reset-deassert-high-z` flag's effect was not read |
| Chip select | GPIO `13.00` (`function-enable_cs`) plus the SPI controller's own select | level read `0` once; role not established |
| SPI controller | `spi1` at CPU address `0x82100000` (`reg 0x2100000` plus the `0x80000000` offset of `arm-io`) | clock gate `0x16` reads `0` (off) in STAGE 37; with the gate set to `0xF` for a moment (STAGE 38) its idle registers read without a stall (SETUP `0x8`, PIN `0x3`, CLKDIV `1`, others `0`), then the gate was restored |
| Attention | interrupt `0xb4` on the GPIO interrupt controller | not read, the controller is treated as dangerous |
| Firmware | a bootload path named in the driver's strings | open: whether it is needed here is not established; source in the IPSW not looked up |

## W1 and W2 done (STAGE 38, bundle `20261016T165854Z-trace_spi1_gate`)

With the explicit go of the person holding the phone, a payload made two writes, both to the
`spi1` clock-gate register `0xBF1000D0` (`0xBF100078 + 4 * 0x16`), and nothing else: the register
read `0x00000000`; it was written with `0x0000000F` and read back `0x0000000F`; the idle registers
of `spi1` were then read; the register was written back to `0x00000000` and read `0x00000000`. The
nonce was confirmed and the phone was still reachable in Recovery mode afterwards.

| Register (offset from `0x82100000`) | Value read with the gate on |
|---|---|
| CONTROL `0x00` | `0x00000000` |
| SETUP `0x04` | `0x00000008` |
| STATUS `0x08` | `0x00000000` |
| PIN `0x0C` | `0x00000003` |
| CLKDIVIDER `0x30` | `0x00000001` |
| SPCNT `0x34` | `0x00000000` |
| SPIDD `0x38` | `0x00000000` |

What it shows: the write to a clock gate takes (0 to `0xF`, and back), which was not known before
(STAGE 23 had started from gates already at `0xF`); the block answers reads without stalling the
bus once its clock is on; the block at `0x82100000` answers reads. (An earlier version of this sentence attributed that to the device tree's gate `0x16`; see "Correction: which clock-gate numbers are physical", which withdraws it.) Values that are not
all zero and not all ones are what a live register block returns.

What it does not show: that this register layout is the one of this chip (it comes from OpeniBoot's
header for the S5L89xx family). In that header SETUP bit 3 is the master bit, and SETUP reads `0x8`,
which is consistent with a controller in master mode but is not a test of it. The meaning of PIN
`0x3` and CLKDIVIDER `1` was not decoded. Nothing was configured, and the clock was left off as
found.

## The SPI controller driver: start and the per-device setting (static, 2026-10-16)

Catalog: `AppleS5L8900X/` (`SPI-START-*`, `SPI-VALID-*`). The controller class
`AppleS5L8900XSPIController` lives in the kext `AppleS5L8900X` (extracted with the class name as the
search key, because the bundle identifier is a prefix of three kexts). Read in code, not run:

- `start` reads `spi-version` (the `spi1` node has `1`) and stores version-dependent constants:
  `0xf`, `0x4000`, `0x2000`, `0x10` and `0x40000f` for version 1, `0xd`, `0x1000`, `0x800`, `0x8` for
  version 0. They are not decoded (plausibly field widths and buffer sizes). It also looks up two clocks
  named `pclk` and `nclk` (64-bit values), reads the flag `internal-cs`, and maps the register block.
- A validation routine checks the setting a child device hands to the controller: a chip-select index
  below 4 (and, unless `internal-cs`, the chip-select function from the property `function-spi_cs<n>`,
  which is GPIO `13.00` for chip select 0 here), a non-zero period whose product with `nclk` is at most
  2048 * 10^9, and a word size of 8, 16 or 32 bits.

Reading further into the controller class (catalog `SPI-CONF-*`, `SPI-REG-*`, `SPI-XFER-*`) decodes
the setting structure and the registers it feeds. In the configuration routine, the structure is read
as: word 0 chip select; word 1 a period; byte 8 a polarity-like flag; byte 9 a phase-like flag;
byte `0xa` another flag; byte `0xb` the word size (8, 16 or 32); word 3 a delay; word 4 another delay.
It builds the SETUP value from them (a base `0x4000` or the like per `spi-version`, master bits
`0x18`, flag bits, and the word size shifted by 15 for version 1, the same shift OpeniBoot's header
gives), CLKDIVIDER as the ceiling of (period times `nclk`) over 10^9, and SPIDD as the ceiling of
(delay times `pclk`) over 10^9. That arithmetic reads as a period and delays in **nanoseconds** and two
clock values in **hertz**; the Hz reading of the clock values is an assumption.

STATUS and PIN are also decoded in the code: for `spi-version` 1 the TX FIFO count is
`(STATUS >> 6) & 0x1f` with a depth of 16, and the RX count is `(STATUS >> 11) & 0x1f`; bit 1 of PIN is
the software chip-select level (a flag clears it, so it is active low). The idle values read in STAGE 38
(STATUS `0`, PIN `3`) are compatible with that: empty FIFOs and a chip select left high. The register
offsets of OpeniBoot's header (`0x00`, `0x04`, `0x08`, `0x0C`, `0x30`, `0x34`, `0x38`) are the ones the
kernel code uses, plus `0x4c` for version 1.

The offsets in the validation routine (word 0, word 1, byte at offset `0xb`) match the first words
of the touch node's `reg` property, `0x0 0x7c 0x8010000 0x1388 0x2710 0 0 0`: chip select `0`, a period
of `0x7c` = 124, a word size of `8` and the flag bytes `00 00 01` in `0x8010000`, and two delays, 5000 and
10000. **The link from the property to the structure is still an inference from matching offsets**, not
a traced call chain (`SPI-VALID-Q1`, `SPI-CONF-Q1`). Under that inference, with the units above, the
touch controller would be addressed with a period of 124 ns (about 8 MHz if `nclk` is the divided
clock), 8-bit words, and delays of 5 and 10 microseconds, the 5 and 10 of the iDroid driver's setting
(there with a slower clock). The SETUP value the driver would build for it is `0x4018` (base `0x4000`,
master `0x18`, no flag bit, word size field 0), later ORed with `0x40` for a receive; that is a
computation from the inference, not an observed value.

Data path, transmit by FIFO (`SPI-XFER-002`, `-003`): bytes are written one at a time to register `0x10`
(TXDATA), up to the FIFO depth (16 for version 1); SETUP is then written with extra bits (`0x180`, and
`0x200000` for version 1) to start, the driver sleeps until an interrupt-set flag, SETUP is rewritten
without those bits, and the chip select is released. The chip select is either the controller's own PIN
bit (when the node has `internal-cs`) or the `function-spi_cs<n>` object called with `1` at the start and
`0` at the end. The `spi1` node has no `internal-cs` property, so its chip select is the GPIO `13.00`
function, not the PIN register. What `1` and `0` mean as pin levels depends on that GPIO function's own
flags (`0x10001` on `spi1`'s property, `0x101` on the touch node's `function-enable_cs`), which were not
decoded.

The interrupt service routine (`SPI-ISR-*`) reads as a full-duplex byte pump: each pass it reads STATUS, drains the
receive FIFO through RXDATA (`0x20`), stores or discards the bytes (a skip count discards the bytes clocked in
while the command is being sent), refills the transmit FIFO through TXDATA (`0x10`) with the command bytes and
then `0xFF` filler bytes to clock a reply out, and writes the STATUS value it read back to STATUS (which reads as
write-one-to-clear, not tested). At the end SETUP is rewritten without the start bits. So a transaction is a
write of N bytes followed by a read of M bytes clocked with `0xFF`. The DMA branch and the register at `0x4c`
were not read.

Two observations in the touch driver itself (`AppleMultitouchSPI`, strings and a few instructions, not a
traced path):

- It has a routine that, for the match name `multi-touch,z1` only, reads the current SPI clock period
  through a virtual method and, if it is `0x1c20` (7200), sets `0x2710` (10000) through another and logs
  "Manually setting spiClockPeriod to %uns for EVT hardware". It does not apply to this phone's `n88`
  match. It does show a period held and logged in nanoseconds in the touch driver, and values of
  7200 and 10000 for the older variant, against 124 in this node's `reg` read under the inference above.
  It also shows that `0x2710`, the fifth word of this node's `reg`, is a value the driver itself uses as a
  period in nanoseconds elsewhere, which makes the word-by-word reading of `reg` above less certain
  than it looked (`SPI-VALID-Q1`).
- Its clock routine logs "enabling clock", "done enabling clock" and "disabling clock" around a call of a
  virtual method of the object at `this+0x7e0` with the address of a local value. So the driver does
  switch the clock through the function object with a value, which supports the "set with a value" path
  of the PMU fiches; that this call reaches the PMU GPIO routine `0x8075bb8c` is not traced.

## The SoC GPIO function and the levels of the touch lines (static, 2026-10-16)

Catalog: `AppleS5L8900X/gpio_function_apply_0x8045b1f0.yaml` (`SOC-GPIOFN-*`). The class that handles
the SoC's own `function-*` GPIO properties reads the same four descriptor bytes as the PMU class, with
partly different roles. For a value it sets the pin level to `(polarity byte XOR 1) XOR value`: a
polarity byte of `1` means level = value, `0` means level = NOT value. By default, when the type byte is
`1`, it sets the level `polarity XOR initial XOR 1`, where the initial value is byte `+0xe`.

Applied to the touch node's lines, whose flags words are `0x10001` (bytes `01 00 01 00`) and `0x101`
(bytes `01 01 00 00`):

| Property | Pin | Bytes | Polarity | Initial value | Default level | Level for logical 1 |
|---|---|---|---|---|---|---|
| `function-reset` | 14.01 | `01 00 01 00` | 0 (inverted) | 1 | 0 | 0 |
| `function-spi_cs0` (of `spi1`) | 13.00 | `01 00 01 00` | 0 (inverted) | 1 | 0 | 0 |
| `function-enable_cs` (touch node) | 13.00 | `01 01 00 00` | 1 | 0 | 0 | 1 |

So by this reading the reset line is asserted at level `0`, and the chip-select is asserted at level `0`
when driven through `function-spi_cs0` (call with `1` at the start of a transfer, `0` at the end). The two
levels read in STAGE 33 were both `0`, which is what the default levels above predict; that is a
consistency between a reading and two reads, not a test of the polarity. Nothing was driven. The
controller methods that set and read a pin level (slots `0x394`, `0x398`, `0x39c`) were not read, and the
meaning of the bytes `+0xe` and `+0xf` is taken from this one routine.

## The touch driver's protocol (static, 2026-10-16)

Catalog: `AppleMultitouchSPI/` (`MT-CHECK-*`, `MT-BOOT-*`). Read from the kext that holds the N1 class, in code
that was not run. Its call chain from the N1 class was not traced (`MT-CHECK-Q1`).

- **A bootloader check that needs no firmware.** One routine builds the four bytes `05 00 00 06`, sends them
  to the SPI device with a four-byte receive buffer, and treats a first received byte of **`0xD0`** as "device
  is in its bootloader"; any other value is "not in bootloader", with a retry count and a short wait between
  tries. The last byte may be a checksum of the first three plus one; that is a guess.
- **A firmware check by a report request.** A second routine asks for report `0xD3` (named SENSORINFO by the
  iDroid driver) with a four-byte buffer and takes success as "firmware is running".
- **The bootload flow.** A timer handler tries a bootload method up to three times when no firmware is
  flagged, then tests whether firmware runs anyway, and otherwise gives up with "No firmware running, and
  couldn't load any". After a successful bootload it caches the device properties (family ID, sensor rows
  and columns, region descriptors, surface size, the interface version).
- **Not found.** The body of the bootload method: the packet format, the source of the firmware and its
  size (`MT-BOOT-Q1`). The strings suggest a property named "A-Speed Firmware" and a "preconstructed" packet
  type; the origin of that property (supplied from user space, or embedded) was not established.

What this changes for a first observation. If the controller powers up in its bootloader, a single
four-byte exchange, `05 00 00 06` out and four bytes back, would answer whether the first byte is `0xD0`. That
needs no firmware and no further protocol knowledge, and is attributable to the controller: the answer
is a chosen byte from a defined request. It does not need the bootload packets, and a reply that is not
`0xD0` (for instance firmware already running, or a controller that is not powered) would also be
informative only if the power, clock, reset and chip-select preconditions were met. The shape of the
exchange (how the transfer routine frames it, `MT-CHECK-Q2`) was not read, and nothing was sent.

## Shape of the CHECK exchange, and W3 + W4 prepared (STAGE 39, not run)

**Shape of the exchange.** The routine at `0x8056cbd8` that the CHECK code calls last is a log helper (it
tags the buffers "TX" and "RX"); the exchange itself is the call of slot `0x368` on the SPI device object
just before it, with the four packet bytes, a length of 4, a four-byte receive buffer, a length of 4 and a
zero flag (`MT-CHECK-002`; the slot's body was not read). In the controller (`SPI-XFER-004`), equal lengths
mean no skipped byte and no filler: the byte received at position `i` is clocked in while byte `i` of the
packet is clocked out. So the expected `0xD0` would arrive during the first byte of `05 00 00 06`, with the
chip select (GPIO `13.00`) held for the four bytes. This is the reading of both routines, untested.

**W3 + W4 as prepared** (`tools/payloads/trace.c`, `STAGE 39`, `trace_pmu_w34.bin`; built, not run). Its
only writes are to two PMU registers at `0x74`, each changed once and restored once, behind a guard:

| Step | Register | Guard / value | Restore |
|---|---|---|---|
| guard | reads `0x11` and `0x50` | nothing is written unless `0x11` reads `0xBE`, `0x50` reads `0x11` and both reads are acknowledged | n/a |
| W4 | `0x11` | `old | 0x40` (`0xBE` to `0xFE`) | write the value read at the start |
| W3 | `0x50` | `(old & 0x19) | 0x02` (`0x11` to `0x13`) | write the value read at the start |
| observe | read both back, wait 20 ms, read both again | | |
| restore | `0x50` first, then `0x11` (reverse order), read both | | |

No SoC GPIO, clock gate or SPI register is touched, and the SPI clock stays off. The disassembly shows the
guard (`0xBE` and `0x11`), the W4 value (`old | 0x40`), the W3 value (`(old & 0x19) | 0x02`) and four PMU
writes in all.

What this run could show: whether the PMU accepts the two writes (read-back), and whether they persist for
20 ms. What it cannot show: that the touch controller is powered (nothing talks to it), that the registers
mean what the static reading says, or what the LDO and the pin do electrically. Risks: the bit `0x40` of
register `0x11` may not be the touch LDO's enable as read, in which case another rail is switched for
about 20 ms and then restored; the clock-enable pin may gate something else; a PMU register that does not
behave as read could leave a rail changed, which a return to DFU resets. The guard stops the run if the
registers do not read as in STAGE 36.

## W3 and W4 done (STAGE 39, bundle `20261016T173129Z-trace_pmu_w34`)

With the explicit go of the person holding the phone, the payload prepared above ran (nonce confirmed), under
its guard, and made its four PMU writes:

| Register | Before | Written | Read back | 20 ms later | Restored and read |
|---|---|---|---|---|---|
| `0x11` | `0xBE` | `0xFE` (W4, bit `0x40`) | `0xFE` | `0xFE` | `0xBE` |
| `0x50` | `0x11` | `0x13` (W3, `(0x11 & 0x19) | 0x02`) | `0x13` | `0x13` | `0x11` |

All writes were acknowledged, the guard passed, and the phone was still reachable in Recovery mode afterwards.
What it shows: the PMU accepts both writes and keeps them for at least 20 ms, and the values it holds are
exactly the ones the static reading computed. What it does not show: that the touch controller was powered or
clocked (nothing was connected to it, and the SPI clock stayed off), that bit `0x40` of `0x11` is its supply
or `0x50` its clock enable, or any electrical effect. Both registers were left as found.

## Dry run of the power-up sequence, prepared (STAGE 40, not run)

`tools/payloads/trace.c` `STAGE 40` (`trace_touch_power.bin`, built, **not run**). It makes the four steps of the plan
that need the three supplies at once, **without sending any packet** to the touch controller, then restores
everything:

| Step | Write | Value | Restore |
|---|---|---|---|
| W1 | `spi1` clock gate `0xBF1000D0` | `0` to `0xF` | write the value read at the start |
| W4 | PMU `0x11` | `0xBE` to `0xFE` | value read at the start |
| W3 | PMU `0x50` | `0x11` to `0x13` | value read at the start |
| settle | wait 20 ms | | |
| W5 | SoC GPIO `14.01` (touch reset) register at `0x83000284` | `0x212` to `0x210` (released to input, the "high impedance" the personality flag suggests) | back to `0x212` (driven low) |
| observe | wait 15 ms, then read only: the reset and chip-select registers, SPI1 STATUS, PIN, SETUP, CLKDIVIDER | | |
| restore | reset GPIO, PMU `0x50`, PMU `0x11`, clock gate (reverse order), then read all four | | |

Guard: nothing is written unless PMU `0x11` reads `0xBE`, PMU `0x50` reads `0x11`, the clock gate reads `0` and the reset GPIO
register reads `0x212` (the values of STAGES 33, 36 and 37). The disassembly shows two stores to the clock gate (on, restore), the
four PMU writes through the helper, one `i2c_release` and one `i2c_low` on the reset GPIO address `0x83000284`, and the usual
I2C pin drive on `0x83000080` and `0x83000084`. No chip-select, SPI data or other GPIO register is written, and no SPI byte is sent.

What it could show: whether the reset line reads high once released (a pull-up exists) or stays low; whether the SPI controller's
idle registers change once the supplies are on (they should not without a transfer); and that the whole sequence is
reversible. What it cannot show: that the controller is powered or alive (nothing is exchanged), the polarity, the unit of the
driver's delay (`0xf`; 15 ms is the iDroid driver's value, and a shorter real delay would only make this one longer), or what
happens on an actual CHECK.

Risks beyond those of W3 and W4: with the reset released the controller may start its bootloader and drive its attention
line, an interrupt this project does not read; a floating reset line could pick up noise for 15 ms; the gate and PMU states
are restored at the end, a return to DFU resets them if the run is interrupted.

## Power-up dry run done (STAGE 40, bundle `20261016T173428Z-trace_touch_power`)

With the explicit go of the person holding the phone, the sequence prepared above ran (nonce confirmed) under its guard,
with no SPI byte sent, and everything was restored:

| Item | Before | During (supplies on, reset released, 35 ms in) | After restore |
|---|---|---|---|
| clock gate `0x16` | `0x0` | `0xF` | `0x0` |
| PMU `0x11` / `0x50` | `0xBE` / `0x11` | `0xFE` / `0x13` | `0xBE` / `0x11` |
| touch reset GPIO `14.01` register | `0x212` | **`0x211`** | `0x212` |
| chip-select GPIO `13.00` register | `0x212` | `0x212` | not rechecked |
| SPI1 STATUS / PIN / SETUP / CLKDIVIDER | `0 / 3 / 8 / 1` | `0 / 3 / 8 / 1` | n/a |

All PMU writes were acknowledged and the phone was still in Recovery mode afterwards.

What it shows: the whole sequence runs and is reversible; the SPI controller's idle registers do not change with the supplies on
(as expected without a transfer); and **the reset line reads high once it is released** (register `0x211`: input, level 1,
against `0x212` when driven low). A line that reads high after being released to an input is held up by something: a pull-up on the
board, or the touch controller driving it. This run cannot tell which: the supplies were on, so it is not a control. What it
does not show: that the controller is alive, powered, or out of reset (nothing was exchanged, and a pull-up would give the same
reading), the polarity, or the unit of the driver's delay. The chip select stayed driven low throughout (it was not touched).

A cheap control separates the two readings of the high level: release the reset line with **no** power applied. If it still
reads high, there is a pull-up independent of the controller and this observation carries no information about the controller; if
it reads low, the high level above came with the supplies, which would be a first sign of the controller itself.

## Control for the reset reading, done (STAGE 41, bundle `20261016T173630Z-trace_touch_rst_ctl`)

With the explicit go of the person holding the phone, `STAGE 41` ran (nonce confirmed, guard passed). Its only write was the touch
reset GPIO register (`0x83000284`): `0x212` to `0x210` (released to input), read at once and 15 ms later, then restored to `0x212`.
The clock gate and the PMU registers were only read, and ended as found (gate `0`, `0xBE`, `0x11`). The phone was still in Recovery
mode afterwards.

| Reset register `0x83000284` | Supplies | Value | Level |
|---|---|---|---|
| released, at once | **none** | `0x210` | 0 |
| released, 15 ms later | **none** | `0x210` | 0 |
| released, 35 ms after the supplies went on (STAGE 40) | gate, PMU `0x11` and `0x50` | `0x211` | **1** |

What it shows: with no supply, the released line reads low, so **there is no pull-up on the line that works without the touch
supplies**. The high level of STAGE 40 appeared only with the supplies on. Something connected to that line is therefore powered by
what the sequence enabled: a pull-up to the rail that the touch LDO feeds (which would also read high with no controller at all), or the
controller driving the line itself. The control cannot tell those two apart. It also does not say which of the three supplies of STAGE
40 (clock gate, PMU `0x11` bit `0x40`, PMU `0x50`) mattered, since they were applied together. It is a modest piece of support for the
reading that the register bits written do power part of the touch domain; it is not evidence that the controller is alive.

A narrower run would apply only the LDO (PMU `0x11`) and then release the reset, to see whether the LDO alone raises the line. That
isolates the supply that is plausibly responsible and needs no SPI traffic.

## LDO-only run, done (STAGE 42, bundle `20261016T173946Z-trace_touch_ldo`)

With the explicit go of the person holding the phone, `STAGE 42` ran (nonce confirmed, guard passed): PMU `0x11` `0xBE` to `0xFE`, 20 ms,
the touch reset register `0x83000284` `0x212` to `0x210`, read at once and after 15 ms, then reset and LDO restored. The clock gate and PMU
`0x50` were not written and stayed `0x0` and `0x11`. Everything ended as found and the phone was still in Recovery mode.

| Reset register `0x83000284`, line released | Supplies | Value | Level |
|---|---|---|---|
| at once | none (STAGE 41) | `0x210` | 0 |
| 15 ms later | none (STAGE 41) | `0x210` | 0 |
| at once | **LDO only** (STAGE 42) | `0x210` | **0** |
| 15 ms later | **LDO only** (STAGE 42) | `0x211` | **1** |
| about 35 ms after power-up | LDO, PMU `0x50` and clock gate (STAGE 40) | `0x211` | 1 |

What it shows: the LDO alone is enough for the line to go high once released, with neither the PMU clock enable nor the SPI clock gate; and it
**does not go high at once**, but within 15 ms (low at the first read, high at the second). The chip-select register stayed `0x212`
(driven low, untouched). A line that rises over milliseconds after release fits a controller finishing its own power-on or reset sequence, and
it also fits a pull-up working against a large capacitance; this run cannot tell them apart, and the first read came microseconds after the
release, so "not at once" says only that the rise takes more than that. It is still not an exchange with the controller: nothing was sent, and
the reading is one line at two instants.

What would sharpen it: several reads between 0 and 15 ms and beyond (the rise time), and the same with the LDO on for longer or shorter before the
release. Both stay read-only apart from the reset and the LDO already used.

## Why W6 (the CHECK exchange) is not ready

Preparing W6 against the OpeniBoot SPI driver for this SoC family and the kernel code read above showed three gaps, none of which a
payload should paper over:

1. **The SPI pads are not known.** (Update: the iBSS's pad table was read and gives no SPI function; see "The iBSS pad configuration table".) The controller drives its clock, data-out and data-in pads only if the corresponding GPIO pads are
   set to the SPI function. The `spi1` node lists only `function-spi_cs0` (GPIO `13.00`). No source read so far names the pads of
   SCLK, MOSI and MISO for `spi1`, nor says whether iBoot has already set them to the SPI function at this stage of the boot. OpeniBoot's
   header for this SoC family has no pad definitions for the SPI buses (its `plat-s5l8900` header has some for an older chip, where the
   same bus also carries the LCD panel and the touch chip-select). A transfer programmed on the controller would otherwise reach nothing.
2. **The register recipe for a transfer is not settled.** OpeniBoot sets CONTROL `0`, then SETUP (master `0x18`, plus `0x20`, plus
   `0x4000` for the clock source), CLKDIVIDER, PIN `2`, CONTROL `1`; to send it ORs `0x200100` into SETUP, writes the length to `0x4c`, sets
   CONTROL bits `4` and `8` first, and rewrites CONTROL to `1`. The kernel code read above builds SETUP without the `0x20`, writes
   CONTROL from a field that nothing in the part read assigns (so it reads as `0`), and starts with `0x180` plus `0x200000` in SETUP. Which
   of the two is right for this chip, or whether both work, is not shown. The clock divider also depends on the `nclk` frequency, which was not
   read (`nclk` = 24 MHz would make a divider of 3 give 8 MHz, matching the 124 ns period, but that is a coincidence of two guesses).
3. **The chip-select call has no checked semantics.** The `function-spi_cs0` object is called with `1` at the start of a transfer and `0`
   at the end, and the SoC GPIO function reads that as level 0 then 1 (active low). That reading has not been exercised on the pin.

The full GPIO survey (hardware map, bundle `20261016T174429Z-trace_gpio_survey`) did not close gap 1: it validates the bit layout on known pads
but names no group of pads as the SPI1 clock and data lines.

A safe way to settle 1 and 2 before any exchange with the touch controller is a self-test of the controller with no peripheral: clock gate on,
chip select untouched, send four bytes, and watch STATUS (the TX count draining, the RX count filling) to see whether the recipe works. For 1,
a read-only dump of the GPIO registers, to look for pads whose function bits differ from plain input or output, is cheap and was done before for
376 pins without incident.

## Correction: which clock-gate numbers are physical

Three sources number the clock gates differently, and this project has used two of them without saying so:

| Block | Device tree `clock-gates` | iBSS table (decrypted, 4.3.5 n88ap) | OpeniBoot `plat-s5l8920` |
|---|---|---|---|
| SPI0, SPI1, SPI2 | `0x15`, `0x16`, `0x17` | `0x09`, `0x0a`, `0x0b` (catalog `IBSS-TAB-001`) | `0x09`, `0x0a`, `0x0b` (SPI3, SPI4: `0x0c`, `0x0d`) |
| I2C0 | `0x20` | `0x13` (`IBSS-TAB-002`) | `0x24` |
| NAND controller 0 | `0x13`, `0x14` | not read | `0x26` |
| IOP | `0x27` | not read | not listed |

The iBSS and OpeniBoot agree for SPI; the device tree differs. Observations: gates `0x09` to `0x0D` read `0xF` and `0x15` to `0x17` read
`0` (STAGE 37), and the `spi1` registers read the same values **with no gate write at all** (STAGE 44, bundle
`20261016T174843Z-trace_spi1_nogate`: gate `0x0a` = `0xF`, gate `0x16` = `0`, SETUP `0x8`, PIN `0x3`, CLKDIVIDER `1`, the others `0`).

What this corrects:

- The statement in the "W1 and W2 done" section that the device tree's gate `0x16` rendered the block readable is **withdrawn**. STAGE 38
  showed that a write to a clock-gate register takes (physical register number `0x16`, `0` to `0xF` and back); it did not show that this register
  gates `spi1`. What physical gate `0x16` controls is unknown.
- The `spi1` clock was already on (the iBSS's gate `0x0a` read `0xF`), so **no clock-gate write is needed for an SPI exchange** and W1 is
  unnecessary on that account. STAGES 40 and 42 wrote or did not write gate `0x16`; either way it was not what made `spi1` readable.
- The reading of the device tree's numbers as physical register indexes is not supported where it was used elsewhere in this project: STAGE 28
  read gate `0x27` (the device tree's number for the IOP) as `0xF` and concluded the IOP clock was on, and STAGE 23 used the device tree's `0x10` for the
  SDIO block. The physical gate at those indexes may belong to other blocks (OpeniBoot puts a NAND controller at `0x26`, enabled by iBoot).
  Those conclusions need review before they are relied on; they are listed as open.

## The iBSS pad configuration table (static, 2026-10-16)

Catalog: `iBSS_n88ap_4.3.5/gpio_configure_0x84001354.yaml` (`IBSS-GPIO-*`). The decrypted iBSS has a routine that configures one pad from
a pin id (port in the upper byte, pin in the low bits) and a mode from 0 to 7. The address is `0x83000000 + 4 * (8 * port + pin)`, formed by
adding `-0x7d000000`, so no `0x8300xxxx` literal appears in the image. The modes write the patterns the project's I2C code already
uses (`0x210` released, `0x212` driven low, `0x213` driven high) and a few more (`0x230`, `0x250`, `0x270`); mode 4 takes its value from a
376-entry table of 16-bit words at `0x84017690`, one per pad. That table is therefore the iBSS's default configuration of every pad.

What this gives and does not give:

- The survey of the pad registers (STAGE 43) and this table agree on 175 of 376 pads in their low ten bits and differ on 201: the iBSS
  reconfigures pads after the table, and the dedicated pads of ports `1C` to `24` and the unimplemented ports `25` to `2E` read differently from
  their table entries. So the table is an initial state, not the state seen.
- No table value was identified as the SPI1 clock or data pad function. The values other than the plain GPIO modes (`0x290`, `0x310`, `0x250`,
  `0x330`, `0x6B0`, `0x630`, `0x09E`, `0x11E`, `0x41E`, `0x610`) have no documented meaning here.
- The device tree lists a chip-select function for `spi1` but no clock or data pad, and the UART transmit pads (`07.06`, `08.06`) are GPIO ports in the tree. One
  reading that fits all of this is that the SPI clock and data pads are dedicated pins outside the GPIO block, so that no pad configuration is needed for
  them; **this is a hypothesis**, not shown, and a transfer that returns nothing would not distinguish it from a missing pad configuration.

## SPI1 controller self-test, kernel recipe (STAGE 45, device, 2026-10-16)

Bundle `20261016T190939Z-trace_spi1_selftest`, nonce confirmed. No peripheral was powered; the only writes went to the `spi1` register block
(`0x82100000`), after a guard (gate `0x0A` = `0xF` and the seven idle registers at their STAGE 44 values; it passed), and all were restored.
The recipe was the kernel's: `CLKDIVIDER` 240, `STATUS` `0x40000f`, `SETUP` `0x4018`, `CONTROL` 0, `SPCNT` 4, register `0x4C` 4, four bytes
`05 00 00 06` into `TXDATA`, then `SETUP` `0x4018 | 0x180 | 0x200000`.

Observed: `STATUS` read 0 before the start, at 0, 50 us, 1 ms, 5 ms and at the end; no byte in the receive FIFO (`nrx` 0); the transmit counter did not rise
after the four `TXDATA` writes. `SPCNT` and register `0x4C` held 4, and `SETUP` read `0x4018` afterwards (the start bits did not stay). After the restore
every register read its initial value. The VIC0 word read 0 before and after; it is not an enable register as assumed, so the concern about the SPI1 interrupt
was neither confirmed nor excluded by this run.

What it shows: with `CONTROL` at 0, writes to `TXDATA` did not register in the transmit counter and the start produced no observable transfer. What it does
not show: why. A controller left disabled by `CONTROL` 0 fits (OpeniBoot sets `CONTROL` to 1), but the order of writes, register `0x4C` or a missing `SETUP` bit are not
excluded. STAGE 46 (OpeniBoot's recipe: `CONTROL` 0 then 1, `PIN` 2, `CONTROL |= 4 | 8`, `SETUP |= 0x200100`, `0x4C` as the transmit count) is built
(`trace_spi1_selftest2.bin`) but **has not been run**: the phone's battery ran flat and it no longer enumerates.

## The iBSS clock initialisation (static, 2026-10-16)

Catalog: `iBSS_n88ap_4.3.5/clock_gate_init_0x8400dd58.yaml` (`IBSS-CLK-*`). The decrypted iBSS has a routine (`0x8400dd04`) that takes a 64-bit mask and a flag
and, for each set bit `i` from 0 to `0x33`, sets (flag nonzero) or clears the low nibble of the word at `0xBF100078 + 4 * i`. Its wrapper (`0x8400dd58`) calls it
twice: gates `0x00`, `0x18`, `0x24`, `0x2C`, `0x2D` and `0x31` on, then every other gate of the 52 off. The same code path stores the base
frequency `0x16E3600` (24 000 000) before calling it, which is the only place the 24 MHz figure appears in this image (the device-tree-independent source of the
"24 MHz" assumed in the SPI divider arithmetic; whether it is the SPI source clock is not shown).

What this gives, and what it does not:

- It confirms the physical meaning of the gate register array: 52 words, a low nibble `0xF` or `0`, indexed by the number the iBSS uses.
- **Prediction, not an observation:** after this routine, only those six gates are on, unless something turns more on later. The gates read so far agree for
  `0x15` to `0x17` (read `0`) and disagree for `0x09` to `0x0D`, `0x19` and `0x27` (read `0xF`): something turned those on afterwards. Which code did is open (`IBSS-CLK-Q2`).
- Notably, `0x15` to `0x17`, the device tree's SPI gates, are among those the routine turns off, and the iBSS's own SPI gates (`0x09` to `0x0B`) are not in the list it
  leaves on. The numbering correction above stands; this adds a second reason to distrust the device tree's numbers as physical indexes, without proving it.
- A read-only survey of all 52 words (`trace_gate_survey.bin`, STAGE 47) is built and **not run**; its result would confirm or refute the prediction.

## Candidate write sequence (a proposal, nothing here is authorised or done)

Each line is one write, to be proposed again, with its exact value, before it is made. The values
come from the readings above; none of the write semantics was ever exercised on this device.

| # | Write | Address and value | Current | How to undo | What it can show | Main risk |
|---|---|---|---|---|---|---|
| W1 (done, STAGE 38; **not needed**, see the correction) | gate register `0x16` | `0xBF100078 + 4 * 0x16` = `0xBF1000D0`, `0` to `0xF` (the pattern read on enabled gates) | `0` | write `0` | whether the gate takes the write (the only earlier gate-write test, STAGE 23, was uninformative: both gates already read `0xF`), then the idle registers of `spi1` become readable | enabling the clock of a block in an unknown state; the numbering of gates is the device tree's, not tested |
| W2 (done, STAGE 38) | read only | `spi1` CONTROL, SETUP, STATUS, PIN, CLKDIVIDER, SPCNT, SPIDD at `0x82100000` (STAGE 37, second half) | not read | none needed | the reset state of the controller | none beyond W1 |
| W3 (done, STAGE 39) | PMU GPIO 0 (touch clock enable) | PMU register `0x50`, `0x11` to `0x13` (rebuilt as `(old & 0x19) | 0x02`; equal to `old | 0x02` only for this old value) | `0x11` | write `0x11` | nothing visible alone; read-back only | changes the level of a PMU pin whose load is only known from the device tree |
| W4 (done, STAGE 39) | touch LDO | PMU register `0x11`, `0xBE` to `0xFE` (set bit `0x40`), read-modify-write | `0xBE` | write `0xBE` | nothing visible alone except the read-back and possibly the controller's behaviour on SPI | powers a rail; if the index-to-rail reading is wrong, a different rail is switched |
| W5 (done in the dry run, STAGE 40) | reset line | SoC GPIO `14.01` released (logical 0, which the descriptor reads as level 1, or high impedance as the personality's flag suggests), then the driver's delay (`0xf`, unit unknown) | level `0` (asserted by the reading) | drive low again | a change on SPI/attention if the controller starts | the polarity rests on a code reading; the unit of the delay is unknown |
| W6 (blocked: see "Why W6 is not ready") | first SPI exchange | the CHECK packet `05 00 00 06` (reply first byte `0xD0` = bootloader), `spi1` registers, chip select `13.00`; if the `reg` reading above holds, word size 8 and a period of about 124 ns | n/a | n/a | any answer | requires the controller's register layout confirmed on this chip (CLKDIVIDER and PIN undecoded), the `reg` inference confirmed, and the bootloader protocol |

The order that keeps the risk lowest is W1 and then W2, and stopping to read the result before
anything touches the PMU. The PMU writes (W3, W4) follow the driver's apparent order of power first
and reset after, but the apparent order is a reading of strings, not of the control flow.

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
