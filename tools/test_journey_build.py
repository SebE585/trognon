"""Tests for tools/journey_build.py: the checks that keep the guided journey from drifting away from the reference pages."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import journey_build as jb  # noqa: E402


def node(i, to=(), kind="step", tag=None, **extra):
    n = {"id": i, "kind": kind, "title": i, "tag": tag, "text": "x", "choices": [{"text": "go", "to": t} for t in to]}
    n.update(extra)
    return n


def graph(*nodes, start="a"):
    return {"start": start, "nodes": list(nodes), "_src": "test.yaml"}


class CheckTests(unittest.TestCase):
    def test_valid_graph(self):
        self.assertEqual(jb.check(graph(node("a", ["b"], kind="start"), node("b", ["a"]))), [])

    def test_dead_end(self):
        errs = jb.check(graph(node("a", ["b"], kind="start"), node("b")))
        self.assertTrue(any("dead end" in e for e in errs))

    def test_missing_target(self):
        errs = jb.check(graph(node("a", ["zzz"], kind="start")))
        self.assertTrue(any("missing node" in e for e in errs))

    def test_unreachable(self):
        errs = jb.check(graph(node("a", ["a"], kind="start"), node("orphan", ["a"])))
        self.assertTrue(any("not reachable" in e for e in errs))

    def test_device_needs_evidence(self):
        errs = jb.check(graph(node("a", ["a"], kind="start", tag="DEVICE")))
        self.assertTrue(any("names no evidence" in e for e in errs))

    def test_missing_evidence_bundle(self):
        errs = jb.check(graph(node("a", ["a"], kind="start", tag="DEVICE", evidence=["no-such-bundle"])))
        self.assertTrue(any("does not exist" in e for e in errs))

    def test_reference_must_exist(self):
        errs = jb.check(graph(node("a", ["a"], kind="start", refs=[{"label": "x", "page": "en/no-such-page.md"}])))
        self.assertTrue(any("does not exist" in e for e in errs))

    def test_reference_heading_must_exist(self):
        errs = jb.check(graph(node("a", ["a"], kind="start", refs=[{"label": "x", "page": "en/01-boot-chain.md#no-such-heading"}])))
        self.assertTrue(any("no heading" in e for e in errs))

    def test_frontier_needs_know_guess_next_and_refs(self):
        errs = jb.check(graph(node("a", ["f"], kind="start"), node("f", ["a"], kind="frontier")))
        self.assertTrue(any("needs 'know'" in e for e in errs))
        self.assertTrue(any("at least one reference" in e for e in errs))

    def test_side_branch_is_not_in_the_trail(self):
        g = graph(node("a", ["s", "b"], kind="start"), node("s", ["c"], kind="side"), node("b", ["c"]), node("c", ["a"]))
        self.assertEqual(jb.routes(g)["c"], ["a", "b", "c"])


class StructureTests(unittest.TestCase):
    def test_parity_detects_a_different_choice(self):
        en = graph(node("a", ["b"], kind="start"), node("b", ["a"]))
        en.update(lang="en", _src="x.yaml")
        fr = graph(node("a", ["b"], kind="start"), node("b", ["b"]))
        fr.update(lang="fr", _src="x.fr.yaml")
        self.assertTrue(any("differs in structure" in e for e in jb.parity([en, fr])))

    def test_parity_accepts_a_translation_with_other_texts(self):
        en = graph(node("a", ["b"], kind="start"), node("b", ["a"]))
        en.update(lang="en", _src="x.yaml")
        fr = graph(node("a", ["b"], kind="start", title="autre titre"), node("b", ["a"]))
        fr.update(lang="fr", _src="x.fr.yaml")
        self.assertEqual(jb.parity([en, fr]), [])

    def test_a_translation_needs_an_english_graph(self):
        fr = graph(node("a", ["a"], kind="start"))
        fr.update(lang="fr", _src="y.fr.yaml")
        self.assertTrue(any("no English graph" in e for e in jb.parity([fr])))

    def test_front_matter_with_a_colon_in_the_title_is_valid(self):
        page = "\n".join(jb.front_matter("The phone listens: DFU mode", "t.yaml")) + "\nbody\n"
        self.assertEqual(jb.check_pages({"dfu.md": page}), [])

    def test_unquoted_colon_in_front_matter_is_caught(self):
        self.assertTrue(jb.check_pages({"bad.md": "---\ntitle: The phone listens: DFU mode\n---\n"}))

    def test_correction_needs_a_date_and_both_sides(self):
        n = node("a", ["a"], kind="start", corrections=[{"date": "yesterday", "was": "", "now": "x", "ref": {"label": "l", "page": "en/01-boot-chain.md"}}])
        errs = jb.check(graph(n))
        self.assertTrue(any("ISO date" in e for e in errs))
        self.assertTrue(any("needs 'was'" in e for e in errs))

    def test_unknown_notebook_note_is_caught(self):
        errs = jb.check(graph(node("a", ["a"], kind="start", notebook=["NO-SUCH-NOTE"])))
        self.assertTrue(any("not in the approved catalog" in e for e in errs))

    def test_every_language_has_the_same_interface_keys(self):
        self.assertEqual(set(jb.I18N["en"]), set(jb.I18N["fr"]))
        for lang in jb.I18N:
            self.assertEqual(set(jb.VISUAL_CAPTIONS[lang]), set(jb.VISUAL_STATES))

    def test_markdown_unsafe_titles_are_escaped_in_the_map(self):
        entries = {"B": [{"_file": "f.yaml", "function_name_apple": "A::start(IOService*)", "review": {"status": "partial", "unresolved": []}, "claims": []}]}
        text = jb.knowledge_map("en", entries, {"B": "grimoire-b"})
        self.assertNotIn("*)", text)


class AccessibilityTests(unittest.TestCase):
    def test_scenes_hide_the_documentation_footer(self):
        self.assertIn("  - footer", jb.front_matter("T", "t.yaml"))

    def test_a_map_tile_has_a_symbol_and_an_accessible_name(self):
        entries = {"B": [{"_file": "f.yaml", "function_name_apple": "A::f()", "review": {"status": "partial", "unresolved": [{"id": "Q"}]}, "claims": []}]}
        text = jb.knowledge_map("en", entries, {"B": "grimoire-b"})
        self.assertIn("[◐1](grimoire-b.md)", text)
        self.assertIn('aria-label="A::f(), partly reviewed, 1 open question(s)"', text)

    def test_every_review_state_has_a_distinct_symbol(self):
        out = {}
        for status in ("complete", "partial", None):
            e = {"B": [{"_file": "f.yaml", "function_name_apple": "x", "review": {"status": status, "unresolved": []}, "claims": []}]}
            out[status] = jb.knowledge_map("en", e, {"B": "g"})
        self.assertIn("✓0", out["complete"])
        self.assertIn("◐0", out["partial"])
        self.assertIn("○0", out[None])

    def test_a_binary_page_lists_the_missions_that_concern_it(self):
        node = {"binary": "B", "blurb": "", "fiches": [{"_file": "f.yaml", "function_name_apple": "x", "claims": [],
                "review": {"status": "partial", "unresolved": [{"id": "S5L-CLK-Q1", "text": "t"}]}}]}
        self.assertIn("Missions on this binary", jb.grimoire_body(node, "en"))
        self.assertIn("../../en/missions.md", jb.grimoire_body(node, "fr"))

    def test_a_binary_with_no_mission_has_no_missions_section(self):
        node = {"binary": "B", "blurb": "", "fiches": [{"_file": "f.yaml", "function_name_apple": "x", "claims": [],
                "review": {"status": "partial", "unresolved": [{"id": "NO-MISSION", "text": "t"}]}}]}
        self.assertNotIn("Missions on this binary", jb.grimoire_body(node, "en"))


if __name__ == "__main__":
    unittest.main()
