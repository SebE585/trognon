"""Tests for tools/check_device_evidence.py: a DEVICE section cites existing evidence, or declares that it has none."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import check_device_evidence as cde  # noqa: E402

B = "20261016T101710Z-trace_accel_i2c"


def run(body, existing=(B,)):
    return cde.check_section("T", body, lambda b: b in existing)


class SectionTests(unittest.TestCase):
    def test_a_non_device_section_is_not_checked(self):
        self.assertEqual(run("\n`x` · **HYPOTHESIS**\n\nnothing cited"), ([], False))

    def test_device_with_an_existing_bundle_passes(self):
        self.assertEqual(run("\n`x` · **DEVICE**\n\n[b](https://x/evidence/%s)" % B), ([], False))

    def test_device_with_a_bundle_in_backticks_passes(self):
        self.assertEqual(run("\n**DEVICE** · bundle `%s`\n" % B), ([], False))

    def test_device_with_no_bundle_fails(self):
        errs, gap = run("\n**DEVICE**\n\ntext only")
        self.assertTrue(errs and not gap)

    def test_a_bundle_that_does_not_exist_fails(self):
        errs, _ = run("\n**DEVICE**\n\n`20260101T000000Z-trace_nope`")
        self.assertTrue(any("does not exist" in e for e in errs))

    def test_a_declared_gap_is_allowed_and_reported(self):
        self.assertEqual(run("\n**DEVICE**\n\nLinks to evidence: none dedicated."), ([], True))
        self.assertEqual(run("\n**DEVICE**\n\nLiens : aucun dédié."), ([], True))

    def test_a_declared_gap_is_reported_even_when_another_bundle_is_named(self):
        self.assertEqual(run("\n**DEVICE**\n\nnone dedicated; see `%s` for a different claim" % B), ([], True))

    def test_mixed_device_hypothesis_counts_as_device(self):
        errs, _ = run("\nmixed **DEVICE** / **HYPOTHESIS** · 2026-10-16\n\ntext")
        self.assertTrue(errs)

    def test_sections_split_on_second_level_headings(self):
        self.assertEqual([t for t, _ in cde.sections("# H1\n\n## A\n\nx\n\n## B\n\ny\n")], ["A", "B"])


if __name__ == "__main__":
    unittest.main()
