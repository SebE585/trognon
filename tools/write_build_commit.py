"""Write docs/snippets/build-commit.{en,fr}.md: which commit the published site was built from.

Generated, never hand-written (CLAUDE.md: "ce qui peut perimer est genere, pas ecrit"). Run before
`mkdocs build` (see `make docs-build`) so every published page can show it was built from a known,
named commit -- the Pages workflow has no other visible proof of which source produced the live site.
"""
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent


def main() -> int:
    try:
        commit = subprocess.check_output(['git', '-C', str(REPO), 'rev-parse', '--short', 'HEAD'],
                                          text=True).strip()
        dirty = bool(subprocess.check_output(['git', '-C', str(REPO), 'status', '--porcelain', '--', '.', ':(exclude)docs/snippets/build-commit.en.md', ':(exclude)docs/snippets/build-commit.fr.md'], text=True).strip())
    except subprocess.CalledProcessError as e:
        print(f"write_build_commit: git failed: {e}", file=sys.stderr)
        return 1
    url = f'https://github.com/SebE585/trognon/commit/{commit}'
    suffix = ' (uncommitted changes present)' if dirty else ''
    texts = {
        'en': f'Built from commit [`{commit}`]({url}){suffix}.',
        'fr': f'Construit depuis le commit [`{commit}`]({url}){suffix}.',
    }
    for lang, text in texts.items():
        path = REPO / 'docs' / 'snippets' / f'build-commit.{lang}.md'
        path.write_text(text + '\n', encoding='utf-8')
    print(f"write_build_commit: {commit}{suffix}")
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
