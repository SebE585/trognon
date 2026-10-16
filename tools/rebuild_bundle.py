#!/usr/bin/env python3
"""Rebuild the payload of an evidence bundle and compare its SHA-256 with the bundle's manifest.

    python3 tools/rebuild_bundle.py evidence/20261016T190939Z-trace_spi1_selftest
    python3 tools/rebuild_bundle.py BUNDLE --search-later     # when the recorded commit cannot rebuild it
    python3 tools/rebuild_bundle.py --all                     # every bundle (slow)

It checks out the commit the manifest records in a temporary worktree (your working tree is not touched), applies source.diff if the
bundle has one, builds with `make -C tools/payloads`, and prints one row for tools/verifications.yaml: match, mismatch or
not-reproducible, with the commit that worked and your toolchain. A mismatch is a result, not an error: a different compiler can
produce different bytes, and that is worth recording. It needs git history and arm-none-eabi-gcc; it never touches a phone.
"""
import argparse
import hashlib
import json
import os
import platform
import shutil
import subprocess
import sys
import tempfile
import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
EVIDENCE = ROOT / "evidence"


def git(*args, cwd=ROOT):
    return subprocess.run(["git", *args], capture_output=True, text=True, cwd=cwd)


def toolchain():
    gcc = shutil.which("arm-none-eabi-gcc")
    if not gcc:
        return None
    out = subprocess.run([gcc, "--version"], capture_output=True, text=True).stdout.splitlines()
    return out[0].strip() if out else "arm-none-eabi-gcc (version unknown)"


def verdict(expected, got):
    """match when the built bytes hash to the manifest's value, mismatch when they hash to something else, not-reproducible when nothing was built."""
    if got is None:
        return "not-reproducible"
    return "match" if got == expected else "mismatch"


def build_at(commit, payload, patch=None):
    """SHA-256 of the payload built at a commit (with an optional patch applied), or None when it cannot be built."""
    wt = tempfile.mkdtemp(prefix="trognon-rebuild-")
    try:
        if git("worktree", "add", "--detach", "-f", wt, commit).returncode:
            return None
        if patch is not None and git("apply", str(patch), cwd=wt).returncode:
            return None
        target = Path(wt) / "tools" / "payloads"
        subprocess.run(["make", "-C", str(target), payload], capture_output=True, text=True)
        built = target / payload
        return hashlib.sha256(built.read_bytes()).hexdigest() if built.is_file() else None
    finally:
        git("worktree", "remove", "--force", wt)
        shutil.rmtree(wt, ignore_errors=True)


def later_commits(commit):
    return git("log", "--format=%H", "--reverse", commit + "..HEAD", "--", "tools/payloads").stdout.split()


def verify(bundle, search_later=False):
    """The verification record for one bundle directory."""
    bundle = Path(bundle)
    manifest = json.loads((bundle / "manifest.json").read_text())
    payload, expected = manifest["payload"], manifest["sha256"]
    commit = manifest["git"]["commit"]
    patch = bundle / "source.diff" if (bundle / "source.diff").is_file() else None
    notes = ""
    got = build_at(commit, payload, patch)
    used = commit
    if got != expected and search_later and manifest["git"].get("dirty") and patch is None:
        for c in later_commits(commit):
            h = build_at(c, payload)
            if h == expected:
                got, used = h, c
                notes = "Recorded with a dirty tree and no source.diff; rebuilds only at this later commit."
                break
    result = verdict(expected, got)
    if result == "not-reproducible" and not notes:
        notes = "No commit that was tried rebuilds this exact payload."
    if result == "mismatch":
        notes = "Built bytes differ from the manifest (built %s)." % got
    return {"bundle": bundle.name, "kind": "rebuild", "date": datetime.date.today().isoformat(), "by": "YOUR-GITHUB-HANDLE",
            "environment": {"host": "%s %s" % (platform.system(), platform.machine()), "toolchain": toolchain() or "none"},
            "result": result, "commit": used if result in ("match", "mismatch") else None, "notes": notes}


def as_yaml(rec):
    lines = ["  - bundle: %s" % rec["bundle"], "    kind: %s" % rec["kind"], '    date: "%s"' % rec["date"], "    by: %s" % rec["by"],
             "    environment: { host: %s, toolchain: %s }" % (json.dumps(rec["environment"]["host"]), json.dumps(rec["environment"]["toolchain"])),
             "    result: %s" % rec["result"]]
    if rec.get("commit"):
        lines.append("    commit: %s" % rec["commit"])
    lines.append("    notes: %s" % json.dumps(rec.get("notes", "")))
    return "\n".join(lines)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("bundle", nargs="?", help="a bundle directory under evidence/")
    ap.add_argument("--all", action="store_true", help="every bundle under evidence/ (slow)")
    ap.add_argument("--search-later", action="store_true", help="when the recorded commit cannot rebuild a dirty bundle, try the later commits that touched tools/payloads")
    ap.add_argument("--by", default="YOUR-GITHUB-HANDLE", help="who ran this, written in the row")
    args = ap.parse_args(argv)
    if not toolchain():
        print("arm-none-eabi-gcc is not installed: install it to rebuild a payload (the result would be meaningless without it)", file=sys.stderr)
        return 2
    targets = sorted(p for p in EVIDENCE.iterdir() if (p / "manifest.json").is_file()) if args.all else [Path(args.bundle)]
    if not args.all and not args.bundle:
        ap.error("give a bundle directory, or --all")
    print("verifications:")
    for t in targets:
        rec = verify(t, args.search_later)
        rec["by"] = args.by
        print(as_yaml(rec))
    return 0


if __name__ == "__main__":
    sys.exit(main())
