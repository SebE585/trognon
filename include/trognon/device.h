/* SPDX-License-Identifier: MIT */
/*
 * Trognon — static device/SoC catalog.
 *
 * Read-only data: no code here talks to a bus. Sources and dates for every
 * entry are in docs/research/2026-10-13-a7-a11-mainline.md and
 * docs/research/2026-10-13-a5-a6-32bit.md.
 */
#ifndef TROGNON_DEVICE_H
#define TROGNON_DEVICE_H

#include <stddef.h>
#include <stdint.h>

enum trognon_soc_family {
    TROGNON_SOC_UNKNOWN = 0,
    TROGNON_SOC_S5L8920, /* iPhone 3GS — predates Apple's "A" naming entirely */
    TROGNON_SOC_A4,   /* S5L8930 — 32-bit, predates checkm8's scope */
    TROGNON_SOC_A5,   /* S5L8940 / S5L8942 / S5L8945 — 32-bit */
    TROGNON_SOC_A6,   /* S5L8950 / S5L8955 — 32-bit */
    TROGNON_SOC_A7,   /* S5L8960X — arm64, device tree: s5l8960x */
    TROGNON_SOC_A8,   /* T7000 / T7001 — device tree: t7000 / t7001 */
    TROGNON_SOC_A9,   /* S8000 / S8001 / S8003 */
    TROGNON_SOC_A10,  /* T8010 / T8011 */
    TROGNON_SOC_A11,  /* T8015 */
};

/* checkm8 needs a USB-host helper board (Pico/Arduino) on A5; A6 upward
 * works over the phone's own USB port driven by the host. A4 predates
 * checkm8 entirely and uses a different, older exploit (limera1n, 2010)
 * that also runs host-direct. See
 * docs/research/2026-10-13-untethered-boot.md and
 * docs/research/2026-10-14-a4-limera1n.md. */
enum trognon_unlock_path {
    TROGNON_UNLOCK_UNKNOWN = 0,
    TROGNON_UNLOCK_HOST_DIRECT,   /* ipwndfu / gaster (A7-A11) or
                                      limera1n-based tooling (A4), plain
                                      USB from host, no helper board */
    TROGNON_UNLOCK_HELPER_BOARD,  /* checkm8-a5 via Pico or Arduino, A5/A6 only */
};

/* Mainline/community support, as surveyed 2026-10-13. Re-check before
 * trusting an entry older than ~6 months (per project CLAUDE.md policy). */
struct trognon_support {
    int framebuffer;   /* boot console (simplefb) */
    int touch;
    int wifi;
    int storage;        /* internal NAND/NVMe as a Linux block device */
    int gpu;
    const char *notes;  /* short pointer, e.g. "Hoolock downstream only" */
};

struct trognon_device {
    const char *model_name;      /* e.g. "iPhone 4S" */
    const char *model_identifier; /* e.g. "iPhone4,1" — informational only,
                                      never appears literally on the USB bus */
    enum trognon_soc_family soc;
    const char *soc_codename;     /* e.g. "S5L8940" */
    uint32_t cpid;                 /* Chip ID as reported in the Recovery/DFU
                                       serial string ("CPID:xxxx", hex).
                                       Apple's codenames encode it directly:
                                       T8015 -> 0x8015, S5L8940 -> 0x8940.
                                       This is the only reliable field to
                                       identify a device while it is in
                                       Recovery or DFU mode; the literal
                                       model identifier is NOT present on
                                       the bus there. */
    const char *dt_platform;      /* mainline Linux DT name, or NULL if none */
    enum trognon_unlock_path unlock_path;
    const char *unlock_tool_hint; /* Which upstream tool trognon-unlock would
                                      point at for this device. Guidance
                                      text, not a command line: exact CLI
                                      syntax for each tool is not yet
                                      verified on real hardware, so nothing
                                      in this repo auto-generates argv from
                                      it. See docs/upstream-tools.md. */
    struct trognon_support support;
};

/* Static, read-only catalog. Defined in src/device_db.c. */
extern const struct trognon_device trognon_device_catalog[];
extern const size_t trognon_device_catalog_count;

/* Look up by model identifier string, e.g. "iPhone4,1". This string is
 * never read off the USB bus directly (see cpid above) — it's for callers
 * that already know the model some other way (user input, a prior lookup
 * by cpid). Returns NULL if unknown. Pure lookup, no I/O. */
const struct trognon_device *trognon_device_find_by_identifier(const char *model_identifier);

/* Look up by CPID, parsed from a Recovery/DFU serial string's "CPID:xxxx"
 * field. This is the lookup that actually works while probing a real
 * device. Returns NULL if unknown or cpid is 0. Pure lookup, no I/O. */
const struct trognon_device *trognon_device_find_by_cpid(uint32_t cpid);

#endif /* TROGNON_DEVICE_H */
