#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Verify selected clock-function table facts in the owner's local kext.

Usage: python3 -I tools/iop_analysis/probe_clock_function_tables.py KEXT.bin
No disassembly, binary excerpts or firmware files are exported. This checks
pointer/literal identity, not runtime reachability or the receiver's class.
"""
import argparse
import hashlib
import struct
from pathlib import Path

SHA256 = "5439f665a7531f36c0c869fd16e7a33e361e4f72232130fe5055b86a38985175"
WORDS = (
    ("clkG selector literal", 0x8045E710, 0x636C6B47),
    ("clkG constructor table literal", 0x8045E164, 0x804632B8),
    ("clkG table +0x4c", 0x804632B8 + 0x4C, 0x8045DBE1),
    ("clkG table +0x50", 0x804632B8 + 0x50, 0x8045DA45),
    ("initializer metaclass literal", 0x8045DC0C, 0x80464688),
    ("safeMetaCast Thumb pointer", 0x8045DC10, 0x801AEEC1),
    ("ARST selector literal", 0x8045E718, 0x41525354),
    ("ARST constructor table literal", 0x8045E06C, 0x80463200),
    ("ARST table +0x54", 0x80463200 + 0x54, 0x8045DA5D),
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    args = parser.parse_args()
    data = args.binary.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if digest != SHA256:
        parser.error(f"unsupported binary SHA-256: {digest}; expected {SHA256}")
    # Exact hash pins the Mach-O layout; do not reuse a guessed image base.
    segments = []
    offset = 28
    for _ in range(struct.unpack_from("<I", data, 16)[0]):
        command, size = struct.unpack_from("<II", data, offset)
        if command == 1:  # LC_SEGMENT (32-bit)
            address, _, file_offset, file_size = struct.unpack_from("<4I", data, offset + 24)
            segments.append((address, file_offset, file_size))
        offset += size
    for label, address, expected in WORDS:
        mappings = [f + address - v for v, f, n in segments if v <= address and address + 4 <= v + n]
        if len(mappings) != 1:
            parser.error(f"{address:#x}: expected one file-backed segment mapping")
        value = struct.unpack_from("<I", data, mappings[0])[0]
        if value != expected:
            parser.error(f"{label}: {value:#x} != {expected:#x}")
        print(f"{label}: {address:#010x} -> {value:#010x} OK")
    print("9 pointer/literal checks passed; runtime class and final gate operation remain open.")


if __name__ == "__main__":
    main()
