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

/* Outcomes of fill_result(). Never fails the whole scan, but the caller counts the
 * devices it could not read so "nothing found" is not confused with "found but unreadable". */
enum fill_outcome { FILL_OK = 1, FILL_SKIP = 0, FILL_DESC_FAILED = -1, FILL_OPEN_FAILED = -2 };

/* Fills `result` from one already-opened handle/descriptor. */
static int
fill_result(libusb_device *dev, struct trognon_probe_result *result, int *open_error)
{
    struct libusb_device_descriptor desc;
    if (libusb_get_device_descriptor(dev, &desc) < 0)
        return FILL_DESC_FAILED;
    if (desc.idVendor != TROGNON_APPLE_VENDOR_ID)
        return FILL_SKIP;

    libusb_device_handle *handle = NULL;
    int orc = libusb_open(dev, &handle);
    if (orc < 0) {
        if (open_error)
            *open_error = orc;
        return FILL_OPEN_FAILED;
    }

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

    struct trognon_serial_info info = trognon_serial_parse(result->serial, desc.idProduct);
    result->state = info.state;
    result->cpid = info.cpid;
    if (result->cpid)
        result->device = trognon_device_find_by_cpid(result->cpid);
    /* In normal mode there is nothing in the descriptors to identify the
     * model from — result->device stays NULL. See usb.h. */

    libusb_close(handle);
    return FILL_OK;
}

int
trognon_usb_probe_all(struct trognon_probe_result *results, size_t max, size_t *count)
{
    return trognon_usb_probe_all_ex(results, max, count, NULL);
}

int
trognon_usb_probe_all_ex(struct trognon_probe_result *results, size_t max, size_t *count,
                         struct trognon_probe_stats *stats)
{
    if (stats)
        memset(stats, 0, sizeof(*stats));
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
        struct trognon_probe_result candidate;
        int open_error = 0;
        int outcome = fill_result(list[i], &candidate, &open_error);
        if (stats) {
            if (outcome == FILL_DESC_FAILED)
                stats->descriptor_failed++;
            else if (outcome == FILL_OPEN_FAILED) {
                stats->apple_seen++;
                stats->open_failed++;
                stats->last_open_error = open_error;
            } else if (outcome == FILL_OK)
                stats->apple_seen++;
        }
        if (outcome != FILL_OK)
            continue;

        /* macOS's IOKit backend can report the same physical device more
         * than once, in a non-deterministic order, as several distinct
         * libusb_device entries sharing one serial number — observed
         * 2026-10-14 (one iPhone 3GS showed up 3 times) and documented
         * for other Apple USB devices: see
         * https://github.com/LedgerHQ/ledgercomm/pull/6. Deduplicate by
         * non-empty serial before keeping a result; an empty serial (seen
         * if the string descriptor read failed) is never deduplicated
         * against, since we can't tell devices apart in that case. */
        int is_duplicate = 0;
        if (candidate.serial[0]) {
            for (size_t j = 0; j < found; j++) {
                if (strcmp(results[j].serial, candidate.serial) == 0) {
                    is_duplicate = 1;
                    break;
                }
            }
        }
        if (is_duplicate)
            continue;

        results[found++] = candidate;
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
