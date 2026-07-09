#include <citro2d.h>

#include "draw.h"

bool C2D_Pomelo_DrawRectangleSingleColor(float x, float y, float w, float h, u32 clr) {
    return C2D_DrawRectangle(x, y, 0, w, h, clr, clr, clr, clr);
}

void C2D_Pomelo_DrawNdsGridDither(float w, float h, u32 dither_dark_clr) {
    // Fine 4px dither: the target is assumed to already be cleared to the
    // light color, so only the dark half of each 2px/2px stripe is drawn.
    for (float y = 2.f; y < h; y += 4.f) {
        C2D_Pomelo_DrawRectangleSingleColor(0, y, w, 2.f, dither_dark_clr);
    }
}

void C2D_Pomelo_DrawNdsGridLines(const float *x_lines, int x_line_count,
								 const float *y_lines, int y_line_count,
								 float w, float h, u32 line_clr,
								 float line_w) {
    // Vertical lines, each spanning the full height
    for (int i = 0; i < x_line_count; i++) {
        C2D_Pomelo_DrawRectangleSingleColor(x_lines[i], 0, line_w, h,
											line_clr);
    }

    // Horizontal lines, each spanning the full width
    for (int i = 0; i < y_line_count; i++) {
        C2D_Pomelo_DrawRectangleSingleColor(0, y_lines[i], w, line_w,
											line_clr);
    }
}

void C2D_Pomelo_DrawNdsIconCell(float x, float y, float w, float h,
								 u32 fill_clr, u32 border_clr,
								 float border_w, float top_border_w) {
    // Base rect: doubles as the border, since the inner fill drawn after
    // it never covers the outer ring. The top inset uses top_border_w
    // instead of border_w so the top edge can be drawn thicker, matching
    // the real DS System Menu's button chrome.
    C2D_Pomelo_DrawRectangleSingleColor(x, y, w, h, border_clr);

    float inner_x = x + border_w;
    float inner_y = y + top_border_w;
    float inner_w = w - (border_w * 2.f);
    float inner_h = h - border_w - top_border_w;
    C2D_Pomelo_DrawRectangleSingleColor(inner_x, inner_y, inner_w, inner_h,
										 fill_clr);
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
