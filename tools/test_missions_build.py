"""Tests for tools/missions_build.py: the checks that keep the Missions page honest."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import missions_build as mb  # noqa: E402

OPEN = {"Q-1": {"text": "A question.", "proof": "x", "binary": "B", "file": "f.yaml", "title": "t"}}


def mission(**kw):
    m = {"question": "Q-1", "title": "T", "work": "static", "needs": ["a"], "expected": "e", "state": "open",
         "unlocks": "u", "skills": ["s"]}
    m.update(kw)
    return m


class CheckTests(unittest.TestCase):
    def test_valid(self):
        self.assertEqual(mb.check([mission()], OPEN, set()), [])

    def test_unknown_question(self):
        self.assertTrue(any("not a question" in e for e in mb.check([mission(question="NOPE")], OPEN, set())))

    def test_listed_twice(self):
        self.assertTrue(any("listed twice" in e for e in mb.check([mission(), mission()], OPEN, set())))

    def test_blocked_needs_a_reason(self):
        self.assertTrue(any("must say what blocks" in e for e in mb.check([mission(state="blocked")], OPEN, set())))

    def test_closed_needs_an_outcome_and_the_merged_contribution(self):
        done = "https://github.com/SebE585/trognon/pull/1"
        self.assertTrue(any("needs an outcome" in e for e in mb.check([mission(state="closed", integrated_in=done)], OPEN, set())))
        self.assertTrue(any("link the merged contribution" in e for e in mb.check([mission(state="closed", outcome="refuted")], OPEN, set())))
        self.assertEqual(mb.check([mission(state="closed", outcome="refuted", integrated_in=done)], OPEN, set()), [])

    def test_integrated_in_belongs_to_a_closed_mission(self):
        errs = mb.check([mission(integrated_in="https://github.com/SebE585/trognon/pull/1")], OPEN, set())
        self.assertTrue(any("only a closed mission has integrated_in" in e for e in errs))

    def test_a_run_on_a_phone_lists_its_preconditions(self):
        errs = mb.check([mission(work="survey")], OPEN, set())
        self.assertTrue(any("preconditions" in e for e in errs))
        self.assertEqual(mb.check([mission(work="survey", preconditions=["read it first"])], OPEN, set()), [])

    def test_unlocks_and_skills_are_required(self):
        errs = mb.check([mission(unlocks="", skills=[])], OPEN, set())
        self.assertTrue(any("'unlocks'" in e for e in errs))
        self.assertTrue(any("'skills'" in e for e in errs))

    def test_a_contribution_needs_a_github_url(self):
        errs = mb.check([mission(contributions=[{"title": "x", "url": "http://example.com"}])], OPEN, set())
        self.assertTrue(any("GitHub URL" in e for e in errs))

    def test_only_a_closed_mission_has_an_outcome(self):
        self.assertTrue(any("only a closed mission" in e for e in mb.check([mission(outcome="confirmed")], OPEN, set())))

    def test_write_level_is_never_open(self):
        errs = mb.check([mission(work="write", needs=[], preconditions=["x"])], OPEN, set())
        self.assertTrue(any("never offered as open" in e for e in errs))

    def test_a_resolved_question_closes_its_mission(self):
        errs = mb.check([mission(state="open")], {}, {"Q-1"})
        self.assertTrue(any("must be closed" in e for e in errs))

    def test_blocked_until_must_name_a_catalog_question(self):
        errs = mb.check([mission(state="blocked", blocked_until=["NOPE"])], OPEN, set())
        self.assertTrue(any("not a question of the catalog" in e for e in errs))

    def test_a_mission_waiting_on_a_resolved_question_must_be_opened(self):
        errs = mb.check([mission(state="blocked", blocked_until=["Q-2"])], OPEN, {"Q-2"})
        self.assertTrue(any("can be opened" in e for e in errs))

    def test_a_mission_waiting_on_an_open_question_stays_blocked(self):
        self.assertEqual(mb.check([mission(state="blocked", blocked_until=["Q-1"])], OPEN, set()), [])

    def test_static_work_lists_what_it_needs(self):
        self.assertTrue(any("needs a list" in e for e in mb.check([mission(needs=[])], OPEN, set())))

    def test_the_take_form_is_prefilled(self):
        self.assertIn("template=mission.yml", mb.issue_url("S5L-CLK-Q1"))
        self.assertIn("mission=S5L-CLK-Q1", mb.issue_url("S5L-CLK-Q1"))

    def test_every_level_has_a_label(self):
        self.assertEqual(set(mb.WORK), {"static", "survey", "write"})


if __name__ == "__main__":
    unittest.main()
