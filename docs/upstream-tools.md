# Relying on existing tools instead of reimplementing them

Status: draft, 2026-10-13. Sources and dates in `docs/research/`.

Trognon never ships exploit code. The bootrom bug (checkm8, CVE-2019-8900)
and its per-device variants are already implemented, published and
maintained upstream. Reimplementing them here would fork effort the
community already carries, and would turn this repository into something
its README explicitly says it is not: a single tool whose purpose is
circumvention. Trognon's own code starts **after** a device is already in a
documented, standard DFU state.

## Tools Trognon orchestrates, never vendors

| Tool | Scope | Why we call it instead of rewriting it |
|---|---|---|
| [axi0mX/ipwndfu](https://github.com/axi0mX/ipwndfu) | A single `-p` flag dispatches by CPID to the right exploit: limera1n (0x8920/0x8922), SHAtter (0x8930), or checkm8 (0x8947, 0x8950, 0x8955, 0x8960, 0x8002, 0x8004, 0x8010, 0x8011, 0x8015) | One tool covers both our pre-A5 devices and most of A6-A11; confirmed 2026-10-14 by reading its source, not yet run against hardware |
| [gaster](https://github.com/verygenericname/gaster) | checkm8 on A7-A11, used by Hoolock's documented boot chain | Faster, scriptable, what Hoolock's own docs recommend |
| [LukeZGD/checkm8-a5](https://github.com/LukeZGD/checkm8-a5) | checkm8 on A5/A5X via Arduino + USB Host Shield or RP2040 Pico | The *only* documented path for the 4S generation; needs a microcontroller, not something a host-side CLI can do alone |
| [HoolockLinux](https://github.com/HoolockLinux) (m1n1 `idevice` branch, kernel) | pongoOS, then m1n1, then Linux boot chain, A7-A11 device trees | Upstream target for any A7-A11 kernel work: contribute there, don't fork |
| [rzxvx/Project-Cascadia](https://github.com/rzxvx/Project-Cascadia) | Linux 6.12 on iPad mini 1 (S5L8942, A5) | Closest existing base for the iPhone 4S (S5L8940) port |

## What this means for `trognon-unlock`

The orchestration layer (`src/chain.c`, see `docs/architecture.md`) invokes
these tools as external subprocesses, pinned to a known-good version/commit,
and checks their exit status. It does not link against their internals or
copy their code. If a tool disappears upstream, that is tracked as a project
risk, not a reason to absorb it into this tree.

## Firmware

Per `docs/research/2026-10-13-legal-frame.md`: Trognon never redistributes
Apple firmware blobs. Where a boot stage needs one (Wi-Fi firmware, SEP
images), it is extracted from the device owner's own IPSW at install time,
the way Asahi Linux's installer extracts macOS firmware. It is never hosted here.

## Gap: A8 and A9 (iPhone 6, iPhone 6s)

`ipwndfu`'s own checkm8 CPID list (above) does not include `0x7000`
(T7000, A8) or `0x8000` (S8000, A9). Checked 2026-10-14 by reading its
source. Both are in `device_db.c` as `TROGNON_UNLOCK_HOST_DIRECT`, which is
still correct (no helper board needed), but the tool to use is unverified:
likely `gaster`, not `ipwndfu`. Don't assume `ipwndfu -p` works on these two
without checking first.

## Open question

`checkm8-a5` needs a second piece of hardware (Pico or Arduino) that the host
CLI cannot drive by itself over the phone's own USB port. `trognon-unlock`
therefore has two distinct code paths from the start: "host does everything"
(A7-A11) and "host drives a helper board" (A5/A6). See
`docs/research/2026-10-13-untethered-boot.md` for why a permanent dongle
would need the same helper-board model.
