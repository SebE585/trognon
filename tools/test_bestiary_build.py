"""Tests for tools/bestiary_build.py: a life bar must follow the proofs, never a typed number."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import bestiary_build as bb  # noqa: E402

B = "20261015T171033Z-trace_nand"
TITLES = {"M-1": ("A mission", "static")}


def front(state="open", **kw):
    f = {"state": state, "text": {"en": "t", "fr": "t"}}
    f.update(kw)
    return f


def creature(*fronts, section="nand", **kw):
    c = {"id": "c", "name": {"en": "N", "fr": "N"}, "block": {"en": "B", "fr": "B"}, "section": section, "fronts": list(fronts)}
    c.update(kw)
    return c


def errs(*fronts, **kw):
    return bb.check({"creatures": [creature(*fronts, **kw)]}, titles=TITLES, bundles={B})


class LifeTests(unittest.TestCase):
    def test_open_is_two_wounded_one_down_zero(self):
        self.assertEqual(bb.life(creature(front("open"), front("wounded"), front("down"))), (3, 6))

    def test_status_follows_the_fronts(self):
        self.assertEqual(bb.status(creature(front("open"))), "intact")
        self.assertEqual(bb.status(creature(front("open"), section=None)), "unspotted")
        self.assertEqual(bb.status(creature(front("down"), front("open"))), "wounded")
        self.assertEqual(bb.status(creature(front("down"))), "defeated")

    def test_the_bar_never_shows_empty_while_life_is_left(self):
        self.assertIn("▰", bb.bar(1, 100))
        self.assertEqual(bb.bar(0, 4), "▱" * bb.SEGMENTS)
        self.assertEqual(bb.bar(4, 4), "▰" * bb.SEGMENTS)


class ProofTests(unittest.TestCase):
    def test_a_valid_creature_has_no_error(self):
        self.assertEqual(errs(front("down", evidence=[B]), front("open", missions=["M-1"])), [])

    def test_down_needs_an_existing_bundle(self):
        self.assertTrue(any("needs the evidence" in e for e in errs(front("down"))))
        self.assertTrue(any("does not exist" in e for e in errs(front("down", evidence=["nope"]))))

    def test_wounded_needs_a_reading_that_exists(self):
        self.assertTrue(any("holds the reading" in e for e in errs(front("wounded"))))
        self.assertTrue(any("does not exist under docs" in e for e in errs(front("wounded", ref="nope.md"))))
        self.assertEqual(errs(front("wounded", ref="en/03-hardware-map.md")), [])

    def test_proof_on_the_wrong_state_is_refused(self):
        self.assertTrue(any("only a front that is down" in e for e in errs(front("open", evidence=[B]))))
        self.assertTrue(any("only a wounded" in e for e in errs(front("open", ref="en/03-hardware-map.md"))))

    def test_unknown_mission_section_state_and_missing_french_are_refused(self):
        self.assertTrue(any("not in missions.yaml" in e for e in errs(front("open", missions=["X"]))))
        self.assertTrue(any("not an anchor" in e for e in errs(front("open"), section="no-such-section")))
        self.assertTrue(any("state must be" in e for e in errs(front("dead"))))
        bad = creature(front("open"))
        bad["name"] = {"en": "N"}
        self.assertTrue(any("both languages" in e for e in bb.check({"creatures": [bad]}, titles=TITLES, bundles={B})))


class DigitTests(unittest.TestCase):
    def d(self, **kw):
        d = {"label": {"en": "l", "fr": "l"}, "value": "3B", "proof": "who_am_i=0000003B"}
        d.update(kw)
        return d

    def test_a_digit_is_accepted_when_its_proof_line_is_in_the_bundle(self):
        b = "20261016T101710Z-trace_accel_i2c"
        self.assertEqual(bb.check({"creatures": [creature(front("down", evidence=[b], digit=self.d()))]}, titles=TITLES, bundles={b}), [])

    def test_a_digit_without_its_proof_in_the_bundle_is_refused(self):
        b = "20261016T101710Z-trace_accel_i2c"
        e = bb.check({"creatures": [creature(front("down", evidence=[b], digit=self.d(proof="who_am_i=DEADBEEF")))]}, titles=TITLES, bundles={b})
        self.assertTrue(any("in none of the front's bundles" in x for x in e))

    def test_only_a_front_that_is_down_reveals_a_digit(self):
        self.assertTrue(any("only a front that is down reveals" in x for x in errs(front("open", digit=self.d()))))

    def test_the_strip_shows_digits_checks_and_question_marks(self):
        c = creature(front("down", digit={"value": "3B"}), front("down"), front("wounded", ref="x"), front("open"))
        self.assertEqual(bb.code_strip(c), ["3B", "✓", "?", "?"])


def cfront(i, state="open", **kw):
    kw.setdefault("down_needs", "unknown")
    return front(state, id=i, **kw)


def camp(*fronts, **kw):
    c = creature(*fronts, campaign=True, campaign_intro={"en": "i", "fr": "i"})
    c.update(kw)
    return c


class CampaignTests(unittest.TestCase):
    def chk(self, c):
        return bb.check({"creatures": [c]}, titles=TITLES, bundles={B})

    def test_the_route_respects_needs_and_keeps_the_file_order_otherwise(self):
        c = camp(cfront("a", needs=["b"]), cfront("b"), cfront("c"))
        self.assertEqual([f["id"] for f in bb.route(c)], ["b", "a", "c"])

    def test_a_cycle_is_refused(self):
        self.assertTrue(any("cycle" in e for e in self.chk(camp(cfront("a", needs=["b"]), cfront("b", needs=["a"])))))

    def test_an_unknown_or_self_need_is_refused(self):
        self.assertTrue(any("not a front id" in e for e in self.chk(camp(cfront("a", needs=["zz"])))))
        self.assertTrue(any("needs itself" in e for e in self.chk(camp(cfront("a", needs=["a"])))))

    def test_a_front_is_locked_until_every_need_is_down(self):
        c = camp(cfront("a", "down", evidence=[B]), cfront("b", "wounded", ref="en/03-hardware-map.md"), cfront("c", needs=["a"]), cfront("d", needs=["b"]))
        av = bb.availability(c)
        self.assertEqual([av[k][0] for k in range(4)], ["done", "available", "available", "locked"])

    def test_a_campaign_needs_ids_down_needs_and_an_intro(self):
        self.assertTrue(any("needs an id" in e for e in self.chk(camp(front("open", down_needs="unknown")))))
        self.assertTrue(any("down_needs" in e for e in self.chk(camp(front("open", id="a")))))
        self.assertTrue(any("intro" in e for e in self.chk(camp(cfront("a"), campaign_intro={"en": "i"}))))
        self.assertTrue(any("down_needs must be" in e for e in self.chk(camp(cfront("a", down_needs="magic")))))
        self.assertEqual(self.chk(camp(cfront("a"), cfront("b", needs=["a"]))), [])

    def test_the_page_names_the_next_move_the_locks_and_what_brings_a_front_down(self):
        c = camp(cfront("a"), cfront("b", needs=["a"], down_needs="write"))
        page = bb.render_campaign(c, "en", titles=TITLES)
        self.assertIn("## Next move", page)
        self.assertIn("Locked until this is down:", page)
        self.assertIn("hardware writes: the maintainer only", page)
        self.assertIn("Campagne", bb.render_campaign(c, "fr", titles=TITLES))


class StoryTests(unittest.TestCase):
    def test_a_story_must_be_a_step_of_the_guided_path(self):
        self.assertTrue(any("not a step of the guided path" in e for e in errs(front("open"), story="no-such-step")))
        self.assertEqual(errs(front("open"), story="f-wifi"), [])

    def test_the_pages_link_back_to_the_story(self):
        c = creature(front("open"), story="f-wifi")
        self.assertIn("journey/f-wifi.md", bb.render({"creatures": [c]}, "en", titles=TITLES))


class RenderTests(unittest.TestCase):
    def data(self):
        return {"creatures": [creature(front("down", evidence=[B]), front("open", missions=["M-1"]), front("open"))]}

    def test_the_page_shows_life_proof_missions_and_a_way_to_propose(self):
        page = bb.render(self.data(), "en", titles=TITLES)
        self.assertIn("Life: 4 of 6", page)
        self.assertIn("evidence/%s" % B, page)
        self.assertIn("A mission", page)
        self.assertIn("template=mission.yml", page)

    def test_the_french_page_is_in_french_and_the_bar_is_labelled(self):
        page = bb.render(self.data(), "fr", titles=TITLES)
        self.assertIn("Vie : 4 sur 6", page)
        self.assertIn('aria-label="Vie', page)
        self.assertIn("À terre", page)

    def test_the_real_data_is_valid(self):
        self.assertEqual(bb.check(bb.load()), [])


if __name__ == "__main__":
    unittest.main()
