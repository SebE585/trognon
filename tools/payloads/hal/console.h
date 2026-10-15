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

/* Everything the console draws is also kept as text, one slot per screen row (26 rows of 26 cells),
 * so the host can read exactly what the phone displayed (result_screen() publishes it over USB). */
#define TR_ROWS 26
#define TR_COLS 26
struct tr_row {
	u32 fg;                  /* colour of the first character of the row, 0x00RRGGBB */
	char text[TR_COLS + 2];  /* NUL terminated */
};
void transcript_clear(void);
const volatile struct tr_row *transcript(void);
#endif
