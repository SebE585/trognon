# Contributing to Trognon

Trognon is an open reverse-engineering project on the iPhone 3GS. What makes a contribution useful here is that it can be
checked, not that it is large: a claim is only as good as the evidence and the method behind it.

## Where to start

The [Missions page](https://sebe585.github.io/trognon/en/missions/) lists open questions of the decompilation catalog, each with
what you need and what a good answer looks like. Take one by opening its form, so that nobody works on it twice by accident.

| Level | Work | Open to contributors |
|---|---|---|
| 1 | Reading and static analysis, no phone | yes |
| 2 | A read-only run on a phone, with a reviewed payload | yes |
| 3 | Writes to a phone's hardware | no: only after a written description and the maintainer's agreement |

## What to send

A pull request, using the template: what you looked for, with which sources and versions, what you observed, what you conclude and
what remains uncertain. Keep **observed** and **concluded** apart. Negative and inconclusive results are welcome and count as answers:
a refuted hypothesis closes a mission as well as a discovery.

For anything seen on a phone, include the evidence bundle (`evidence/<timestamp>-<payload>/`), produced by `tools/go.sh`. It carries
the payload hash, the commit, and a random number that must come back identical, which is what makes a run checkable.

## Ground rules

- **No Apple firmware, keys or extracted binaries** in a contribution. Extract what you need from your own copy of the firmware, on your own machine.
- **No hardware write** unless it was described beforehand (address, value, current state, way back) and agreed.
- **Say no more than the evidence shows.** The documentation tags every statement `DEVICE`, `SOURCE` or `HYPOTHESIS`; keep to that.
- **Say when an AI assistant helped.** Name it in the pull request and say what you checked yourself: an assistant's output is a proposal, not evidence, and review treats it as such.
- English for everything a contributor sees in the repository.

## The Bestiary and its campaigns

The [Bestiary](https://sebe585.github.io/trognon/en/bestiary/) shows each block of the phone we have not finished understanding as a creature, and a
creature as a code to find. It is generated from `tools/bestiary.yaml` (`make bestiary`); never edit `docs/en/bestiary.md`, `docs/fr/bestiaire.md`
or the campaign pages. What you can add, and what each needs:

- **A front** (an open point of a creature), with `state`: `open` needs nothing; `wounded` needs `ref`, the page of `docs/` that holds the reading in
  the code; `down` needs `evidence`, the bundle(s) under `evidence/` that show it. A front only moves when its proof is attached.
- **A code fragment** on a front that is down (`fragment: {label, value, proof}`): a register value or an identifier. `proof` must be a line that appears
  as is in the `screen.txt` or `vars.txt` of the front's bundles, and `value` must appear in that line; the generator refuses it otherwise.
- **A creature**: a block, with its names and its bounded `goal` in English and French. The life bar is computed from the states of the fronts; what a
  measurement demonstrates is decided by review, in the pull request.
- **A campaign** (`campaign: true`, plus `campaign_intro` in both languages): every front gets an `id`, optionally `needs` (the fronts that come first in our
  working order, a plan and not a proven requirement) and, unless it is down, `down_needs` (`survey`, `write` or `unknown`: what would bring it down).
  A front is *locked* until every front it needs is down; locked fronts can still be studied in the code. Hardware writes stay with the maintainer.

`make bestiary-check` runs its tests and refuses a front without its proof, a mission, bundle or section that does not exist, a cycle in `needs`,
and a stale generated page.

## Checks

`make docs-check` (documentation, generated pages, links) and `make iop-coverage` (the catalog and its tests) must pass; CI runs both.
The guided journey, the missions page and the Bestiary are generated: edit `docs/journey-src/*.yaml`, `tools/iop_analysis/missions.yaml` and
`tools/bestiary.yaml`, then run `make journey`, `make missions` and `make bestiary`, never the generated pages.

## Licences

Code is GPL-2.0-only or MIT as marked per file; documentation is CC-BY-SA-4.0. By submitting a contribution you confirm you have the
right to do so under these licences.
