"""Tests for tools/project_sync.py: the plan that keeps the board a mirror of missions.yaml, with no network."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import project_sync as ps  # noqa: E402


def mission(q="Q-1", **kw):
    m = {"question": q, "title": "T " + q, "work": "static", "state": "open"}
    m.update(kw)
    return m


def card(m, **override):
    w = ps.wanted(m)
    item = {"id": "PVTI_" + m["question"], "title": w["title"], "content": {"id": "DI_" + m["question"], "body": w["body"]},
            "mission state": w["mission state"], "level": w["level"] + " something", "question id": m["question"]}
    item.update(override)
    return item


class PlanTests(unittest.TestCase):
    def test_in_step(self):
        m = mission()
        self.assertEqual(ps.plan([m], [card(m)]), [])

    def test_a_new_mission_is_created_once(self):
        self.assertEqual(ps.plan([mission()], []), [("create", "Q-1", "T Q-1")])

    def test_no_duplicate_is_created_when_the_card_exists(self):
        m = mission()
        self.assertFalse([a for a in ps.plan([m], [card(m)]) if a[0] == "create"])

    def test_a_changed_state_is_an_update(self):
        m = mission()
        actions = ps.plan([mission(state="closed")], [card(m)])
        self.assertEqual(actions, [("update", "Q-1", ["mission state"])])

    def test_a_card_renamed_on_the_board_is_found_by_its_question_id(self):
        m = mission()
        actions = ps.plan([m], [card(m, title="renamed by hand")])
        self.assertEqual(actions, [("update", "Q-1", ["title"])])

    def test_a_card_with_no_mission_is_an_orphan_not_deleted(self):
        m, other = mission(), mission("Q-9")
        actions = ps.plan([m], [card(m), card(other)])
        self.assertEqual(actions, [("orphan", "Q-9", "T Q-9")])

    def test_a_card_without_a_question_id_is_reported(self):
        actions = ps.plan([], [{"id": "x", "title": "loose", "content": {}}])
        self.assertEqual(actions, [("orphan", None, "loose")])

    def test_two_cards_for_one_question_are_reported(self):
        m = mission()
        actions = ps.plan([m], [card(m), card(m)])
        self.assertEqual(actions, [("duplicate", "Q-1", "T Q-1")])


    def test_a_first_contribution_is_keyed_by_its_fc_id(self):
        fc = {"id": "FC-X", "recurring": True, "title": "T", "work": "static", "state": "open"}
        self.assertEqual(ps.plan([fc], []), [("create", "FC-X", "T")])
        item = {"id": "i", "title": "T", "content": {"id": "d", "body": ps.body(fc)}, "mission state": "Open", "level": "1 x", "question id": "FC-X"}
        self.assertEqual(ps.plan([fc], [item]), [])


if __name__ == "__main__":
    unittest.main()
