#include "i2c_gpio.h"

/* Accelerometer probe over bit-banged i2c0 (decoded 2026-10-16 from the device tree, see
 * soc/s5l8920.h for the full chain). This is the FIRST bit-bang on these two pins: STAGE 29
 * reads their idle level with both held as inputs (no write at all) before doing anything
 * else -- if there is no pull-up on this bus, that read-only step already tells us (both
 * read 0 and stay 0), without risking a short. Only if that looks sane does it go on to a
 * real I2C transaction (START, address+write, register address, repeated START,
 * address+read, one byte, STOP) asking for the accelerometer's WHO_AM_I (register 0x0F).
 * GPIO mode encoding is OpeniBoot's own gpio_custom_io() (plat-s5l8920/gpio.c):
 * use_as_input = value 0x210 / mask 0x27E (releases the pin, no level forced -- this is what
 * makes the open-drain emulation safe: we only ever drive LOW or let go, never drive HIGH),
 * clear_output = value 0x212 / mask 0x27F (drive low). Bit timing is generous (~20 us/half-bit,
 * well under I2C "standard mode" 100 kHz) using the already-confirmed 24.001 MHz free-running
 * tick. Reversible by construction: an I2C bus is designed to be idle (both lines released)
 * between transactions, which is exactly the state this leaves. Plain file-scope functions,
 * not nested ones: a nested function under GCC needs a stack trampoline, which has no defined
 * behaviour on this bare-metal, MMU-mapped, cacheable payload section. */
static volatile u32 *const i2c_tick = (volatile u32 *)SOC_TICK_REG;

static void i2c_mode(volatile u32 *reg, u32 value, u32 mask) { *reg = (*reg & ~mask) | (value & mask); }
void i2c_release(volatile u32 *reg) { i2c_mode(reg, SOC_GPIO_INPUT_VAL, SOC_GPIO_INPUT_MASK); }
void i2c_low(volatile u32 *reg)     { i2c_mode(reg, SOC_GPIO_LOW_VAL, SOC_GPIO_LOW_MASK); }
u32 i2c_level(volatile u32 *reg)   { return *reg & 1; }
void i2c_delay(u32 us) { u32 t0 = *i2c_tick; while (*i2c_tick - t0 < SOC_TICKS_PER_US * us) ; }

void i2c_start(volatile u32 *scl, volatile u32 *sda) {
	i2c_release(sda); i2c_release(scl); i2c_delay(20);
	i2c_low(sda); i2c_delay(20);
	i2c_low(scl); i2c_delay(20);
}
void i2c_stop(volatile u32 *scl, volatile u32 *sda) {
	i2c_low(sda); i2c_delay(20);
	i2c_release(scl); i2c_delay(20);
	i2c_release(sda); i2c_delay(20);
}
void i2c_wbit(volatile u32 *scl, volatile u32 *sda, u32 bit) {
	if (bit) i2c_release(sda); else i2c_low(sda);
	i2c_delay(20);
	i2c_release(scl); i2c_delay(20);
	i2c_low(scl); i2c_delay(20);
}
u32 i2c_rbit(volatile u32 *scl, volatile u32 *sda) {
	i2c_release(sda); i2c_delay(10);
	i2c_release(scl); i2c_delay(20);
	u32 b = i2c_level(sda);
	i2c_low(scl); i2c_delay(20);
	return b;
}
u32 i2c_wbyte(volatile u32 *scl, volatile u32 *sda, u32 byte) {
	for (int i = 7; i >= 0; i--) i2c_wbit(scl, sda, (byte >> i) & 1);
	return !i2c_rbit(scl, sda);   /* ACK = slave pulls SDA low */
}
u32 i2c_rbyte(volatile u32 *scl, volatile u32 *sda) {
	u32 byte = 0;
	for (int i = 0; i < 8; i++) byte = (byte << 1) | i2c_rbit(scl, sda);
	i2c_wbit(scl, sda, 1);   /* master NACK: single-byte read, no more wanted */
	return byte;
}
/* PMU (i2c0 0x74) one-byte register read and write. Read: write the register number, repeated start,
 * read one byte (same sequence as STAGE 36). Write: START, address+write, register number, data byte, STOP;
 * returns 1 only if all three bytes were acknowledged. */
u32 pmu_read(volatile u32 *scl, volatile u32 *sda, u32 reg, u32 *ok) {
	i2c_start(scl, sda);
	u32 a1 = i2c_wbyte(scl, sda, (SOC_PMU_I2C_ADDR << 1) | 0);
	u32 a2 = i2c_wbyte(scl, sda, reg);
	i2c_start(scl, sda);
	u32 a3 = i2c_wbyte(scl, sda, (SOC_PMU_I2C_ADDR << 1) | 1);
	u32 v = i2c_rbyte(scl, sda);
	i2c_stop(scl, sda);
	i2c_delay(200);
	*ok = a1 & a2 & a3;
	return v;
}
u32 pmu_write(volatile u32 *scl, volatile u32 *sda, u32 reg, u32 val) {
	i2c_start(scl, sda);
	u32 a1 = i2c_wbyte(scl, sda, (SOC_PMU_I2C_ADDR << 1) | 0);
	u32 a2 = i2c_wbyte(scl, sda, reg);
	u32 a3 = i2c_wbyte(scl, sda, val);
	i2c_stop(scl, sda);
	i2c_delay(200);
	return a1 & a2 & a3;
}
