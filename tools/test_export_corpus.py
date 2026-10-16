"""Tests for tools/export_corpus.py: nothing private, nothing untracked, always stamped."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import export_corpus as ec  # noqa: E402

TRACKED = ["README.md", "CONTRIBUTING.md", "docs/index.md", "docs/en/03-hardware-map.md", "docs/research/a.md",
           "TODO.md", "CLAUDE.md", "CLAUDE.local.md", "docs/journal/2026-10-16.md", ".claude/x.md", "MEMORY.md",
           "docs/en/journey/start.md", "tools/other.md"]


class SelectTests(unittest.TestCase):
    def test_private_paths_are_never_selected(self):
        everything = ec.select(TRACKED + ["TODO.md"], ("",))
        for bad in ("TODO.md", "CLAUDE.md", "CLAUDE.local.md", "docs/journal/2026-10-16.md", ".claude/x.md", "MEMORY.md"):
            self.assertNotIn(bad, everything)

    def test_only_allow_listed_prefixes(self):
        got = ec.select(TRACKED, ec.GROUPS["reference"])
        self.assertIn("README.md", got)
        self.assertIn("docs/en/03-hardware-map.md", got)
        self.assertNotIn("tools/other.md", got)
        self.assertNotIn("docs/research/a.md", got)

    def test_an_untracked_file_cannot_be_selected(self):
        self.assertEqual(ec.select(["README.md"], ("docs/en/",)), [])

    def test_the_header_carries_the_commit_and_the_date(self):
        h = ec.header("reference", "abc1234", "2026-10-16 22:00 UTC")
        self.assertIn("abc1234", h)
        self.assertIn("2026-10-16", h)
        self.assertIn("corrections", h)

    def test_the_claims_are_one_json_object_per_line_with_their_status(self):
        import json
        lines = ec.claims("abc1234").splitlines()
        self.assertTrue(lines)
        for line in lines[:20]:
            r = json.loads(line)
            self.assertIn(r["kind"], ("claim", "open-question"))
            self.assertEqual(r["commit"], "abc1234")
            self.assertIn("status", r)


if __name__ == "__main__":
    unittest.main()
