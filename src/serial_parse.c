/* SPDX-License-Identifier: MIT */
/* Pure parsing — no I/O, no libusb. See include/trognon/serial.h. */
#include <stdlib.h>
#include <string.h>

#include "trognon/serial.h"

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

    info.cpid = (uint32_t)strtoul(cpid_field + strlen("CPID:"), NULL, 16);

    if (strstr(serial, "PWND:")) {
        info.state = TROGNON_STATE_DFU_PWNED;
        return info;
    }

    /* Recovery and DFU carry an identical-looking CPID field; they are
     * told apart by the USB product id, not the string. */
    if (product_id == TROGNON_PID_RECOVERY || product_id == TROGNON_PID_RECOVERY_ALT)
        info.state = TROGNON_STATE_RECOVERY;
    else if (product_id == TROGNON_PID_DFU)
        info.state = TROGNON_STATE_DFU;
    else
        info.state = TROGNON_STATE_UNKNOWN; /* CPID present but an
                                                 unrecognized PID — don't
                                                 guess */

    return info;
}
