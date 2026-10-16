"""Fail if any built page has more or fewer than one <h1> (external audit, 2026-10-16).

Catches the class of bug that shipped on the IOP report: a source line wrapped so that
"#6." (part of a sentence, "see 'Open questions' #6.") landed at the start of a physical
line, and Python-Markdown (unlike strict CommonMark) treats a line-initial "#" as an ATX
heading even mid-paragraph -- silently rendering two extra, wrong <h1> elements inside the
body text. `mkdocs build --strict` does not catch this; it is not a broken link or a missing
reference, just a heading that should not exist.

Usage: run after `mkdocs build` (site/ must exist). Exits 1 and lists every offending page.
"""
import re
import sys
from pathlib import Path

SITE = Path(__file__).resolve().parent.parent / "site"
H1_RE = re.compile(r"<h1\b", re.IGNORECASE)
SKIP_NAMES = {"404.html"}  # the 404 page is not a doc page and is allowed its own shape


def main() -> int:
    if not SITE.is_dir():
        print(f"check_headings: {SITE} does not exist -- run `mkdocs build` first", file=sys.stderr)
        return 1
    bad = []
    for page in sorted(SITE.rglob("index.html")):
        if page.name in SKIP_NAMES:
            continue
        count = len(H1_RE.findall(page.read_text(encoding="utf-8", errors="replace")))
        if count != 1:
            bad.append((page.relative_to(SITE), count))
    if bad:
        print("check_headings: expected exactly one <h1> per page, found:", file=sys.stderr)
        for page, count in bad:
            print(f"  {count} <h1> in {page}", file=sys.stderr)
        return 1
    print(f"check_headings: {len(list(SITE.rglob('index.html'))) - len(SKIP_NAMES)} pages, all with exactly one <h1>")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
