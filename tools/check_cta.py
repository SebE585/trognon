#!/usr/bin/env python3
"""Check the calls to action that bring content back into the repository: issues and pull requests.

    python3 -I tools/check_cta.py            # offline: the built site (run mkdocs build first) and the files of the repository
    python3 -I tools/check_cta.py --remote   # also ask GitHub: do the files exist on main, does it parse the issue forms, do the labels exist

What it refuses, because each one is a dead end for a reader who wants to help:
  - a page without the participation strip (correction, observation, edit, missions);
  - an edit link to a file that is not in the repository, or to a GENERATED file (an edit there would be overwritten; the link must go to its source);
  - an issue link to a template that does not exist, or that pre-fills a field the template does not have;
  - an issue form GitHub would reject (no name, no body, an input with no id or label, duplicate ids, an unknown element type);
  - an open mission whose "Take this mission" link is missing from the Missions page, or a first contribution with no way to submit.
What it cannot do: GitHub exposes no way to ask whether an issue form is valid. The GraphQL `issueTemplates` field and the community
profile list only the old Markdown templates, and are EMPTY even for repositories whose YAML forms work (checked on pypa/pip). So the
remote check validates the forms against the published schema (SchemaStore's github-issue-forms), which is the best available proxy, and
the only decisive test is to open each form in a browser while signed in.
"""
import argparse
import json
import re
import subprocess
import sys
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import parse_qs, urlparse

import yaml

ROOT = Path(__file__).resolve().parent.parent
SITE = ROOT / "site"
TEMPLATES = ROOT / ".github" / "ISSUE_TEMPLATE"
REPO = "SebE585/trognon"
BASE = "https://github.com/" + REPO
ELEMENTS = {"markdown", "input", "textarea", "dropdown", "checkboxes"}
FREE_PARAMS = {"template", "title", "labels", "assignees", "projects", "body"}
SKIP_PAGES = {"404.html"}


class Page(HTMLParser):
    """Collects the links of a page and the links inside its participation strip."""

    def __init__(self):
        super().__init__()
        self.links, self.strip, self._depth = [], [], 0

    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        if tag == "aside" and "page-actions" in (a.get("class") or "").split():
            self._depth = 1
        elif self._depth and tag == "aside":
            self._depth += 1
        if tag == "a" and a.get("href"):
            self.links.append(a["href"])
            if self._depth:
                self.strip.append(a["href"])

    def handle_endtag(self, tag):
        if tag == "aside" and self._depth:
            self._depth -= 1

    @property
    def has_strip(self):
        return bool(self.strip) or self._depth > 0


def parse_issue_url(url):
    """(template file, params) for an issue-form link, or None."""
    u = urlparse(url.replace("&amp;", "&"))
    if not u.path.endswith("/issues/new"):
        return None
    q = {k: v[0] for k, v in parse_qs(u.query, keep_blank_values=True).items()}
    return q.get("template"), q


def form_ids(form):
    return {e.get("id") for e in form.get("body", []) if e.get("type") != "markdown" and e.get("id")}


def check_form(name, form):
    """Problems GitHub would reject in an issue form."""
    errs = []
    if not isinstance(form, dict):
        return ["%s: not a YAML mapping" % name]
    for k in ("name", "description", "body"):
        if not form.get(k):
            errs.append("%s: missing %r" % (name, k))
    seen = set()
    for i, e in enumerate(form.get("body") or []):
        where = "%s body[%d]" % (name, i)
        if not isinstance(e, dict):
            errs.append("%s: not a mapping" % where)
            continue
        t = e.get("type")
        if t not in ELEMENTS:
            errs.append("%s: unknown element type %r" % (where, t))
            continue
        attrs = e.get("attributes") or {}
        if t == "markdown":
            if not attrs.get("value"):
                errs.append("%s: markdown needs attributes.value" % where)
            continue
        if not attrs.get("label"):
            errs.append("%s: %s needs attributes.label" % (where, t))
        if not e.get("id"):
            errs.append("%s: %s needs an id (pre-filling and review rely on it)" % (where, t))
        elif not re.fullmatch(r"[A-Za-z0-9_-]+", str(e["id"])):
            errs.append("%s: id %r has characters GitHub does not allow" % (where, e["id"]))
        elif e["id"] in seen:
            errs.append("%s: duplicate id %r" % (where, e["id"]))
        seen.add(e.get("id"))
        if t == "dropdown" and not attrs.get("options"):
            errs.append("%s: dropdown needs options" % where)
        if t == "checkboxes" and not all((o or {}).get("label") for o in attrs.get("options") or [{}]):
            errs.append("%s: every checkbox needs a label" % where)
        req = (e.get("validations") or {}).get("required")
        if req is not None and not isinstance(req, bool):
            errs.append("%s: validations.required must be true or false" % where)
    return errs


def generated_paths():
    """Repository paths of the generated pages: an edit there would be overwritten."""
    out = set()
    for f in (ROOT / "docs").rglob("*.md"):
        if "<!-- generated by" in f.read_text(errors="ignore")[:1500]:
            out.add(f.relative_to(ROOT).as_posix())
    return out


def tracked():
    return set(subprocess.run(["git", "ls-files"], capture_output=True, text=True, cwd=ROOT).stdout.splitlines())


def check_page(rel, parser, tracked_files, generated, forms):
    """Problems for one page: its strip and every call to action it contains."""
    errs = []
    if not parser.has_strip:
        errs.append("%s: no participation strip (an issue and a pull request)" % rel)
    else:
        kinds = {"correction": "template=correction.yml", "observation": "template=observation.yml", "edit": "/edit/main/", "missions": "/missions/"}
        for name, needle in kinds.items():
            if not any(needle in h for h in parser.strip):
                errs.append("%s: the participation strip has no %s link" % (rel, name))
    for href in parser.links:
        if not href.startswith(BASE):
            continue
        m = re.match(re.escape(BASE) + r"/edit/main/([^?#]+)", href)
        if m:
            path = m.group(1)
            if path not in tracked_files:
                errs.append("%s: edit link to %s, which is not in the repository" % (rel, path))
            if path in generated:
                errs.append("%s: edit link to the generated file %s (it must point to its source)" % (rel, path))
        issue = parse_issue_url(href)
        if issue:
            template, params = issue
            if template not in forms:
                errs.append("%s: issue link to a template that does not exist: %r" % (rel, template))
                continue
            unknown = set(params) - FREE_PARAMS - form_ids(forms[template])
            if unknown:
                errs.append("%s: issue link pre-fills %s, which %s does not have" % (rel, ", ".join(sorted(unknown)), template))
    return errs


def check_missions(site_dir, missions):
    """Every open mission can be taken, every first contribution says where to submit."""
    errs = []
    page = site_dir / "en" / "missions" / "index.html"
    html = page.read_text() if page.is_file() else ""
    for m in missions:
        if m.get("recurring"):
            if m["submit"]["url"] not in html:
                errs.append("missions page: no submission link for %s" % m["id"])
        elif m["state"] == "open" and ("mission=%s" % m["question"]) not in html:
            errs.append("missions page: no 'Take this mission' link for %s" % m["question"])
    return errs


SCHEMA_URL = "https://www.schemastore.org/github-issue-forms.json"


def schema_errors(forms, schema):
    """Errors of each form against the published issue-forms schema (needs the jsonschema package)."""
    import jsonschema
    v = jsonschema.validators.validator_for(schema)(schema)
    out = []
    for name, form in forms.items():
        for e in sorted(v.iter_errors(form), key=lambda e: [str(x) for x in e.path]):
            out.append("schema: %s %s: %s" % (name, list(e.path), e.message[:160]))
    return out


def remote_checks(edit_paths, forms, labels_used):
    """Ask GitHub: do the edit targets exist on main, do the labels exist; validate the forms against the published schema."""
    errs = []
    def gh(*args):
        r = subprocess.run(["gh", *args], capture_output=True, text=True)
        if r.returncode:
            raise SystemExit("gh %s failed: %s" % (" ".join(args[:2]), r.stderr.strip()))
        return r.stdout
    tree = json.loads(gh("api", "repos/%s/git/trees/main?recursive=1" % REPO))
    if tree.get("truncated"):
        errs.append("remote: the tree listing was truncated, so file existence cannot be checked")
    remote = {t["path"] for t in tree.get("tree", [])}
    for p in sorted(edit_paths - remote):
        errs.append("remote: edit link to %s, which is not on main (push first?)" % p)
    try:
        import jsonschema  # noqa: F401
        import urllib.request
        schema = json.loads(urllib.request.urlopen(SCHEMA_URL, timeout=20).read())
        errs += schema_errors(forms, schema)
        validated = len(forms)
    except ImportError:
        print("note: the jsonschema package is missing, so the forms were not validated against the published schema", file=sys.stderr)
        validated = 0
    existing = {l["name"] for l in json.loads(gh("label", "list", "--limit", "200", "--json", "name"))}
    for lab in sorted(labels_used - existing):
        errs.append("remote: a form applies the label %r, which does not exist in the repository" % lab)
    return errs, len(remote), validated


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--remote", action="store_true", help="also ask GitHub (needs the gh CLI)")
    args = ap.parse_args(argv)
    if not SITE.is_dir():
        print("site/ is missing: run mkdocs build first", file=sys.stderr)
        return 2
    forms, errors = {}, []
    for f in sorted(TEMPLATES.glob("*.yml")):
        if f.name == "config.yml":
            continue
        forms[f.name] = yaml.safe_load(f.read_text())
        errors += check_form(f.name, forms[f.name])
    for need in (".github/PULL_REQUEST_TEMPLATE.md", "CONTRIBUTING.md"):
        if not (ROOT / need).is_file():
            errors.append("missing %s" % need)
    tr, gen = tracked(), generated_paths()
    pages = [p for p in sorted(SITE.rglob("index.html")) if p.name not in SKIP_PAGES]
    edit_paths = set()
    for p in pages:
        parser = Page()
        parser.feed(p.read_text())
        errors += check_page(p.relative_to(SITE).as_posix(), parser, tr, gen, forms)
        for h in parser.links:
            m = re.match(re.escape(BASE) + r"/edit/main/([^?#]+)", h)
            if m:
                edit_paths.add(m.group(1))
    missions = yaml.safe_load((ROOT / "tools" / "iop_analysis" / "missions.yaml").read_text())["missions"]
    errors += check_missions(SITE, missions)
    note = ""
    if args.remote:
        labels = {l for f in forms.values() for l in (f.get("labels") or [])}
        rerrs, n, validated = remote_checks(edit_paths, forms, labels)
        errors += rerrs
        note = "; main has %d files, all edit targets exist there, %d forms validated against the published schema (open each in a browser for the decisive test)" % (n, validated)
    for e in errors:
        print(e)
    if errors:
        return 1
    print("cta: %d pages each offer an issue and a pull request; %d edit targets, %d forms, %d missions checked%s" % (len(pages), len(edit_paths), len(forms), len(missions), note))
    return 0


if __name__ == "__main__":
    sys.exit(main())
