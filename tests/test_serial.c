/* SPDX-License-Identifier: MIT */
/*
 * Fixture-based tests for the serial string parser. No hardware required —
 * written while waiting for a real device to finish charging (2026-10-14),
 * precisely so progress doesn't depend on having a device plugged in.
 *
 * Fixture strings are hand-built from documented formats in
 * docs/research/, not captured from real hardware yet. Replace with real
 * captures as devices become available (flagged in "Not verified" below).
 */
#include <assert.h>
#include <stdio.h>

#include "trognon/serial.h"

int
main(void)
{
    /* Normal mode: plain serial number, no CPID field. Matches what we
     * actually read off a real (booted) iPhone 3GS on 2026-10-14:
     * "DLC335502MRKD56AC". */
    {
        struct trognon_serial_info info = trognon_serial_parse("DLC335502MRKD56AC");
        assert(info.state == TROGNON_STATE_NORMAL);
        assert(info.cpid == 0);
    }

    /* Recovery/DFU, iPhone 4 (A4, S5L8930) — field layout per
     * docs/architecture.md and docs/research/2026-10-14-pre-a5-limera1n.md.
     * Not yet verified against real hardware. */
    {
        struct trognon_serial_info info = trognon_serial_parse(
            "CPID:8930 CPRV:20 CPFM:03 SCEP:01 BDID:00 ECID:0123456789ABCDEF "
            "IBFL:1B SRTG:[iBoot-359.3.2]");
        assert(info.state == TROGNON_STATE_DFU);
        assert(info.cpid == 0x8930);
    }

    /* Recovery/DFU after an unlock step ran: PWND marker present. */
    {
        struct trognon_serial_info info = trognon_serial_parse(
            "CPID:8940 CPRV:11 CPFM:03 SCEP:01 BDID:00 ECID:ABCDEF0123456789 "
            "IBFL:03 SRTG:[iBoot-2817.5.6] PWND:[checkm8]");
        assert(info.state == TROGNON_STATE_DFU_PWNED);
        assert(info.cpid == 0x8940);
    }

    /* Degenerate inputs must never crash. */
    {
        struct trognon_serial_info info = trognon_serial_parse(NULL);
        assert(info.state == TROGNON_STATE_UNKNOWN);
        assert(info.cpid == 0);
    }
    {
        struct trognon_serial_info info = trognon_serial_parse("");
        assert(info.state == TROGNON_STATE_UNKNOWN);
        assert(info.cpid == 0);
    }
    {
        /* Garbage that happens to contain "CPID:" with no hex digits after it. */
        struct trognon_serial_info info = trognon_serial_parse("CPID:");
        assert(info.state == TROGNON_STATE_DFU);
        assert(info.cpid == 0);
    }

    printf("test_serial: all fixture checks passed\n");
    return 0;
}
