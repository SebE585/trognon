#!/usr/bin/env python3
"""Regression tests for gen_coverage.py's validator and counters (added 2026-10-16, per
review: the catalog's own mechanisms need tests as much as the catalog's content needs
review -- two real bugs were found by hand in gen_coverage.py itself the same night this was
written, which is exactly the kind of mistake a test would catch on the next change instead).

Builds a disposable scratch catalog per test (tempfile.TemporaryDirectory) -- never touches
the real tools/iop_analysis/catalog/. Run with: python3 -m unittest tools/iop_analysis/test_gen_coverage.py
or just: python3 tools/iop_analysis/test_gen_coverage.py
"""
import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_coverage as gc  # noqa: E402


SHA = "a" * 64


def write_tool_coverage(subdir, **overrides):
    data = {
        "binary": "Fake.kext.bin",
        "binary_sha256": SHA,
        "date": "2026-10-16",
        "text_section_bytes": 0x100,
        "text_section_base": "0x1000",
        "auto_recognized_bytes": 0,
        "auto_recognized_functions": 0,
        "recognized_ranges": [],
        "tool": "radare2",
        "tool_version": "6.2.4",
        "method": "test fixture",
        "note": "test fixture",
    }
    data.update(overrides)
    with open(os.path.join(subdir, "tool_coverage.json"), "w") as f:
        json.dump(data, f)


def write_fiche(subdir, name, **overrides):
    import yaml
    fiche = {
        "binary": "Fake.kext.bin",
        "binary_sha256": SHA,
        "function_name_apple": None,
        "function_name_assigned": "test fn",
        "decode_mode": "thumb",
        "subsystem": "test",
        "review": {"status": "partial", "reviewed_ranges": [], "unresolved": []},
        "claims": [],
        "hardware_tests": [],
        "publication": {"status": "approved"},
    }
    for k, v in overrides.items():
        if isinstance(v, dict) and k in fiche and isinstance(fiche[k], dict):
            fiche[k].update(v)
        else:
            fiche[k] = v
    with open(os.path.join(subdir, f"{name}.yaml"), "w") as f:
        yaml.safe_dump(fiche, f)


class CatalogTestCase(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.catalog = os.path.join(self._tmp.name, "catalog")
        self.binary_dir = os.path.join(self.catalog, "Fake")
        os.makedirs(self.binary_dir)

    def tearDown(self):
        self._tmp.cleanup()


class TestSHAConsistency(CatalogTestCase):
    def test_matching_sha_passes(self):
        write_tool_coverage(self.binary_dir)
        write_fiche(self.binary_dir, "f1")
        binaries = gc.load_catalog(self.catalog)
        gc.validate(binaries)  # must not raise

    def test_mismatched_sha_fails(self):
        write_tool_coverage(self.binary_dir)
        write_fiche(self.binary_dir, "f1", binary_sha256="b" * 64)
        binaries = gc.load_catalog(self.catalog)
        with self.assertRaises(SystemExit) as ctx:
            gc.validate(binaries)
        self.assertIn("binary_sha256", str(ctx.exception))


class TestRangeSanity(CatalogTestCase):
    def test_reviewed_range_outside_text_section_fails(self):
        write_tool_coverage(self.binary_dir)  # text: [0x1000, 0x1100)
        write_fiche(self.binary_dir, "f1", review={"reviewed_ranges": [["0x2000", "0x2010"]]})
        binaries = gc.load_catalog(self.catalog)
        with self.assertRaises(SystemExit) as ctx:
            gc.validate(binaries)
        self.assertIn("falls outside", str(ctx.exception))

    def test_reviewed_range_within_text_section_passes(self):
        write_tool_coverage(self.binary_dir)
        write_fiche(self.binary_dir, "f1", review={"reviewed_ranges": [["0x1010", "0x1020"]]})
        binaries = gc.load_catalog(self.catalog)
        gc.validate(binaries)

    def test_inverted_range_fails(self):
        write_tool_coverage(self.binary_dir)
        write_fiche(self.binary_dir, "f1", review={"reviewed_ranges": [["0x1020", "0x1010"]]})
        binaries = gc.load_catalog(self.catalog)
        with self.assertRaises(SystemExit) as ctx:
            gc.validate(binaries)
        self.assertIn("start >= end", str(ctx.exception))


class TestOverlapUnion(unittest.TestCase):
    def test_overlapping_ranges_counted_once(self):
        # [0x10,0x20) + [0x18,0x30) overlap on [0x18,0x20) -- union is [0x10,0x30) = 0x20 bytes,
        # NOT (0x10 + 0x18) = 0x28 bytes (which double-counts the overlap).
        self.assertEqual(gc.union_bytes([["0x10", "0x20"], ["0x18", "0x30"]]), 0x20)

    def test_disjoint_ranges_summed(self):
        self.assertEqual(gc.union_bytes([["0x10", "0x20"], ["0x30", "0x40"]]), 0x20)

    def test_adjacent_ranges_merged(self):
        # [0x10,0x20) and [0x20,0x30) touch at 0x20 but do not overlap (half-open) -- still a
        # contiguous union, 0x20 bytes, same result as the overlap case above for a different
        # reason: this exercises the merge boundary condition (s <= merged[-1][1]), not overlap.
        self.assertEqual(gc.union_bytes([["0x10", "0x20"], ["0x20", "0x30"]]), 0x20)


class TestCacheSelfConsistency(CatalogTestCase):
    def test_matching_cache_passes(self):
        write_tool_coverage(
            self.binary_dir,
            recognized_ranges=[["0x1000", "0x1010"], ["0x1020", "0x1030"]],
            auto_recognized_bytes=0x20,
            auto_recognized_functions=2,
        )
        write_fiche(self.binary_dir, "f1")
        binaries = gc.load_catalog(self.catalog)
        gc.validate(binaries)

    def test_drifted_byte_total_fails(self):
        write_tool_coverage(
            self.binary_dir,
            recognized_ranges=[["0x1000", "0x1010"]],
            auto_recognized_bytes=0x99,  # wrong on purpose -- real range sums to 0x10
            auto_recognized_functions=1,
        )
        write_fiche(self.binary_dir, "f1")
        binaries = gc.load_catalog(self.catalog)
        with self.assertRaises(SystemExit) as ctx:
            gc.validate(binaries)
        self.assertIn("does not match the union", str(ctx.exception))

    def test_drifted_function_count_fails(self):
        write_tool_coverage(
            self.binary_dir,
            recognized_ranges=[["0x1000", "0x1010"]],
            auto_recognized_bytes=0x10,
            auto_recognized_functions=5,  # wrong on purpose -- only one range given
        )
        write_fiche(self.binary_dir, "f1")
        binaries = gc.load_catalog(self.catalog)
        with self.assertRaises(SystemExit) as ctx:
            gc.validate(binaries)
        self.assertIn("auto_recognized_functions", str(ctx.exception))


class TestHardwareExecutedCounting(CatalogTestCase):
    def test_planned_test_excluded_from_executed_count(self):
        write_tool_coverage(self.binary_dir)
        write_fiche(
            self.binary_dir, "f1",
            hardware_tests=[
                {"name": "planned_test", "claim_tested": "x", "execution_status": "planned", "result": None},
            ],
        )
        binaries = gc.load_catalog(self.catalog)
        gc.validate(binaries)
        out = gc.render(binaries)
        self.assertIn("0 / 1", out)  # 0 executed, out of 1 test total

    def test_executed_test_counted(self):
        write_tool_coverage(self.binary_dir)
        write_fiche(
            self.binary_dir, "f1",
            hardware_tests=[
                {"name": "real_test", "claim_tested": "x", "execution_status": "executed", "result": "ok"},
            ],
        )
        binaries = gc.load_catalog(self.catalog)
        gc.validate(binaries)
        out = gc.render(binaries)
        self.assertIn("1 / 1", out)

    def test_missing_execution_status_fails_validation(self):
        write_tool_coverage(self.binary_dir)
        write_fiche(
            self.binary_dir, "f1",
            hardware_tests=[{"name": "bad_test", "claim_tested": "x", "result": None}],
        )
        binaries = gc.load_catalog(self.catalog)
        with self.assertRaises(SystemExit) as ctx:
            gc.validate(binaries)
        self.assertIn("execution_status", str(ctx.exception))


class TestPublicationGate(CatalogTestCase):
    def test_pending_review_claim_excluded_from_table(self):
        write_tool_coverage(self.binary_dir)
        write_fiche(
            self.binary_dir, "f1",
            claims=[{"id": "FAKE-001", "text": "secret-ish claim", "status": "hypothesis", "evidence": ""}],
            publication={"status": "pending_review"},
        )
        binaries = gc.load_catalog(self.catalog)
        gc.validate(binaries)
        out = gc.render(binaries)
        self.assertNotIn("FAKE-001", out)
        self.assertIn("further claim(s) exist", out)


class TestFunctionRecords(CatalogTestCase):
    def test_adjacent_functions_counted_separately(self):
        import json
        write_tool_coverage(self.binary_dir)
        path = os.path.join(self.binary_dir, "tool_coverage.json")
        tool = json.load(open(path))
        tool.pop("recognized_ranges", None)
        tool["recognized_functions"] = [["0x1000", "0x1010"], ["0x1010", "0x1020"], ["0x1020", "0x1030"]]
        tool["auto_recognized_functions"] = 3
        tool["auto_recognized_bytes"] = 0x30
        json.dump(tool, open(path, "w"))
        gc.validate(gc.load_catalog(self.catalog))

    def test_function_count_must_match_records(self):
        import json
        write_tool_coverage(self.binary_dir)
        path = os.path.join(self.binary_dir, "tool_coverage.json")
        tool = json.load(open(path))
        tool.pop("recognized_ranges", None)
        tool["recognized_functions"] = [["0x1000", "0x1010"], ["0x1010", "0x1020"]]
        tool["auto_recognized_functions"] = 1
        tool["auto_recognized_bytes"] = 0x20
        json.dump(tool, open(path, "w"))
        with self.assertRaises(SystemExit):
            gc.validate(gc.load_catalog(self.catalog))


class TestConstantRanges(CatalogTestCase):
    def test_constants_not_counted_as_code(self):
        write_tool_coverage(self.binary_dir)
        write_fiche(self.binary_dir, "f1", review={"status": "complete",
            "reviewed_ranges": [["0x1000", "0x1010"]],
            "constant_ranges": [["0x1010", "0x1020"]], "unresolved": []})
        binaries = gc.load_catalog(self.catalog)
        gc.validate(binaries)
        self.assertIn("| 16 B |", gc.render(binaries))

    def test_constants_overlapping_code_fail(self):
        write_tool_coverage(self.binary_dir)
        write_fiche(self.binary_dir, "f1", review={"status": "complete",
            "reviewed_ranges": [["0x1000", "0x1010"]],
            "constant_ranges": [["0x100c", "0x1020"]], "unresolved": []})
        with self.assertRaises(SystemExit) as ctx:
            gc.validate(gc.load_catalog(self.catalog))
        self.assertIn("overlaps", str(ctx.exception))


class TestOpenLinks(CatalogTestCase):
    def test_open_link_and_planned_test_rendered_with_proof(self):
        write_tool_coverage(self.binary_dir)
        write_fiche(
            self.binary_dir, "f1",
            review={"status": "complete", "reviewed_ranges": [], "unresolved": [
                {"id": "FAKE-Q1", "text": "indirect call", "proof_needed": "read slot 0x34c"}]},
            hardware_tests=[{"name": "t1", "claim_tested": "x", "execution_status": "planned", "result": None}],
        )
        binaries = gc.load_catalog(self.catalog)
        gc.validate(binaries)
        out = gc.render(binaries)
        self.assertIn("`FAKE-Q1`", out)
        self.assertIn("read slot 0x34c", out)
        self.assertIn("| t1 | `test fn` | planned | not run |", out)

    def test_open_link_of_unpublished_fiche_not_rendered(self):
        write_tool_coverage(self.binary_dir)
        write_fiche(
            self.binary_dir, "f1",
            review={"status": "partial", "reviewed_ranges": [], "unresolved": [
                {"id": "HIDDEN-Q1", "text": "x", "proof_needed": "y"}]},
            publication={"status": "pending_review"},
        )
        binaries = gc.load_catalog(self.catalog)
        gc.validate(binaries)
        self.assertNotIn("HIDDEN-Q1", gc.render(binaries))


class TestDuplicateIds(CatalogTestCase):
    def test_duplicate_claim_id_across_files_fails(self):
        write_tool_coverage(self.binary_dir)
        write_fiche(self.binary_dir, "f1", claims=[{"id": "DUP-001", "text": "a", "status": "hypothesis", "evidence": ""}])
        write_fiche(self.binary_dir, "f2", claims=[{"id": "DUP-001", "text": "b", "status": "hypothesis", "evidence": ""}])
        binaries = gc.load_catalog(self.catalog)
        with self.assertRaises(SystemExit) as ctx:
            gc.validate(binaries)
        self.assertIn("duplicate claim id", str(ctx.exception))

    def test_duplicate_question_id_across_files_fails(self):
        write_tool_coverage(self.binary_dir)
        write_fiche(self.binary_dir, "f1", review={"unresolved": [{"id": "DUP-Q1", "text": "a"}]})
        write_fiche(self.binary_dir, "f2", review={"unresolved": [{"id": "DUP-Q1", "text": "b"}]})
        binaries = gc.load_catalog(self.catalog)
        with self.assertRaises(SystemExit) as ctx:
            gc.validate(binaries)
        self.assertIn("duplicate open-question id", str(ctx.exception))

    def test_bare_string_question_fails(self):
        write_tool_coverage(self.binary_dir)
        write_fiche(self.binary_dir, "f1", review={"unresolved": ["a bare string, no id"]})
        binaries = gc.load_catalog(self.catalog)
        with self.assertRaises(SystemExit) as ctx:
            gc.validate(binaries)
        self.assertIn("has no id", str(ctx.exception))


if __name__ == "__main__":
    unittest.main()
