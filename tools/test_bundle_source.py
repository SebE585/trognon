"""Tests for tools/bundle_source.py: a dirty run must stay rebuildable, and say so when it is not."""
import hashlib
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import bundle_source as bs  # noqa: E402


class SourceFieldTests(unittest.TestCase):
    def test_a_clean_tree_records_only_the_commit(self):
        block, patch = bs.source_fields("abc", False, "", [])
        self.assertEqual(block, {"commit": "abc", "dirty": False})
        self.assertIsNone(patch)

    def test_a_dirty_tree_saves_the_diff_and_its_hash(self):
        diff = "diff --git a/x b/x\n+1"
        block, patch = bs.source_fields("abc", True, diff, [])
        self.assertTrue(patch.endswith("\n"))
        self.assertEqual(block["source_diff"], "source.diff")
        self.assertEqual(block["source_diff_sha256"], hashlib.sha256(patch.encode()).hexdigest())

    def test_a_dirty_tree_with_no_tracked_change_has_no_patch(self):
        block, patch = bs.source_fields("abc", True, "", ["tools/payloads/new.c"])
        self.assertIsNone(patch)
        self.assertNotIn("source_diff", block)
        self.assertEqual(block["untracked"], ["tools/payloads/new.c"])

    def test_untracked_files_are_named_and_sorted(self):
        block, _ = bs.source_fields("abc", True, "", ["b", "a"])
        self.assertEqual(block["untracked"], ["a", "b"])

    def test_the_run_warns_when_the_tree_is_dirty(self):
        block, _ = bs.source_fields("abc", True, "d", [])
        self.assertTrue(any("source.diff" in w for w in bs.warnings(block)))

    def test_the_run_warns_when_files_cannot_be_saved(self):
        block, _ = bs.source_fields("abc", True, "", ["tools/payloads/new.c"])
        self.assertTrue(any("NOT saved" in w for w in bs.warnings(block)))

    def test_a_clean_run_says_nothing(self):
        block, _ = bs.source_fields("abc", False, "", [])
        self.assertEqual(bs.warnings(block), [])


if __name__ == "__main__":
    unittest.main()
