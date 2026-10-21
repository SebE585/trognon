/* Clock-gate words. One 32-bit word per gate at SOC_CLOCK_GATE_BASE + 4 * i, i = 0 to 0x33 on the 3GS. A gate reads
 * 0xF in its low nibble when the block is clocked. Physical indexes, not the device tree's numbering. */
#ifndef HAL_CLOCK_H
#define HAL_CLOCK_H
#include "hal.h"

#define CLOCK_NGATES 0x34

static inline volatile u32 *clock_gates(void) { return (volatile u32 *)SOC_CLOCK_GATE_BASE; }
static inline volatile u32 *clock_gate_reg(u32 i) { return &clock_gates()[i]; }
static inline u32 clock_gate_is_on(u32 i) { return (*clock_gate_reg(i) & 0xF) == 0xF; }
#endif
