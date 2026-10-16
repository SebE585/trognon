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

# not yet disassembled this session, candidates for opcode 5/6/7 real
# register access:
#   0x6580, 0x63d0, 0x638c   (opcode 5 three sub-calls)
#   0x67f0                   (opcode 6 tail-call target)
#   0x6898                   (opcode 7 call target)

# Known device-struct offsets (not registers -- fields of the per-device
# state struct passed as the first argument to most functions above):
#   [dev+0x00] "configured" flag, checked by every opcode handler
#   [dev+0x04] allocated buffer pointer (set by op2, cleared by op3)
#   [dev+0x2f] a one-byte state flag (bits 0-2 read/written by the poll helper)
#   [dev+0x34], [dev+0x36] two 16-bit "desired register" bitmasks built by
#                the reset sub-functions, applied to hardware somewhere
#                NOT YET FOUND (probably an IRQ handler or deferred task)
#   [message+0x00] opcode byte (dispatched 1-7, see sdio_message_loop)
#   [message+0x08] response status field (same offset the host side
#                sendIOPCommand writes pending=1 into before sending)

# Confirmed negative: neither 0x80000000 (SDIO) nor 0x81200000/0x81300000
# (NAND H2FMI0/1) appear anywhere in this binary as a literal 32-bit word,
# searched exhaustively, byte by byte alignment. Addresses are
# parameterised at runtime, not hardcoded -- consistent on both
# subsystems, so this is not why NAND worked directly and SDIO did not.
