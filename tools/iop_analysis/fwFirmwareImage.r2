# Trognon: known function names for IOP_s5l8920x_firmware raw payload section
# (the "fwFirmwareImage" blob inside the IOP_s5l8920x_firmware.kext wrapper --
# see docs/research/2026-10-16-wifi-iop-architecture.md for how it was extracted
# and decrypted from the device owner IPSW; the firmware binary itself is never
# committed here, only our own naming of its addresses).
#
# Usage, once you have re-extracted fwFirmwareImage.bin yourself (see
# docs/research/2026-10-16-wifi-iop-architecture.md, "Extracting and reading
# the real driver"):
#   r2 -q -a arm -b 32 -i tools/iop_analysis/fwFirmwareImage.r2 fwFirmwareImage.bin
#
# Standard r2 batch syntax (`f name @ addr`); run `aaa` first (as this script
# does) or the flags may not take in batch mode -- seen in this project on
# 2026-10-16, not fully root-caused. If `fl` still shows nothing afterwards,
# open interactively (`r2 -a arm -b 32 fwFirmwareImage.bin`) and paste the `f`
# lines below one at a time; that always works.
#
# All addresses are file offsets == r2 vaddr when loaded with no base (-B),
# consistent with how this file was analysed (a raw/null-format load, not a
# Mach-O: fwFirmwareImage.bin has no Mach-O header of its own).

aaa

# -- the SDIO message-processing loop (ping/switch, 2026-10-16) --
f sym.sdio_message_loop @ 0x1730
f sym.sdio_op1_ping @ 0x1844
f sym.sdio_op2_alloc @ 0x1a58
f sym.sdio_op3_free @ 0x1a24
f sym.sdio_op4_reset @ 0x19c4
f sym.sdio_op5_set_bus_params @ 0x192c
f sym.sdio_op6_send_command_maybe @ 0x190c
f sym.sdio_op7_transfer_data_maybe @ 0x18bc

# -- reset path, traced 5 levels deep, all software bitmask bookkeeping,
#    no literal MMIO address found at any level (2026-10-16) --
f sym.reset_set_bits_0x34_0x36_a @ 0x6c24
f sym.reset_set_bits_0x34_0x36_b @ 0x6d04
f sym.generic_poll_retry_100x @ 0x64b0
f sym.state_set_bits_0x2f @ 0x6e30
f sym.state_check_bits_0x2f @ 0x6e44

# real low-level driver function, named via its own log-tag string
f sym.sdiodrv_resetSDHC @ 0x64b0
f sym.sdiodrv_sendCommand @ 0x67f0
f sym.sdiodrv_completeCommand_inline @ 0x6724
f sym.sdiodrv_performDMA_or_transferData @ 0x6898
f sym.command_inhibited_check @ 0x6ac8

# static task table at 0xe848 (16 bytes/task: name ptr, entry, stack,
# priority), confirms fmi0/fmi1/sdio are peer OS tasks, "iop" is the
# highest-priority (0) master/control task:
f sym.task_iop_control @ 0xafc
#   priority 0 "iop"  entry 0xafc   (iop_control_task, see below)
#   priority 5 "fmi0" entry 0x1114
#   priority 6 "fmi1" entry 0x1114
#   priority 3 "sdio" entry 0x1730 (sdio_message_loop)

# NOTATION (fixed 2026-10-16, a reviewer caught the conflation): use
#   message+0x0c          -- a location, inside the 64-byte message slot
#   base = *(message+0x0c) -- the VALUE there: a pointer, the real SDHCI base
#   base+N                -- real SDHCI registers (N from the table below)
# NOT "message+0x0c+N" -- that was the earlier error, and it also caused a
# false "offset 0x42 overflows a 64-byte slot" inconsistency that is
# resolved once base+N is understood as a separate address space.

# CHECKED AND RULED OUT (2026-10-16): does the HOST supply `base`?
# Re-disassembled AppleIOPSDIO.kext.bin correctly this time (it's Thumb-2;
# an earlier plain-ARM pass of this same kext was wrong -- use
# `-e asm.bits=16`). sendIOPCommand (0x8057c424) has exactly one caller,
# which passes the command BUFFER'S OWN PHYSICAL ADDRESS (logged as
# "paddr=0x%lX") to the real send() call -- standard shared-memory IPC,
# not a separate hardware address. sendIOPCommand also requires the cmd
# object to be exactly one single, fixed, pre-allocated buffer
# (this->0x14, checked by pointer identity). The kext has ZERO strings/
# calls resembling getDeviceMemoryWithIndex/mapDeviceMemory -- the host
# CPU very plausibly never maps or touches the SDHC registers itself.
# CONCLUSION: `base` is not host-supplied. It must come from the IOP side
# -- a boot-time constant from a smaller IOP boot stage not yet extracted,
# or built via movw/movt rather than a single literal (not yet checked).

# fcn.00008528 / fcn.000084a0: the function sdio_message_loop calls right
# after receiving a raw message is a GENERIC physical<->IOP-local address
# translator (table at 0xf07c, 27 call sites across the firmware, not
# SDIO-specific). This is how `message` becomes dereferenceable from
# whatever raw physical address the sender supplied -- plumbing only, does
# not supply any SDIO-specific value.

# CHECKED (2026-10-16, user's priority-1 request): does sdio's task entry
# (0x1730) itself produce `base`, before the receive loop even starts?
# Compared instruction-by-instruction against fmi0/fmi1's SHARED entry
# (0x1114). Real structural difference found: fmi0/fmi1 allocate a 0x1b8-
# byte instance struct, zero it, and REGISTER it into a global array at
# 0x10ec4 (counter at 0x10ec0) -- the same array fcn.000015fc's two-phase
# boot-init dispatcher iterates. sdio's entry (0x1730) does NONE of this;
# it goes straight to creating its "sdio message" queue via fcn.00000db4
# (shared by all 4 tasks). So there is no sdio-specific "device
# registration" step visible in the task entry at all.
#
# Traced fcn.00000db4 (generic queue creation) in full: for every slot in
# the new ring (count = the task's own arg1 struct field[1]), it writes
# the literal value 1 into [slot+0] (almost certainly the opcode byte --
# a safe default, matching real opcode 1 = ping) and does NOT touch
# [slot+0xc]. Since .bss is zero at load, base = *(message+0x0c) should
# start at 0 for a freshly created channel -- which should fail every
# opcode handler's null-check. So whatever writes a real value into
# message+0x0c for the FIRST time has not been found yet; it is not the
# generic queue-creation code, and (per the earlier check) not the host.
#
# METHODOLOGY CORRECTION (2026-10-16): found, while reading a neighbour of
# the generic log function (fcn.00007d94, jumps into fcn.000074d4), a real
# hardware address built across TWO instructions: `mov r3,0x86000000;
# add r3,r3,0x300000` -> 0x86300000 (likely the IOP's own debug/log
# peripheral, not SDHC). This CONFIRMS the firmware does construct some
# addresses as split mov+add (or movw+movt) sequences rather than single
# 32-bit literals -- the earlier exhaustive byte-search for 0x80000000 as
# a whole word is NOT sufficient and needs redoing as an instruction-level
# search for mov/movw+add/movt pairs that construct an address in the
# 0x80xxxxxx range, especially near sdio_op4_reset/sdio_op6 and the task
# entry/queue-creation path.

# METHODOLOGY FOLLOW-UP (2026-10-16): grepped a full linear disassembly
# for every `mov r_, 0x8......` immediate load in the whole firmware (the
# split-construction concern above). Found several real 0x80000000/
# 0x81000000-prefixed immediates (0x59e4, 0x5b58, and others) -- but on
# inspection every one of them is a STATUS/ERROR CODE built right before a
# function return (e.g. "mov r0,0x80000000; ...; addne r5,r0,0x200000;
# ...; mov r0,r5; pop{...}" at 0x5b54-0x5bc4, inside h2fmi_tx_bootpage_pio
# -- a NAND function), not a hardware address. NONE of these immediates
# fall inside sdio_message_loop, any opcode handler, sdiodrv_sendCommand/
# performDMA, or the reset functions. So `base` is not built from a
# single-instruction or mov+add immediate anywhere in the SDIO code path
# either -- it must be a genuine LOAD (ldr) from somewhere, most likely
# the task's own arg1 parameter struct (see "CHECKED" note above) or a
# literal pool value not yet traced back to its own producer. Finding the
# real task-creation call site (not yet located -- the task TABLE at
# 0xe848 has no visible arg1-struct field, so some other code must build
# and pass it) is the most direct remaining way to find this.

# FULL SCHEDULER TRACE (2026-10-16), checked instruction-by-instruction
# per a reviewer's 3-point bounded test (not accepting "arg1" by name):
# 1. Stack recount @ 0x1730: push{r4,r5,r7,lr} never touches r0; "mov
#    r4,r0" @ 0x1740 captures the RAW incoming r0 before anything else
#    uses r0. ldm r4,{r0,r5} really does dereference the true arg1.
# 2. First-dispatch found: fcn.00009ff4 ("resume task" trampoline,
#    installed as the fake initial return addr by fcn.00009fa4 at task
#    creation) does r3=*(0xf0a8)[current-task TCB ptr]; r0=[r3+0x74];
#    mov lr,pc; pc=[r3+0x70]. +0x74 traced to its write site
#    (fcn.00008988: "str r8,[r4,0x74]", r8 = the per-priority
#    context-slot addr 0xf028+priority*8 computed by the task-creation
#    loop). CONFIRMS: R0 at first entry = &(0xf028+priority*8) itself.
# 3. Indexed-write search: grepped every "str _,[reg,0x74]" in the WHOLE
#    firmware. Only 2 exist: the TCB-init write above (stores the slot
#    ADDRESS into the TCB, doesn't write THROUGH it), and one more at
#    0x1be0 inside fcn.00001ac4 -- called from fmi0/fmi1's SHARED entry
#    (0x1114, i.e. NAND), building a similarly-shaped record
#    (+0x58/0x5c/0x60/0x64/0x68/0x6c/0x70/0x74, a generic
#    "register handler+context" idiom feeding the two-phase boot
#    dispatcher) -- NOT called by sdio's entry at all. No other indexed
#    write to the 0xf028 range exists anywhere in the firmware.
#
# CONCLUSION: the chain from R0-at-entry back to its producer is fully
# traced from every angle available in this blob, and the value is
# either static zero or simply never set by anything SDIO-specific.
# Combined with NAND having a whole parallel per-instance registration
# mechanism (fcn.00001ac4 + the 0x1b8-byte struct registry) that SDIO's
# task entry never calls at all, the two remaining explanations are:
# (a) SDIO support in this exact firmware build is incomplete/not fully
# wired up (consistent with every real SDHC register reading 0 on
# hardware), or (b) real init happens in an earlier IOP boot stage not
# extracted (before this ~0x1b000-byte blob even starts running).

# LOWER-PRIORITY LEAD: iop_control_task (0xafc, priority 0 in the static
# task table at 0xe848) is a dispatcher for HOST-sent commands on a
# SEPARATE "iop control" channel (not "sdio message"). Its "inst" (install)
# case (tag 0x696e7374, matched at 0xbb8 -> 0xc64) builds a per-client
# config struct, copying 5 field-pairs from the received command, plus
# [r4+0x34] = arg_0h -- the iop_control task's OWN third creation-time
# parameter, not copied from the host message. NOT confirmed to be the
# same memory as `base`; kept only as a secondary candidate.

# not yet disassembled this session, candidates for opcode 5's real
# register access, and for sendCommand/performDMA's own sub-calls:
#   0x6580, 0x63d0, 0x638c   (opcode 5 three sub-calls)
#   0x6c70, 0x6c90, 0x6cf0, 0x6c00, 0x6f18     (sendCommand sub-calls)
#   0x6804, 0x6d50, 0xa7d8, 0x8dfc             (performDMA sub-calls)

# MAJOR CORRECTION 2026-10-16 (third review): this IS real SDHCI hardware,
# one pointer indirection away -- everything below calling these "software
# bitmask bookkeeping" was wrong. The offsets/masks match the real,
# standard SDHCI register layout (Linux sdhci.h) exactly:
#   +0x08 = SDHCI_ARGUMENT, +0x0e = SDHCI_COMMAND, +0x10 = SDHCI_RESPONSE,
#   +0x24 = SDHCI_PRESENT_STATE (bit0 = CMD_INHIBIT, matches the
#           "Command inhibited!" string logged right at that check),
#   +0x2f = SDHCI_SOFTWARE_RESET (bit0=all,1=CMD,2=DAT -- mask 7 = reset all),
#   +0x30/+0x32 = SDHCI_INT_STATUS normal/error,
#   +0x34/+0x36 = SDHCI_INT_ENABLE normal/error.
# Too exact to be coincidence. The missing piece: re-read instruction by
# instruction (not from memory) whether [r0+0x2f] etc use "dev" itself or a
# pointer LOADED FROM dev. Answer: loaded. sdio_op4_reset (0x19c4) does
# "r4 = [r0]" FIRST (dereferences [dev+0] ONCE) and passes r4 -- not dev --
# into generic_poll_retry_100x/reset_set_bits_0x34_0x36_a/_b. sdio_op6
# (0x190c) does the identical "r0 = [r0]" before tail-calling
# sdiodrv_sendCommand. So [dev+0] is not a "configured" boolean, it is a
# POINTER -- null-checked like any driver checks its mapped register
# window -- and everything downstream (all offsets in the earlier note
# below) operates through THAT pointer, i.e. is very plausibly the real
# SDHC register window, not a RAM struct.
#
# What is now the real open question: not software-vs-hardware (hardware,
# convincingly) but WHAT VALUE is stored at [dev+0] (= [message+0xc+0]) and
# WHO WRITES IT. Neither sdio_op2_alloc nor any handler disassembled so far
# sets this field -- all of them only read/null-check it. Must be
# initialised elsewhere (IOP channel-open/init code, not yet located).
#
# Earlier note (2026-10-16, second review, still true as background): what
# was called "dev" is not a separate struct -- for every one of the 6
# non-ping opcode handlers, the dispatcher passes (message+0xc, message+0x14)
# as arguments (confirmed in the switch's own disassembly, all 6 cases
# identical). "message" itself resolves through a fixed table at literal
# address 0x10da4 (channel descriptor * 0x20, 64-byte message-slot ring) --
# that part (the message/channel plumbing) is genuinely RAM. It is
# [message+0xc+0]'s CONTENTS (a pointer value stored there, not the slot's
# own address) that the third review now shows is the real hardware base.
#
#   [message+0x00] opcode byte (dispatched 1-7, see sdio_message_loop)
#   [message+0x08] response status field (same offset the host side
#                sendIOPCommand writes pending=1 into before sending)
#   [message+0xc+0x00] the real SDHC hardware base pointer (was wrongly
#                called "configured flag"); null-checked, never written by
#                any traced opcode handler
#   [message+0xc+0x04] allocated buffer pointer (set by op2, cleared by op3)
#                -- this one IS plain RAM bookkeeping, unrelated to the base
#                pointer above
#   base+0x08, base+0xe, base+0x10, base+0x24, base+0x2f, base+0x30,
#   base+0x32, base+0x34, base+0x36 -- see the real-SDHCI mapping above

# Confirmed negative: neither 0x80000000 (SDIO) nor 0x81200000/0x81300000
# (NAND H2FMI0/1) appear anywhere in this binary as a literal 32-bit word,
# searched exhaustively, byte by byte alignment. Addresses are
# parameterised at runtime, not hardcoded -- consistent on both
# subsystems, so this is not why NAND worked directly and SDIO did not.
