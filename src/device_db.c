/* SPDX-License-Identifier: MIT */
/*
 * Static device catalog. Read-only, no bus access.
 *
 * Support fields reflect docs/research/2026-10-13-*.md (surveyed
 * 2026-10-13) and, for the pre-A5 siblings, 2026-10-17-pre-a5-older-siblings.md.
 * Entries from the A5 on only identify a unit: Trognon follows those chips
 * upstream (scope decision 2026-10-17). "notes" points to the upstream/downstream status, not a
 * guarantee — re-check dates before relying on this in code.
 */
#include <string.h>
#include <stddef.h>

#include "trognon/device.h"

const struct trognon_device trognon_device_catalog[] = {
    {
        .model_name = "iPhone 2G",
        .provenance = TROGNON_PROV_UPSTREAM,
        .provenance_date = "2026-10-17",
        .model_identifier = "iPhone1,1",
        .cpid = 0x8900,
        .soc = TROGNON_SOC_S5L8900,
        .soc_codename = "S5L8900",
        .dt_platform = NULL,
        .unlock_path = TROGNON_UNLOCK_UNKNOWN,
        .unlock_tool_hint = "no pwned-DFU path known: ipwndfu has no CPID:8900 "
                             "branch (checked 2026-10-17). The known flaw is "
                             "the unchecked LLB signature (Pwnage), a "
                             "different mechanism, not tried",
        .support = {
            .framebuffer = 0, .touch = 0, .wifi = 0, .storage = 0, .gpu = 0,
            .notes = "Not tried. S5L8900 is ARMv6 (ARM1176JZF-S): code written "
                     "for the 3GS's Cortex-A8 does not carry over unchanged "
                     "(hypothesis). CPID 0x8900 follows the naming rule, "
                     "not yet read from a unit. See "
                     "docs/research/2026-10-17-pre-a5-older-siblings.md.",
        },
    },
    {
        .model_name = "iPhone 3G",
        .provenance = TROGNON_PROV_UPSTREAM,
        .provenance_date = "2026-10-17",
        .model_identifier = "iPhone1,2",
        .cpid = 0x8900,
        .soc = TROGNON_SOC_S5L8900,
        .soc_codename = "S5L8900",
        .dt_platform = NULL,
        .unlock_path = TROGNON_UNLOCK_UNKNOWN,
        .unlock_tool_hint = "no pwned-DFU path known: ipwndfu has no CPID:8900 "
                             "branch (checked 2026-10-17). The known flaw is "
                             "the unchecked LLB signature (Pwnage), a "
                             "different mechanism, not tried",
        .support = {
            .framebuffer = 0, .touch = 0, .wifi = 0, .storage = 0, .gpu = 0,
            .notes = "Not tried. S5L8900 is ARMv6 (ARM1176JZF-S): code written "
                     "for the 3GS's Cortex-A8 does not carry over unchanged "
                     "(hypothesis). CPID 0x8900 follows the naming rule, "
                     "not yet read from a unit. See "
                     "docs/research/2026-10-17-pre-a5-older-siblings.md.",
        },
    },
    {
        .model_name = "iPod touch 1G",
        .provenance = TROGNON_PROV_UPSTREAM,
        .provenance_date = "2026-10-17",
        .model_identifier = "iPod1,1",
        .cpid = 0x8900,
        .soc = TROGNON_SOC_S5L8900,
        .soc_codename = "S5L8900",
        .dt_platform = NULL,
        .unlock_path = TROGNON_UNLOCK_UNKNOWN,
        .unlock_tool_hint = "no pwned-DFU path known: ipwndfu has no CPID:8900 "
                             "branch (checked 2026-10-17). The known flaw is "
                             "the unchecked LLB signature (Pwnage), a "
                             "different mechanism, not tried",
        .support = {
            .framebuffer = 0, .touch = 0, .wifi = 0, .storage = 0, .gpu = 0,
            .notes = "Not tried. S5L8900 is ARMv6 (ARM1176JZF-S): code written "
                     "for the 3GS's Cortex-A8 does not carry over unchanged "
                     "(hypothesis). CPID 0x8900 follows the naming rule, "
                     "not yet read from a unit. See "
                     "docs/research/2026-10-17-pre-a5-older-siblings.md.",
        },
    },
    {
        .model_name = "iPod touch 2G",
        .provenance = TROGNON_PROV_UPSTREAM,
        .provenance_date = "2026-10-17",
        .model_identifier = "iPod2,1",
        .cpid = 0x8720,
        .soc = TROGNON_SOC_S5L8720,
        .soc_codename = "S5L8720",
        .dt_platform = NULL,
        .unlock_path = TROGNON_UNLOCK_HOST_DIRECT,
        .unlock_tool_hint = "ipwndfu -p (dispatches to its steaks4uce exploit for "
                             "CPID 0x8720, read in the tool's own source "
                             "2026-10-17)",
        .support = {
            .framebuffer = 0, .touch = 0, .wifi = 0, .storage = 0, .gpu = 0,
            .notes = "Not tried. S5L8720 is close to the S5L8900 (Wikipedia); the "
                     "core is not stated in the sources read. See "
                     "docs/research/2026-10-17-pre-a5-older-siblings.md.",
        },
    },
    {
        .model_name = "iPhone 3GS",
        .provenance = TROGNON_PROV_DEVICE,
        .provenance_date = "2026-10-15",
        .model_identifier = "iPhone2,1",
        .cpid = 0x8920,
        .soc = TROGNON_SOC_S5L8920,
        .soc_codename = "S5L8920",
        .dt_platform = NULL, /* pre-A4; no modern port effort found. */
        .unlock_path = TROGNON_UNLOCK_HOST_DIRECT, /* limera1n applies to
                                                        S5L8920 too — same
                                                        unpatchable bootrom
                                                        bug as A4. Some
                                                        bootrom revisions
                                                        (359.3.2) need
                                                        alloc8 instead;
                                                        both are host-direct. */
        .unlock_tool_hint = "ipwndfu -p (dispatches to its limera1n "
                             "exploit for CPID 0x8920, confirmed in the "
                             "tool's own source 2026-10-14)",
        .support = {
            .framebuffer = 1, .touch = 0, .wifi = 0, .storage = 0, .gpu = 0,
            .notes = "Reference device of the bare-metal track: Trognon code "
                     "runs on it (console, exceptions, IRQ, buttons). "
                     "Framebuffer confirmed (CLCD, text drawn). NAND "
                     "(H2FMI) identified: Toshiba, 32 GB, CE0/CE1 only, "
                     "geometry and capacity computed from a real chip-ID "
                     "read -- read-only, no page read/write yet, so "
                     "storage stays 0. Wi-Fi (BCM4325/SDIO) address, "
                     "clock-gate, IRQ and reset/wake GPIO are known and "
                     "the reset line toggles, but every register still "
                     "reads 0: the leading explanation is a second, "
                     "embedded IOP processor that owns this block "
                     "directly, not a wrong offset -- see "
                     "docs/research/2026-10-16-wifi-iop-architecture.md. "
                     "No modern OS port effort exists for this SoC.",
        },
    },
    {
        .model_name = "iPod touch 3G",
        .provenance = TROGNON_PROV_UPSTREAM,
        .provenance_date = "2026-10-17",
        .model_identifier = "iPod3,1",
        .cpid = 0x8922,
        .soc = TROGNON_SOC_S5L8922,
        .soc_codename = "S5L8922",
        .dt_platform = NULL,
        .unlock_path = TROGNON_UNLOCK_HOST_DIRECT,
        .unlock_tool_hint = "ipwndfu -p (dispatches to its limera1n exploit for "
                             "CPID 0x8922, read in the tool's own source "
                             "2026-10-17)",
        .support = {
            .framebuffer = 0, .touch = 0, .wifi = 0, .storage = 0, .gpu = 0,
            .notes = "Not tried. Wikipedia lists both S5L8920 and S5L8922 for "
                     "this model, so which CPID a given unit reports is "
                     "unsettled. See "
                     "docs/research/2026-10-17-pre-a5-older-siblings.md.",
        },
    },
    {
        .model_name = "iPhone 4",
        .provenance = TROGNON_PROV_UPSTREAM,
        .provenance_date = "2026-10-14",
        .model_identifier = "iPhone3,1", /* GSM, model A1332. CDMA A1349 is
                                             iPhone3,3 — same A4, not yet in
                                             this table. */
        .cpid = 0x8930,
        .soc = TROGNON_SOC_A4,
        .soc_codename = "S5L8930",
        .dt_platform = NULL, /* predates checkm8's A5-A11 scope entirely;
                                 no modern Linux port effort found (last
                                 related work: OpeniBoot/iDroid, dead since
                                 2012). */
        .unlock_path = TROGNON_UNLOCK_HOST_DIRECT, /* Same unpatchable
                                                        bootrom-era bug
                                                        family as S5L8920,
                                                        host-direct — but a
                                                        *different* exploit:
                                                        SHAtter, not
                                                        limera1n. Corrected
                                                        2026-10-14 after
                                                        checking ipwndfu's
                                                        own CPID dispatch
                                                        table; the research
                                                        doc originally
                                                        conflated the two. */
        .unlock_tool_hint = "ipwndfu -p (dispatches to its SHAtter "
                             "exploit for CPID 0x8930, confirmed in the "
                             "tool's own source 2026-10-14 — not limera1n, "
                             "see docs/research/2026-10-14-pre-a5-limera1n.md "
                             "correction)",
        .support = {
            .framebuffer = 0, .touch = 0, .wifi = 0, .storage = 0, .gpu = 0,
            .notes = "No modern OS port; useful as a boot-chain test device "
                     "(SHAtter/redsn0w-era tooling), not a realistic Linux "
                     "target today.",
        },
    },
    {
        .model_name = "iPod touch 4G",
        .provenance = TROGNON_PROV_UPSTREAM,
        .provenance_date = "2026-10-17",
        .model_identifier = "iPod4,1",
        .cpid = 0x8930,
        .soc = TROGNON_SOC_A4,
        .soc_codename = "S5L8930",
        .dt_platform = NULL,
        .unlock_path = TROGNON_UNLOCK_HOST_DIRECT,
        .unlock_tool_hint = "ipwndfu -p (dispatches to its SHAtter exploit for CPID "
                             "0x8930, read in the tool's own source "
                             "2026-10-17)",
        .support = {
            .framebuffer = 0, .touch = 0, .wifi = 0, .storage = 0, .gpu = 0,
            .notes = "Not tried. Shares CPID 0x8930 with the iPhone 4 (and the "
                     "iPad 1): a lookup by CPID returns the iPhone 4 "
                     "entry. See "
                     "docs/research/2026-10-17-pre-a5-older-siblings.md.",
        },
    },
    {
        .model_name = "iPhone 4S",
        .provenance = TROGNON_PROV_UPSTREAM,
        .provenance_date = "2026-10-13",
        .model_identifier = "iPhone4,1",
        .cpid = 0x8940,
        .soc = TROGNON_SOC_A5,
        .soc_codename = "S5L8940",
        .dt_platform = NULL, /* no mainline DT; closest base is Project
                                 Cascadia (S5L8942, iPad mini 1) */
        .unlock_path = TROGNON_UNLOCK_HELPER_BOARD,
        .unlock_tool_hint = "LukeZGD/checkm8-a5 via a Pico RP2040 or "
                             "Arduino+MAX3421E helper board — host CLI "
                             "cannot drive this exploit alone",
        .support = {
            .framebuffer = 0, .touch = 0, .wifi = 0, .storage = 0, .gpu = 0,
            .notes = "No port exists yet; nearest work is Project Cascadia "
                     "on the sibling S5L8942 SoC.",
        },
    },
    {
        .model_name = "iPhone 5",
        .provenance = TROGNON_PROV_UPSTREAM,
        .provenance_date = "2026-10-13",
        .model_identifier = "iPhone5,1",
        .cpid = 0x8950,
        .soc = TROGNON_SOC_A6,
        .soc_codename = "S5L8950",
        .dt_platform = NULL,
        .unlock_path = TROGNON_UNLOCK_HOST_DIRECT,
        .unlock_tool_hint = "ipwndfu -p (checkm8) — this CPID is in ipwndfu's own checkm8 dispatch list, confirmed 2026-10-14",
        .support = {
            .framebuffer = 0, .touch = 0, .wifi = 0, .storage = 0, .gpu = 0,
            .notes = "checkm8 works over plain USB (ipwndfu); no OS port found.",
        },
    },
    {
        .model_name = "iPhone 5s",
        .provenance = TROGNON_PROV_UPSTREAM,
        .provenance_date = "2026-10-13",
        .model_identifier = "iPhone6,1",
        .cpid = 0x8960,
        .soc = TROGNON_SOC_A7,
        .soc_codename = "S5L8960X",
        .dt_platform = "s5l8960x",
        .unlock_path = TROGNON_UNLOCK_HOST_DIRECT,
        .unlock_tool_hint = "ipwndfu -p (checkm8) — this CPID is in ipwndfu's own checkm8 dispatch list, confirmed 2026-10-14",
        .support = {
            .framebuffer = 1, .touch = 0, .wifi = 0, .storage = 0, .gpu = 0,
            .notes = "Mainline since Linux 6.13 (framebuffer, buttons).",
        },
    },
    {
        .model_name = "iPhone 6",
        .provenance = TROGNON_PROV_UPSTREAM,
        .provenance_date = "2026-10-13",
        .model_identifier = "iPhone7,2",
        .cpid = 0x7000,
        .soc = TROGNON_SOC_A8,
        .soc_codename = "T7000",
        .dt_platform = "t7000",
        .unlock_path = TROGNON_UNLOCK_HOST_DIRECT,
        .unlock_tool_hint = "gaster, not ipwndfu — CPID 0x7000 is absent "
                             "from ipwndfu's own checkm8 CPID list "
                             "(checked 2026-10-14), unverified which tool "
                             "actually covers it; don't assume ipwndfu works",
        .support = {
            .framebuffer = 1, .touch = 0, .wifi = 0, .storage = 0, .gpu = 0,
            .notes = "Mainline since Linux 6.13.",
        },
    },
    {
        .model_name = "iPhone 6s",
        .provenance = TROGNON_PROV_UPSTREAM,
        .provenance_date = "2026-10-13",
        .model_identifier = "iPhone8,1",
        .cpid = 0x8000,
        .soc = TROGNON_SOC_A9,
        .soc_codename = "S8000",
        .dt_platform = "s8000",
        .unlock_path = TROGNON_UNLOCK_HOST_DIRECT,
        .unlock_tool_hint = "gaster, not ipwndfu — CPID 0x8000 is absent "
                             "from ipwndfu's own checkm8 CPID list "
                             "(checked 2026-10-14), unverified which tool "
                             "actually covers it; don't assume ipwndfu works",
        .support = {
            .framebuffer = 1, .touch = 0, .wifi = 0, .storage = 0, .gpu = 0,
            .notes = "Mainline since Linux 6.13; brightness since 6.15.",
        },
    },
    {
        .model_name = "iPhone 7",
        .provenance = TROGNON_PROV_UPSTREAM,
        .provenance_date = "2026-10-13",
        .model_identifier = "iPhone9,1",
        .cpid = 0x8010,
        .soc = TROGNON_SOC_A10,
        .soc_codename = "T8010",
        .dt_platform = "t8010",
        .unlock_path = TROGNON_UNLOCK_HOST_DIRECT,
        .unlock_tool_hint = "ipwndfu -p (checkm8) — this CPID is in ipwndfu's own checkm8 dispatch list, confirmed 2026-10-14",
        .support = {
            .framebuffer = 1, .touch = 0, .wifi = 0, .storage = 0, .gpu = 0,
            .notes = "Mainline since Linux 6.13. First Linux boot via "
                     "pongoOS reported January 2020 (checkra1n era).",
        },
    },
    {
        .model_name = "iPhone 8",
        .provenance = TROGNON_PROV_UPSTREAM,
        .provenance_date = "2026-10-13",
        .model_identifier = "iPhone10,1",
        .cpid = 0x8015,
        .soc = TROGNON_SOC_A11,
        .soc_codename = "T8015",
        .dt_platform = "t8015",
        .unlock_path = TROGNON_UNLOCK_HOST_DIRECT,
        .unlock_tool_hint = "ipwndfu -p (checkm8) — this CPID is in ipwndfu's own checkm8 dispatch list, confirmed 2026-10-14",
        .support = {
            .framebuffer = 1, .touch = 0, .wifi = 0, .storage = 1, .gpu = 0,
            .notes = "Internal NVMe storage mainline since Linux 6.18 "
                     "(A11 only). Brightness since 6.15.",
        },
    },
    {
        .model_name = "iPhone X",
        .provenance = TROGNON_PROV_UPSTREAM,
        .provenance_date = "2026-10-13",
        .model_identifier = "iPhone10,3",
        .cpid = 0x8015,
        .soc = TROGNON_SOC_A11,
        .soc_codename = "T8015",
        .dt_platform = "t8015",
        .unlock_path = TROGNON_UNLOCK_HOST_DIRECT,
        .unlock_tool_hint = "ipwndfu -p (checkm8) — this CPID is in ipwndfu's own checkm8 dispatch list, confirmed 2026-10-14",
        .support = {
            .framebuffer = 1, .touch = 0, .wifi = 0, .storage = 1, .gpu = 0,
            .notes = "Same T8015 as iPhone 8; brightness control not "
                     "being worked on for this model specifically.",
        },
    },
};

const size_t trognon_device_catalog_count =
    sizeof(trognon_device_catalog) / sizeof(trognon_device_catalog[0]);

const struct trognon_device *
trognon_device_find_by_identifier(const char *model_identifier)
{
    if (!model_identifier)
        return NULL;

    for (size_t i = 0; i < trognon_device_catalog_count; i++) {
        if (strcmp(trognon_device_catalog[i].model_identifier, model_identifier) == 0)
            return &trognon_device_catalog[i];
    }
    return NULL;
}

const struct trognon_device *
trognon_device_find_by_cpid(uint32_t cpid)
{
    if (cpid == 0)
        return NULL;

    for (size_t i = 0; i < trognon_device_catalog_count; i++) {
        if (trognon_device_catalog[i].cpid == cpid)
            return &trognon_device_catalog[i];
    }
    return NULL;
}
