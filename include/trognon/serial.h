/* SPDX-License-Identifier: MIT */
/*
 * Pure parsing of Apple's Recovery/DFU USB serial string descriptor.
 * No I/O, no libusb dependency — testable with fixture strings and no
 * hardware attached. src/usb_probe.c calls into this; tests/test_serial.c
 * exercises it directly.
 *
 * Format, confirmed against a real forum capture (CPID:8920 ... IBFL:01
 * SRNM:[...] IMEI:[...], iPhone 3GS era) and theapplewiki.com's USB
 * Product IDs page:
 *   "CPID:8930 CPRV:20 CPFM:03 SCEP:01 BDID:00 ECID:0123456789ABCDEF
 *    IBFL:1B SRNM:[...]"
 * and, once an unlock step has run:
 *   "... PWND:[somepwnedstring]"
 *
 * Recovery and DFU both carry CPID and look identical in the serial
 * string itself. They are told apart by the USB *product id*, not
 * anything in the string — see TROGNON_PID_DFU / TROGNON_PID_RECOVERY
 * below (sources: https://www.theiphonewiki.com/wiki/WTF_0x1227,
 * https://oemdrivers.com/devid/usb-vid_05ac-pid_1281). An earlier version
 * of this parser tried to use the IBFL field for that distinction and a
 * fabricated "SRTG:" field name — both unverified guesses, replaced
 * 2026-10-14 once the real field names and PIDs were confirmed.
 */
#ifndef TROGNON_SERIAL_H
#define TROGNON_SERIAL_H

#include <stdint.h>

enum trognon_bus_state {
    TROGNON_STATE_UNKNOWN = 0,
    TROGNON_STATE_NORMAL,     /* booted iOS, plain serial number, no CPID */
    TROGNON_STATE_RECOVERY,   /* iBoot recovery mode, USB PID 0x1281/0x1280 */
    TROGNON_STATE_DFU,        /* standard DFU, USB PID 0x1227, SecureROM
                                  signature checks active */
    TROGNON_STATE_DFU_PWNED,  /* DFU after an unlock step ran (PWND: present) */
};

/* Apple's USB product IDs for iBoot Recovery and SecureROM DFU.
 * Confirmed: https://www.theiphonewiki.com/wiki/WTF_0x1227,
 * https://oemdrivers.com/devid/usb-vid_05ac-pid_1281. Some older devices
 * use 0x1280 for a Recovery composite interface; not yet confirmed which
 * devices in this catalog do, so it's listed but not relied upon. */
#define TROGNON_PID_DFU            0x1227
#define TROGNON_PID_RECOVERY       0x1281
#define TROGNON_PID_RECOVERY_ALT   0x1280

struct trognon_serial_info {
    enum trognon_bus_state state;
    uint32_t cpid; /* 0 if not present (normal mode, or unparsable) */
};

/* Parses a NUL-terminated serial string as read off the USB bus, combined
 * with the device's USB product id (needed to tell Recovery apart from
 * DFU — see above). Never fails on malformed input: an empty or
 * unrecognized string yields TROGNON_STATE_UNKNOWN / cpid=0 rather than an
 * error, since the real caller (usb_probe) can't do anything about a bad
 * descriptor anyway. */
struct trognon_serial_info trognon_serial_parse(const char *serial, uint16_t product_id);

#endif /* TROGNON_SERIAL_H */
