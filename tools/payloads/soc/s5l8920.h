/* iPhone 3GS, S5L8920. Values read on the device 2026-10-15 unless a line says otherwise
 * (evidence levels: docs/en/README.md; register map: docs/en/03-hardware-map.md). */
#ifndef SOC_S5L8920_H
#define SOC_S5L8920_H

#define SOC_NAME              "s5l8920"

/* Where Recovery's send_data lands and `run` jumps (patched iBSS 4.3.5, n88ap). DEVICE */
#define SOC_PAYLOAD_BASE      0x41000000
/* Our own choices inside the mapped 0x410xxxxx section (page table checked on the device). DEVICE */
#define SOC_SCRATCH           0x41080000
/* Screen transcript (26 rows x 32 bytes) and register snapshots: more of our own mapped section. */
#define SOC_TRANSCRIPT       0x41081000
#define SOC_SNAPSHOTS        0x41084000
#define SOC_HANDLER_STACK_TOP 0x410f8000
#define SOC_IRQ_STACK_TOP     0x410f4000

/* Display: CLCD window 0, RGB888 0x00RRGGBB, mapped cacheable write-back. DEVICE */
#define SOC_FB_BASE           0x4fd00000
#define SOC_FB_W              320
#define SOC_FB_H              480
#define SOC_CLCD_BASE         0x85400000
/* Exception breadcrumb bar: last 14 rows, 10 rows tall. Numeric, used inside assembly. */
#define SOC_BREADCRUMB_ROW    466
#define SOC_BREADCRUMB_WORDS  3200          /* 10 rows * SOC_FB_W */
#define SOC_BREADCRUMB_LINES  400           /* SOC_BREADCRUMB_WORDS * 4 / 32-byte cache lines */

/* iBSS internal env_set(), Thumb bit set. DEVICE (disassembly + run on the device) */
#define SOC_ENV_SET           0x84011799

/* Exception vector pointer words (vector page at VA 0 = iBSS image). Hooked on the device:
 * undef, prefetch, data. IRQ/FIQ words read on the device, hooking them is not yet proven. */
#define SOC_VEC_UNDEF         0x24
#define SOC_VEC_PABT          0x2c
#define SOC_VEC_DABT          0x30
#define SOC_VEC_IRQ           0x38

/* Interrupt controllers, GPIO, clock gate. Read on the device (read-only survey). DEVICE */
#define SOC_VIC0              0xBF200000
#define SOC_VIC1              0xBF210000
#define SOC_VIC2              0xBF220000
#define SOC_GPIO_BASE         0x83000000
/* Timer block. Base readable on the device (it is also the clock-gate base); the timer register
 * offsets used by the survey are SOURCE (OpeniBoot) until read on the phone. */
#define SOC_TIMER_BASE        0xBF100000
#define SOC_TIMER5_BASE       (SOC_TIMER_BASE + 0xC0)       /* SOURCE offset; STATE read 0 on the device */
#define SOC_TIMER_IRQSTAT     (SOC_TIMER_BASE + 0x10000)    /* SOURCE, never read before STAGE 10 */
#define SOC_TIMER_IRQLATCH    (SOC_TIMER_BASE + 0xF8)       /* SOURCE, never read before STAGE 10 */
#define SOC_TIMER_VIC0_BIT    6                             /* SOURCE: TIMER_IRQ 6, enabled by the iBSS (EN bit 6) */
#define SOC_CLOCK_GATE_2C     0xBF100128

/* NAND (H2FMI), bus 0 / CE0 only, read-only chip ID probe. SOURCE (OpeniBoot plat-s5l8920/h2fmi.c
 * and includes/hardware/h2fmi.h), none of this read on the device yet. Offsets are relative to
 * SOC_H2FMI0_BASE (H2FMI_REG macro in the source). Clock gate 0x26 = fmi0 (plat-s5l8920/h2fmi.c:217). */
#define SOC_H2FMI0_BASE       0x81200000
#define SOC_CLOCK_GATE_26     0xBF100110    /* CLOCK_GATE_BASE(0xBF100078) + 4*0x26, clock.h:29 */
#define SOC_FMI_REG2_400      (SOC_H2FMI0_BASE + 0x400)   /* H2FMI_UNKREG2 */
#define SOC_FMI_REG1_408      (SOC_H2FMI0_BASE + 0x408)   /* H2FMI_UNKREG1, timing cache */
#define SOC_FMI_REG9_418      (SOC_H2FMI0_BASE + 0x418)   /* H2FMI_UNKREG9 */
#define SOC_FMI_REG10_420     (SOC_H2FMI0_BASE + 0x420)   /* H2FMI_UNKREG10 */
#define SOC_FMI_REG5_410      (SOC_H2FMI0_BASE + 0x410)   /* H2FMI_UNKREG5, command */
#define SOC_FMI_REG6_444      (SOC_H2FMI0_BASE + 0x444)   /* H2FMI_UNKREG6, status/done */
#define SOC_FMI_REG4_414      (SOC_H2FMI0_BASE + 0x414)   /* H2FMI_UNKREG4 */
#define SOC_FMI_REG8_424      (SOC_H2FMI0_BASE + 0x424)   /* H2FMI_UNKREG8 */
#define SOC_FMI_DATA_448      (SOC_H2FMI0_BASE + 0x448)   /* H2FMI_DATA */
#define SOC_FMI_CHIPMASK_40C  (SOC_H2FMI0_BASE + 0x40c)   /* H2FMI_CHIP_MASK, fmi0 */
#define SOC_FMI_REG3_814      (SOC_H2FMI0_BASE + 0x814)   /* H2FMI_UNKREG3 */
/* CE 8-15 route their enable/disable mask to fmi1's own register (h2fmi_enable_chip()/
 * h2fmi_disable_chip(), h2fmi.c:338-354), even while the command sequencer stays on fmi0
 * for this sweep (h2fmi_init()'s own first call: h2fmi_reset_and_read_chipids(&fmi0, ...)
 * loops chip 0..15, h2fmi.c:1926). Not yet read on the device. */
#define SOC_H2FMI1_BASE       0x81300000
#define SOC_FMI1_CHIPMASK_40C (SOC_H2FMI1_BASE + 0x40c)

/* SDIO/Wi-Fi host controller. Base + size from the device's own decrypted device tree
 * (/device-tree/arm-io/sdio, reg=0,0x1000; see docs/journal/2026-10-15.md), NOT from any
 * OpeniBoot source (none exists for s5l8920). Internal register layout UNKNOWN: two
 * incompatible candidates exist upstream (plat-s5l8900's simple CTRL/CMD/STATE layout vs
 * plat-a4's SDHCI-standard BLK/CMD/RESP layout), neither confirmed for this SoC. */
#define SOC_SDIO0_BASE        0x80000000
#define SOC_CLOCK_GATE_10     0xBF1000B8    /* CLOCK_GATE_BASE(0xBF100078) + 4*0x10, device tree "clock-gates" */

/* Embedded IOP ("EmbeddedIOP") control/boot register window, index 0 of the device tree's
 * /device-tree/arm-io/iop node (reg = 0x06300000,0x1000; CPU-visible with the standard high bit
 * set). Cross-confirmed two ways without reading the device: the same literal 0x86300000 is
 * hardcoded in the IOP firmware's own generic log function, and this is the address
 * AppleARM7M::start() caches from mapDeviceMemoryWithIndex(0) (see
 * docs/research/2026-10-16-wifi-iop-architecture.md). Offsets from AppleARM7M::startCPU's own
 * disassembly: +0x100 control (host writes 0x10 then polls bit 1, then writes 1 to "go"), +0x110
 * firmware pointer, +0x114 size, +0x118 self-address. SOURCE (disassembly + device tree), not yet
 * read on the device. */
#define SOC_IOP_REG_BASE      0x86300000
/* Second register window for the same "iop" device tree node (its "reg" property has TWO
 * address/size pairs, not one -- decoded 2026-10-16, late session, see
 * tools/iop_analysis/appleARM7M_notes.md). Device tree value 0x3F300000, translated through
 * arm-io's own "ranges" rule (+0x80000000 below 0x40000000, same rule already used for
 * SOC_IOP_REG_BASE above) to this CPU address. AppleARM7M::start()'s own REQUIRE strings name
 * the matching driver field "_vicRegisterMap" (mapDeviceMemoryWithIndex(1), index 0 being
 * SOC_IOP_REG_BASE above) -- a real Apple name for the field, not our naming, but its exact
 * register role (interrupt controller vs. something else) is NOT confirmed: the "VIC" in the
 * name and the address family (0xBF1xxxxx timer, 0xBF2xxxxx VIC0-2, this one 0xBF3xxxxx) are
 * circumstantial, not a register read. SOURCE only, never read on the device. */
#define SOC_IOP_VIC_BASE      0xBF300000
/* The "iop" device tree node's own clock-gates property (0x27 = 39), read-only check: is this
 * gate already enabled? CLOCK_GATE_BASE(0xBF100078) + 4*0x27. Not written anywhere in
 * AppleARM7M::start()'s own disassembly (checked 2026-10-16) -- if this reads enabled already,
 * something else (generic platform-bus matching, or it is simply not needed) accounts for it. */
#define SOC_CLOCK_GATE_27     (0xBF100078 + 4 * 0x27)

/* i2c0 bus (DEVICE TREE, decoded 2026-10-16): soft (bit-banged) I2C, compatible
 * "i2c,s5l8920x","iic,soft". GPIO function codes for SDA/SCL use the same port.pin encoding
 * already confirmed for the Wi-Fi reset/wake lines (high byte = port, low byte = pin, array
 * index = 8*port+pin): function-iic_sda = 0x401 -> 04.01, function-iic_scl = 0x400 -> 04.00.
 * The accelerometer (compatible "accelerometer,lis331dl","accelerometer,lis302dl") sits at I2C
 * address 0x1d on this bus, device tree "reg" property; WHO_AM_I (register 0x0F) read 0x3B on
 * the device (DEVICE, 2026-10-16, trace_accel_i2c.bin) -- the documented value for LIS331DL, but
 * ST's LIS302DL datasheet (table 16) also lists 0x3B for some revisions, so this alone does not
 * distinguish the two compatible-string candidates. A compass (AKM8973S, addr 0x1e) and a second
 * compass1 (addr 0x1f, no interrupts property) share the same bus; not probed yet. */
#define SOC_I2C0_SCL_IDX      (8 * 0x04 + 0x00)
#define SOC_I2C0_SDA_IDX      (8 * 0x04 + 0x01)
#define SOC_ACCEL_I2C_ADDR    0x1d
#define SOC_ACCEL_WHO_AM_I_REG 0x0F

/* Axis registers (SOURCE: ST LIS302DL/LIS331DL datasheet register map, same family, not device
 * tree). CTRL_REG1 bit layout (LIS302DL datasheet): bit7 DR (0=100Hz,1=400Hz), bit6 PD
 * (0=power-down,1=active), bit5 FS (0=+-2g,1=+-8g), bits4:3 self-test, bits2:0 Zen/Yen/Xen.
 * 0x47 = active, 100Hz, +-2g, X/Y/Z all enabled -- the common "power up and read" value used by
 * reference drivers for this family. Output registers are single signed bytes (8-bit resolution,
 * not 16-bit like later accelerometers), one per axis. Not yet confirmed identical on LIS331DL;
 * the two parts are pin/register-compatible per ST's own family documentation but this specific
 * bit layout has not been cross-checked against a LIS331DL datasheet. */
#define SOC_ACCEL_CTRL_REG1    0x20
#define SOC_ACCEL_CTRL1_ACTIVE_XYZ_2G_100HZ 0x47
#define SOC_ACCEL_STATUS_REG   0x27
#define SOC_ACCEL_OUT_X        0x29
#define SOC_ACCEL_OUT_Y        0x2B
#define SOC_ACCEL_OUT_Z        0x2D

/* Bluetooth (DEVICE TREE, decoded 2026-10-16): compatible "bluetooth,n88", transport is UART3
 * (NOT SDIO -- a different, simpler path than Wi-Fi), transport-speed 0x2dc6c0 (3,000,000 baud).
 * Reset and wake GPIO function codes, same encoding as above: function-bt_reset = 0x1506 ->
 * 15.06, function-bt_wake = 0x1602 -> 16.02. The baseband node's own function-bt_reset is the
 * same 0x1506 (shared physical line). Neither read on the device yet. */
#define SOC_BT_RESET_IDX      (8 * 0x15 + 0x06)
#define SOC_BT_WAKE_IDX       (8 * 0x16 + 0x02)

/* Touch controller (DEVICE TREE, decoded 2026-10-16 with tools/dt_dump.py): node
 * /device-tree/arm-io/spi1/multi-touch, compatible "multi-touch,n88". Pin words use the same
 * encoding as above (high byte = port, low byte = pin, both hexadecimal): function-reset =
 * 0x1401 -> 14.01, function-enable_cs = 0x1300 -> 13.00. Power (function-power_ldo) and the
 * clock enable (function-clock_enable) go through the PMU, not through these GPIOs. The
 * attention line is an interrupt (0xb4) on the GPIO interrupt controller, which this project
 * treats as dangerous (see GPIOIC), so it is NOT read. Neither pin read on the device yet. */
#define SOC_TOUCH_RESET_IDX   (8 * 0x14 + 0x01)
#define SOC_TOUCH_CS_EN_IDX   (8 * 0x13 + 0x00)

#endif
