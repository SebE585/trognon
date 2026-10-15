/* SPDX-License-Identifier: MIT */
/* Pure parsing, no I/O, no libusb. See include/trognon/serial.h. */
#include <stdlib.h>
#include <string.h>

#include "trognon/serial.h"

/* Strict CPID: 1 to 8 hex digits right after "CPID:", then end of string or a
 * space. Anything else (empty, partial, non-hex, longer than 8 digits, glued to
 * trailing text) is rejected, never guessed. Returns 1 and sets *out on success. */
static int
parse_cpid(const char *p, uint32_t *out)
{
    uint32_t v = 0;
    int digits = 0;
    for (; digits < 8; digits++) {
        char c = p[digits];
        uint32_t d;
        if (c >= '0' && c <= '9')
            d = (uint32_t)(c - '0');
        else if (c >= 'a' && c <= 'f')
            d = (uint32_t)(c - 'a') + 10u;
        else if (c >= 'A' && c <= 'F')
            d = (uint32_t)(c - 'A') + 10u;
        else
            break;
        v = (v << 4) | d;
    }
    if (digits == 0)
        return 0;
    if (p[digits] != '\0' && p[digits] != ' ')
        return 0; /* trailing garbage, or a 9th hex digit */
    *out = v;
    return 1;
}

struct trognon_serial_info
trognon_serial_parse(const char *serial, uint16_t product_id)
{
    struct trognon_serial_info info = { TROGNON_STATE_UNKNOWN, 0 };

    if (!serial || serial[0] == '\0')
        return info;

    const char *cpid_field = strstr(serial, "CPID:");
    if (!cpid_field) {
        info.state = TROGNON_STATE_NORMAL; /* booted iOS: plain serial number */
        return info;
    }

    uint32_t cpid;
    if (!parse_cpid(cpid_field + strlen("CPID:"), &cpid))
        return info; /* CPID present but malformed: UNKNOWN, cpid 0 */
    info.cpid = cpid; /* kept even when the mode cannot be classified below */

    /* The USB product id decides the mode, always. The string only refines DFU:
     * a PWND: marker means "pwned" only on the DFU product id. On any other PID
     * the same marker is unexpected and is not trusted. */
    if (product_id == TROGNON_PID_DFU) {
        info.state = strstr(serial, "PWND:") ? TROGNON_STATE_DFU_PWNED : TROGNON_STATE_DFU;
    } else if (product_id == TROGNON_PID_RECOVERY || product_id == TROGNON_PID_RECOVERY_ALT) {
        info.state = strstr(serial, "PWND:") ? TROGNON_STATE_UNKNOWN : TROGNON_STATE_RECOVERY;
    } /* else: CPID present but an unrecognized PID: state stays UNKNOWN, don't guess */
    return info;
}
