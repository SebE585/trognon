# 8. Evidence: from claims to reproducible runs

A claim tagged **DEVICE** is only as good as the run behind it. This page ties each one to an **evidence bundle**
that anyone with a 3GS can regenerate, and says plainly which claims have no bundle yet.

## What a bundle contains

Each run of `tools/send.sh` or `tools/go.sh` writes a directory under [`evidence/`](https://github.com/SebE585/trognon/tree/main/evidence):

| File | What it proves |
|---|---|
| `manifest.json` | the payload's SHA-256, the git commit it was built from, whether the tree was clean, the nonce sent and read back, the ipwndfu commit, the command line, the device CPID (the ECID is deliberately not recorded) |
| `screen.txt` | the exact text and colour of every console row the phone displayed, **published by the payload itself over USB**: no photo, no OCR |
| `screen.png` | the console re-drawn from `screen.txt`. A reconstruction, not a photo of the phone |
| `vars.txt` | extra values the payload published (`--vars`) |

**Why the nonce matters.** The iBSS keeps its variables between runs, so a stale value can look like a result. A random
32-bit nonce is appended to the binary, the payload republishes it, and a bundle is written only if it matches.
The screen rows are published in full on every run (empty ones too) for the same reason.

## Bundles

--8<-- "docs/snippets/evidence-table.en.md"

Regenerate this table with `make evidence-index`. Re-run the stages with `tools/evidence_all.sh` (device in DFU).
To check a bundle: check out its commit, `make -C tools/payloads <payload>`, compare the SHA-256 with `manifest.json`
(the build is deterministic with the same toolchain), then read `screen.txt` against the claim.

## Claims and their bundle

| Claim (DEVICE) | Bundle | Status |
|---|---|---|
| UNDEF and data abort handled and resumed, `FSR`/`FAR`/`LR`/`SPSR` as documented | `trace` | bundled |
| Prefetch abort (`bkpt`), `IFSR = 2` | `trace_pabt` | bundled |
| Software-triggered IRQ on VIC0 taken once, enables restored | `trace_irq` | bundled |
| CLCD, GPIO and clock-gate register values | `trace_regs` | bundled |
| VIC0/1/2 state left by the iBSS, IRQ/FIQ vector words | `trace_vic` | bundled |
| Timer block state, free-running tick | `trace_timer` | bundled |
| Tick rate 24 MHz against the host clock | `tools/tick_rate.py` | **not bundled yet**: measured and logged in the journal only |
| Code runs from DFU with nothing written to the phone; patched iBSS, `run` | the nonce of every bundle | bundled for execution; the DFU and pwn steps themselves are not |
| Page table has no fault entry, all sections in domain 0, `DACR = 3` | earlier payloads | **not bundled**: an earlier run, not re-run |
| Timer 5 interrupt | none | **not proven** (see the [hardware map](03-hardware-map.md)) |

A claim without a bundle is documented from an earlier run and should be treated as a lead until re-run.
