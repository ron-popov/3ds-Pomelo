#include <citro2d.h>

#include "draw.h"
#include "utils.h"

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

void C2D_Pomelo_DrawSelectionCorners(float x, float y, float w, float h,
									 float corner_len, float thickness,
									 float outset, u32 clr) {
    float left = x - outset;
    float top = y - outset;
    float right = x + w + outset - thickness;
    float bottom = y + h + outset - thickness;

    // Top-left
    C2D_Pomelo_DrawRectangleSingleColor(left, top, corner_len, thickness, clr);
    C2D_Pomelo_DrawRectangleSingleColor(left, top, thickness, corner_len, clr);

    // Top-right
    C2D_Pomelo_DrawRectangleSingleColor(right - corner_len + thickness, top,
										corner_len, thickness, clr);
    C2D_Pomelo_DrawRectangleSingleColor(right, top, thickness, corner_len,
										clr);

    // Bottom-left
    C2D_Pomelo_DrawRectangleSingleColor(left, bottom, corner_len, thickness,
										clr);
    C2D_Pomelo_DrawRectangleSingleColor(left, bottom - corner_len + thickness,
										thickness, corner_len, clr);

    // Bottom-right
    C2D_Pomelo_DrawRectangleSingleColor(right - corner_len + thickness,
										bottom, corner_len, thickness, clr);
    C2D_Pomelo_DrawRectangleSingleColor(right,
										bottom - corner_len + thickness,
										thickness, corner_len, clr);
}

int C2D_Pomelo_BuildGridLinePositions(float offset, float pitch,
									 float extent, float *out_positions) {
    int count = 0;
    for (float p = offset; p < extent; p += pitch) {
        out_positions[count++] = p;
    }
    return count;
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

// Draws the DS-style grid background (fine dither plus coarse tiled grid
// lines) covering the whole bottom screen. Must be called after
// C2D_SceneBegin() targets the bottom screen.
void drawBottomScreenGridBackground(void) {
	C2D_Pomelo_DrawNdsGridDither(BOTTOM_SCREEN_WIDTH, BOTTOM_SCREEN_HEIGHT,
								 rgb_to_C2D_Color32(COL_GRID_DITHER_DARK));

	// Coarse grid lines tiled at a fixed pitch across the whole screen,
	// matching ds.css's `.ds-grid` (a repeating grid of square cells),
	// rather than lining up with the row boxes
	float grid_x_lines[(int)(BOTTOM_SCREEN_WIDTH / GRID_CELL_PX) + 1];
	int grid_x_line_count = C2D_Pomelo_BuildGridLinePositions(
		GRID_X_OFFSET, GRID_CELL_PX, BOTTOM_SCREEN_WIDTH, grid_x_lines);

	float grid_y_lines[(int)(BOTTOM_SCREEN_HEIGHT / GRID_CELL_PX) + 1];
	int grid_y_line_count = C2D_Pomelo_BuildGridLinePositions(
		GRID_Y_OFFSET, GRID_CELL_PX, BOTTOM_SCREEN_HEIGHT, grid_y_lines);

	C2D_Pomelo_DrawNdsGridLines(grid_x_lines, grid_x_line_count, grid_y_lines,
								grid_y_line_count, BOTTOM_SCREEN_WIDTH,
								BOTTOM_SCREEN_HEIGHT,
								rgb_to_C2D_Color32(COL_GRID_LINE),
								GRID_LINE_W);
}

// Draws a single game row (icon on the left, name on the right) at its
// scroll-adjusted position, like the DS System Menu's PICTOCHAT / DS
// Download Play buttons.
void drawGameRow(int row, titleGame *game, bool is_selected,
					C2D_TextBuf textBuf, C2D_Font font) {
	u32 fill_clr = rgb_to_C2D_Color32(COL_ROW_FILL);
	u32 border_clr = rgb_to_C2D_Color32(COL_ROW_BORDER);

	float row_start_x = LIST_MARGIN_X;
	float row_start_y = LIST_TOP_OFFSET_Y + LIST_ROW_GAP_Y +
						row * (LIST_ROW_GAP_Y + LIST_ROW_H);

	C2D_Pomelo_DrawNdsIconCell(row_start_x, row_start_y, LIST_ROW_W,
							   LIST_ROW_H, fill_clr, border_clr,
							   ROW_BORDER_W, ROW_BORDER_TOP_W);

	if (is_selected) {
		C2D_Pomelo_DrawSelectionCorners(
			row_start_x, row_start_y, LIST_ROW_W, LIST_ROW_H,
			SELECTION_CORNER_LEN, SELECTION_CORNER_THICKNESS,
			SELECTION_CORNER_OUTSET,
			rgb_to_C2D_Color32(COL_SELECTION_CORNER));
	}

	float icon_x = row_start_x + LIST_ICON_PADDING;
	float icon_y = row_start_y + LIST_ICON_PADDING;

	C2D_Image image = {.tex = &game->large_icon_tex, .subtex = &icon_subtex};
	C2D_DrawImageAt(image, icon_x, icon_y, 1.0f, NULL, 1.0f, 1.0f);

	C2D_Text row_gamename;
	C2D_TextFontParse(&row_gamename, font, textBuf, game->name);
	C2D_TextOptimize(&row_gamename);

	float text_gamename_h, text_gamename_w;
	C2D_TextGetDimensions(&row_gamename, TEXT_ROW_SCALE_X, TEXT_ROW_SCALE_Y, &text_gamename_h,
						  &text_gamename_w);

	float text_gamename_x = icon_x + LIST_ICON_SIZE + LIST_TEXT_GAP;
	float text_gamename_y = row_start_y + (LIST_ROW_H - text_gamename_w) / 4.f;
	// float text_gamename_y = icon_y;

	C2D_DrawText(&row_gamename, C2D_WithColor, text_gamename_x, text_gamename_y, 0, TEXT_ROW_SCALE_X,
				TEXT_ROW_SCALE_Y, rgb_to_C2D_Color32(COL_TEXT));

	C2D_Text row_publisher;
	C2D_TextFontParse(&row_publisher, font, textBuf, game->publisher);
	C2D_TextOptimize(&row_publisher);

	float text_publisher_w, text_publisher_h;
	C2D_TextGetDimensions(&row_publisher, TEXT_ROW_SCALE_X, TEXT_ROW_SCALE_Y,
						  &text_publisher_w, &text_publisher_h);

	float text_publisher_x = icon_x + LIST_ICON_SIZE + LIST_TEXT_GAP;
	float text_publisher_y = icon_y + (LIST_ROW_H - text_publisher_h) / 2.f;

	C2D_DrawText(&row_publisher, C2D_WithColor, text_publisher_x, text_publisher_y, 0, TEXT_ROW_SCALE_X,
				 TEXT_ROW_SCALE_Y, rgb_to_C2D_Color32(COL_TEXT));
}
