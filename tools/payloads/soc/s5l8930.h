/* iPhone 4, A4 (S5L8930). NOT VERIFIED ON A DEVICE: every value below is SOURCE (OpeniBoot
 * plat-a4), none has been read on a phone. The build is refused unless you pass ALLOW_UNVERIFIED=1,
 * and even then these constants only get you as far as the first failed check. */
#ifndef SOC_S5L8930_H
#define SOC_S5L8930_H

#ifndef ALLOW_UNVERIFIED
# error "soc/s5l8930.h is unverified (SOURCE only): re-read each constant on an iPhone 4 first, or pass -DALLOW_UNVERIFIED"
#endif

#define SOC_NAME              "s5l8930"

/* SOURCE: OpeniBoot clcd.h lists two variants, 0x4F700000 and 0x5F700000 (which one applies is open). */
#define SOC_FB_BASE           0x4F700000
/* HYPOTHESIS: iPhone 4 panel is 640x960. */
#define SOC_FB_W              640
#define SOC_FB_H              960
/* SOURCE: OpeniBoot plat-a4 */
#define SOC_VIC0              0xBF200000
#define SOC_VIC1              0xBF210000
#define SOC_VIC2              0xBF220000
#define SOC_VIC3              0xBF230000
#define SOC_GPIO_BASE         0xBFA00000

/* STILL TO FIND on an iPhone 4 (build fails until defined): SOC_PAYLOAD_BASE, SOC_SCRATCH,
 * SOC_HANDLER_STACK_TOP, SOC_IRQ_STACK_TOP, SOC_ENV_SET, SOC_CLCD_BASE, SOC_VEC_*, breadcrumb sizes. */

#endif
