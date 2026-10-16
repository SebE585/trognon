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

# PROMISING LEAD for [dev+0]'s origin (2026-10-16): iop_control_task
# (0xafc) is a dispatcher for HOST-sent commands on a separate "iop
# control" channel. Its "inst" (install) case (tag 0x696e7374, matched at
# 0xbb8 -> 0xc64) builds a per-client config struct, copying 5 field-pairs
# from the received command, plus [r4+0x34] = arg_0h -- the iop_control
# task's OWN third creation-time parameter, not copied from the host
# message. This is the best candidate so far for a fixed, boot-time
# hardware-base constant being threaded into a per-channel struct -- NOT
# YET CONFIRMED to be the same memory later read as [dev+0] by the SDIO
# opcode handlers. Next: find what value "iop" is created with as its 3rd
# param, and whether "install"'s target struct aliases a sdio channel slot.

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
