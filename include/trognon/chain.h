/* SPDX-License-Identifier: MIT */
/*
 * Unlock orchestration — planning only, for now.
 *
 * trognon_chain_plan() is a pure function: given what trognon_usb_probe()
 * already found, it decides what *should* happen next and says so in
 * plain text. It does not fork/exec anything. See docs/architecture.md:
 * the step that actually drives ipwndfu/gaster/checkm8-a5 is a later,
 * separate addition, once each tool's real command-line syntax has been
 * confirmed on real hardware — fabricating argv for an unverified tool
 * invocation would be worse than not automating it yet.
 */
#ifndef TROGNON_CHAIN_H
#define TROGNON_CHAIN_H

#include "trognon/usb.h"

enum trognon_chain_action {
    TROGNON_CHAIN_NOTHING_TO_DO = 0,  /* not a device we can act on right now */
    TROGNON_CHAIN_ENTER_DFU,          /* device is in Recovery or normal mode;
                                          user must put it in DFU first */
    TROGNON_CHAIN_RUN_UNLOCK_TOOL,    /* device is in locked DFU; an external
                                          tool (see tool_hint) is the next
                                          step — not invoked by this build */
    TROGNON_CHAIN_ALREADY_UNLOCKED,   /* PWND marker present */
};

struct trognon_chain_plan {
    enum trognon_chain_action action;
    const char *tool_hint; /* NULL unless action == RUN_UNLOCK_TOOL */
    const char *reason;    /* one-line human explanation */
};

/* Pure: no I/O, no subprocess. Safe to call with a probe result that came
 * from a fixture in a test, not just from real hardware. */
struct trognon_chain_plan trognon_chain_plan(const struct trognon_probe_result *probe);

#endif /* TROGNON_CHAIN_H */
