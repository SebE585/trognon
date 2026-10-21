/* Bit-banged I2C over two GPIO pin registers (open-drain emulation: a line is only ever driven low or released), and
 * the PMU (I2C address 0x74) register access built on it. `scl` and `sda` are the pins' words, e.g. from gpio_reg().
 * Timing and the pin mode encoding are explained at the top of i2c_gpio.c. */
#ifndef HAL_I2C_GPIO_H
#define HAL_I2C_GPIO_H
#include "hal.h"

void i2c_start(volatile u32 *scl, volatile u32 *sda);
void i2c_stop(volatile u32 *scl, volatile u32 *sda);
u32  i2c_wbyte(volatile u32 *scl, volatile u32 *sda, u32 byte);   /* returns 1 if acknowledged */
u32  i2c_rbyte(volatile u32 *scl, volatile u32 *sda);             /* reads one byte, then NACKs */
void i2c_delay(u32 us);
void i2c_release(volatile u32 *reg);
void i2c_low(volatile u32 *reg);
u32  i2c_level(volatile u32 *reg);

u32  pmu_read(volatile u32 *scl, volatile u32 *sda, u32 reg, u32 *ok);
u32  pmu_write(volatile u32 *scl, volatile u32 *sda, u32 reg, u32 val);   /* 1 only if all three bytes were acknowledged */
#endif
