#!/usr/bin/env python3
"""Keep the GitHub project board a mirror of tools/iop_analysis/missions.yaml.

    python3 -I tools/project_sync.py            # compare and report the differences, write nothing (exit 1 if any)
    python3 -I tools/project_sync.py --apply    # create, update and re-stamp; never deletes a card

The file is the source of truth. Each card is keyed by its `Question id` field, so a mission is never created twice and a card
renamed on the board is found again. A card that matches no mission is reported as an orphan, not deleted. The board's README
records the date and the commit of the last synchronisation. It needs the `gh` CLI with the `project` scope.
"""
import argparse
import datetime
import json
import subprocess
import sys
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "tools" / "iop_analysis" / "missions.yaml"
STATE_LABEL = {"open": "Open", "in-progress": "In progress", "blocked": "Blocked", "closed": "Closed"}
LEVEL = {"static": "1", "survey": "2", "write": "3"}
PAGE = "https://sebe585.github.io/trognon/en/missions/"


def key(m):
    return m.get("question") or m.get("id")


def body(m):
    return ("Question `%s`. What you need and what a good answer looks like: %s\n\n"
            "This card mirrors the generated Missions page (tools/iop_analysis/missions.yaml); the file is the source of truth."
            % (key(m), PAGE))


def wanted(m):
    return {"title": m["title"], "body": body(m), "mission state": STATE_LABEL[m["state"]], "level": LEVEL[m["work"]]}


def current(item):
    return {"title": item.get("title"), "body": (item.get("content") or {}).get("body", ""),
            "mission state": item.get("mission state"), "level": str(item.get("level", ""))[:1]}


def plan(missions, items):
    """The actions that bring the board in line with the file: [(kind, question id, detail)]."""
    actions, by_q = [], {}
    for it in items:
        q = it.get("question id")
        if not q:
            actions.append(("orphan", None, it.get("title")))
            continue
        if q in by_q:
            actions.append(("duplicate", q, it.get("title")))
            continue
        by_q[q] = it
    known = set()
    for m in missions:
        q = key(m)
        known.add(q)
        if q not in by_q:
            actions.append(("create", q, m["title"]))
            continue
        want, have = wanted(m), current(by_q[q])
        changed = sorted(k for k in want if want[k] != have[k])
        if changed:
            actions.append(("update", q, changed))
    for q, it in by_q.items():
        if q not in known:
            actions.append(("orphan", q, it.get("title")))
    return actions


def gh(*args):
    r = subprocess.run(["gh", *args], capture_output=True, text=True)
    if r.returncode:
        raise SystemExit("gh %s failed: %s" % (" ".join(args[:3]), r.stderr.strip()))
    return r.stdout


def git(*args):
    return subprocess.run(["git", *args], capture_output=True, text=True, cwd=ROOT).stdout.strip()


def stamp():
    commit = git("log", "-1", "--format=%h", "--", str(SRC.relative_to(ROOT)))
    dirty = bool(git("status", "--porcelain", "--", str(SRC.relative_to(ROOT))))
    now = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%d %H:%M UTC")
    return ("Mirror of `tools/iop_analysis/missions.yaml`, which is the source of truth: a card edited here is overwritten at the next "
            "synchronisation. Missions, with what a good answer looks like: %s\n\n"
            "Last synchronised: %s, from missions.yaml as of commit `%s`%s."
            % (PAGE, now, commit or "unknown", " (with uncommitted changes)" if dirty else ""))


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--apply", action="store_true", help="write the changes (the default only reports them)")
    args = ap.parse_args(argv)
    cfg = yaml.safe_load(SRC.read_text())
    owner, number = cfg["project"]["owner"], str(cfg["project"]["number"])
    missions = cfg["missions"]
    items = json.loads(gh("project", "item-list", number, "--owner", owner, "--limit", "200", "--format", "json"))["items"]
    actions = plan(missions, items)
    for kind, q, detail in actions:
        print("%-9s %s %s" % (kind, q or "-", detail))
    if not args.apply:
        print("project: %s" % ("in step with missions.yaml" if not actions else "%d difference(s); run with --apply" % len(actions)))
        return 1 if actions else 0
    fields = {f["name"]: f for f in json.loads(gh("project", "field-list", number, "--owner", owner, "--format", "json"))["fields"]}
    project_id = json.loads(gh("project", "view", number, "--owner", owner, "--format", "json"))["id"]
    state_opt = {o["name"]: o["id"] for o in fields["Mission state"]["options"]}
    level_opt = {o["name"][0]: o["id"] for o in fields["Level"]["options"]}
    by_q = {it["question id"]: it for it in items if it.get("question id")}
    by_m = {key(m): m for m in missions}

    def set_fields(item_id, m):
        gh("project", "item-edit", "--id", item_id, "--project-id", project_id, "--field-id", fields["Mission state"]["id"],
           "--single-select-option-id", state_opt[STATE_LABEL[m["state"]]])
        gh("project", "item-edit", "--id", item_id, "--project-id", project_id, "--field-id", fields["Level"]["id"],
           "--single-select-option-id", level_opt[LEVEL[m["work"]]])
        gh("project", "item-edit", "--id", item_id, "--project-id", project_id, "--field-id", fields["Question id"]["id"], "--text", key(m))

    for kind, q, detail in actions:
        if kind == "create":
            m = by_m[q]
            it = json.loads(gh("project", "item-create", number, "--owner", owner, "--title", m["title"], "--body", body(m), "--format", "json"))
            set_fields(it["id"], m)
        elif kind == "update":
            m, it = by_m[q], by_q[q]
            if "title" in detail or "body" in detail:
                gh("project", "item-edit", "--id", it["content"]["id"], "--title", m["title"], "--body", body(m))
            set_fields(it["id"], m)
    gh("project", "edit", number, "--owner", owner, "--readme", stamp())
    print("project: synchronised (%d change(s)); the board's README records the date and the commit" % len(actions))
    return 0


if __name__ == "__main__":
    sys.exit(main())
