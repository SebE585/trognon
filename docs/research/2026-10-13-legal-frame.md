# Legal frame (EU/France, US): not legal advice

Research date: 2026-10-13. Secondary sources where EUR-Lex/Légifrance were
unreachable.

## Software: Directive 2009/24/EC

- Art. 5(1) alteration necessary for use; 5(3) observe/study/test.
- Art. 6(1) decompilation for interoperability without authorisation; 6(2)
  limits reuse and onward communication of the information.
- Art. 8: contrary contract terms void.
- Art. 7(1)(c): remedies against tools whose **sole intended purpose** is to
  circumvent a program's protection. A multi-purpose boot/OS project is
  arguably outside it, though that is a reading, not settled case law.
- https://www.legislation.gov.uk/eudr/2009/24

## Circumvention of TPMs

- InfoSoc 2001/29 art. 1(2)(a), recital 50: software TPMs are left to
  2009/24 (art. 7(1)(c) above), not InfoSoc art. 6.
- France, CPI L.331-5 covers works "autre qu'un logiciel"; TPMs must not
  prevent interoperability.
- Conseil d'État, 2008-07-16, APRIL, n° 301843: free software circumventing a
  TPM for interoperability is not an infringing "dispositif".
  https://www.marche-public.fr/Marches-publics/Textes/Jurisprudence/CE-301843-APRIL-2008-07-16.htm
- Not verified: current L.331-7, L.335-3-1 penal provisions; CJEU case law.

## US: DMCA §1201, 2024 cycle

- 89 FR 85448 (2024-10-28). 37 CFR 201.40(b)(9): smartphones, interoperability
  of applications or **removal of software**; (b)(15) repair.
- Full OS replacement plausibly but not explicitly covered.
- Exemptions cover the act, **not trafficking** (§1201(a)(2)); relevant to
  distributing a checkm8 tool. Next renewal ~October 2027 (inferred).
- https://www.copyright.gov/1201/2024/

## Right to repair / Ecodesign

- (EU) 2023/1670 applies since 2025-06-20: ≥5 years OS updates, 7 years spare
  parts after end of sales. EU repairability label replaces the French index
  for smartphones.
- None require unlockable bootloaders: a policy argument, not a right.

## Apple firmware blobs: clearest risk

- No exception allows redistributing Apple firmware.
- Asahi pattern: the installer extracts firmware from the user's own system at
  install time; the project never hosts it. https://lwn.net/Articles/975445
- **Rule for Trognon: extract from the user's own IPSW/device, never host.**

## Activation Lock / stolen devices

- checkm8 runs before iOS, so a replacement OS can make locked (possibly
  stolen) phones usable again: reputational and possibly criminal risk
  (receiving stolen goods).
- No official LineageOS/postmarketOS policy found. Asahi inherits Apple's
  ownership check by requiring the owner's macOS credentials.
- Apple's public lock-status checker was removed.
  https://www.techradar.com/news/one-of-iphones-strongest-anti-theft-tools-has-gone-missing
- Suggested mitigations: require an activated, unlocked device (owner signs in
  / pairs) before install; document a refusal policy for locked devices; no
  Activation Lock bypass feature.

## Not verified

EUR-Lex texts of 2023/1670 and 2023/1669; current Légifrance penalties; any
ruling on checkm8; firmware licence terms; official LineageOS/postmarketOS
positions; scope of §201.40(b)(9) for full OS replacement.
