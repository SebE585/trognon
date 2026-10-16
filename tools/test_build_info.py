"""Test for hooks/build_info.py: every build records the commit and the date."""
import importlib.util
import re
import unittest
from pathlib import Path

spec = importlib.util.spec_from_file_location("build_info", Path(__file__).resolve().parent.parent / "hooks" / "build_info.py")
bi = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bi)


class Cfg:
    def __init__(self):
        self.extra = {}


class HookTests(unittest.TestCase):
    def test_the_hook_records_commit_dirtiness_and_date(self):
        cfg = bi.on_config(Cfg())
        self.assertRegex(cfg.extra["build_commit"], r"^[0-9a-f]{7,}$|^unknown$")
        self.assertIn(cfg.extra["build_dirty"], (True, False))
        self.assertRegex(cfg.extra["build_date"], r"^\d{4}-\d{2}-\d{2}$")


if __name__ == "__main__":
    unittest.main()
