/* Draw a test card on the S5L8920 CLCD window-0 framebuffer (320x480 RGB888).
 * Freestanding Thumb payload, linked at 0x41000000 (see payload.ld).
 * Framebuffer address read on the device 2026-10-15 (CLCD_BASE+0x24).
 * No cache clean: not needed in the iBSS boot state (see docs/en/03-hardware-map.md).
 */
typedef unsigned int u32;
typedef unsigned char u8;

#define FB        ((volatile u32 *)0x4fd00000)
#define W         320
#define H         480
#define ENV_SET   ((void (*)(const char *, const char *, int))0x84011799) /* iBSS 4.3.5 n88ap */

/* 5x7 glyphs, one row per byte, bit 4 = leftmost pixel. */
struct glyph { char c; u8 row[7]; };
static const struct glyph font[] = {
	{'T', {0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
	{'R', {0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11}},
	{'O', {0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e}},
	{'G', {0x0e, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0f}},
	{'N', {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}},
	{'3', {0x1e, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x1e}},
	{'S', {0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e}},
	{'K', {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}},
	{0, {0}},
};

static void rect(int x, int y, int w, int h, u32 color)
{
	if (x < 0 || y < 0 || x + w > W || y + h > H)
		return;        /* never write outside the framebuffer */
	for (int j = y; j < y + h; j++)
		for (int i = x; i < x + w; i++)
			FB[j * W + i] = color;
}

static void text(int x, int y, int scale, u32 color, const char *s)
{
	for (; *s; s++, x += 6 * scale) {
		const struct glyph *g = font;
		while (g->c && g->c != *s)
			g++;
		for (int r = 0; r < 7; r++)
			for (int c = 0; c < 5; c++)
				if (g->row[r] & (0x10 >> c))
					rect(x + c * scale, y + r * scale, scale, scale, color);
	}
}

__attribute__((section(".text.start"))) void payload_main(void)
{
	rect(0, 0, W, H, 0x00101820);          /* background */
	rect(0, 0, W, 8, 0x000D32B2);          /* top bar, blue */
	rect(0, H - 8, W, 8, 0x000D32B2);      /* bottom bar */
	rect(10, 60, W - 20, 4, 0x00FFFFFF);   /* rule: 300 px wide, centred */
	text(37, 90, 6, 0x00FFFFFF, "TROGNON");
	text(37, 190, 6, 0x000D32B2, "3GS");
	text(37, 290, 6, 0x0000B050, "OK");
	ENV_SET("trognon", "draw-1", 1);
}
