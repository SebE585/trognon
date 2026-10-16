"""Tests for tools/missions_build.py: the checks that keep the Missions page honest."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import missions_build as mb  # noqa: E402

OPEN = {"Q-1": {"text": "A question.", "proof": "x", "binary": "B", "file": "f.yaml", "title": "t"}}


def mission(**kw):
    m = {"question": "Q-1", "title": "T", "work": "static", "needs": ["a"], "expected": "e", "state": "open"}
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

    def test_closed_needs_an_outcome(self):
        self.assertTrue(any("needs an outcome" in e for e in mb.check([mission(state="closed")], OPEN, set())))
        self.assertEqual(mb.check([mission(state="closed", outcome="refuted")], OPEN, set()), [])

    def test_only_a_closed_mission_has_an_outcome(self):
        self.assertTrue(any("only a closed mission" in e for e in mb.check([mission(outcome="confirmed")], OPEN, set())))

    def test_write_level_is_never_open(self):
        errs = mb.check([mission(work="write", needs=[])], OPEN, set())
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
