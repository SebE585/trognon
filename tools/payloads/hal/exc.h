#ifndef HAL_EXC_H
#define HAL_EXC_H
#include "hal/hal.h"
/* Hook (on=1) or restore (on=0) the undef, prefetch-abort and data-abort vectors. */
void hook(int on);
u32 exc_c(u32 kind, u32 lr, u32 spsr, u32 *r);
#endif
