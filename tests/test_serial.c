/* SPDX-License-Identifier: MIT */
/*
 * Fixture-based tests for the serial string parser. No hardware required —
 * written while waiting for real devices to finish charging (2026-10-14),
 * precisely so progress doesn't depend on having a device plugged in.
 *
 * Fixture strings: one is a real capture off our own iPhone 3GS (normal
 * mode). The DFU/Recovery ones are built from a real forum capture
 * ("CPID:8920 CPRV:14 CPFM:03 SCEP:03 BDID:00 ECID:0000030A72095D2C
 * IBFL:01 SRNM:[889224W43NP] IMEI:[011950007000980]") plus confirmed PIDs
 * from theapplewiki.com/wiki/WTF_0x1227 and
 * oemdrivers.com/devid/usb-vid_05ac-pid_1281 — not yet captured from our
 * own hardware in those states.
 */
#include <assert.h>
#include <stdio.h>

#include "trognon/serial.h"

int
main(void)
{
    /* Normal mode: plain serial number, no CPID field. Matches what we
     * actually read off our iPhone 3GS on 2026-10-14. Product id is
     * irrelevant here. */
    {
        struct trognon_serial_info info = trognon_serial_parse("DLC335502MRKD56AC", 0x1463);
        assert(info.state == TROGNON_STATE_NORMAL);
        assert(info.cpid == 0);
    }

    /* Recovery mode, iPhone 3GS-era CPID (0x8920), PID 0x1281. */
    {
        struct trognon_serial_info info = trognon_serial_parse(
            "CPID:8920 CPRV:14 CPFM:03 SCEP:03 BDID:00 ECID:0000030A72095D2C "
            "IBFL:01 SRNM:[889224W43NP] IMEI:[011950007000980]",
            TROGNON_PID_RECOVERY);
        assert(info.state == TROGNON_STATE_RECOVERY);
        assert(info.cpid == 0x8920);
    }

    /* Same CPID, but PID 0x1227: DFU, not Recovery. Confirms the PID is
     * what decides it, not the string content. */
    {
        struct trognon_serial_info info = trognon_serial_parse(
            "CPID:8920 CPRV:14 CPFM:03 SCEP:03 BDID:00 ECID:0000030A72095D2C "
            "IBFL:01 SRNM:[889224W43NP] IMEI:[011950007000980]",
            TROGNON_PID_DFU);
        assert(info.state == TROGNON_STATE_DFU);
        assert(info.cpid == 0x8920);
    }

    /* DFU after an unlock step ran: PWND marker present. PID is ignored
     * once PWND: is found. */
    {
        struct trognon_serial_info info = trognon_serial_parse(
            "CPID:8940 CPRV:11 CPFM:03 SCEP:01 BDID:00 ECID:ABCDEF0123456789 "
            "IBFL:03 SRNM:[somesn] PWND:[checkm8]",
            TROGNON_PID_DFU);
        assert(info.state == TROGNON_STATE_DFU_PWNED);
        assert(info.cpid == 0x8940);
    }

    /* CPID present but an unrecognized product id: don't guess. */
    {
        struct trognon_serial_info info = trognon_serial_parse(
            "CPID:8930 CPRV:20 CPFM:03 SCEP:01 BDID:00 ECID:0123456789ABCDEF "
            "IBFL:1B SRNM:[somesn]",
            0x9999);
        assert(info.state == TROGNON_STATE_UNKNOWN);
        assert(info.cpid == 0x8930); /* cpid is still extracted even if we
                                         can't classify the mode */
    }

    /* Degenerate inputs must never crash. */
    {
        struct trognon_serial_info info = trognon_serial_parse(NULL, TROGNON_PID_DFU);
        assert(info.state == TROGNON_STATE_UNKNOWN);
        assert(info.cpid == 0);
    }
    {
        struct trognon_serial_info info = trognon_serial_parse("", TROGNON_PID_DFU);
        assert(info.state == TROGNON_STATE_UNKNOWN);
        assert(info.cpid == 0);
    }
    {
        /* Garbage that happens to contain "CPID:" with no hex digits after it. */
        struct trognon_serial_info info = trognon_serial_parse("CPID:", TROGNON_PID_DFU);
        assert(info.state == TROGNON_STATE_DFU);
        assert(info.cpid == 0);
    }

    printf("test_serial: all fixture checks passed\n");
    return 0;
}
