/* SPDX-License-Identifier: MIT */
/*
 * Pure parsing of Apple's Recovery/DFU USB serial string descriptor.
 * No I/O, no libusb dependency — testable with fixture strings and no
 * hardware attached. src/usb_probe.c calls into this; tests/test_serial.c
 * exercises it directly.
 *
 * Format (Recovery/DFU only — normal mode carries no such fields):
 *   "CPID:8930 CPRV:20 CPFM:03 SCEP:01 BDID:00 ECID:0123456789ABCDEF
 *    IBFL:1B SRTG:[iBoot-359.3.2]"
 * and, once an unlock step has run:
 *   "... PWND:[somepwnedstring]"
 */
#ifndef TROGNON_SERIAL_H
#define TROGNON_SERIAL_H

#include <stdint.h>

enum trognon_bus_state {
    TROGNON_STATE_UNKNOWN = 0,
    TROGNON_STATE_NORMAL,     /* booted iOS, plain serial number, no CPID */
    TROGNON_STATE_RECOVERY,   /* iBoot recovery mode */
    TROGNON_STATE_DFU,        /* standard DFU, SecureROM signature checks active */
    TROGNON_STATE_DFU_PWNED,  /* DFU after an unlock step ran (PWND: present) */
};

struct trognon_serial_info {
    enum trognon_bus_state state;
    uint32_t cpid; /* 0 if not present (normal mode, or unparsable) */
};

/* Parses a NUL-terminated serial string as read off the USB bus. Never
 * fails on malformed input: an empty or unrecognized string yields
 * TROGNON_STATE_UNKNOWN / cpid=0 rather than an error, since the real
 * caller (usb_probe) can't do anything about a bad descriptor anyway. */
struct trognon_serial_info trognon_serial_parse(const char *serial);

#endif /* TROGNON_SERIAL_H */
