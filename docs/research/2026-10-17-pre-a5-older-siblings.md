# Pre-A5, older siblings: iPhone 2G/3G and iPod touch 1 to 4

Research date: 2026-10-17. Question: which chip and which boot exploit go with each of the iPhone 2G, iPhone 3G
and iPod touch 1G to 4G, so that the device catalog can identify them. Not tried on any unit.

Tags: **SOURCE** read in a named source. **HYPOTHESIS** derived or unconfirmed.

## Which chip in which device

| Device | Chip | Evidence |
|---|---|---|
| iPhone 2G, iPhone 3G, iPod touch 1G | S5L8900, ARM1176JZF-S (**ARMv6**) | **SOURCE**: Wikipedia, "Early iPhone systems-on-chip" ("used in the first-generation iPhone, the iPhone 3G, and the first-generation iPod Touch"; "a 32-bit ARMv6 compatible ARM 1176JZF-S CPU core") |
| iPod touch 2G | S5L8720 | **SOURCE**: same page ("used in the iPod touch (2nd generation)", "similar to the S5L8900") |
| iPhone 3GS | S5L8920, Cortex-A8 | **SOURCE**: same page; **DEVICE** on our unit |
| iPod touch 3G | S5L8922, "similar to the S5L8920" | **SOURCE**: same page lists S5L8922 for the 3rd-generation iPod touch **and** also lists S5L8920 for it. Which CPID a given unit reports is **not settled** here |
| iPhone 4, iPod touch 4G | A4 (S5L8930) | **SOURCE**: Wikipedia, "Apple A4" (also iPad 1 and Apple TV 2) |

Consequence: the S5L8900 family is **ARMv6**, not ARMv7. Our CPU-level code (written for the Cortex-A8 of the 3GS)
does not carry over to it unchanged (**HYPOTHESIS**: not tested, the core differs on paper).

## Which exploit

- **CPID 8720** (iPod touch 2G): `ipwndfu -p` runs **steaks4uce**; its README says "Pwned DFU Mode with steaks4uce
  exploit for S5L8720 devices". **SOURCE**: `ipwndfu` at commit `0e28932`, files `ipwndfu` and `steaks4uce.py`.
- **CPID 8920 and 8922** (3GS, iPod touch 3G): `ipwndfu -p` runs **limera1n**. **SOURCE**: same file.
- **CPID 8930** (iPhone 4, iPod touch 4G): `ipwndfu -p` runs **SHAtter**. **SOURCE**: same file.
- **S5L8900** (iPhone 2G/3G, iPod touch 1G): `ipwndfu` has **no branch** for it (checked in the same file). The
  known flaw is a different kind: the bootrom does not check the signature of LLB, so a crafted LLB takes control
  (the "Pwnage" exploit, iPhone Dev Team). **SOURCE**: theapplewiki.com pages "S5L8900" and "Pwnage", as quoted by a
  search engine. The bootrom fix ships from the iPod touch 2G on. Whether a pwned DFU exists for this chip is
  **not found**.

## Not found, and limits

- The CPID that the S5L8900 reports in DFU is **not stated** in anything read. `0x8900` follows Apple's naming rule
  used elsewhere in the catalog (`S5L8940` is `0x8940`), so it is a **HYPOTHESIS** until a unit is probed.
- `theapplewiki.com` refused a direct fetch (HTTP 403); its claims here come from search excerpts, so each is worth
  re-reading on the page before it is relied on.
- Model identifiers `iPhone1,1`, `iPhone1,2`, `iPod1,1` to `iPod4,1` are the usual ones and were not re-read in a source
  today.
