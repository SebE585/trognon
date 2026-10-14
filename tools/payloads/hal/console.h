#ifndef HAL_CONSOLE_H
#define HAL_CONSOLE_H
#include "hal/hal.h"
void rect(int x, int y, int w, int h, u32 color);
void fb_clean(void);
void putc_(char c);
void puts_(const char *s);
void hex(u32 v);
void kv(const char *k, u32 v);
void at(int x, int y, u32 fg);
#endif
