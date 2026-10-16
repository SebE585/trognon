"""Targeted tests for the refusals added to extract_kext.py (2026-10-16).

Run: python3 -m unittest tools.iop_analysis.test_extract_kext -v
The SHA-256 checks against the three known kexts cover compatibility; these
cover the new refusals.
"""
import importlib.util
import os
import struct
import unittest

_spec = importlib.util.spec_from_file_location(
    "extract_kext", os.path.join(os.path.dirname(__file__), "extract_kext.py"))
ek = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(ek)


def macho(segment_fileoff, segment_filesize, cmdsize=56, ncmds=1, sizeofcmds=None, nsects=0):
    """A minimal ARM MH_OBJECT with one LC_SEGMENT."""
    seg = struct.pack("<II16sIIIIIIII", 1, cmdsize, b"", 0x80000000, 0x1000,
                      segment_fileoff, segment_filesize, 7, 7, nsects, 0)
    seg = seg[:cmdsize].ljust(cmdsize, b"\0")
    if sizeofcmds is None:
        sizeofcmds = len(seg)
    head = struct.pack("<7I", 0xFEEDFACE, 12, 9, 1, ncmds, sizeofcmds, 0)
    return head + seg


class TestLzssRefusals(unittest.TestCase):
    def test_empty_input_with_announced_size_fails(self):
        with self.assertRaises(ValueError):
            ek.lzss(b"", 10)

    def test_truncated_literal_stream_fails(self):
        # flag byte says "8 literals" but only 2 follow
        with self.assertRaises(ValueError):
            ek.lzss(bytes([0xFF, 0x41, 0x42]), 8)

    def test_input_ending_inside_match_token_fails(self):
        # flag byte 0x00: eight match tokens, but only one byte follows
        with self.assertRaises(ValueError):
            ek.lzss(bytes([0x00, 0x01]), 4)

    def test_exact_literal_stream_succeeds(self):
        self.assertEqual(ek.lzss(bytes([0xFF]) + b"ABCDEFGH", 8), b"ABCDEFGH")


class TestMachoBounds(unittest.TestCase):
    def test_wellformed_object_is_found(self):
        blob = macho(0x40, 0x10).ljust(0x40 + 0x10, b"\0")
        self.assertEqual([(o, n) for o, n, _ in ek.objects(blob)], [(0, 0x50)])

    def test_segment_beyond_buffer_is_skipped(self):
        blob = macho(0x40, 0x1000).ljust(0x60, b"\0")
        self.assertEqual(list(ek.objects(blob)), [])

    def test_command_size_beyond_declared_commands_is_skipped(self):
        blob = macho(0x40, 0x10, cmdsize=56, sizeofcmds=16).ljust(0x60, b"\0")
        self.assertEqual(list(ek.objects(blob)), [])

    def test_too_small_command_size_is_skipped(self):
        blob = macho(0x40, 0x10, cmdsize=4).ljust(0x60, b"\0")
        self.assertEqual(list(ek.objects(blob)), [])

    def test_segment_command_shorter_than_its_structure_is_skipped(self):
        # 40 bytes is below the 56 the LC_SEGMENT structure needs; the fields read at +48 would
        # lie beyond the command (regression found by review, 2026-10-16).
        blob = macho(0x40, 0x10, cmdsize=40).ljust(0x60, b"\0")
        self.assertEqual(list(ek.objects(blob)), [])

    def test_declared_sections_that_do_not_fit_are_skipped(self):
        blob = macho(0x40, 0x10, cmdsize=56, nsects=3).ljust(0x60, b"\0")
        self.assertEqual(list(ek.objects(blob)), [])

    def test_header_cut_short_is_skipped(self):
        self.assertEqual(list(ek.objects(b"\xce\xfa\xed\xfe\x0c\x00")), [])


if __name__ == "__main__":
    unittest.main()
