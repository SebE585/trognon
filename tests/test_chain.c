/* SPDX-License-Identifier: MIT */
/*
 * Fixture-based tests for the unlock-planning logic. No hardware required —
 * trognon_chain_plan() is pure, so a fabricated probe result exercises it
 * exactly like a real one would. See include/trognon/chain.h.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "trognon/chain.h"
#include "trognon/device.h"

int
main(void)
{
    const struct trognon_device *iphone4s = trognon_device_find_by_cpid(0x8940);
    assert(iphone4s != NULL);

    /* Locked DFU, identified device: recommend its tool hint. */
    {
        struct trognon_probe_result probe;
        memset(&probe, 0, sizeof(probe));
        probe.found = 1;
        probe.state = TROGNON_STATE_DFU;
        probe.cpid = 0x8940;
        probe.device = iphone4s;

        struct trognon_chain_plan plan = trognon_chain_plan(&probe);
        assert(plan.action == TROGNON_CHAIN_RUN_UNLOCK_TOOL);
        assert(plan.tool_hint == iphone4s->unlock_tool_hint);
    }

    /* Locked DFU, CPID not in the catalog: no guess. */
    {
        struct trognon_probe_result probe;
        memset(&probe, 0, sizeof(probe));
        probe.found = 1;
        probe.state = TROGNON_STATE_DFU;
        probe.cpid = 0xdead;
        probe.device = NULL;

        struct trognon_chain_plan plan = trognon_chain_plan(&probe);
        assert(plan.action == TROGNON_CHAIN_NOTHING_TO_DO);
        assert(plan.tool_hint == NULL);
    }

    /* Normal mode: user needs to put the device in DFU first. */
    {
        struct trognon_probe_result probe;
        memset(&probe, 0, sizeof(probe));
        probe.found = 1;
        probe.state = TROGNON_STATE_NORMAL;

        struct trognon_chain_plan plan = trognon_chain_plan(&probe);
        assert(plan.action == TROGNON_CHAIN_ENTER_DFU);
    }

    /* Recovery mode: same as normal, still needs DFU. */
    {
        struct trognon_probe_result probe;
        memset(&probe, 0, sizeof(probe));
        probe.found = 1;
        probe.state = TROGNON_STATE_RECOVERY;

        struct trognon_chain_plan plan = trognon_chain_plan(&probe);
        assert(plan.action == TROGNON_CHAIN_ENTER_DFU);
    }

    /* Already unlocked: say so, don't suggest running a tool again. */
    {
        struct trognon_probe_result probe;
        memset(&probe, 0, sizeof(probe));
        probe.found = 1;
        probe.state = TROGNON_STATE_DFU_PWNED;

        struct trognon_chain_plan plan = trognon_chain_plan(&probe);
        assert(plan.action == TROGNON_CHAIN_ALREADY_UNLOCKED);
    }

    /* No device found at all. */
    {
        struct trognon_probe_result probe;
        memset(&probe, 0, sizeof(probe));
        probe.found = 0;

        struct trognon_chain_plan plan = trognon_chain_plan(&probe);
        assert(plan.action == TROGNON_CHAIN_NOTHING_TO_DO);
    }

    printf("test_chain: all fixture checks passed\n");
    return 0;
}
