/* SPDX-License-Identifier: MIT */
/*
 * Read-only USB identification: enumerate, read descriptors, parse the
 * serial string. No writes, no control transfers beyond standard
 * GET_DESCRIPTOR. See include/trognon/usb.h for the scope boundary.
 */
#include <stdio.h>
#include <string.h>

#include <libusb.h>

#include "trognon/usb.h"

/* iOS/iBoot serial string descriptors look like:
 *   "CPID:8940 CPRV:11 CPFM:03 SCEP:01 BDID:00 ECID:... IBFL:03 ... PWND:[...]"
 * (DFU/recovery) or just the model identifier in normal mode. We only parse
 * what's needed to classify state and, if present, the PWND marker — we
 * never set it ourselves. */
static enum trognon_bus_state
classify_state(const char *serial)
{
    if (!serial || serial[0] == '\0')
        return TROGNON_STATE_UNKNOWN;
    if (strstr(serial, "PWND:"))
        return TROGNON_STATE_DFU_PWNED;
    if (strstr(serial, "CPID:"))
        return TROGNON_STATE_DFU; /* also covers plain recovery strings;
                                      refined later once we have real
                                      descriptor samples to test against */
    return TROGNON_STATE_NORMAL;
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

        result->state = classify_state(result->serial);

        /* Best-effort extraction of "iPhoneX,Y" out of the serial string,
         * if present. Normal-mode serials don't carry it; this is a
         * placeholder until we have real device samples. */
        const char *model = strstr(result->serial, "iPhone");
        if (model) {
            strncpy(result->model_identifier, model,
                    sizeof(result->model_identifier) - 1);
        }

        if (result->model_identifier[0])
            result->device = trognon_device_find_by_identifier(result->model_identifier);

        libusb_close(handle);
        break; /* first match only, for now */
    }

    libusb_free_device_list(list, 1);
    libusb_exit(ctx);
    return 0;
}
