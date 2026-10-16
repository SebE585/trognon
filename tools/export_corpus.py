#!/usr/bin/env python3
"""Export the public documentation as a dated corpus, for review, a notebook or a retrieval index.

    python3 -I tools/export_corpus.py              # write build/corpus/*.md and a claims file (English)
    python3 -I tools/export_corpus.py --lang fr    # add the French pages

Why this exists: a model answers from the sources it is given, and a stale or private snapshot gives confident wrong answers.
So the export (1) takes only files tracked by git in the public repository, from an explicit allow-list, and refuses the paths the
project keeps private; (2) stamps every file with the commit and the date, and each page with its last change; (3) writes the
claims and open questions of the decompilation catalog as one JSON object per line, with their status and source.
"""
import argparse
import datetime
import json
import subprocess
import sys
from pathlib import Path

import yaml

sys.path.insert(0, str(Path(__file__).resolve().parent))
import journey_build as jb  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "build" / "corpus"
# Never exported, whatever the allow-list says (the project keeps these out of the public repository).
FORBIDDEN = ("CLAUDE", "MEMORY.md", "TODO.md", "AGENTS.md", "docs/journal/", ".claude/", ".env")
GROUPS = {
    "reference": ("README.md", "CONTRIBUTING.md", "docs/index.md", "docs/en/0", "docs/en/README.md"),
    "research": ("docs/research/", "docs/spec/"),
    "journey-and-missions": ("docs/en/journey/", "docs/en/missions.md", "docs/en/verifications.md"),
}
FR_GROUPS = {"reference-fr": ("docs/fr/README.md", "docs/fr/0"), "journey-fr": ("docs/fr/journey/",)}


def git(*args):
    return subprocess.run(["git", *args], capture_output=True, text=True, cwd=ROOT).stdout.strip()


def allowed(path):
    return not any(f in path for f in FORBIDDEN)


def select(tracked, prefixes):
    """The tracked files that match an allow-list prefix and are not forbidden, in a stable order."""
    return sorted(p for p in tracked if p.endswith((".md", ".yaml")) and allowed(p) and any(p.startswith(x) for x in prefixes))


def header(group, sha, date):
    return ("# Trognon public corpus: %s\n\n"
            "State of the public documentation at commit `%s`, exported %s. Nothing after that commit is included, and nothing that is not "
            "tracked in the public repository. Statements carry the levels DEVICE (seen on a real phone), SOURCE (read in code or a "
            "source) and HYPOTHESIS (supposed). A conclusion that was withdrawn is recorded as a correction, not deleted: read "
            "the corrections before quoting a result. Each page below gives the date of its last change.\n\n" % (group, sha, date))


def claims(sha):
    """One JSON object per claim or open question of the approved catalog entries."""
    lines = []
    for binary, fiches in jb.catalog_entries().items():
        for y in fiches:
            base = {"commit": sha, "binary": binary, "entry": y["_file"], "function": jb.fiche_title(y),
                    "binary_sha256": str(y.get("binary_sha256", "")), "analysed": y["_date"]}
            for c in y.get("claims") or []:
                lines.append(json.dumps(dict(base, kind="claim", id=c["id"], status=c.get("status"), text=" ".join(str(c.get("text", "")).split())), sort_keys=True))
            for q in (y.get("review") or {}).get("unresolved") or []:
                lines.append(json.dumps(dict(base, kind="open-question", id=q["id"], status="open", text=" ".join(str(q.get("text", "")).split()),
                                             proof_needed=" ".join(str(q.get("proof_needed", "")).split())), sort_keys=True))
    return "\n".join(lines) + "\n"


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--lang", choices=("en", "fr"), default="en", help="fr adds the French pages")
    args = ap.parse_args(argv)
    tracked = git("ls-files").splitlines()
    sha = git("rev-parse", "--short", "HEAD")
    date = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%d %H:%M UTC")
    groups = dict(GROUPS)
    if args.lang == "fr":
        groups.update(FR_GROUPS)
    OUT.mkdir(parents=True, exist_ok=True)
    for old in OUT.glob("trognon-*"):
        old.unlink()
    for group, prefixes in groups.items():
        files = select(tracked, prefixes)
        parts = [header(group, sha, date)]
        for f in files:
            changed = git("log", "-1", "--format=%cs", "--", f) or "uncommitted"
            parts.append("\n\n=== %s (last changed %s) ===\n\n%s" % (f, changed, (ROOT / f).read_text()))
        target = OUT / ("trognon-%s-%s.md" % (group, sha))
        target.write_text("".join(parts))
        print("%-26s %3d files, %6d words" % (target.name, len(files), len("".join(parts).split())))
    (OUT / ("trognon-claims-%s.jsonl" % sha)).write_text(claims(sha))
    print("trognon-claims-%s.jsonl   %d records" % (sha, len((OUT / ("trognon-claims-%s.jsonl" % sha)).read_text().splitlines())))
    return 0


if __name__ == "__main__":
    sys.exit(main())
