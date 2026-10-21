/* GPIO pin registers. One 32-bit word per pin at SOC_GPIO_BASE + 4 * (8 * port + pin): the `port.pin` convention
 * the device tree and OpeniBoot use. Header-only accessors, no state. */
#ifndef HAL_GPIO_H
#define HAL_GPIO_H
#include "hal.h"

static inline u32 gpio_index(u32 port, u32 pin) { return 8 * port + pin; }
static inline volatile u32 *gpio_regs(void) { return (volatile u32 *)SOC_GPIO_BASE; }
static inline volatile u32 *gpio_reg(u32 port, u32 pin) { return &gpio_regs()[gpio_index(port, pin)]; }
/* Level of a pin configured as an input (bit 0 of its word). */
static inline u32 gpio_level(u32 port, u32 pin) { return *gpio_reg(port, pin) & 1; }
#endif
