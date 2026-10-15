/* On-screen text console on the linear framebuffer. NEVER read the framebuffer (write-back, no
 * write-allocate: reading it is how a slow, misleading screen happens), so there is no scrolling. */
#include "hal/hal.h"
#include "hal/console.h"
#include "font.h"

#define FB       ((volatile u32 *)SOC_FB_BASE)
#define W        SOC_FB_W
#define H        SOC_FB_H
#define SCALE    2
#define CELL_W   (6 * SCALE)
#define CELL_H   (9 * SCALE)

void rect(int x, int y, int w, int h, u32 color)
{
	if (x < 0 || y < 0 || x + w > W || y + h > H)
		return;
	for (int j = y; j < y + h; j++)
		for (int i = x; i < x + w; i++)
			FB[j * W + i] = color;
}

/* Debug aid: push the framebuffer out of the write-back D-cache so the screen shows what the CPU
 * really got through if it freezes (dirty lines are otherwise lost). Costs ~19200 MCRs. */
void fb_clean(void)
{
	for (u32 a = SOC_FB_BASE; a < SOC_FB_BASE + W * H * 4; a += 32)
		asm volatile("mcr p15, 0, %0, c7, c10, 1" :: "r"(a) : "memory");
	asm volatile("dsb sy" ::: "memory");
}

#define TR ((volatile struct tr_row *)SOC_TRANSCRIPT)

void transcript_clear(void)
{
	for (u32 r = 0; r < TR_ROWS; r++) {
		TR[r].fg = 0;
		for (u32 i = 0; i < TR_COLS + 2; i++)
			TR[r].text[i] = 0;
	}
}

const volatile struct tr_row *transcript(void) { return TR; }

void putc_(char c)
{
	if (c == '\n' || SC->cx + CELL_W > W) {
		SC->cx = 4;
		SC->cy += CELL_H;
		if (c == '\n')
			return;
	}
	if (SC->cy + CELL_H > H)
		return;                       /* no scrolling: it would read the framebuffer */
	{
		u32 row = SC->cy / CELL_H, col = (SC->cx - 4) / CELL_W;
		if (row < TR_ROWS && col < TR_COLS) {
			if (!TR[row].text[0])
				TR[row].fg = SC->fg;
			TR[row].text[col] = c;
			for (u32 k = 0; k < col; k++)          /* unwritten cells before this one read as spaces */
				if (!TR[row].text[k])
					TR[row].text[k] = ' ';
		}
	}
	const u8 *g = font5x7[c & 0x7f];
	for (int r = 0; r < 7; r++)
		for (int k = 0; k < 5; k++)
			if (g[r] & (0x10 >> k))
				rect(SC->cx + k * SCALE, SC->cy + r * SCALE, SCALE, SCALE, SC->fg);
	SC->cx += CELL_W;
}

void puts_(const char *s) { while (*s) putc_(*s++); fb_clean(); }

void hex(u32 v)
{
	for (int i = 28; i >= 0; i -= 4)
		putc_("0123456789ABCDEF"[(v >> i) & 15]);
}

void kv(const char *k, u32 v) { puts_(k); putc_(' '); hex(v); putc_('\n'); fb_clean(); }

void at(int x, int y, u32 fg) { SC->cx = x; SC->cy = y; SC->fg = fg; }

