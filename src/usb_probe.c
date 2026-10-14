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

/* Fills `result` from one already-opened handle/descriptor. Returns 1 if
 * it's an Apple device worth reporting, 0 to skip (not Apple, or we
 * couldn't open it — e.g. permissions). Never fails the whole scan: a
 * device we can't read is just absent from the results, not an error. */
static int
fill_result(libusb_device *dev, struct trognon_probe_result *result)
{
    struct libusb_device_descriptor desc;
    if (libusb_get_device_descriptor(dev, &desc) < 0)
        return 0;
    if (desc.idVendor != TROGNON_APPLE_VENDOR_ID)
        return 0;

    libusb_device_handle *handle = NULL;
    if (libusb_open(dev, &handle) < 0)
        return 0;

    memset(result, 0, sizeof(*result));
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
    /* In normal mode there is nothing in the descriptors to identify the
     * model from — result->device stays NULL. See usb.h. */

    libusb_close(handle);
    return 1;
}

int
trognon_usb_probe_all(struct trognon_probe_result *results, size_t max, size_t *count)
{
    if (count)
        *count = 0;
    if (!results || max == 0)
        return -1;

    libusb_context *ctx = NULL;
    int rc = libusb_init(&ctx);
    if (rc < 0)
        return rc;

    libusb_device **list = NULL;
    ssize_t n = libusb_get_device_list(ctx, &list);
    if (n < 0) {
        libusb_exit(ctx);
        return (int)n;
    }

    size_t found = 0;
    for (ssize_t i = 0; i < n && found < max; i++) {
        if (fill_result(list[i], &results[found]))
            found++;
    }

    if (count)
        *count = found;

    libusb_free_device_list(list, 1);
    libusb_exit(ctx);
    return 0;
}

int
trognon_usb_probe(struct trognon_probe_result *result)
{
    if (!result)
        return -1;
    memset(result, 0, sizeof(*result));

    size_t count = 0;
    int rc = trognon_usb_probe_all(result, 1, &count);
    if (rc < 0)
        return rc;
    /* result->found stays 0 if count == 0 — fill_result already zeroed
     * nothing was written to `result` in that case, memset above covers it. */
    return 0;
}
