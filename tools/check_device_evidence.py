#!/usr/bin/env python3
"""Check that every section of the hardware map tagged DEVICE cites evidence that exists.

    python3 -I tools/check_device_evidence.py

A section is DEVICE when the line right under its title carries the DEVICE tag (including "mixed DEVICE / HYPOTHESIS"). It must name at
least one evidence bundle, written as the folder name under evidence/ (a link or in backticks), and every bundle it names must exist.
A section with no dedicated bundle must say so with the exact phrase "none dedicated" (English) or "aucun dédié" (French): the gap is then
declared and listed, never silent, even when a bundle for a different claim is also named.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
EVIDENCE = ROOT / "evidence"
PAGES = ("docs/en/03-hardware-map.md", "docs/fr/03-hardware-map.md")
BUNDLE = re.compile(r"\b(\d{8}T\d{6}Z-[A-Za-z0-9_]+)\b")
DECLARED_GAP = ("none dedicated", "aucun dédié")


def sections(text):
    """[(title, body)] for each '## ' section."""
    parts = re.split(r"(?m)^## ", text)
    return [(p.split("\n", 1)[0].strip(), p.split("\n", 1)[1] if "\n" in p else "") for p in parts[1:]]


def is_device(body):
    for line in body.splitlines():
        if line.strip():
            return "**DEVICE" in line or "DEVICE**" in line
    return False


def check_section(title, body, exists):
    """(errors, declared_gap) for one section."""
    if not is_device(body):
        return [], False
    bundles = sorted(set(BUNDLE.findall(body)))
    missing = [b for b in bundles if not exists(b)]
    if missing:
        return ["%s: names evidence that does not exist: %s" % (title, ", ".join(missing))], False
    if any(p in body for p in DECLARED_GAP):
        return [], True  # declared and listed, even if a bundle for a different claim is also named
    if bundles:
        return [], False
    return ["%s: tagged DEVICE but cites no evidence bundle and does not declare a gap" % title], False


def main():
    errors, gaps, checked = [], [], 0
    for page in PAGES:
        for title, body in sections((ROOT / page).read_text()):
            if not is_device(body):
                continue
            checked += 1
            errs, gap = check_section(title, body, lambda b: (EVIDENCE / b).is_dir())
            errors += ["%s: %s" % (page, e) for e in errs]
            if gap:
                gaps.append("%s: %s" % (page, title))
    for e in errors:
        print(e)
    for g in gaps:
        print("declared gap (no dedicated bundle): " + g)
    if errors:
        return 1
    print("device evidence: %d DEVICE sections checked, %d declared gap(s)" % (checked, len(gaps)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
