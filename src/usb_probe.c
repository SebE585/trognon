/* SPDX-License-Identifier: MIT */
/*
 * Read-only USB identification: enumerate, read descriptors, hand the
 * serial string to trognon_serial_parse() (src/serial_parse.c — pure,
 * testable without hardware). No writes, no control transfers beyond
 * standard GET_DESCRIPTOR. See include/trognon/usb.h for the scope
 * boundary and include/trognon/serial.h for the string format.
 */
#include <string.h>

#include <libusb.h>

#include "trognon/usb.h"

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

        struct trognon_serial_info info = trognon_serial_parse(result->serial);
        result->state = info.state;
        result->cpid = info.cpid;

        if (result->cpid)
            result->device = trognon_device_find_by_cpid(result->cpid);
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
