#pragma once
#include <3ds.h>

bool C2D_Pomelo_DrawRectangleSingleColor(float x, float y, float w, float h, u32 clr);

// Draws the fine 4px NDS-style dither texture on top of whatever the
// target was just cleared to. Colors are already expected to be in
// C2D_Color32 form (e.g. via rgb_to_C2D_Color32).
void C2D_Pomelo_DrawNdsGridDither(float w, float h, u32 dither_dark_clr);

// Draws the coarse NDS-style grid lines (the ones that read as visible
// "squares") at caller-supplied x/y coordinates. Each x coordinate gets a
// full-height vertical line, each y coordinate a full-width horizontal
// line. Colors are already expected to be in C2D_Color32 form.
void C2D_Pomelo_DrawNdsGridLines(const float *x_lines, int x_line_count,
								 const float *y_lines, int y_line_count,
								 float w, float h, u32 line_clr,
								 float line_w);

// Draws a DS-style icon cell: a flat white tile with a thin border,
// matching the plain button chrome used across the real DS System Menu
// (PICTOCHAT / DS Download Play buttons). The top edge is drawn at
// top_border_w instead of border_w, since the reference buttons have a
// visibly thicker top rule than the side/bottom border. Colors are already
// expected to be in C2D_Color32 form.
void C2D_Pomelo_DrawNdsIconCell(float x, float y, float w, float h,
								 u32 fill_clr, u32 border_clr,
								 float border_w, float top_border_w);

u8 get_red(u32 color);
u8 get_green(u32 color);
u8 get_blue(u32 color);