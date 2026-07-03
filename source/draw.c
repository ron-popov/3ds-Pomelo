#include <citro2d.h>

#include "draw.h"

bool C2D_Pomelo_DrawRectangleSingleColor(float x, float y, float w, float h, u32 clr) {
    return C2D_DrawRectangle(x, y, 0, w, h, clr, clr, clr, clr);
}

void C2D_Pomelo_DrawNdsGridBackground(float w, float h, u32 dither_dark_clr, u32 line_clr) {
    // Fine 4px dither: the target is assumed to already be cleared to the
    // light color, so only the dark half of each 2px/2px stripe is drawn.
    for (float y = 2.f; y < h; y += 4.f) {
        C2D_Pomelo_DrawRectangleSingleColor(0, y, w, 2.f, dither_dark_clr);
    }

    // Coarse 32px grid lines on top of the dither
    for (float x = 30.f; x < w; x += 32.f) {
        C2D_Pomelo_DrawRectangleSingleColor(x, 0, 2.f, h, line_clr);
    }
    for (float y = 30.f; y < h; y += 32.f) {
        C2D_Pomelo_DrawRectangleSingleColor(0, y, w, 2.f, line_clr);
    }
}

void C2D_Pomelo_DrawNdsIconCell(float x, float y, float w, float h,
								 u32 fill_clr, u32 border_clr, u32 bevel_clr,
								 float border_w) {
    // Outer border ring
    C2D_Pomelo_DrawRectangleSingleColor(x, y, w, h, border_clr);

    // Inner fill
    float inner_x = x + border_w;
    float inner_y = y + border_w;
    float inner_w = w - (border_w * 2.f);
    float inner_h = h - (border_w * 2.f);
    C2D_Pomelo_DrawRectangleSingleColor(inner_x, inner_y, inner_w, inner_h,
										 fill_clr);

    // Translucent bevel band across the top of the inner area
    float bevel_h = inner_h * 0.4f;
    C2D_DrawRectangle(inner_x, inner_y, 0, inner_w, bevel_h, bevel_clr,
					   bevel_clr, bevel_clr, bevel_clr);
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
