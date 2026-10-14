/* SPDX-License-Identifier: MIT */
/*
 * Static device catalog. Read-only, no bus access.
 *
 * Support fields reflect docs/research/2026-10-13-*.md (surveyed
 * 2026-10-13). "notes" points to the upstream/downstream status, not a
 * guarantee — re-check dates before relying on this in code.
 */
#include <string.h>
#include <stddef.h>

#include "trognon/device.h"

const struct trognon_device trognon_device_catalog[] = {
    {
        .model_name = "iPhone 3GS",
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
        .support = {
            .framebuffer = 0, .touch = 0, .wifi = 0, .storage = 0, .gpu = 0,
            .notes = "Oldest family in this catalog. No modern OS port "
                     "effort; boot-chain test device only, like the A4.",
        },
    },
    {
        .model_name = "iPhone 4",
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
        .unlock_path = TROGNON_UNLOCK_HOST_DIRECT, /* limera1n (2010),
                                                        runs from a normal
                                                        host via USB control
                                                        transfers, no helper
                                                        board — unlike A5. */
        .support = {
            .framebuffer = 0, .touch = 0, .wifi = 0, .storage = 0, .gpu = 0,
            .notes = "No modern OS port; useful as a boot-chain test device "
                     "(limera1n/redsn0w-era tooling), not a realistic Linux "
                     "target today.",
        },
    },
    {
        .model_name = "iPhone 4S",
        .model_identifier = "iPhone4,1",
        .cpid = 0x8940,
        .soc = TROGNON_SOC_A5,
        .soc_codename = "S5L8940",
        .dt_platform = NULL, /* no mainline DT; closest base is Project
                                 Cascadia (S5L8942, iPad mini 1) */
        .unlock_path = TROGNON_UNLOCK_HELPER_BOARD,
        .support = {
            .framebuffer = 0, .touch = 0, .wifi = 0, .storage = 0, .gpu = 0,
            .notes = "No port exists yet; nearest work is Project Cascadia "
                     "on the sibling S5L8942 SoC.",
        },
    },
    {
        .model_name = "iPhone 5",
        .model_identifier = "iPhone5,1",
        .cpid = 0x8950,
        .soc = TROGNON_SOC_A6,
        .soc_codename = "S5L8950",
        .dt_platform = NULL,
        .unlock_path = TROGNON_UNLOCK_HOST_DIRECT,
        .support = {
            .framebuffer = 0, .touch = 0, .wifi = 0, .storage = 0, .gpu = 0,
            .notes = "checkm8 works over plain USB (ipwndfu); no OS port found.",
        },
    },
    {
        .model_name = "iPhone 5s",
        .model_identifier = "iPhone6,1",
        .cpid = 0x8960,
        .soc = TROGNON_SOC_A7,
        .soc_codename = "S5L8960X",
        .dt_platform = "s5l8960x",
        .unlock_path = TROGNON_UNLOCK_HOST_DIRECT,
        .support = {
            .framebuffer = 1, .touch = 0, .wifi = 0, .storage = 0, .gpu = 0,
            .notes = "Mainline since Linux 6.13 (framebuffer, buttons).",
        },
    },
    {
        .model_name = "iPhone 6",
        .model_identifier = "iPhone7,2",
        .cpid = 0x7000,
        .soc = TROGNON_SOC_A8,
        .soc_codename = "T7000",
        .dt_platform = "t7000",
        .unlock_path = TROGNON_UNLOCK_HOST_DIRECT,
        .support = {
            .framebuffer = 1, .touch = 0, .wifi = 0, .storage = 0, .gpu = 0,
            .notes = "Mainline since Linux 6.13.",
        },
    },
    {
        .model_name = "iPhone 6s",
        .model_identifier = "iPhone8,1",
        .cpid = 0x8000,
        .soc = TROGNON_SOC_A9,
        .soc_codename = "S8000",
        .dt_platform = "s8000",
        .unlock_path = TROGNON_UNLOCK_HOST_DIRECT,
        .support = {
            .framebuffer = 1, .touch = 0, .wifi = 0, .storage = 0, .gpu = 0,
            .notes = "Mainline since Linux 6.13; brightness since 6.15.",
        },
    },
    {
        .model_name = "iPhone 7",
        .model_identifier = "iPhone9,1",
        .cpid = 0x8010,
        .soc = TROGNON_SOC_A10,
        .soc_codename = "T8010",
        .dt_platform = "t8010",
        .unlock_path = TROGNON_UNLOCK_HOST_DIRECT,
        .support = {
            .framebuffer = 1, .touch = 0, .wifi = 0, .storage = 0, .gpu = 0,
            .notes = "Mainline since Linux 6.13. First Linux boot via "
                     "pongoOS reported January 2020 (checkra1n era).",
        },
    },
    {
        .model_name = "iPhone 8",
        .model_identifier = "iPhone10,1",
        .cpid = 0x8015,
        .soc = TROGNON_SOC_A11,
        .soc_codename = "T8015",
        .dt_platform = "t8015",
        .unlock_path = TROGNON_UNLOCK_HOST_DIRECT,
        .support = {
            .framebuffer = 1, .touch = 0, .wifi = 0, .storage = 1, .gpu = 0,
            .notes = "Internal NVMe storage mainline since Linux 6.18 "
                     "(A11 only). Brightness since 6.15.",
        },
    },
    {
        .model_name = "iPhone X",
        .model_identifier = "iPhone10,3",
        .cpid = 0x8015,
        .soc = TROGNON_SOC_A11,
        .soc_codename = "T8015",
        .dt_platform = "t8015",
        .unlock_path = TROGNON_UNLOCK_HOST_DIRECT,
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
