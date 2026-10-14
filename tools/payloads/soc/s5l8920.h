/* iPhone 3GS, S5L8920. Values read on the device 2026-10-15 unless a line says otherwise
 * (evidence levels: docs/en/README.md; register map: docs/en/03-hardware-map.md). */
#ifndef SOC_S5L8920_H
#define SOC_S5L8920_H

#define SOC_NAME              "s5l8920"

/* Where Recovery's send_data lands and `run` jumps (patched iBSS 4.3.5, n88ap). DEVICE */
#define SOC_PAYLOAD_BASE      0x41000000
/* Our own choices inside the mapped 0x410xxxxx section (page table checked on the device). DEVICE */
#define SOC_SCRATCH           0x41080000
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
#define SOC_CLOCK_GATE_2C     0xBF100128

#endif
