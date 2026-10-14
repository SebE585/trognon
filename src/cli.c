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

static int
cmd_probe(void)
{
    struct trognon_probe_result result;
    int rc = trognon_usb_probe(&result);
    if (rc < 0) {
        fprintf(stderr, "trognon: USB probe failed (libusb error %d)\n", rc);
        return 1;
    }
    if (!result.found) {
        printf("No Apple USB device found.\n");
        return 0;
    }

    printf("Found Apple device: vendor=%04x product=%04x state=%s\n",
           result.vendor_id, result.product_id, state_name(result.state));
    if (result.serial[0])
        printf("  serial string: %s\n", result.serial);
    if (result.cpid)
        printf("  CPID: 0x%04x\n", result.cpid);

    if (result.device) {
        printf("  identified as: %s (%s)\n",
               result.device->model_name, result.device->model_identifier);
        printf("  SoC: %s, unlock path: %s\n",
               result.device->soc_codename,
               result.device->unlock_path == TROGNON_UNLOCK_HELPER_BOARD
                   ? "needs a USB-host helper board (Pico/Arduino)"
                   : "host-direct (ipwndfu/gaster)");
        printf("  support notes: %s\n", result.device->support.notes);
    } else {
        printf("  not in trognon's device catalog yet.\n");
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
        fprintf(stderr, "usage: %s {probe|list-devices}\n", argv[0]);
        return 1;
    }
    if (strcmp(argv[1], "probe") == 0)
        return cmd_probe();
    if (strcmp(argv[1], "list-devices") == 0)
        return cmd_list_devices();

    fprintf(stderr, "trognon: unknown command '%s'\n", argv[1]);
    return 1;
}
