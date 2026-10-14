/* SPDX-License-Identifier: MIT */
/*
 * Read-only USB identification: enumerate, read descriptors, parse the
 * serial string. No writes, no control transfers beyond standard
 * GET_DESCRIPTOR. See include/trognon/usb.h for the scope boundary.
 *
 * Important: in Recovery/DFU mode, Apple's serial string descriptor looks
 * like:
 *   "CPID:8930 CPRV:20 CPFM:03 SCEP:01 BDID:00 ECID:... IBFL:1B SRTG:[...]"
 * It does NOT contain a literal model identifier such as "iPhone4,1" — the
 * only reliable field is CPID (hex). In normal mode (booted iOS), the
 * serial string is just the device's actual serial number and carries no
 * chip or model information at all. This was wrong in an earlier version
 * of this file (it searched for the substring "iPhone", which never
 * matches either case) — verified 2026-10-14 against a real iPhone 3GS.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libusb.h>

#include "trognon/usb.h"

static enum trognon_bus_state
classify_state(const char *serial, uint32_t *cpid_out)
{
    *cpid_out = 0;
    if (!serial || serial[0] == '\0')
        return TROGNON_STATE_UNKNOWN;

    const char *cpid_field = strstr(serial, "CPID:");
    if (!cpid_field)
        return TROGNON_STATE_NORMAL; /* booted iOS: plain serial number */

    *cpid_out = (uint32_t)strtoul(cpid_field + strlen("CPID:"), NULL, 16);

    if (strstr(serial, "PWND:"))
        return TROGNON_STATE_DFU_PWNED;

    /* Recovery and DFU both carry CPID; telling them apart needs another
     * field (IBFL's DFU bit) we don't parse yet. Report DFU for now —
     * refine once we have samples of both from a real device. */
    return TROGNON_STATE_DFU;
}

int
trognon_usb_probe(struct trognon_probe_result *result)
{
    if (!result)
        return -1;
    memset(result, 0, sizeof(*result));

    libusb_context *ctx = NULL;
    int rc = libusb_init(&ctx);
    if (rc < 0)
        return rc;

    libusb_device **list = NULL;
    ssize_t count = libusb_get_device_list(ctx, &list);
    if (count < 0) {
        libusb_exit(ctx);
        return (int)count;
    }

    for (ssize_t i = 0; i < count; i++) {
        struct libusb_device_descriptor desc;
        rc = libusb_get_device_descriptor(list[i], &desc);
        if (rc < 0 || desc.idVendor != TROGNON_APPLE_VENDOR_ID)
            continue;

        libusb_device_handle *handle = NULL;
        rc = libusb_open(list[i], &handle);
        if (rc < 0)
            continue; /* e.g. permissions — not fatal, try the next device */

        result->found = 1;
        result->vendor_id = desc.idVendor;
        result->product_id = desc.idProduct;

        if (desc.iSerialNumber) {
            unsigned char buf[256];
            int n = libusb_get_string_descriptor_ascii(
                handle, desc.iSerialNumber, buf, (int)sizeof(buf) - 1);
            if (n > 0) {
                buf[n] = '\0';
                strncpy(result->serial, (char *)buf, sizeof(result->serial) - 1);
            }
        }

        uint32_t cpid = 0;
        result->state = classify_state(result->serial, &cpid);
        result->cpid = cpid;

        if (cpid)
            result->device = trognon_device_find_by_cpid(cpid);
        /* In normal mode there is nothing in the descriptors to identify
         * the model from — result->device stays NULL. A future version
         * could maintain a separate table of normal-mode product ids
         * (result->product_id), but that's a distinct, less reliable
         * identification path and not implemented here. */

        libusb_close(handle);
        break; /* first match only, for now */
    }

    libusb_free_device_list(list, 1);
    libusb_exit(ctx);
    return 0;
}
