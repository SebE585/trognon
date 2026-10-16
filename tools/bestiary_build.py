#!/usr/bin/env python3
"""Build and check the Bestiary pages from tools/bestiary.yaml.

    python3 -I tools/bestiary_build.py            # write docs/en/bestiary.md and docs/fr/bestiaire.md
    python3 -I tools/bestiary_build.py --check    # validate, and fail if a generated page is stale

A creature is a block of the phone that is not fully understood; its fronts are the open points of the hardware map. The life bar is
computed from the fronts and a front only moves with a proof attached: `wounded` needs a page of docs/ that holds the reading in the code,
`down` needs an evidence bundle. The checks refuse a front without its proof, a mission or bundle or section that does not exist,
and a creature without a French name.
"""
import argparse
import sys
from pathlib import Path

import yaml

sys.path.insert(0, str(Path(__file__).resolve().parent))
import journey_build as jb  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
DOCS = ROOT / "docs"
SRC = ROOT / "tools" / "bestiary.yaml"
MISSIONS = ROOT / "tools" / "iop_analysis" / "missions.yaml"
REPO = "https://github.com/SebE585/trognon"
STATES = ("open", "wounded", "down")
SEGMENTS = 10
OUT = {"en": DOCS / "en" / "bestiary.md", "fr": DOCS / "fr" / "bestiaire.md"}

TXT = {
    "en": {
        "title": "Bestiary", "lead": "Blocks of the phone we have not finished understanding, shown as creatures to face.",
        "intro": ("Each creature is a block of the phone. Its **fronts** are the precise points the [hardware map](03-hardware-map.md) still lists as open. "
                  "The life bar is computed from them and nothing else: a front only goes down when a proof is attached to it. "
                  "Nobody, the maintainer included, can lower a bar by writing a sentence."),
        "rules_h": "How a front goes down", "rules": [
            "**Open**: nothing established. The bar is full for this front.",
            "**Wounded**: read in the firmware or in code (SOURCE), not yet seen working on the phone. Half a point of life is gone.",
            "**Down**: observed on the real phone (DEVICE), with an evidence folder you can open and rebuild. The point is gone.",
            "A creature is **defeated** when every front is down. It is called *unspotted* while it is not even in the hardware map."],
        "life": "Life", "of": "of", "points": "points left", "state": {"open": "Open", "wounded": "Wounded: read in code", "down": "Down: seen on the phone"},
        "status": {"unspotted": "Unspotted", "intact": "Intact", "wounded": "Wounded", "defeated": "Defeated"},
        "map": "Its place in the hardware map", "attack": "Attack it", "none": "No mission yet for this front.",
        "propose": "Propose one", "proof": "Proof", "src": "Reading", "missions_page": "missions.md", "map_page": "03-hardware-map.md",
        "take_h": "Take one on", "take": ("Pick a front, then take its mission on the [Missions page](missions.md), or propose a mission for a front that has none. "
                                        "A front goes down when a pull request brings the proof, and the page is rebuilt from the data."),
        "unit": {"static": "no phone", "survey": "needs a phone, reads only", "write": "maintainer only"},
    },
    "fr": {
        "title": "Bestiaire", "lead": "Les blocs du téléphone que nous n'avons pas fini de comprendre, présentés comme des créatures à affronter.",
        "intro": ("Chaque créature est un bloc du téléphone. Ses **fronts** sont les points précis que la [carte matérielle](03-hardware-map.md) donne encore pour ouverts. "
                  "La barre de vie se calcule à partir d'eux et de rien d'autre : un front ne tombe que si une preuve lui est attachée. "
                  "Personne, mainteneur compris, ne peut baisser une barre en écrivant une phrase."),
        "rules_h": "Comment un front tombe", "rules": [
            "**Ouvert** : rien d'établi. La barre est pleine pour ce front.",
            "**Blessé** : lu dans le firmware ou dans du code (SOURCE), pas encore vu fonctionner sur le téléphone. Un demi-point de vie est parti.",
            "**À terre** : observé sur le vrai téléphone (DEVICE), avec un dossier de preuve que vous pouvez ouvrir et reconstruire. Le point est parti.",
            "Une créature est **vaincue** quand tous ses fronts sont à terre. Elle est dite *non repérée* tant qu'elle n'est même pas dans la carte matérielle."],
        "life": "Vie", "of": "sur", "points": "points restants", "state": {"open": "Ouvert", "wounded": "Blessé : lu dans le code", "down": "À terre : vu sur le téléphone"},
        "status": {"unspotted": "Non repérée", "intact": "Intacte", "wounded": "Blessée", "defeated": "Vaincue"},
        "map": "Sa place dans la carte matérielle", "attack": "L'attaquer", "none": "Pas encore de mission pour ce front.",
        "propose": "En proposer une", "proof": "Preuve", "src": "Lecture", "missions_page": "../en/missions.md", "map_page": "03-hardware-map.md",
        "take_h": "En affronter une", "take": ("Choisissez un front, puis prenez sa mission sur la [page Missions](../en/missions.md) (en anglais), ou proposez-en une pour un front qui n'en a pas. "
                                              "Un front tombe quand une pull request apporte la preuve, et la page est reconstruite depuis les données."),
        "unit": {"static": "sans téléphone", "survey": "avec un téléphone, lecture seule", "write": "mainteneur seulement"},
    },
}


def load(path=SRC):
    return yaml.safe_load(path.read_text())


def mission_titles():
    out = {}
    for m in yaml.safe_load(MISSIONS.read_text())["missions"]:
        out[m.get("question") or m.get("id")] = (m["title"], m.get("work", ""))
    return out


def life(creature):
    """(points left, total points): an open front is 2, a wounded one 1, a front that is down 0."""
    left = sum({"open": 2, "wounded": 1, "down": 0}[f["state"]] for f in creature["fronts"])
    return left, 2 * len(creature["fronts"])


def status(creature):
    states = {f["state"] for f in creature["fronts"]}
    if states == {"down"}:
        return "defeated"
    if states == {"open"}:
        return "unspotted" if not creature.get("section") else "intact"
    return "wounded"


def bar(left, total):
    filled = round(SEGMENTS * left / total) if total else 0
    if left and not filled:
        filled = 1
    return "▰" * filled + "▱" * (SEGMENTS - filled)


def check(data, titles=None, bundles=None):
    errors = []
    titles = mission_titles() if titles is None else titles
    bundles = {p.name for p in (ROOT / "evidence").iterdir() if p.is_dir()} if bundles is None else bundles
    anchors = jb.headings(DOCS / "en" / "03-hardware-map.md")
    seen = set()
    for c in data.get("creatures", []):
        i = c.get("id", "?")
        if i in seen:
            errors.append("%s: duplicate creature id" % i)
        seen.add(i)
        for lang in ("en", "fr"):
            if not (c.get("name") or {}).get(lang) or not (c.get("block") or {}).get(lang):
                errors.append("%s: name and block need both languages (%s is missing)" % (i, lang))
        if c.get("section") and c["section"] not in anchors:
            errors.append("%s: section %r is not an anchor of the hardware map" % (i, c["section"]))
        if not c.get("fronts"):
            errors.append("%s: a creature needs at least one front" % i)
        for k, f in enumerate(c.get("fronts", [])):
            where = "%s front %d" % (i, k + 1)
            if f.get("state") not in STATES:
                errors.append("%s: state must be one of %s" % (where, ", ".join(STATES)))
            if not (f.get("text") or {}).get("en") or not (f.get("text") or {}).get("fr"):
                errors.append("%s: text needs both languages" % where)
            if f.get("state") == "down":
                if not f.get("evidence"):
                    errors.append("%s: a front that is down needs the evidence bundle(s) that show it" % where)
                for b in f.get("evidence", []):
                    if b not in bundles:
                        errors.append("%s: evidence bundle %r does not exist" % (where, b))
            elif f.get("evidence"):
                errors.append("%s: only a front that is down carries evidence" % where)
            if f.get("state") == "wounded":
                if not f.get("ref"):
                    errors.append("%s: a wounded front names the page of docs/ that holds the reading (ref)" % where)
                elif not (DOCS / f["ref"]).is_file():
                    errors.append("%s: ref %r does not exist under docs/" % (where, f["ref"]))
            elif f.get("ref"):
                errors.append("%s: only a wounded front carries a ref" % where)
            for m in f.get("missions", []):
                if m not in titles:
                    errors.append("%s: mission %r is not in missions.yaml" % (where, m))
    return errors


def render(data, lang, titles=None, fr_anchors=None):
    t = TXT[lang]
    col = " : " if lang == "fr" else ": "
    titles = mission_titles() if titles is None else titles
    page_map = "03-hardware-map.md"
    fr_anchors = jb.headings(DOCS / "fr" / "03-hardware-map.md") if fr_anchors is None and lang == "fr" else (fr_anchors or set())
    lines = ["---", "title: %s" % t["title"], "description: \"%s\"" % t["lead"].replace('"', "'"), "source: tools/bestiary.yaml", "---", "",
             "<!-- Generated by tools/bestiary_build.py from tools/bestiary.yaml. Do not edit by hand. -->", "",
             "# %s" % t["title"], "", t["lead"], "", t["intro"], "", "## %s" % t["rules_h"], ""]
    lines += ["- %s" % r for r in t["rules"]] + [""]
    for c in data["creatures"]:
        left, total = life(c)
        st = status(c)
        lines += ["## %s" % c["name"][lang], "", "*%s* · **%s**" % (c["block"][lang], t["status"][st]), "",
                  '<p class="bestiary-bar" role="img" aria-label="%s%s%d %s %d %s">%s</p>' % (t["life"], col, left, t["of"], total, t["points"], bar(left, total)),
                  "", "%s%s%d %s %d" % (t["life"], col, left, t["of"], total), ""]
        for f in c["fronts"]:
            line = "- **%s.** %s" % (t["state"][f["state"]], f["text"][lang])
            if f["state"] == "down":
                line += " " + t["proof"] + col + ", ".join("[`%s`](%s/tree/main/evidence/%s)" % (b, REPO, b) for b in f["evidence"]) + "."
            if f["state"] == "wounded":
                line += " %s%s[`%s`](../%s)." % (t["src"], col, f["ref"], f["ref"])
            lines.append(line)
            if f["state"] != "down":
                ms = f.get("missions", [])
                if ms:
                    lines.append("    - %s%s%s" % (t["attack"], col, " · ".join("[%s](%s) (%s)" % (titles[m][0], t["missions_page"], t["unit"].get(titles[m][1], titles[m][1])) for m in ms)))
                else:
                    lines.append("    - %s [%s](%s/issues/new?template=mission.yml)." % (t["none"], t["propose"], REPO))
        if c.get("section"):
            lang_anchor = c["section"] if lang == "en" or c["section"] in fr_anchors else None
            target = "%s#%s" % (page_map, lang_anchor) if lang_anchor else page_map
            lines += ["", "%s%s[%s](%s)" % (t["map"], col, c["block"][lang], target)]
        lines.append("")
    lines += ["## %s" % t["take_h"], "", t["take"], ""]
    return "\n".join(lines)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--check", action="store_true", help="validate and compare with the files on disk, write nothing")
    args = ap.parse_args(argv)
    data = load()
    errors = check(data)
    if errors:
        for e in errors:
            print(e)
        return 1
    pages = {lang: render(data, lang) for lang in OUT}
    status_code = 0
    for lang, text in pages.items():
        if args.check:
            if not OUT[lang].is_file() or OUT[lang].read_text() != text:
                print("%s is missing or stale (run make bestiary)" % OUT[lang].relative_to(ROOT))
                status_code = 1
        else:
            OUT[lang].write_text(text)
            print("%d creatures written to %s" % (len(data["creatures"]), OUT[lang].relative_to(ROOT)))
    if args.check and status_code == 0:
        print("bestiary: %d creatures valid, generated pages up to date" % len(data["creatures"]))
    return status_code


if __name__ == "__main__":
    sys.exit(main())
