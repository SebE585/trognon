/* SPDX-License-Identifier: MIT */
/* Pure parsing — no I/O, no libusb. See include/trognon/serial.h. */
#include <stdlib.h>
#include <string.h>

#include "trognon/serial.h"

struct trognon_serial_info
trognon_serial_parse(const char *serial)
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

    /* Recovery and DFU both carry CPID; telling them apart needs the IBFL
     * field's DFU bit, which we don't decode yet (see "Not verified" in
     * tests/test_serial.c). Report DFU for now. */
    info.state = TROGNON_STATE_DFU;
    return info;
}
