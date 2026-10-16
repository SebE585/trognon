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


class ShallowTests(unittest.TestCase):
    def test_is_shallow_answers_with_a_boolean(self):
        self.assertIn(vb.is_shallow(), (True, False))

    def test_a_row_is_not_rejected_for_a_missing_commit_when_the_check_is_told_history_is_absent(self):
        rows = [row(commit="deadbeef")]
        self.assertTrue(any("not in the repository history" in e for e in errs(rows)))
        self.assertEqual(vb.check(rows, exists=lambda b: True, commit_ok=lambda c: True), [])


class BreakdownTests(unittest.TestCase):
    REC = {"B1": "c1", "B2": "c2", "B3": "c3", "B4": "c4"}

    def test_each_bundle_is_counted_once_and_the_parts_add_up(self):
        rows = [row(bundle="B1", commit="c1"), row(bundle="B2", commit="c9"),
                row(bundle="B3", result="not-reproducible", commit=None, notes="x")]
        bd = vb.breakdown(rows, ["B1", "B2", "B3", "B4"], self.REC)
        self.assertEqual(bd, {"match-recorded": 1, "match-later": 1, "mismatch": 0, "not-reproducible": 1, "unchecked": 1})
        self.assertEqual(sum(bd.values()), 4)

    def test_the_best_result_wins_when_a_bundle_has_several_rows(self):
        rows = [row(bundle="B1", result="not-reproducible", commit=None, notes="x"), row(bundle="B1", commit="c1")]
        self.assertEqual(vb.breakdown(rows, ["B1"], self.REC)["match-recorded"], 1)

    def test_a_claim_audit_does_not_count_as_a_rebuild(self):
        rows = [row(bundle="B1", kind="claim-audit", claim="c", page="p")]
        self.assertEqual(vb.breakdown(rows, ["B1"], self.REC)["unchecked"], 1)

    def test_the_page_says_how_many_checks_are_independent(self):
        text = vb.render([row(by="maintainer", bundle="B1", commit="abc123")], ["B1"], {"B1": "abc123"})
        self.assertIn("Independent checks: 0", text)
        self.assertIn("| **Total** | **1** |", text)


class AuditPageTests(unittest.TestCase):
    def test_the_audit_model_comes_from_the_data(self):
        a = row(kind="claim-audit", claim="THE CLAIM", page="p", result="match", notes="n")
        self.assertIn("THE CLAIM", "\n".join(vb.audit_example([row(), a])))

    def test_a_template_is_shown_when_no_audit_exists_yet(self):
        self.assertIn("claim-audit", "\n".join(vb.audit_example([row()])))

    def test_the_page_gives_a_draft_apart_from_the_real_example(self):
        text = vb.render([row(by="maintainer")], ["B1"], {"B1": "abc123"})
        self.assertIn("<your GitHub handle>", text)
        self.assertIn("A real example, as an illustration", text)
        self.assertIn("blob/<commit from manifest.json>", text)

    def test_the_page_explains_what_a_bundle_does_not_show(self):
        text = vb.render([row(by="maintainer")], ["B1"], {"B1": "abc123"})
        self.assertIn("Results, not inputs", text)
        self.assertIn("Agreement is not independence", text)


if __name__ == "__main__":
    unittest.main()
