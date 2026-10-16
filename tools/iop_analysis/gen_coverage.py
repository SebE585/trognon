#!/usr/bin/env python3
"""Generate a coverage table from the per-function YAML catalog in
tools/iop_analysis/catalog/, and write it to docs/snippets/ for inclusion
in a documentation page via mkdocs' snippets extension.

Pilot scope (2026-10-16): AppleARM7M and IOSlaveProcessor only, per the
agreed first step -- validate the approach on a small, real surface before
extending to the rest of the IOP firmware and other drivers.

Separates deliberately, per this project's own legal/publication stance
(decompilation for interoperability, Article L122-6-1: conditional, limits
use and communication of what is learned):
  - "Tool coverage" (bytes auto-recognized by r2's own analysis) comes from
    a committed JSON cache per binary (tools/iop_analysis/catalog/<Binary>/
    tool_coverage.json), produced once locally with the binary and r2
    installed -- never requires the binary or r2 at generation time, so
    this script runs in CI with neither.
  - "Human review" counts come only from the YAML fiches' own
    reviewed_ranges -- what was actually read and interpreted, which can
    be smaller OR larger than tool coverage (a human can trace a function
    the automatic analysis missed entirely, as happened with
    AppleARM7M::start()).
  - Every range (reviewed_ranges and tool_coverage's recognized_ranges) is
    a half-open interval [start, end): the byte at `end` is NOT included.
  - "Claims" are only rendered in the public table when
    publication.status == "approved". A fiche with publication.status
    "pending_review" or "excluded" still counts in the aggregate numbers
    (so the table doesn't silently hide how much review exists) but its
    claim text is never emitted.

IMPORTANT, publication scope (2026-10-16, per review): publication.status
gates what this generator PRINTS into docs/snippets/ -- it does NOT make a
fiche private. Every *.yaml file under catalog/ is committed to this
public repository and is as visible on GitHub as any other file in it,
regardless of its publication.status. Treat every fiche, from the moment
it is committed, as public: publication.status is an editorial curation
decision about the generated table, not an access-control mechanism and
not a legal certification. Do not put anything in a committed fiche
(an excerpt, a verbatim string dump long enough to be a reproduction
rather than a fact) that should not be public. A real private tier (e.g.
a gitignored catalog_private/ for genuinely pre-publication drafts) is a
future option, not implemented here.

"Functions documented" below counts fiches, not a true fraction of either
binary's real function count: it is measured against the number of
functions radare2's own analysis detected (cached in each binary's
tool_coverage.json, itself not guaranteed complete -- see the AppleARM7M
note), not against an unknown true total. Read it as "fiches in this
catalog versus a tool-detected candidate pool," not "percent of the
firmware understood."

No confidence score is computed or published. Evidence and open questions
only -- see this project's own DEVICE/SOURCE/HYPOTHESIS discipline.

Usage: python3 gen_coverage.py [--catalog DIR] [--out FILE]
"""
import argparse
import glob
import json
import os
import sys

try:
    import yaml
except ImportError:
    print("gen_coverage: PyYAML is required (pip install pyyaml); "
          "it is already a transitive dependency of mkdocs in this project", file=sys.stderr)
    sys.exit(1)

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_CATALOG = os.path.join(HERE, "catalog")
DEFAULT_OUT = os.path.join(HERE, "..", "..", "docs", "snippets", "iop-coverage.en.md")
REPO_URL = "https://github.com/SebE585/trognon"

REVIEW_STATUSES = {"not_reviewed", "partial", "complete"}
CLAIM_STATUSES = {"hypothesis", "source_verified", "device_disassembly_verified", "device_tested"}
PUBLICATION_STATUSES = {"approved", "pending_review", "excluded"}
EXECUTION_STATUSES = {"planned", "executed"}

CLAIM_STATUS_LABEL = {
    "hypothesis": "hypothesis",
    "source_verified": "source (not disassembled)",
    "device_disassembly_verified": "disassembly",
    "device_tested": "hardware-observed",
}


class CatalogError(SystemExit):
    def __init__(self, msg):
        super().__init__(f"gen_coverage: VALIDATION FAILED: {msg}")


def load_catalog(catalog_dir):
    binaries = {}  # subdir name -> {"tool": {...}, "fiches": [...]}
    for sub in sorted(os.listdir(catalog_dir)):
        subdir = os.path.join(catalog_dir, sub)
        if not os.path.isdir(subdir):
            continue
        tool_path = os.path.join(subdir, "tool_coverage.json")
        if not os.path.exists(tool_path):
            raise CatalogError(
                f"{subdir} has no tool_coverage.json -- every catalog subdirectory must "
                f"carry one, even if its auto_recognized_bytes is 0 (say so explicitly, "
                f"don't omit the file)"
            )
        with open(tool_path) as f:
            tool = json.load(f)
        fiches = []
        for yml_path in sorted(glob.glob(os.path.join(subdir, "*.yaml"))):
            with open(yml_path) as f:
                fiche = yaml.safe_load(f)
            fiche["_path"] = os.path.relpath(yml_path, os.path.join(HERE, ".."))
            fiches.append(fiche)
        binaries[sub] = {"tool": tool, "fiches": fiches}
    return binaries


def hex_to_int(h):
    return int(h, 16)


def validate(binaries):
    """Fail loudly before generating anything, rather than publish a table built on an
    inconsistent catalog. Checked, in order: SHA-256 agreement between each fiche and its
    binary's tool_coverage.json; range sanity (start < end, within [0, text_section_bytes));
    known status enums; unique claim ids across the WHOLE catalog, not just per file."""
    errors = []
    seen_claim_ids = {}

    for sub, data in binaries.items():
        tool = data["tool"]
        text_base = hex_to_int(tool["text_section_base"])
        text_end = text_base + tool["text_section_bytes"]

        for r in tool.get("recognized_ranges", []):
            s, e = hex_to_int(r[0]), hex_to_int(r[1])
            if not (s < e):
                errors.append(f"{sub}/tool_coverage.json: recognized_range {r} has start >= end")
            if not (text_base <= s and e <= text_end):
                errors.append(
                    f"{sub}/tool_coverage.json: recognized_range {r} falls outside its own "
                    f"declared __text section [{hex(text_base)}, {hex(text_end)})"
                )

        for fiche in data["fiches"]:
            path = fiche["_path"]

            if fiche.get("binary_sha256") != tool.get("binary_sha256"):
                errors.append(
                    f"{path}: binary_sha256 ({fiche.get('binary_sha256')!r}) does not match "
                    f"{sub}/tool_coverage.json ({tool.get('binary_sha256')!r})"
                )

            rstatus = fiche.get("review", {}).get("status")
            if rstatus not in REVIEW_STATUSES:
                errors.append(f"{path}: review.status {rstatus!r} not in {sorted(REVIEW_STATUSES)}")

            for rng in fiche.get("review", {}).get("reviewed_ranges", []):
                s, e = hex_to_int(rng[0]), hex_to_int(rng[1])
                if not (s < e):
                    errors.append(f"{path}: reviewed_range {rng} has start >= end")
                if not (text_base <= s and e <= text_end):
                    errors.append(
                        f"{path}: reviewed_range {rng} falls outside the __text section "
                        f"[{hex(text_base)}, {hex(text_end)}) declared in {sub}/tool_coverage.json"
                    )

            pstatus = fiche.get("publication", {}).get("status")
            if pstatus not in PUBLICATION_STATUSES:
                errors.append(f"{path}: publication.status {pstatus!r} not in {sorted(PUBLICATION_STATUSES)}")

            for c in fiche.get("claims", []):
                cid = c.get("id")
                if not cid:
                    errors.append(f"{path}: a claim is missing its id")
                elif cid in seen_claim_ids:
                    errors.append(f"{path}: duplicate claim id {cid!r} (already used in {seen_claim_ids[cid]})")
                else:
                    seen_claim_ids[cid] = path
                cstatus = c.get("status")
                if cstatus not in CLAIM_STATUSES:
                    errors.append(f"{path}: claim {cid!r} status {cstatus!r} not in {sorted(CLAIM_STATUSES)}")

            for h in fiche.get("hardware_tests", []) or []:
                estatus = h.get("execution_status")
                if estatus not in EXECUTION_STATUSES:
                    errors.append(
                        f"{path}: hardware_tests entry {h.get('name')!r} has execution_status "
                        f"{estatus!r}, not in {sorted(EXECUTION_STATUSES)} -- a bare non-empty "
                        f"hardware_tests list is not enough, every entry must say whether it "
                        f"actually ran"
                    )

    if errors:
        raise CatalogError("\n  - " + "\n  - ".join(errors))


def union_bytes(ranges):
    spans = sorted((hex_to_int(a), hex_to_int(b)) for a, b in ranges)
    merged = []
    for s, e in spans:
        if merged and s <= merged[-1][1]:
            merged[-1] = (merged[-1][0], max(merged[-1][1], e))
        else:
            merged.append((s, e))
    return sum(e - s for s, e in merged)


def evidence_link(evidence, from_dir="docs/research"):
    """Render an evidence path as a clickable link where that can be done without an
    unverified claim. docs/-relative paths get a real mkdocs-relative link (resolvable by the
    site build, checked by tools/check_internal_links.py like any other internal link).
    evidence/<bundle>/ paths get a GitHub tree link (a directory, no anchor). Anything else
    (e.g. a path into tools/, outside docs_dir -- mkdocs cannot resolve it) gets a GitHub blob
    link to the FILE only, with any #anchor fragment deliberately dropped: GitHub's own heading
    anchors were not independently verified against ours for every such link, and a wrong
    anchor would silently land on the top of the file rather than fail loudly."""
    if not evidence:
        return ""
    path, _, anchor = evidence.partition("#")
    if path.startswith("docs/"):
        # Keep both sides rooted at the repo root (don't strip the "docs/" prefix from just one
        # of them) -- os.path.relpath silently gives a wrong answer otherwise, caught by hand
        # tonight: relpath("research/X.md", "docs/research") computed "../../research/X.md"
        # instead of "X.md", because the two inputs were relative to different implied roots.
        rel = os.path.relpath(path, from_dir)
        frag = f"#{anchor}" if anchor else ""
        return f"[{evidence}]({rel}{frag})"
    if path.startswith("evidence/"):
        return f"[{evidence}]({REPO_URL}/tree/main/{path})"
    if path.endswith(".md"):
        return f"[{path}]({REPO_URL}/blob/main/{path})"
    return evidence


def render(binaries):
    lines = []
    lines.append("<!-- Generated by tools/iop_analysis/gen_coverage.py -- do not edit by hand.")
    lines.append("     Re-run `make iop-coverage` after editing the catalog. -->")
    lines.append("")
    lines.append(
        "| Binary | Text size | Tool-detected (candidate pool) | Fiches documented | "
        "Bytes reviewed | Hardware-executed | Claims published |"
    )
    lines.append("|---|---|---|---|---|---|---|")

    total_claims_approved = 0
    total_claims_other = 0

    for name, data in binaries.items():
        tool = data["tool"]
        fiches = data["fiches"]
        reviewed_ranges = [r for f in fiches for r in f.get("review", {}).get("reviewed_ranges", [])]
        bytes_reviewed = union_bytes(reviewed_ranges) if reviewed_ranges else 0
        n_documented = sum(1 for f in fiches if f.get("review", {}).get("status") in ("partial", "complete"))
        n_hw_executed = sum(
            1 for f in fiches
            for h in (f.get("hardware_tests") or [])
            if h.get("execution_status") == "executed"
        )
        n_hw_total = sum(len(f.get("hardware_tests") or []) for f in fiches)
        n_claims = sum(len(f.get("claims", [])) for f in fiches)
        n_approved = sum(
            1 for f in fiches for c in f.get("claims", [])
            if f.get("publication", {}).get("status") == "approved"
        )
        total_claims_approved += n_approved
        total_claims_other += (n_claims - n_approved)

        lines.append(
            f"| `{tool['binary']}` | {tool['text_section_bytes']} B | "
            f"{tool['auto_recognized_functions']} fn tool-detected, {tool['auto_recognized_bytes']} B "
            f"({tool['auto_recognized_bytes'] * 100 // tool['text_section_bytes']}%) | "
            f"{n_documented} fiches ({len(fiches)} total) | {bytes_reviewed} B | "
            f"{n_hw_executed} / {n_hw_total} | {n_approved} / {n_claims} |"
        )

    lines.append("")
    lines.append(
        f"\"Tool-detected\" is r2's own automatic function count for that binary — a candidate "
        f"pool, not a verified total (see each binary's own `tool_coverage.json` note; it has "
        f"already been shown wrong once, undercounting `AppleARM7M.kext.bin` by missing three "
        f"hand-found functions entirely). \"Fiches documented\" counts catalog entries, not a "
        f"fraction of either binary's true function count — a full inventory of candidate "
        f"functions (reviewed vs. not) is a planned follow-up, not built yet. \"Hardware-executed\" "
        f"counts only tests whose own `execution_status` is `executed`; a non-empty test list "
        f"with every entry still `planned` counts as zero here."
    )
    lines.append("")
    lines.append("## Claims by subsystem")
    lines.append("")
    lines.append(
        "Only claims with `publication.status: approved` are listed below; "
        f"{total_claims_other} further claim(s) exist in the catalog but are not yet cleared "
        "for publication (status `pending_review` or `excluded`). **Every fiche in the catalog "
        "is committed to this public repository regardless of this status** — "
        "`publication.status` controls only what is printed in this generated table, not who "
        "can read the YAML source; see this script's own module docstring."
    )
    lines.append("")
    lines.append("| Claim | Function | Provenance | Evidence |")
    lines.append("|---|---|---|---|")
    for name, data in binaries.items():
        for f in data["fiches"]:
            if f.get("publication", {}).get("status") != "approved":
                continue
            fn_label = f.get("function_name_apple") or f.get("function_name_assigned") or f["_path"]
            for c in f.get("claims", []):
                text = " ".join(c["text"].split())
                provenance = CLAIM_STATUS_LABEL.get(c.get("status"), c.get("status", ""))
                evidence = evidence_link(c.get("evidence", ""))
                lines.append(f"| `{c['id']}` — {text} | `{fn_label}` | {provenance} | {evidence} |")

    return "\n".join(lines) + "\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--catalog", default=DEFAULT_CATALOG)
    ap.add_argument("--out", default=DEFAULT_OUT)
    args = ap.parse_args()

    binaries = load_catalog(args.catalog)
    if not binaries:
        raise SystemExit(f"gen_coverage: no catalog subdirectories found under {args.catalog}")
    validate(binaries)
    out = render(binaries)
    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    with open(args.out, "w") as f:
        f.write(out)
    print(f"gen_coverage: wrote {args.out} ({len(binaries)} binaries, "
          f"{sum(len(d['fiches']) for d in binaries.values())} fiches)")


if __name__ == "__main__":
    main()
