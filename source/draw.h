#pragma once
#include <3ds.h>

bool C2D_Pomelo_DrawRectangleSingleColor(float x, float y, float w, float h, u32 clr);

u8 get_red(u32 color);
u8 get_green(u32 color);
u8 get_blue(u32 color);