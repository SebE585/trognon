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
        self.assertEqual(block, {"commit": "abc", "dirty": False, "source_complete": "complete"})
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

    def test_source_complete_says_whether_the_source_is_all_there(self):
        self.assertEqual(bs.source_fields("a", False, "", [])[0]["source_complete"], "complete")
        self.assertEqual(bs.source_fields("a", True, "diff\n", [])[0]["source_complete"], "with-source-diff")
        self.assertEqual(bs.source_fields("a", True, "diff\n", ["tools/x.c"])[0]["source_complete"], "incomplete")
        self.assertEqual(bs.source_fields("a", True, "", [])[0]["source_complete"], "incomplete")   # dirty, nothing saved
        for combo in ((False, "", []), (True, "d", []), (True, "d", ["x"]), (True, "", [])):
            self.assertIn(bs.source_fields("a", *combo)[0]["source_complete"], bs.SOURCE_COMPLETE)

    def test_the_manifest_never_claims_reproducibility(self):
        for combo in ((False, "", []), (True, "d", []), (True, "d", ["x"])):
            self.assertNotIn("reproducible", bs.source_fields("a", *combo)[0])

    def test_build_environment_records_compiler_and_flags(self):
        class R:
            def __init__(self, out, rc=0): self.stdout, self.returncode = out, rc
        def run(cmd, **k):
            return R("arm-none-eabi-gcc (fake) 13.2\nmore\n") if "--version" in cmd else R("gcc -Os -DSOC_X\n")
        env = bs.build_environment("/p", run=run, which=lambda n: "/usr/bin/" + n)
        self.assertEqual(env, {"compiler": "arm-none-eabi-gcc (fake) 13.2", "cflags": "gcc -Os -DSOC_X"})

    def test_build_environment_without_a_compiler_says_none(self):
        class R:
            stdout, returncode = "", 2
        env = bs.build_environment("/p", run=lambda *a, **k: R(), which=lambda n: None)
        self.assertEqual(env, {"compiler": None, "cflags": None})

    def test_a_clean_run_says_nothing(self):
        block, _ = bs.source_fields("abc", False, "", [])
        self.assertEqual(bs.warnings(block), [])


if __name__ == "__main__":
    unittest.main()
