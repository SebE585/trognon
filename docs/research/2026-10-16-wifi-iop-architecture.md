# The 3GS has a second, embedded IOP processor; it likely — not yet proven — owns Wi-Fi/SDIO

Research date: 2026-10-16. This moves the SDIO investigation started on
2026-10-15 (`docs/research/2026-10-15-touch-wifi-nand-recon.md`) a long way
forward, but does not close it: after extensive register-level probing from
Trognon's own payloads produced only clean negatives, binary reverse
engineering of the device's own firmware (extracted and decrypted from the
owner's IPSW, never redistributed) found a strong architectural candidate
explanation. A second review (see "Known gaps" below) correctly pointed out
that the evidence so far establishes the architecture, not yet the causal
chain for the specific symptom observed on 2026-10-15/16. This document
states both clearly, rather than overclaiming the second.

## Bottom line

The S5L8920 has a second, embedded ARMv7 processor — Apple calls it an
"IOP" (I/O Processor) — running its own small real-time OS with its own
scheduler, heap, exception handlers and interrupt controller. **This part is
established.** What is still a working hypothesis, strongly supported but
not proven: that this second processor, not the application CPU Trognon's
payloads run on, is the exclusive owner of the SDIO/Wi-Fi host controller's
registers, and that this is specifically why those registers read back zero
and ignored writes on 2026-10-15/16. The application CPU does post messages
to the IOP over a shared command channel and wait for a reply — that part is
confirmed from the disassembled driver. The same IOP firmware also drives
the NAND controller (`h2fmi`) and the AES-capable DMA engine (CDMA). The
leading explanation for the 2026-10-15/16 negatives is that every SDIO
register write attempted from the application CPU reached an address whose
real owner never received a wake-up signal — not a wrong register, a wrong
processor — but this causal link has not itself been traced; see "Known
gaps" below.

## How this was found: a second opinion, then a real disassembler

After three clean, carefully isolated negatives from direct register
experiments (two clock-gate hypotheses, a full SDHC-style init sequence, and
a raw write-then-readback test that showed the write never landed; see the
2026-10-15/16 journal), a summary of the investigation was given to another
LLM for review. Three corrections came back, all valid:

1. A zero readback does not by itself prove a write was ignored — an MMIO
   register can be write-only, carry reserved bits, or trigger an action,
   and the AMBA AXI specification explicitly treats these as different from
   plain memory behaviour.
2. Two incompatible register-layout hypotheses had been mixed without
   noticing: writes used the older, simple `CTRL`/`CMD` layout (from
   `plat-s5l8900`/`iDroid-kernel`), while the "the Capabilities register
   should read non-zero" argument assumed an SDHCI-standard layout. Only one
   of the two can be right.
3. The most important: the MMU page table cited as evidence was the one
   **OpeniBoot** builds for itself at its own boot — but OpeniBoot never
   runs on this device. Trognon's payloads execute inside Apple's own
   patched iBSS, whose real MMU state had never been captured directly.

The review's own strongest lead: real iOS kernelcache restore logs for the
3GS name an actual Apple driver class, and that kernelcache is inside the
IPSW already in hand.

## Extracting and reading the real driver

The kernelcache (`kernelcache.release.n88` inside
`iPhone2,1_4.3.5_8L1_Restore.ipsw`, SHA-256
`ed3a62f4c581a8f500303d4d8e0976dd84bfb5d1f39c83d884b6564a7ecb4ab2` for the
IPSW) is an encrypted IMG3 container, same shape as the device tree handled
on 2026-10-15: `TYPE`/`DATA`/`SEPO`/`KBAG`x2/`SHSH`/`CERT` tags, `DATA`
encrypted (`KBAG` present). Decrypted the same way, with the 3GS back in
pwned DFU (not Recovery): `axi0mX/ipwndfu`'s `image3.py`,
`Image3.getDecryptedPayload()` -> `dfuexec.PwnedDFUDevice().decrypt_keybag()`
(the chip's own hardware AES/GID primitive, a pure read, nothing written).
The decrypted payload (SHA-256
`487bcf47e494c1cd3e57f347e9b1a8cba90576c78b6a0752e0b2a60a76ed4a00`) starts
with the ASCII tag `complzss`: Apple's standard LZSS kernelcache
compression. No existing decompressor was available locally, so one was
written from the publicly documented algorithm (4096-byte dictionary
initialised to spaces, 18-byte max match, length = 3 + low nibble, exact
parameters confirmed against `malus-security/lzssdec`'s own README rather
than guessed) and validated: it reproduced the exact expected uncompressed
size (10,047,488 bytes) and produced a structurally valid ARMv7 Mach-O.

The kernelcache's own `__PRELINK_INFO` plist lists the real driver class:
**`AppleIOPSDIO`**, matching `IONameMatch = "sdio,s5l8920x"` (the exact
`compatible` string already found in the device tree on 2026-10-15) and
`IOProviderClass = "AppleARMIODevice"`, loaded at `0x80579000`. Its
`__PRELINK_TEXT` slice is itself a small, complete Mach-O (a prelinked kext),
parsed directly rather than guessed at (segment/section load commands read
with a small script): `__text` at `0x8057a000` (0x343c bytes), `__cstring`
at `0x8057d43c` (0x175d bytes).

A first pass with `arm-none-eabi-objdump` (`-b binary -m arm -M
force-thumb`) decoded cleanly from the start, confirming the code is Thumb
throughout this function range, but manual string-cross-reference by
literal-pool scanning (first by hand, then checked twice) found zero
matches against the target debug strings — not a tooling limitation, an
off-by-8-bytes error in how the target string's own address had been
computed (the substring searched for was offset into the real string, not
its start). Once `radare2` was installed (`brew install radare2`) and run
with full analysis (`aaa`), its own `axt` cross-reference command found the
right function immediately once the correct string address was used:
`fcn.8057c424`.

## What `sendIOPCommand` actually does

Disassembled in full (`radare2`, Thumb, relocations applied). In short:

1. Checks the command pointer passed in against a stored "allocated buffer"
   pointer (`this+0x14`); mismatches fail with "Command is not from the
   command buffer."
2. Checks it is running on the right IOKit workloop gate (two virtual calls
   through the object at `this+0x18`); failing that logs "Must have
   workloop gate to send a command."
3. Fills in the command structure: writes the opcode byte, sets a
   "pending" flag, stores the current command pointer at `this+0x30`.
4. Logs `"Sending command @ vaddr = %p, paddr = 0x%lX, opcode = %lu"`
   (confirming the command descriptor itself carries a virtual *and* a
   physical address — this is a real shared-memory message, not a register
   poke).
5. **The actual send** is a virtual call through a *different* object, at
   `this+0x24`, vtable slot `+0x84` — this is the one that actually talks
   to hardware; `AppleIOPSDIO` itself never touches the SDHC registers for
   this step.
6. On success, loops re-reading `this+0x30` until it is cleared — presumably
   by the completion handler (`handleIOProcCmdResp`, named in the kext's own
   strings) once the IOP actually answers.

## The generic channel, and who else uses it

The object at `this+0x24` is backed by a genuinely separate kernel
extension, confirmed by searching the full kernelcache's `__PRELINK_INFO`
(not just this one kext) for every driver that lists it as a library
dependency:

| `OSBundleLibraries` client of `com.apple.driver.IOSlaveProcessor` | Role |
|---|---|
| `com.apple.driver.AppleARM7M` | names an ARM7-class core directly |
| `com.apple.driver.AppleIOPFMI` | the NAND/flash driver (`h2fmi`) |
| `com.apple.driver.AppleIOPSDIO` | this investigation's Wi-Fi/SDIO driver |
| `com.apple.driver.EmbeddedIOP` | the generic IOP management framework |
| `com.apple.driver.IOP_s5l8920x_firmware` | **the firmware image itself** |

`IOP_s5l8920x_firmware` is not a thin wrapper: `_PrelinkExecutableSize =
0x1d000` (116 KB), `IOProviderClass = IOResources` (a global resource, not a
per-device driver). Extracted (SHA-256
`b8af809426e15cd23b5247a15c7ee950843aa893f8bdc7b4e2cc9e018100cd37`), it is a
real, separate ARMv7 Mach-O (`cputype=12`, `cpusubtype=9`), self-identifying
as `"EmbeddedIOP for s5l8920x, Copyright 2010, Apple Inc."`, built alongside
`iBoot-1072.61`.

Its own debug strings strongly support this reading, but by themselves they
establish a dialogue, not yet a proven causal chain to the specific symptom:

- `"SDHC: Ping received, IOKit <--> IOP Connection established"` — this
  establishes that a ping/connection handshake exists between IOKit and the
  IOP firmware for SDHC. `sendIOPCommand`'s special handling of opcode 0
  (seen above) is a plausible match for this ping. **What is not yet shown**:
  whether this handshake is itself what activates the SDHC hardware, or
  merely opens a logical connection on top of hardware that is initialised
  some other way — that distinction requires tracing the ping's handler
  (transport used, expected message, response, and which initialisations it
  actually triggers), not inferring it from the log line's wording.
- `sdiodrv_resetSDHC`, `sdiodrv_sendCommand`, `sdiodrv_performDMA`,
  `sdiodrv_completeTransfer`: a real low-level SD protocol driver, using the
  exact register names Trognon's own hypotheses used (`cmd`, `arg`, `resp0`,
  `normalInt`, `errorInt`, `state`), runs on the IOP. That it runs there is
  established; that the application CPU is *excluded* from reaching the same
  registers, and that this exclusion is specifically why 2026-10-15/16's
  reads came back zero, is plausible but not proven by this string alone.
- `h2fmi_iop_read_multiple`, `h2fmi_erase_blocks`, `h2fmi_nand_reset_all`,
  and the rest of the `h2fmi_*` family: NAND's real driver lives here too.
  One possible explanation for why Trognon's own NAND chip-ID read
  (2026-10-15) worked without ever pinging anything is that NAND is needed
  to boot iOS and so the IOP's NAND component is already live by the time
  Recovery mode is reached. **This is not established**: the needs of a
  normal iOS boot do not by themselves prove the IOP's actual state along
  Trognon's own tethered DFU/iBSS path, which skips most of that normal
  boot. The chip-ID read succeeding is a fact; why it succeeded is still a
  hypothesis.
- `drivers/apple/cdma/cdma.c`: the AES-capable DMA engine already seen
  referenced in `h2fmi.c`'s source is also IOP-managed.
- `task_create`, `task_yield`, `idle_task_routine`, a heap allocator, ARM
  exception-mode dump strings (`"ARM %s abort in %s mode at 0x%08x"`), and
  its own interrupt-enable/status registers (`rVICINTENABLE`,
  `rVICIRQSTATUS`, distinct from the application CPU's own VIC0/1/2): this
  part is a plain reading of the firmware's own strings, not an inference —
  a complete small embedded OS, not a thin relay.

## What this settles, and what it reopens

What is now established: the S5L8920 has a second embedded processor
running real, separate firmware, and a message-passing dialogue exists
between the application CPU's `AppleIOPSDIO` driver and that firmware for
SDIO. What is not yet established: that this is *why* `0x80000000` ignored
reads and writes from the application CPU on 2026-10-15/16. The architecture
makes that explanation plausible and is now the leading hypothesis, ahead of
the clock-gate and init-sequence hypotheses already ruled out — but it has
not itself been traced end to end.

The ambiguous dynamic-clock-16 read from 2026-10-15 (`0xF18` instead of an
expected `0x718`) is in the same position: the IOP architecture makes it
plausible that clocking this block is not something the application CPU's
clock-gate table alone controls end to end, but this has not been confirmed
either.

### Known gaps (raised by a second review, 2026-10-16)

A second review of this writeup, after the IOP finding, flagged exactly the
two points above as conclusions stated too strongly, and named the decisive
next step precisely: **trace the ping's own handler** — which transport it
uses, what message it expects, what it replies, and which initialisations
it actually triggers before and after the "Connection established" log
line. Until that is traced, "the application CPU cannot reach the block
because only the IOP can" remains the leading hypothesis, not a proven
fact, and the two softened bullets above should be read as open questions,
not settled ones.

### The ping handler, traced: it does not activate hardware

Following a detailed six-step plan (from the same review) for rebuilding
the IOP dialogue, step 2 was completed: the ping's own handler, inside the
IOP's *raw* firmware image (not the ARMv7 Mach-O wrapper kext that merely
carries it — a distinct 0x1b000-byte section, starting with a genuine ARM32
exception vector table, confirmed byte-for-byte: `0e0000ea` decodes as a
`B` instruction), was located (via `radare2`, cross-referencing the "Ping
received" string) and disassembled in full.

It is a message-processing loop (a queue named `"sdio message"`) that reads
an incoming message's opcode field and switches on 7 cases (0-6). **Case 0
— the ping — logs the "Connection established" string, writes a success
status back into the message's response field, and returns to wait for the
next message. Nothing else.** No register access, no call into any
`sdiodrv_*` function. This directly answers the determining question
flagged above: **the ping does not initialise the SDHC hardware; it only
acknowledges a logical connection.** The earlier hypothesis assuming a
direct causal link between the ping and hardware activation is wrong as
stated.

Case 3 of the same switch calls a function that references the
`sdiodrv_resetSDHC` string — this, not the ping, is the opcode that
actually touches hardware. Cases 1, 2, 4, 5 and 6 each call a different,
not-yet-identified function (no directly linked debug string found in the
time available this session).

Updated evidence table (format suggested by the review):

| Claim | Status |
|---|---|
| The IOP is running | Confirmed — real ARMv7 firmware, a genuine vector table, a message-processing loop |
| The transport works | Partial — the message's opcode/status fields are understood; the full shared-buffer format is not |
| The ping activates the SDHC | **Disproven** — the ping handler never touches a register |
| The SDHC works | Not tested — opcode 3 (reset) has not been sent from a payload |

What would be needed to actually drive Wi-Fi from Trognon's own payloads:
the message-channel wire format precisely enough to construct and send both
the ping (opcode 0, now known to be a no-op connection handshake) *and*
opcode 3 (the one that actually calls into `sdiodrv_resetSDHC`) — the
command/message structure seen in `sendIOPCommand`, plus whatever the real
low-level kickoff write at `this+0x24`'s vtable `+0x84` does on the host
side (a separate class, not yet located or disassembled). Sending only the
ping, as the architecture doc first assumed might be enough, is now known
to be insufficient on its own. Not attempted this session; this is a
deliberate stopping point, not an unfinished attempt.

## Tooling notes, for reproducibility

- `radare2` (`brew install radare2`, 6.2.4) was needed: manual
  `objdump`-based literal-pool resolution looked correct but silently
  missed the target function because of a small, easy-to-make address
  arithmetic mistake. `r2`'s own `aaa` analysis plus `axt` found the right
  function immediately once the mistake was caught.
- The LZSS decompressor parameters (4096-byte window, 18-byte max match,
  space-filled dictionary, length threshold 3) came from
  `malus-security/lzssdec`'s README, not from memory or a guess — an
  earlier attempt using remembered classic-LZSS parameters (zero-filled
  dictionary, different length threshold) decoded structurally but produced
  garbage.
- Extracted binaries (kernelcache, device tree, IOP firmware, the small
  kext slice) were kept outside this repository (a temporary working
  directory, not committed), consistent with this project's rule against
  ever hosting Apple firmware: every fact above is cited as a finding
  (string, struct offset, behaviour), never as a redistributed blob.

## Not verified

- The exact wire format of the IOP command/message structure (what each
  byte of the struct pointed to by `sendIOPCommand`'s `cmd` argument means)
  was not reverse-engineered; only its outer shape (opcode byte, pending
  flag, vaddr/paddr pair) is known.
- The class backing the object at `this+0x24` (the actual command channel)
  was identified by dependency (`IOSlaveProcessor`) but its own code was
  not located or disassembled this session.
- Whether the IOP is already initialised/running by the time Trognon's
  iBSS-based payloads execute, or needs to be booted by something the
  normal iOS boot chain does that Trognon's tethered path skips, is not
  established either way.
- `AppleARM7M`'s exact relationship to the physical IOP core (whether it
  names the core itself or a related peripheral) is inferred from its name
  and its shared dependency on `IOSlaveProcessor`, not confirmed from its
  own code.
