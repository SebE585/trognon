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

review.status, defined precisely (2026-10-16, per review -- these two were
being used inconsistently before this was written down):
  - not_reviewed: no bytes of this function have been read.
  - partial: some bytes read, but control flow has NOT been traced to
    every exit/return -- there are still un-read instructions inside the
    function's own body (a branch not followed, a tail past the point
    where reading stopped).
  - complete: every instruction from entry to every exit/return has been
    read and disassembled. This is a claim about REVIEW COVERAGE of the
    body, not about understanding: a `complete` fiche can and often does
    still carry open questions in `unresolved` about what the code means
    (register semantics, a method's real name, hardware behavior) --
    `vic_enable_0x80713510.yaml` is `complete` with two such open
    questions, by design, not an inconsistency.
    BOUNDED TO THE RECONSTRUCTED GRAPH (2026-10-16): `complete` never claims
    that what an indirect call (`blx reg`) or an external call reaches has
    been read. Each such call must appear in `review.unresolved[]` with a
    `proof_needed` line, so the fiche says where the reconstructed graph stops.

Stable ids, for a future cross-reference/RAG use (per review, 2026-10-16):
claims already had their own `<FICHE>-NNN` id; `review.unresolved[]`
entries now carry a parallel `<FICHE>-Q<N>` id instead of being bare
strings, so a later fiche or correction can say "resolves ARM7M-START-Q1"
without depending on wording or addresses staying the same. Both id
spaces are validated for uniqueness across the WHOLE catalog below, not
just per file. `claims[].evidence` itself is still a path(+anchor), not
a separate stable id -- giving evidence its own id registry, decoupled
from file paths, is a larger schema change, not done tonight.

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


def union_bytes(ranges):
    spans = sorted((hex_to_int(a), hex_to_int(b)) for a, b in ranges)
    merged = []
    for s, e in spans:
        if merged and s <= merged[-1][1]:
            merged[-1] = (merged[-1][0], max(merged[-1][1], e))
        else:
            merged.append((s, e))
    return sum(e - s for s, e in merged)


def validate(binaries):
    """Fail loudly before generating anything, rather than publish a table built on an
    inconsistent catalog. Checked, in order: SHA-256 agreement between each fiche and its
    binary's tool_coverage.json; range sanity (start < end, within [0, text_section_bytes));
    known status enums; unique claim ids across the WHOLE catalog, not just per file."""
    errors = []
    seen_claim_ids = {}
    seen_question_ids = {}

    for sub, data in binaries.items():
        tool = data["tool"]
        text_base = hex_to_int(tool["text_section_base"])
        text_end = text_base + tool["text_section_bytes"]

        recognized_ranges = tool.get("recognized_ranges", [])
        for r in recognized_ranges:
            s, e = hex_to_int(r[0]), hex_to_int(r[1])
            if not (s < e):
                errors.append(f"{sub}/tool_coverage.json: recognized_range {r} has start >= end")
            if not (text_base <= s and e <= text_end):
                errors.append(
                    f"{sub}/tool_coverage.json: recognized_range {r} falls outside its own "
                    f"declared __text section [{hex(text_base)}, {hex(text_end)})"
                )

        # Cache self-consistency (per review, 2026-10-16): recompute the stored totals from the
        # cache's own recognized_ranges and refuse any divergence, instead of trusting
        # auto_recognized_bytes/auto_recognized_functions as hand-entered numbers that could
        # silently drift from the ranges they are supposed to summarise.
        if recognized_ranges:
            recomputed_bytes = union_bytes(recognized_ranges)
            if recomputed_bytes != tool.get("auto_recognized_bytes"):
                errors.append(
                    f"{sub}/tool_coverage.json: auto_recognized_bytes "
                    f"({tool.get('auto_recognized_bytes')}) does not match the union of its own "
                    f"recognized_ranges ({recomputed_bytes}) -- the cache has drifted from its "
                    f"own evidence"
                )
            if len(recognized_ranges) != tool.get("auto_recognized_functions"):
                errors.append(
                    f"{sub}/tool_coverage.json: auto_recognized_functions "
                    f"({tool.get('auto_recognized_functions')}) does not match the number of "
                    f"entries in its own recognized_ranges ({len(recognized_ranges)})"
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

            for q in fiche.get("review", {}).get("unresolved", []):
                if not isinstance(q, dict) or "id" not in q:
                    errors.append(
                        f"{path}: an open question in review.unresolved has no id -- "
                        f"use {{id: <FICHE>-Q<N>, text: ...}}, not a bare string"
                    )
                    continue
                qid = q["id"]
                if qid in seen_question_ids:
                    errors.append(f"{path}: duplicate open-question id {qid!r} (already used in {seen_question_ids[qid]})")
                else:
                    seen_question_ids[qid] = path

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

    lines.append("")
    lines.append("## Open links and hardware tests")
    lines.append("")
    lines.append(
        "Open links are the places where a reconstructed control-flow graph stops: an indirect "
        "or external call, an unknown producer, a register meaning. `complete` on a fiche "
        "covers its own instructions only, never what these links lead to. Hardware tests "
        "are listed with their own status, including results that contradicted the reading."
    )
    lines.append("")
    lines.append("| Open link | Function | What is unknown | Proof needed |")
    lines.append("|---|---|---|---|")
    tests = []
    for name, data in binaries.items():
        for f in data["fiches"]:
            if f.get("publication", {}).get("status") != "approved":
                continue
            fn_label = f.get("function_name_apple") or f.get("function_name_assigned") or f["_path"]
            for q in f.get("review", {}).get("unresolved", []):
                text = " ".join(q.get("text", "").split())
                proof = " ".join(str(q.get("proof_needed", "not stated")).split())
                lines.append(f"| `{q['id']}` | `{fn_label}` | {text} | {proof} |")
            for t in f.get("hardware_tests", []):
                tests.append((fn_label, t))
    lines.append("")
    lines.append("| Hardware test | Function | Status | Result |")
    lines.append("|---|---|---|---|")
    for fn_label, t in tests:
        result = " ".join(str(t.get("result") or "not run").split())
        lines.append(f"| {t['name']} | `{fn_label}` | {t.get('execution_status')} | {result} |")

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
