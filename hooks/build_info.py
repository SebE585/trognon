"""MkDocs hook: make the commit and the date of the build available to every template (config.extra.build_*).

A page that is copied, cached or imported into a notebook keeps saying what it said on that day. Stamping every page with the commit it was
built from lets a reader, or a model, see how old its copy is and compare it with the live site.
"""
import datetime
import subprocess


def _git(*args):
    try:
        return subprocess.run(["git", *args], capture_output=True, text=True, timeout=10).stdout.strip()
    except Exception:
        return ""


def on_config(config):
    config.extra["build_commit"] = _git("rev-parse", "--short", "HEAD") or "unknown"
    # the build-commit snippets are rewritten by the build itself: they must not make the tree look modified
    config.extra["build_dirty"] = bool(_git("status", "--porcelain", "--", ".", ":(exclude)docs/snippets/build-commit.en.md", ":(exclude)docs/snippets/build-commit.fr.md"))
    config.extra["build_date"] = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%d")
    return config
