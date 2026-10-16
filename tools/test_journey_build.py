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


if __name__ == "__main__":
    unittest.main()
