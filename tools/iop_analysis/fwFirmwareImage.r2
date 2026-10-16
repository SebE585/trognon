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

# not yet disassembled this session, candidates for opcode 5's real
# register access, and for sendCommand/performDMA's own sub-calls:
#   0x6580, 0x63d0, 0x638c   (opcode 5 three sub-calls)
#   0x6c70, 0x6c90, 0x6cf0, 0x6c00, 0x6f18     (sendCommand sub-calls)
#   0x6804, 0x6d50, 0xa7d8, 0x8dfc             (performDMA sub-calls)

# CORRECTED 2026-10-16 (second review caught this): what earlier notes
# called "dev" is NOT a separate struct -- for every one of the 6
# non-ping opcode handlers, the dispatcher passes (message+0xc, message+0x14)
# as arguments (confirmed in the switch's own disassembly, all 6 cases
# identical). So every "[dev+N]" offset below actually means
# "[message+0xc+N]"; there is one buffer, not two.
#
# dev (= message+0xc) origin, traced (2026-10-16, second review asked for
# this explicitly): fcn.00000eb0 (the receive call) indexes a FIXED TABLE
# at literal address 0x10da4, one 32-byte channel-descriptor per channel
# (channel_id * 0x20), each holding a ring buffer of 64-byte message slots
# (index<<6, masked to 64-byte alignment). This is RAM, part of the
# firmware's own static data -- not an MMIO window, and not something the
# host passes in as a raw hardware pointer. sdiodrv_sendCommand (0x67f0)
# and the command-inhibited check (0x6ac8) both read/write further fields
# of this SAME struct ([r5+0x8], [r5+0xe], [r5+0x10], [r5+0x24], [r5+0x30],
# [r5+0x32]), all still inside this one RAM-based structure -- no literal
# hardware address found at this level either.
#
#   [message+0x00] opcode byte (dispatched 1-7, see sdio_message_loop)
#   [message+0x08] response status field (same offset the host side
#                sendIOPCommand writes pending=1 into before sending)
#   [message+0xc+0x00] "configured" flag, checked by every opcode handler
#   [message+0xc+0x04] allocated buffer pointer (set by op2, cleared by op3)
#   [message+0xc+0x08], [+0xe], [+0x10] cmd/arg-style fields (read by
#                sendCommand, logged as "arg"/"cmd" in its own error strings)
#   [message+0xc+0x24] a status word, &1 checked by command_inhibited_check
#                -- could be a real cached copy of a hardware status bit,
#                or pure software state; not distinguished yet (see below)
#   [message+0xc+0x2f] a one-byte state flag (bits 0-2 read/written by the
#                generic poll helper)
#   [message+0xc+0x30], [+0x32] more fields read by sendCommand/performDMA
#   [message+0xc+0x34], [message+0xc+0x36] two 16-bit "desired register"
#                bitmasks built by the reset sub-functions

# STILL OPEN (second review, 2026-10-16): the RAM origin of this struct is
# now proven, which rules out "dev IS an MMIO window" -- but it does NOT
# yet prove these fields are pure software bookkeeping rather than a
# software-maintained MIRROR of real hardware state (updated by an
# interrupt handler we have not found). The real register access, if it
# exists in this firmware at all outside an ISR, is most likely in one of
# the not-yet-disassembled sub-calls listed above.

# Confirmed negative: neither 0x80000000 (SDIO) nor 0x81200000/0x81300000
# (NAND H2FMI0/1) appear anywhere in this binary as a literal 32-bit word,
# searched exhaustively, byte by byte alignment. Addresses are
# parameterised at runtime, not hardcoded -- consistent on both
# subsystems, so this is not why NAND worked directly and SDIO did not.
