"""Fail if an internal link in the built site points at a page or anchor that does not
exist (external audit, 2026-10-16: broken anchors have already shipped twice this session
-- a mislinked research index, and a diagram node whose id changed under a stale link).

Works on the built HTML under `site/`, not the Markdown source: mkdocs itself resolves
relative .md links into real hrefs and would already fail `--strict` on a link to a page
that does not exist, but it never checks that an `#anchor` on that link actually exists on
the target page. This script closes exactly that gap -- it does not re-check what mkdocs
already checks.

Usage: run after `mkdocs build` (site/ must exist). Exits 1 and lists every broken link.
"""
import re
import sys
from pathlib import Path
from urllib.parse import unquote, urlsplit

SITE = Path(__file__).resolve().parent.parent / "site"

HREF_RE = re.compile(r'<a\b[^>]*\bhref="([^"]+)"', re.IGNORECASE)
ID_RE = re.compile(r'\bid="([^"]+)"', re.IGNORECASE)

SKIP_SCHEMES = ("http://", "https://", "mailto:", "tel:", "javascript:")


def ids_of(page: Path) -> set[str]:
    return set(ID_RE.findall(page.read_text(encoding="utf-8", errors="replace")))


def resolve_target(page: Path, path: str) -> Path | None:
    """Resolve an href's path part (no fragment) to a built page under SITE, or None if it
    does not point at an HTML page we can check (e.g. an image, a PDF, a non-.md asset)."""
    if path == "":
        return page
    target = (page.parent / path).resolve()
    if target.is_dir():
        target = target / "index.html"
    if not str(target).startswith(str(SITE.resolve())):
        return "outside"  # escaped the site root -- treat as its own failure below
    return target


def main() -> int:
    if not SITE.is_dir():
        print(f"check_internal_links: {SITE} does not exist -- run `mkdocs build` first", file=sys.stderr)
        return 1

    pages = sorted(SITE.rglob("index.html"))
    id_cache: dict[Path, set[str]] = {}
    broken: list[tuple[Path, str, str]] = []
    total_links = 0

    for page in pages:
        html = page.read_text(encoding="utf-8", errors="replace")
        for href in HREF_RE.findall(html):
            if href.startswith("#"):
                path, frag = "", unquote(href[1:])
            else:
                split = urlsplit(href)
                if split.scheme or href.startswith(SKIP_SCHEMES):
                    continue  # external link, not this script's job
                path, frag = split.path, unquote(split.fragment)
            total_links += 1

            target = resolve_target(page, path)
            if target == "outside":
                broken.append((page, href, "resolves outside site/"))
                continue
            if target is None:
                continue
            if target.suffix not in ("", ".html") and target.name != "index.html":
                continue  # asset (image, svg, ...), not a page we can check ids on
            if not target.exists():
                broken.append((page, href, f"target page does not exist ({target.relative_to(SITE)})"))
                continue
            if frag:
                if target not in id_cache:
                    id_cache[target] = ids_of(target)
                if frag not in id_cache[target]:
                    broken.append((page, href, f"no element with id=\"{frag}\" in {target.relative_to(SITE)}"))

    if total_links == 0:
        print("check_internal_links: found zero links across the whole site -- parsing is broken, not clean", file=sys.stderr)
        return 1

    if broken:
        print("check_internal_links: broken internal link(s):", file=sys.stderr)
        for page, href, reason in broken:
            print(f"  {page.relative_to(SITE)}: \"{href}\" -- {reason}", file=sys.stderr)
        return 1

    print(f"check_internal_links: {total_links} internal links checked across {len(pages)} pages, none broken")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
