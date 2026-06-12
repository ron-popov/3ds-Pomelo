#include "draw.h"
#include "font.h"

#define FB_W 320
#define FB_H 240

static inline void set_pixel(u8 *fb, int x, int y, u8 r, u8 g, u8 b) {
    if ((unsigned)x >= FB_W || (unsigned)y >= FB_H) return;
    int idx = (x * FB_H + (FB_H - 1 - y)) * 3;
    fb[idx + 0] = b;
    fb[idx + 1] = g;
    fb[idx + 2] = r;
}

void fb_fill(u8 *fb, u8 r, u8 g, u8 b) {
    int total = FB_W * FB_H * 3;
    for (int i = 0; i < total; i += 3) {
        fb[i + 0] = b;
        fb[i + 1] = g;
        fb[i + 2] = r;
    }
}

void fb_rect(u8 *fb, int x, int y, int w, int h, u8 r, u8 g, u8 b) {
    for (int px = x; px < x + w; px++) {
        if ((unsigned)px >= FB_W) continue;
        for (int py = y; py < y + h; py++) {
            if ((unsigned)py >= FB_H) continue;
            int idx = (px * FB_H + (FB_H - 1 - py)) * 3;
            fb[idx + 0] = b;
            fb[idx + 1] = g;
            fb[idx + 2] = r;
        }
    }
}

void fb_border(u8 *fb, int x, int y, int w, int h, int t, u8 r, u8 g, u8 b) {
    fb_rect(fb, x,         y,         w,     t,         r, g, b); // top
    fb_rect(fb, x,         y + h - t, w,     t,         r, g, b); // bottom
    fb_rect(fb, x,         y + t,     t,     h - 2 * t, r, g, b); // left
    fb_rect(fb, x + w - t, y + t,     t,     h - 2 * t, r, g, b); // right
}

void fb_char(u8 *fb, int x, int y, char c, int scale, u8 r, u8 g, u8 b) {
    if ((unsigned char)c < 0x20 || (unsigned char)c > 0x7F)
        c = '?';
    const uint8_t (*glyph)[FONT_BYTES_PER_ROW] = font[(unsigned char)c - 0x20];
    for (int row = 0; row < FONT_GLYPH_H; row++) {
        for (int col = 0; col < FONT_GLYPH_W; col++) {
            if (!(glyph[row][col / 8] & (1 << (col % 8)))) continue;
            for (int sy = 0; sy < scale; sy++)
                for (int sx = 0; sx < scale; sx++)
                    set_pixel(fb, x + col * scale + sx, y + row * scale + sy, r, g, b);
        }
    }
}

void fb_string(u8 *fb, int x, int y, const char *s, int scale, u8 r, u8 g, u8 b) {
    while (*s) {
        fb_char(fb, x, y, *s, scale, r, g, b);
        x += FONT_GLYPH_W * scale;
        s++;
    }
}



void fb_image(u8 *fb, int x, int y, u8 *img, int img_w, int img_h) {
    for (int iy = 0; iy < img_h; iy++) {
        int dy = y + iy;
        if ((unsigned)dy >= FB_H) continue;
        for (int ix = 0; ix < img_w; ix++) {
            int dx = x + ix;
            if ((unsigned)dx >= FB_W) continue;
            int src = (iy * img_w + ix) * 3;
            int dst = (dx * FB_H + (FB_H - 1 - dy)) * 3;
            fb[dst + 0] = img[src + 0]; // B
            fb[dst + 1] = img[src + 1]; // G
            fb[dst + 2] = img[src + 2]; // R
        }
    }
}

u8 get_red(u32 color) {
    u32 temp = color & 0xff0000;
    u8 red_color = temp >> 16;
    return red_color;
}

u8 get_green(u32 color) {
    u32 temp = color & 0x00ff00;
    u8 green_color = temp >> 8;
    return green_color;
}

u8 get_blue(u32 color) {
    u32 temp = color & 0x0000ff;
    return temp;
}