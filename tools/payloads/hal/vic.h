#ifndef HAL_VIC_H
#define HAL_VIC_H
#include "hal/hal.h"
#define VICREG(base, off) (*(volatile u32 *)((base) + (off)))
/* IRQ entry stub, handler and vector hook (PL190/PL192-style VIC, SOURCE for the layout). */
void exc_irq(void);
void irq_c(void);
void hook_irq(int on);
#endif
