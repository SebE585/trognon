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
#include <string.h>

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

    /* DFU after an unlock step ran: PWND marker present, on the DFU PID. */
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
        /* Garbage that happens to contain "CPID:" with no hex digits after it:
         * rejected (UNKNOWN), not classified as DFU with a CPID of 0. */
        struct trognon_serial_info info = trognon_serial_parse("CPID:", TROGNON_PID_DFU);
        assert(info.state == TROGNON_STATE_UNKNOWN);
        assert(info.cpid == 0);
    }

    /* Adversarial table: malformed CPID fields and PID/marker combinations.
     * Each row is (serial, pid, expected state, expected cpid). */
    {
        static const struct {
            const char *serial;
            uint16_t pid;
            enum trognon_bus_state state;
            uint32_t cpid;
        } rows[] = {
            /* CPID field shapes */
            { "CPID:8920",                     TROGNON_PID_DFU, TROGNON_STATE_DFU,     0x8920 },
            { "CPID:8920 CPRV:14",             TROGNON_PID_DFU, TROGNON_STATE_DFU,     0x8920 },
            { "CPID:8",                        TROGNON_PID_DFU, TROGNON_STATE_DFU,     0x8 },
            { "CPID:89200000",                 TROGNON_PID_DFU, TROGNON_STATE_DFU,     0x89200000 },
            { "CPID:892000000",                TROGNON_PID_DFU, TROGNON_STATE_UNKNOWN, 0 },  /* 9 digits */
            { "CPID:FFFFFFFFF",                TROGNON_PID_DFU, TROGNON_STATE_UNKNOWN, 0 },  /* would overflow */
            { "CPID:zzzz",                     TROGNON_PID_DFU, TROGNON_STATE_UNKNOWN, 0 },
            { "CPID:8920garbage",              TROGNON_PID_DFU, TROGNON_STATE_UNKNOWN, 0 },  /* glued text */
            { "CPID: 8920",                    TROGNON_PID_DFU, TROGNON_STATE_UNKNOWN, 0 },  /* space first */
            { "CPID:-1",                       TROGNON_PID_DFU, TROGNON_STATE_UNKNOWN, 0 },
            { "CPID:0x8920",                   TROGNON_PID_DFU, TROGNON_STATE_UNKNOWN, 0 },  /* strtoul would have accepted */
            { "CPID:8920\tCPRV:14",            TROGNON_PID_DFU, TROGNON_STATE_UNKNOWN, 0 },  /* tab, not a space */
            { "cpid:8920",                     TROGNON_PID_DFU, TROGNON_STATE_NORMAL,  0 },  /* case-sensitive: no CPID field */
            /* PWND: only counts on the DFU product id */
            { "CPID:8920 PWND:[x]",            TROGNON_PID_DFU,          TROGNON_STATE_DFU_PWNED, 0x8920 },
            { "CPID:8920 PWND:[x]",            TROGNON_PID_RECOVERY,     TROGNON_STATE_UNKNOWN,   0x8920 },
            { "CPID:8920 PWND:[x]",            TROGNON_PID_RECOVERY_ALT, TROGNON_STATE_UNKNOWN,   0x8920 },
            { "CPID:8920 PWND:[x]",            0x9999,                   TROGNON_STATE_UNKNOWN,   0x8920 },
            { "CPID:8920 PWND:[x]",            0,                        TROGNON_STATE_UNKNOWN,   0x8920 },
            { "CPID:8920",                     0,                        TROGNON_STATE_UNKNOWN,   0x8920 },
            /* PWND: without a CPID field is just a plain serial */
            { "PWND:[x]",                      TROGNON_PID_DFU, TROGNON_STATE_NORMAL, 0 },
            /* PWND: before CPID: still needs a valid CPID */
            { "PWND:[x] CPID:8920",            TROGNON_PID_DFU, TROGNON_STATE_DFU_PWNED, 0x8920 },
            { "PWND:[x] CPID:",                TROGNON_PID_DFU, TROGNON_STATE_UNKNOWN,   0 },
        };
        for (unsigned i = 0; i < sizeof rows / sizeof rows[0]; i++) {
            struct trognon_serial_info info = trognon_serial_parse(rows[i].serial, rows[i].pid);
            if (info.state != rows[i].state || info.cpid != rows[i].cpid) {
                fprintf(stderr, "row %u failed: \"%s\" pid=%#x -> state %d cpid %#x (want %d, %#x)\n",
                        i, rows[i].serial, (unsigned)rows[i].pid, (int)info.state,
                        (unsigned)info.cpid, (int)rows[i].state, (unsigned)rows[i].cpid);
                return 1;
            }
        }
    }
    {
        /* A serial longer than any real descriptor must not overrun anything. */
        char big[1024];
        memset(big, 'A', sizeof big - 1);
        big[sizeof big - 1] = '\0';
        memcpy(big + 100, "CPID:8920", 9);
        struct trognon_serial_info info = trognon_serial_parse(big, TROGNON_PID_DFU);
        assert(info.state == TROGNON_STATE_UNKNOWN);   /* "CPID:8920AAAA..." is glued text */
    }

    printf("test_serial: all fixture checks passed\n");
    return 0;
}
