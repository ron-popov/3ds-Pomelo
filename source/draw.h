#pragma once
#include <3ds.h>

// Raw framebuffer drawing for the 3DS bottom screen (320x240, BGR8, column-major).
// Pixel (x, y) lives at byte offset (x * 240 + (239 - y)) * 3.
// All functions clip silently at screen boundaries.

// JetBrains Mono Medium 16px glyph cell dimensions (exposed so callers can do layout)
#define FB_FONT_GLYPH_W 10
#define FB_FONT_GLYPH_H 22

void fb_fill  (u8 *fb, u8 r, u8 g, u8 b);
void fb_rect  (u8 *fb, int x, int y, int w, int h, u8 r, u8 g, u8 b);
void fb_border(u8 *fb, int x, int y, int w, int h, int t, u8 r, u8 g, u8 b);
void fb_char  (u8 *fb, int x, int y, char c, int scale, u8 r, u8 g, u8 b);
void fb_string(u8 *fb, int x, int y, const char *s, int scale, u8 r, u8 g, u8 b);

u8 get_red(u32 color);
u8 get_green(u32 color);
u8 get_blue(u32 color);