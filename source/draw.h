#pragma once
#include <3ds.h>

bool C2D_Pomelo_DrawRectangleSingleColor(float x, float y, float w, float h, u32 clr);

// Draws the NDS-style graph-paper grid (fine dither + coarse border lines)
// on top of whatever the target was just cleared to. Colors are already
// expected to be in C2D_Color32 form (e.g. via rgb_to_C2D_Color32).
void C2D_Pomelo_DrawNdsGridBackground(float w, float h, u32 dither_dark_clr, u32 line_clr);

// Draws a DS-style icon cell: a bordered tile with a translucent bevel band
// across the top, echoing the raised/pressed gloss look of DS system UI
// icons (ds.css's button/button-square bevel overlays). `bevel_clr` should
// carry a translucent alpha (e.g. white for a raised highlight, black for a
// pressed-in shadow). Colors are already expected to be in C2D_Color32 form.
void C2D_Pomelo_DrawNdsIconCell(float x, float y, float w, float h,
								 u32 fill_clr, u32 border_clr, u32 bevel_clr,
								 float border_w);

u8 get_red(u32 color);
u8 get_green(u32 color);
u8 get_blue(u32 color);