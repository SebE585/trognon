/* Portable ARMv7 / Cortex-A8 layer: no chip address appears under hal/, only SOC_* macros from soc.h. */
#ifndef HAL_H
#define HAL_H
typedef unsigned int u32;
typedef unsigned char u8;
#include "soc.h"

#define STR_(x) #x
#define STR(x) STR_(x)

/* Shared state, placed by the payload at SOC_SCRATCH. The RAM is NOT zeroed between payloads:
 * every field a stage relies on must be initialised by that stage. */
struct scratch {
	u32 old[4];       /* original vector pointers: undef, prefetch, data, irq */
	u32 expected;     /* 1 while a deliberate exception is in flight */
	u32 resume;       /* where a handled, expected exception resumes */
	u32 hit;          /* number of expected exceptions handled */
	u32 cx, cy;       /* console cursor in pixels */
	u32 fg;
	u32 quiet;        /* 1: a handled expected exception draws nothing (register survey) */
	u32 irq;          /* IRQs taken for our soft-interrupt bit */
	u32 stray;        /* IRQs taken for anything else (should stay 0) */
	u32 irq_status;   /* VIC0 STATUS seen by the handler */
	u32 irq_vec;      /* value read from VIC0 VICADDRESS */
	u32 irq_bit;      /* the one VIC0 source we enabled */
};
#define SC ((volatile struct scratch *)SOC_SCRATCH)
#endif
