/* SPDX-License-Identifier: MIT */
/*
 * Trognon — read-only USB identification.
 *
 * Everything here is passive: enumerate what's on the bus and read standard
 * descriptors/strings. Nothing writes to a device, claims an interface for
 * control transfers beyond what descriptor reads need, or sends a payload.
 * The unlock/boot stages (trognon-unlock, trognon-boot) are separate
 * binaries layered on top — see docs/architecture.md.
 */
#ifndef TROGNON_USB_H
#define TROGNON_USB_H

#include <stdint.h>

#include "trognon/device.h"
#include "trognon/serial.h" /* enum trognon_bus_state lives there — pure
                                parsing, no libusb, testable without
                                hardware. See serial.h for the format. */

/* Apple's USB vendor id, for filtering enumeration. Public, well-known
 * constant — not a secret, not an exploit. */
#define TROGNON_APPLE_VENDOR_ID 0x05ac

struct trognon_probe_result {
    int found;
    uint16_t vendor_id;
    uint16_t product_id;
    enum trognon_bus_state state;
    char serial[64];   /* raw USB serial string descriptor, truncated */
    uint32_t cpid;      /* parsed from "CPID:xxxx" in the serial string;
                            0 if the device is in normal mode (no CPID
                            field there) or state is unknown */
    const struct trognon_device *device; /* looked up by cpid; NULL if
                                             unknown or in normal mode */
};

/* Opens libusb, enumerates devices with TROGNON_APPLE_VENDOR_ID, reads
 * string descriptors for the first match. Read-only: no control transfer
 * beyond GET_DESCRIPTOR. Returns 0 on success (result->found tells you
 * whether anything was there), negative libusb error code otherwise. */
int trognon_usb_probe(struct trognon_probe_result *result);

#endif /* TROGNON_USB_H */
