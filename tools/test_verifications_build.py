"""Tests for tools/verifications_build.py: an independent check is recorded honestly, a failure included."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import verifications_build as vb  # noqa: E402


def row(**kw):
    r = {"bundle": "B1", "kind": "rebuild", "date": "2026-10-16", "by": "someone", "environment": {"host": "Linux x86_64", "toolchain": "gcc 13"},
         "result": "match", "commit": "abc123", "notes": ""}
    r.update(kw)
    return r


def errs(rows):
    return vb.check(rows, exists=lambda b: b in ("B1", "B2"), commit_ok=lambda c: c == "abc123")


class CheckTests(unittest.TestCase):
    def test_valid_row(self):
        self.assertEqual(errs([row()]), [])

    def test_a_bundle_that_does_not_exist_is_refused(self):
        self.assertTrue(any("not a bundle" in e for e in errs([row(bundle="NOPE")])))

    def test_a_result_must_fit_the_kind_of_check(self):
        self.assertTrue(any("not one of" in e for e in errs([row(result="inconclusive")])))
        self.assertEqual(errs([row(kind="claim-audit", result="inconclusive", claim="c", page="p", notes="unclear")]), [])

    def test_the_handle_placeholder_is_refused(self):
        self.assertTrue(any("'by'" in e for e in errs([row(by="YOUR-GITHUB-HANDLE")])))

    def test_a_rebuild_records_its_environment(self):
        self.assertTrue(any("host and toolchain" in e for e in errs([row(environment={})])))

    def test_a_match_names_a_commit_that_exists(self):
        self.assertTrue(any("names the commit" in e for e in errs([row(commit=None)])))
        self.assertTrue(any("not in the repository history" in e for e in errs([row(commit="deadbeef")])))

    def test_a_negative_result_needs_an_explanation(self):
        self.assertTrue(any("explanation" in e for e in errs([row(result="mismatch", notes="")])))
        self.assertEqual(errs([row(result="mismatch", notes="built bytes differ")]), [])
        self.assertTrue(any("explanation" in e for e in errs([row(result="not-reproducible", commit=None, notes="")])))

    def test_a_claim_audit_quotes_the_claim(self):
        self.assertTrue(any("quotes the claim" in e for e in errs([row(kind="claim-audit")])))

    def test_a_date_must_be_iso(self):
        self.assertTrue(any("ISO date" in e for e in errs([row(date="yesterday")])))


class StatsTests(unittest.TestCase):
    def test_the_maintainer_is_not_independent(self):
        self.assertFalse(vb.independent(row(by="maintainer")))
        self.assertTrue(vb.independent(row(by="someone")))

    def test_stats_count_later_commits_and_failures(self):
        rows = [row(bundle="B1", commit="c1", by="maintainer"), row(bundle="B2", commit="c9", by="maintainer"),
                row(bundle="B3", result="not-reproducible", commit=None, notes="x", by="maintainer")]
        s = vb.stats(rows, ["B1", "B2", "B3"], {"B1": "c1", "B2": "c2", "B3": "c3"})
        self.assertEqual((s["rebuilt"], s["later"], s["failed"], s["independent"]), (2, 1, 1, 0))

    def test_the_page_says_so_when_nothing_independent_exists(self):
        text = vb.render([row(by="maintainer")], ["B1"], {"B1": "abc123"})
        self.assertIn("None yet", text)

    def test_an_independent_check_is_listed_first(self):
        text = vb.render([row(by="maintainer"), row(by="alice", bundle="B2")], ["B1", "B2"], {"B1": "abc123", "B2": "abc123"})
        self.assertNotIn("None yet", text)
        self.assertLess(text.index("alice"), text.index("The maintainer's own runs"))


if __name__ == "__main__":
    unittest.main()
