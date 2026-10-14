/* SPDX-License-Identifier: MIT */
/* Pure planning logic — no I/O, no subprocess. See include/trognon/chain.h. */
#include "trognon/chain.h"

struct trognon_chain_plan
trognon_chain_plan(const struct trognon_probe_result *probe)
{
    struct trognon_chain_plan plan = { TROGNON_CHAIN_NOTHING_TO_DO, NULL, NULL };

    if (!probe || !probe->found) {
        plan.reason = "no device";
        return plan;
    }

    switch (probe->state) {
    case TROGNON_STATE_DFU_PWNED:
        plan.action = TROGNON_CHAIN_ALREADY_UNLOCKED;
        plan.reason = "PWND marker present — already in unlocked DFU";
        break;

    case TROGNON_STATE_DFU:
        if (probe->device) {
            plan.action = TROGNON_CHAIN_RUN_UNLOCK_TOOL;
            plan.tool_hint = probe->device->unlock_tool_hint;
            plan.reason = "locked DFU, device identified by CPID";
        } else {
            plan.action = TROGNON_CHAIN_NOTHING_TO_DO;
            plan.reason = "locked DFU, but CPID is not in trognon's "
                           "device catalog — nothing to recommend";
        }
        break;

    case TROGNON_STATE_RECOVERY:
    case TROGNON_STATE_NORMAL:
        plan.action = TROGNON_CHAIN_ENTER_DFU;
        plan.reason = "device must be put into DFU before an unlock tool "
                       "can run";
        break;

    case TROGNON_STATE_UNKNOWN:
    default:
        plan.reason = "bus state not recognized";
        break;
    }

    return plan;
}
