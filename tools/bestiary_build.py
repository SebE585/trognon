#!/usr/bin/env python3
"""Build and check the Bestiary pages from tools/bestiary.yaml.

    python3 -I tools/bestiary_build.py            # write docs/en/bestiary.md and docs/fr/bestiaire.md
    python3 -I tools/bestiary_build.py --check    # validate, and fail if a generated page is stale

A creature is a block of the phone that is not fully understood; its fronts are the open points of the hardware map. The life bar is
computed from the fronts and a front only moves with a proof attached: `wounded` needs a page of docs/ that holds the reading in the code,
`down` needs an evidence bundle. The checks refuse a front without its proof, a mission or bundle or section that does not exist,
and a creature without a French name.

A creature marked `campaign: true` also gets a campaign page (docs/en/campaigns/<id>.md, docs/fr/campagnes/<id>.md): its fronts in the order
their `needs` allow, what is available now and what waits, and which kind of run would bring each one down. Availability is computed:
a front is locked while a front it needs is not down: what waits is the run on the phone, a locked front can still be studied in the code.
"""
import argparse
import re
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
DOWN_NEEDS = ("survey", "write", "unknown")
OUT = {"en": DOCS / "en" / "bestiary.md", "fr": DOCS / "fr" / "bestiaire.md"}
CAMPAIGN_DIR = {"en": DOCS / "en" / "campaigns", "fr": DOCS / "fr" / "campagnes"}

TXT = {
    "en": {
        "title": "Bestiary", "lead": "Blocks of the phone we have not finished understanding, shown as creatures to face.",
        "intro": ("Each creature is a block of the phone. Its **fronts** are the precise points the [hardware map](03-hardware-map.md) still lists as open. "
                  "The life bar is computed from the states written in `tools/bestiary.yaml`: a front that is down must cite an evidence folder, and the generator "
                  "checks that the cited line is really in it. What a measurement demonstrates is still decided by a human review, in a pull request. In the spirit of a barcode card, a creature is also a **code to find**: "
                  "each front is one slot of it, shown as `?` until a proof reveals it."),
        "rules_h": "How a front goes down", "rules": [
            "**Open**: nothing established. The bar is full for this front.",
            "**Wounded**: read in the firmware or in code (SOURCE), not yet seen working on the phone. Half a point of life is gone.",
            "**Down**: observed on the real phone (DEVICE), with an evidence folder you can open and rebuild. The point is gone.",
            "A creature is **defeated** when every front is down. That means its **goal** is reached, not that the block is understood: new questions may appear after a victory.",
            "**Ruled out** is not a state: it is a hypothesis shown false, listed under its front with the page or the evidence that shows it. It does not move the bar, but it counts as much as a confirmation, because it shortens the search for whoever comes next.",
            "The bar counts **points investigated**, not how much of the block works. Several small questions answered can leave the one that blocks everything untouched.",
            "It is called *unspotted* while it is not even in the hardware map."],
        "story": "The story behind it", "goal": "Goal", "goal_note": "bounded: reaching it does not mean the block is understood",
        "camp": "Campaign", "camp_title": "Campaign: %s", "camp_lead": "The order in which to attack %s, and what each step needs.",
        "camp_intro": ("A campaign is a creature plus a route. The fronts are not independent: some cannot be attacked before another is understood. "
                       "Availability below is computed from the states in the [Bestiary](../bestiary.md), not typed in. A front is **locked** until every front it needs is down "
                       "(seen on the phone). Locked does not mean closed: what waits is the run on the phone, and a locked front can still be studied in the code. "
                       "The order is our working plan, not a proven requirement: it says what we would try first."),
        "camp_next": "Next move", "camp_none": "Every front is down: this creature is defeated.",
        "camp_route": "The route", "avail": "Available", "locked": "Locked until this is down:", "done": "Done", "step": "Step",
        "needs_h": "To bring it down", "dn": {"survey": "a run on a phone that only reads", "write": "hardware writes: the maintainer only, one at a time, after an exact description and a go",
                                               "unknown": "not known yet which run would settle it"},
        "bestiary_page": "../bestiary.md", "mission_page": "../missions.md", "ref_up": "../../",
        "code": "Code", "found": "found", "slots": "slots", "fragment": "Fragment", "in_bundle": "read in",
        "code_note": "A creature is a code to find. Each front is one slot of it, shown as ? until a proof reveals it.",
        "life": "Life", "of": "of", "points": "points left", "state": {"open": "Open", "wounded": "Wounded: read in code", "down": "Down: seen on the phone"},
        "status": {"unspotted": "Unspotted", "intact": "Intact", "wounded": "Wounded", "defeated": "Defeated"},
        "map": "Its place in the hardware map", "attack": "Attack it", "none": "No mission yet for this front.",
        "propose": "Propose one", "ruled": "Ruled out", "proof": "Proof", "src": "Reading", "missions_page": "missions.md", "map_page": "03-hardware-map.md",
        "take_h": "Take one on", "take": ("Pick a front, then take its mission on the [Missions page](missions.md), or propose a mission for a front that has none. "
                                        "No phone? The missions marked *no phone* are readings of public material, and `python3 tools/rebuild_bundle.py evidence/<bundle>` checks an evidence bundle without any device. "
                                        "A front goes down when a pull request brings the proof, and the page is rebuilt from the data. A refutation, with its source, is as welcome as a confirmation."),
        "unit": {"static": "no phone", "survey": "needs a phone, reads only", "write": "maintainer only"},
    },
    "fr": {
        "title": "Bestiaire", "lead": "Les blocs du téléphone que nous n'avons pas fini de comprendre, présentés comme des créatures à affronter.",
        "intro": ("Chaque créature est un bloc du téléphone. Ses **fronts** sont les points précis que la [carte matérielle](03-hardware-map.md) donne encore pour ouverts. "
                  "La barre de vie se calcule à partir des états écrits dans `tools/bestiary.yaml` : un front à terre doit citer un dossier de preuve, et le générateur "
                  "vérifie que la ligne citée s'y trouve vraiment. Ce qu'une mesure démontre reste décidé par une relecture humaine, dans une pull request. À la manière d'une carte à code-barres, une créature est aussi un **code à trouver** : "
                  "chaque front en est un emplacement, affiché `?` tant qu'une preuve ne le révèle pas."),
        "rules_h": "Comment un front tombe", "rules": [
            "**Ouvert** : rien d'établi. La barre est pleine pour ce front.",
            "**Blessé** : lu dans le firmware ou dans du code (SOURCE), pas encore vu fonctionner sur le téléphone. Un demi-point de vie est parti.",
            "**À terre** : observé sur le vrai téléphone (DEVICE), avec un dossier de preuve que vous pouvez ouvrir et reconstruire. Le point est parti.",
            "Une créature est **vaincue** quand tous ses fronts sont à terre. Cela veut dire que son **objectif** est atteint, pas que le bloc est compris : de nouvelles questions peuvent apparaître après une victoire.",
            "**Écarté** n'est pas un état : c'est une hypothèse montrée fausse, listée sous son front avec la page ou la preuve qui le montre. Elle ne fait pas bouger la barre, mais elle compte autant qu'une confirmation, parce qu'elle raccourcit la recherche de celui qui vient après.",
            "La barre compte des **points étudiés**, pas la part du bloc qui fonctionne. Plusieurs petites questions résolues peuvent laisser intacte celle qui bloque tout.",
            "Elle est dite *non repérée* tant qu'elle n'est même pas dans la carte matérielle."],
        "story": "L'histoire qui y mène", "goal": "Objectif", "goal_note": "borné : l'atteindre ne veut pas dire que le bloc est compris",
        "camp": "Campagne", "camp_title": "Campagne : %s", "camp_lead": "L'ordre dans lequel attaquer %s, et ce dont chaque étape a besoin.",
        "camp_intro": ("Une campagne est une créature plus un itinéraire. Les fronts ne sont pas indépendants : certains ne peuvent pas être attaqués avant qu'un autre soit compris. "
                       "La disponibilité ci-dessous est calculée depuis les états du [Bestiaire](../bestiaire.md), pas saisie. Un front est **verrouillé** tant que chaque front dont il dépend "
                       "n'est pas à terre (vu sur le téléphone). Verrouillé ne veut pas dire fermé : ce qui attend, c'est l'exécution sur le téléphone, et un front verrouillé peut "
                       "encore être étudié dans le code. L'ordre est notre plan de travail, pas une exigence démontrée : il dit ce que nous essaierions en premier."),
        "camp_next": "Prochain pas", "camp_none": "Tous les fronts sont à terre : cette créature est vaincue.",
        "camp_route": "L'itinéraire", "avail": "Disponible", "locked": "Verrouillé jusqu'à ce que ceci soit à terre :", "done": "Fait", "step": "Étape",
        "needs_h": "Pour le faire tomber", "dn": {"survey": "une exécution sur un téléphone qui ne fait que lire", "write": "des écritures matérielles : le mainteneur seulement, une par une, après une description exacte et un go",
                                                   "unknown": "on ne sait pas encore quelle exécution trancherait"},
        "bestiary_page": "../bestiaire.md", "mission_page": "../../en/missions.md", "ref_up": "../../",
        "code": "Code", "found": "trouvés", "slots": "emplacements", "fragment": "Chiffre", "in_bundle": "lu dans",
        "code_note": "Une créature est un code à trouver. Chaque front en est un emplacement, affiché ? tant qu'une preuve ne le révèle pas.",
        "life": "Vie", "of": "sur", "points": "points restants", "state": {"open": "Ouvert", "wounded": "Blessé : lu dans le code", "down": "À terre : vu sur le téléphone"},
        "status": {"unspotted": "Non repérée", "intact": "Intacte", "wounded": "Blessée", "defeated": "Vaincue"},
        "map": "Sa place dans la carte matérielle", "attack": "L'attaquer", "none": "Pas encore de mission pour ce front.",
        "propose": "En proposer une", "ruled": "Écarté", "proof": "Preuve", "src": "Lecture", "missions_page": "../en/missions.md", "map_page": "03-hardware-map.md",
        "take_h": "En affronter une", "take": ("Choisissez un front, puis prenez sa mission sur la [page Missions](../en/missions.md) (en anglais), ou proposez-en une pour un front qui n'en a pas. "
                                              "Pas de téléphone ? Les missions marquées *sans téléphone* sont des lectures de sources publiques, et `python3 tools/rebuild_bundle.py evidence/<dossier>` vérifie un dossier de preuve sans aucun appareil. "
                                              "Un front tombe quand une pull request apporte la preuve, et la page est reconstruite depuis les données. Une réfutation, avec sa source, est aussi bienvenue qu'une confirmation."),
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


def _norm(text):
    """For comparing a displayed value with its proof line: no 0x prefix, spaces or case (0x212 is in 'raw 00000212')."""
    return re.sub(r"0x|\s", "", str(text).lower())


def _bundle_text(bundle):
    parts = []
    for name in ("screen.txt", "vars.txt"):
        f = ROOT / "evidence" / bundle / name
        if f.is_file():
            parts.append(" ".join(f.read_text().split()))
    return " ".join(parts)


def code_strip(creature):
    """One slot per front: the revealed code fragment, a check mark for a front that is down without one, a ? otherwise."""
    return [f["fragment"]["value"] if f.get("fragment") else ("✓" if f["state"] == "down" else "?") for f in creature["fronts"]]


def _campaign_errors(c):
    """The ids and the `needs` of a creature's fronts: unique, existing, without a cycle; a campaign needs them all."""
    errors, i = [], c.get("id", "?")
    ids = [f["id"] for f in c.get("fronts", []) if f.get("id")]
    if len(ids) != len(set(ids)):
        errors.append("%s: front ids must be unique" % i)
    if c.get("campaign"):
        for k, f in enumerate(c.get("fronts", [])):
            if not f.get("id"):
                errors.append("%s front %d: a campaign needs an id on every front" % (i, k + 1))
            if f.get("state") != "down" and f.get("down_needs") is None:
                errors.append("%s front %d: a campaign says what would bring an unfinished front down (down_needs)" % (i, k + 1))
        for k in ("en", "fr"):
            if not (c.get("campaign_intro") or {}).get(k):
                errors.append("%s: a campaign needs an intro in both languages (campaign_intro)" % i)
    for f in c.get("fronts", []):
        for n in f.get("needs", []):
            if n not in ids:
                errors.append("%s: front %r needs %r, which is not a front id of this creature" % (i, f.get("id"), n))
            if n == f.get("id"):
                errors.append("%s: front %r needs itself" % (i, n))
    if not errors and route(c) is None:
        errors.append("%s: the fronts' needs form a cycle" % i)
    return errors


def route(c):
    """The fronts in an order their `needs` allow, keeping the order of the file otherwise; None on a cycle."""
    fronts = c["fronts"]
    done, order = set(), []
    while len(order) < len(fronts):
        progressed = False
        for f in fronts:
            if f in order:
                continue
            if all(n in done for n in f.get("needs", [])):
                order.append(f)
                if f.get("id"):
                    done.add(f["id"])
                progressed = True
                break
        if not progressed:
            return None
    return order


def availability(c):
    """{front index in the file: ('done'|'available'|'locked', [texts of the open fronts it waits for])}."""
    by_id = {f["id"]: f for f in c["fronts"] if f.get("id")}
    out = {}
    for k, f in enumerate(c["fronts"]):
        waits = [by_id[n] for n in f.get("needs", []) if by_id[n]["state"] != "down"]
        out[k] = ("done", []) if f["state"] == "down" else (("locked", waits) if waits else ("available", []))
    return out


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
            if not (c.get("goal") or {}).get(lang):
                errors.append("%s: a creature states its goal in both languages (%s is missing)" % (i, lang))
            if not (c.get("name") or {}).get(lang) or not (c.get("block") or {}).get(lang):
                errors.append("%s: name and block need both languages (%s is missing)" % (i, lang))
        if c.get("story") and not (DOCS / "en" / "journey" / ("%s.md" % c["story"])).is_file():
            errors.append("%s: story %r is not a step of the guided path (docs/en/journey/%s.md)" % (i, c["story"], c["story"]))
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
            d = f.get("fragment")
            if d:
                if f.get("state") != "down":
                    errors.append("%s: only a front that is down reveals a code fragment" % where)
                elif not (d.get("value") and d.get("proof") and (d.get("label") or {}).get("en") and (d.get("label") or {}).get("fr")):
                    errors.append("%s: a code fragment needs value, proof and a label in both languages" % where)
                elif _norm(d["value"]) not in _norm(d["proof"]):
                    errors.append("%s: the value %r does not appear in its proof line %r" % (where, d["value"], d["proof"]))
                elif not any(d["proof"].lower() in _bundle_text(b).lower() for b in f.get("evidence", [])):
                    errors.append("%s: the proof line %r is in none of the front's bundles (screen.txt or vars.txt)" % (where, d["proof"]))
            if f.get("state") == "wounded":
                if not f.get("ref"):
                    errors.append("%s: a wounded front names the page of docs/ that holds the reading (ref)" % where)
                elif not (DOCS / f["ref"]).is_file():
                    errors.append("%s: ref %r does not exist under docs/" % (where, f["ref"]))
            elif f.get("ref"):
                errors.append("%s: only a wounded front carries a ref" % where)
            for k2, r in enumerate(f.get("ruled_out", [])):
                rw = "%s ruled_out %d" % (where, k2 + 1)
                if not (r.get("text") or {}).get("en") or not (r.get("text") or {}).get("fr"):
                    errors.append("%s: text needs both languages (the hypothesis that was shown false)" % rw)
                if not r.get("ref") and not r.get("evidence"):
                    errors.append("%s: a refutation needs its source: a page of docs/ (ref) or evidence bundle(s)" % rw)
                if r.get("ref") and not (DOCS / r["ref"]).is_file():
                    errors.append("%s: ref %r does not exist under docs/" % (rw, r["ref"]))
                for b in r.get("evidence", []):
                    if b not in bundles:
                        errors.append("%s: evidence bundle %r does not exist" % (rw, b))
            for m in f.get("missions", []):
                if m not in titles:
                    errors.append("%s: mission %r is not in missions.yaml" % (where, m))
            if f.get("down_needs") and f["down_needs"] not in DOWN_NEEDS:
                errors.append("%s: down_needs must be one of %s" % (where, ", ".join(DOWN_NEEDS)))
        errors += _campaign_errors(c)
    return errors


def ruled_out_lines(f, lang, t, col, ref_up, indent):
    """One line per hypothesis shown false under a front: the hypothesis, then where it is shown."""
    out = []
    for r in f.get("ruled_out", []):
        src = []
        if r.get("ref"):
            src.append("%s%s[`%s`](%s%s)" % (t["src"], col, r["ref"], ref_up, r["ref"]))
        if r.get("evidence"):
            src.append("%s%s%s" % (t["proof"], col, ", ".join("[`%s`](%s/tree/main/evidence/%s)" % (b, REPO, b) for b in r["evidence"])))
        out.append("%s- **%s.** %s. %s." % (indent, t["ruled"], r["text"][lang].rstrip("."), " ; ".join(src)))
    return out


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
                  "**%s** (%s)%s%s" % (t["goal"], t["goal_note"], col, c["goal"][lang]), "",
                  '<p class="bestiary-bar" role="img" aria-label="%s%s%d %s %d %s">%s</p>' % (t["life"], col, left, t["of"], total, t["points"], bar(left, total)),
                  "", "%s%s%d %s %d" % (t["life"], col, left, t["of"], total), "",
                  '<p class="bestiary-code" role="img" aria-label="%s%s%d %s %d %s">%s</p>' % (
                      t["code"], col, sum(1 for f in c["fronts"] if f["state"] == "down"), t["of"], len(c["fronts"]), t["slots"] + " " + t["found"],
                      " &nbsp;·&nbsp; ".join("<code>%s</code>" % v for v in code_strip(c))), ""]
        for f in c["fronts"]:
            line = "- **%s.** %s" % (t["state"][f["state"]], f["text"][lang])
            if f.get("fragment"):
                line += " %s%s<code>%s</code> (%s)." % (t["fragment"], col, f["fragment"]["value"], f["fragment"]["label"][lang])
            if f["state"] == "down":
                line += " " + t["proof"] + col + ", ".join("[`%s`](%s/tree/main/evidence/%s)" % (b, REPO, b) for b in f["evidence"]) + "."
            if f["state"] == "wounded":
                line += " %s%s[`%s`](../%s)." % (t["src"], col, f["ref"], f["ref"])
            lines.append(line)
            lines += ruled_out_lines(f, lang, t, col, "../", "    ")
            if f["state"] != "down":
                ms = f.get("missions", [])
                if ms:
                    lines.append("    - %s%s%s" % (t["attack"], col, " · ".join("[%s](%s#mission-%s) (%s)" % (titles[m][0], t["missions_page"], m.lower(), t["unit"].get(titles[m][1], titles[m][1])) for m in ms)))
                else:
                    lines.append("    - %s [%s](%s/issues/new?template=mission.yml)." % (t["none"], t["propose"], REPO))
        if c.get("story"):
            lines += ["", "%s%s[%s](journey/%s.md)" % (t["story"], col, c["block"][lang], c["story"])]
        if c.get("campaign"):
            lines += ["", "%s%s[%s](%s/%s.md)" % (t["camp"], col, t["camp_title"] % c["name"][lang], "campaigns" if lang == "en" else "campagnes", c["id"])]
        if c.get("section"):
            lang_anchor = c["section"] if lang == "en" or c["section"] in fr_anchors else None
            target = "%s#%s" % (page_map, lang_anchor) if lang_anchor else page_map
            lines += ["", "%s%s[%s](%s)" % (t["map"], col, c["block"][lang], target)]
        lines.append("")
    lines += ["## %s" % t["take_h"], "", t["take"], ""]
    return "\n".join(lines)


def render_campaign(c, lang, titles=None):
    t = TXT[lang]
    col = " : " if lang == "fr" else ": "
    titles = mission_titles() if titles is None else titles
    left, total = life(c)
    avail = availability(c)
    order = route(c)
    name = c["name"][lang]
    lines = ["---", "title: \"%s\"" % (t["camp_title"] % name), "description: \"%s\"" % (t["camp_lead"] % name).replace('"', "'"),
             "source: tools/bestiary.yaml", "---", "",
             "<!-- Generated by tools/bestiary_build.py from tools/bestiary.yaml. Do not edit by hand. -->", "",
             "# %s" % (t["camp_title"] % name), "", "*%s* · **%s**" % (c["block"][lang], t["status"][status(c)]), "",
             "**%s** (%s)%s%s" % (t["goal"], t["goal_note"], col, c["goal"][lang]), "",
             t["camp_intro"], "", c["campaign_intro"][lang].strip(), "",
             '<p class="bestiary-bar" role="img" aria-label="%s%s%d %s %d %s">%s</p>' % (t["life"], col, left, t["of"], total, t["points"], bar(left, total)), "",
             "%s%s%d %s %d" % (t["life"], col, left, t["of"], total), "",
             '<p class="bestiary-code">%s</p>' % " &nbsp;·&nbsp; ".join("<code>%s</code>" % v for v in code_strip(c)), ""]
    nxt = next((f for f in order if avail[c["fronts"].index(f)][0] == "available"), None)
    lines += ["## %s" % t["camp_next"], ""]
    lines += ["- %s" % (nxt["text"][lang] if nxt else t["camp_none"]), "", "## %s" % t["camp_route"], ""]
    for n, f in enumerate(order, 1):
        kind, waits = avail[c["fronts"].index(f)]
        label = t["done"] if kind == "done" else (t["avail"] if kind == "available" else "%s %s" % (t["locked"], " ; ".join(w["text"][lang].rstrip(".") for w in waits)))
        lines += ["### %s %d. %s" % (t["step"], n, f["text"][lang].rstrip(".")), "", "- **%s** · %s" % (label, t["state"][f["state"]])]
        if f.get("fragment"):
            lines.append("- %s%s<code>%s</code> (%s)" % (t["fragment"], col, f["fragment"]["value"], f["fragment"]["label"][lang]))
        lines += ruled_out_lines(f, lang, t, col, t["ref_up"], "")
        if f["state"] == "down":
            lines.append("- %s%s%s" % (t["proof"], col, ", ".join("[`%s`](%s/tree/main/evidence/%s)" % (b, REPO, b) for b in f["evidence"])))
        else:
            if f["state"] == "wounded":
                lines.append("- %s%s[`%s`](%s%s)" % (t["src"], col, f["ref"], t["ref_up"], f["ref"]))
            lines.append("- %s%s%s" % (t["needs_h"], col, t["dn"][f["down_needs"]]))
            ms = f.get("missions", [])
            if ms:
                lines.append("- %s%s%s" % (t["attack"], col, " · ".join("[%s](%s#mission-%s) (%s)" % (titles[m][0], t["mission_page"], m.lower(), t["unit"].get(titles[m][1], titles[m][1])) for m in ms)))
            else:
                lines.append("- %s [%s](%s/issues/new?template=mission.yml)." % (t["none"], t["propose"], REPO))
        lines.append("")
    if c.get("story"):
        lines += ["%s%s[%s](../journey/%s.md)" % (t["story"], col, c["block"][lang], c["story"]), ""]
    lines += ["[%s](%s)" % (t["title"], t["bestiary_page"]), ""]
    return "\n".join(lines)


def campaign_pages(data):
    """{path: text} of every campaign page."""
    out = {}
    for c in data["creatures"]:
        if c.get("campaign"):
            for lang in ("en", "fr"):
                out[CAMPAIGN_DIR[lang] / ("%s.md" % c["id"])] = render_campaign(c, lang)
    return out


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
    camps = campaign_pages(data)
    status_code = 0
    for lang, text in pages.items():
        if args.check:
            if not OUT[lang].is_file() or OUT[lang].read_text() != text:
                print("%s is missing or stale (run make bestiary)" % OUT[lang].relative_to(ROOT))
                status_code = 1
        else:
            OUT[lang].write_text(text)
            print("%d creatures written to %s" % (len(data["creatures"]), OUT[lang].relative_to(ROOT)))
    for path, text in camps.items():
        if args.check:
            if not path.is_file() or path.read_text() != text:
                print("%s is missing or stale (run make bestiary)" % path.relative_to(ROOT))
                status_code = 1
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text)
            print("campaign written to %s" % path.relative_to(ROOT))
    if args.check:
        for d in CAMPAIGN_DIR.values():
            for f in sorted(d.glob("*.md")) if d.is_dir() else []:
                if f not in camps:
                    print("%s is not a campaign of the data (remove it)" % f.relative_to(ROOT))
                    status_code = 1
    if args.check and status_code == 0:
        print("bestiary: %d creatures valid, generated pages up to date" % len(data["creatures"]))
    return status_code


if __name__ == "__main__":
    sys.exit(main())
