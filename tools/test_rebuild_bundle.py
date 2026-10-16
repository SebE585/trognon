"""Tests for tools/rebuild_bundle.py: the verdict logic and the row it prints (no compiler needed)."""
import sys
import unittest
from pathlib import Path

import yaml

sys.path.insert(0, str(Path(__file__).resolve().parent))
import rebuild_bundle as rb  # noqa: E402


class VerdictTests(unittest.TestCase):
    def test_match(self):
        self.assertEqual(rb.verdict("a", "a"), "match")

    def test_a_different_hash_is_a_mismatch_not_an_error(self):
        self.assertEqual(rb.verdict("a", "b"), "mismatch")

    def test_nothing_built_is_not_reproducible(self):
        self.assertEqual(rb.verdict("a", None), "not-reproducible")

    def test_the_printed_row_is_valid_yaml_the_validator_accepts(self):
        rec = {"bundle": "B", "kind": "rebuild", "date": "2026-10-16", "by": "alice", "environment": {"host": "Linux x86_64", "toolchain": "gcc: 13.2 (x)"},
               "result": "match", "commit": "abc", "notes": 'quote " inside'}
        parsed = yaml.safe_load("verifications:\n" + rb.as_yaml(rec))["verifications"][0]
        self.assertEqual(parsed["by"], "alice")
        self.assertEqual(parsed["environment"]["toolchain"], "gcc: 13.2 (x)")
        self.assertEqual(parsed["notes"], 'quote " inside')

    def test_a_non_matching_row_has_no_commit_line(self):
        rec = {"bundle": "B", "kind": "rebuild", "date": "2026-10-16", "by": "a", "environment": {"host": "h", "toolchain": "t"},
               "result": "not-reproducible", "commit": None, "notes": "n"}
        self.assertNotIn("commit:", rb.as_yaml(rec))


if __name__ == "__main__":
    unittest.main()
