/* SPDX-License-Identifier: MIT */
/*
 * trognon — identification-only CLI.
 *
 * `trognon probe` enumerates Apple USB devices and reports what it sees.
 * It never writes to the device. Unlock/boot are separate, not-yet-written
 * binaries (trognon-unlock, trognon-boot) — see docs/architecture.md.
 */
#include <stdio.h>
#include <string.h>

#include "trognon/chain.h"
#include "trognon/device.h"
#include "trognon/usb.h"

static const char *
state_name(enum trognon_bus_state state)
{
    switch (state) {
    case TROGNON_STATE_NORMAL:    return "normal (booted OS)";
    case TROGNON_STATE_RECOVERY:  return "recovery";
    case TROGNON_STATE_DFU:       return "DFU (locked, SecureROM checks active)";
    case TROGNON_STATE_DFU_PWNED: return "DFU, already unlocked (PWND marker present)";
    default:                      return "unknown";
    }
}

#define TROGNON_MAX_DEVICES 16

static void
print_one(const struct trognon_probe_result *result, int index)
{
    printf("[%d] vendor=%04x product=%04x state=%s\n",
           index, result->vendor_id, result->product_id, state_name(result->state));
    if (result->serial[0])
        printf("    serial string: %s\n", result->serial);
    if (result->cpid)
        printf("    CPID: 0x%04x\n", result->cpid);

    if (result->device) {
        printf("    identified as: %s (%s)\n",
               result->device->model_name, result->device->model_identifier);
        printf("    SoC: %s, unlock path: %s\n",
               result->device->soc_codename,
               result->device->unlock_path == TROGNON_UNLOCK_HELPER_BOARD
                   ? "needs a USB-host helper board (Pico/Arduino)"
                   : "host-direct (ipwndfu/gaster)");
        printf("    support notes: %s\n", result->device->support.notes);
    } else {
        printf("    not in trognon's device catalog yet.\n");
    }
}

static int
cmd_probe(void)
{
    struct trognon_probe_result results[TROGNON_MAX_DEVICES];
    size_t count = 0;
    int rc = trognon_usb_probe_all(results, TROGNON_MAX_DEVICES, &count);
    if (rc < 0) {
        fprintf(stderr, "trognon: USB probe failed (libusb error %d)\n", rc);
        return 1;
    }
    if (count == 0) {
        printf("No Apple USB device found.\n");
        return 0;
    }

    printf("Found %zu Apple device(s):\n", count);
    for (size_t i = 0; i < count; i++)
        print_one(&results[i], (int)i);
    return 0;
}

static const char *
chain_action_name(enum trognon_chain_action action)
{
    switch (action) {
    case TROGNON_CHAIN_ENTER_DFU:        return "put the device in DFU";
    case TROGNON_CHAIN_RUN_UNLOCK_TOOL:  return "run an external unlock tool";
    case TROGNON_CHAIN_ALREADY_UNLOCKED: return "nothing — already unlocked";
    case TROGNON_CHAIN_NOTHING_TO_DO:
    default:                             return "nothing to do";
    }
}

static int
cmd_unlock(void)
{
    struct trognon_probe_result results[TROGNON_MAX_DEVICES];
    size_t count = 0;
    int rc = trognon_usb_probe_all(results, TROGNON_MAX_DEVICES, &count);
    if (rc < 0) {
        fprintf(stderr, "trognon: USB probe failed (libusb error %d)\n", rc);
        return 1;
    }
    if (count == 0) {
        printf("No Apple USB device found.\n");
        return 0;
    }

    printf("trognon-unlock: planning only — no external tool is invoked by "
           "this build. See docs/architecture.md and docs/upstream-tools.md.\n\n");

    for (size_t i = 0; i < count; i++) {
        struct trognon_chain_plan plan = trognon_chain_plan(&results[i]);
        printf("[%zu] product=%04x state=%s\n",
               i, results[i].product_id, state_name(results[i].state));
        printf("    next step: %s (%s)\n",
               chain_action_name(plan.action), plan.reason);
        if (plan.tool_hint)
            printf("    recommended tool: %s\n", plan.tool_hint);
    }
    return 0;
}

static int
cmd_list_devices(void)
{
    for (size_t i = 0; i < trognon_device_catalog_count; i++) {
        const struct trognon_device *d = &trognon_device_catalog[i];
        printf("%-12s %-16s SoC=%-10s DT=%-10s %s\n",
               d->model_name, d->model_identifier, d->soc_codename,
               d->dt_platform ? d->dt_platform : "-",
               d->support.notes);
    }
    return 0;
}

int
main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s {probe|unlock|list-devices}\n", argv[0]);
        return 1;
    }
    if (strcmp(argv[1], "probe") == 0)
        return cmd_probe();
    if (strcmp(argv[1], "unlock") == 0)
        return cmd_unlock();
    if (strcmp(argv[1], "list-devices") == 0)
        return cmd_list_devices();

    fprintf(stderr, "trognon: unknown command '%s'\n", argv[1]);
    return 1;
}
