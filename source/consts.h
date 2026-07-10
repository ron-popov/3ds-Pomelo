#pragma once

#include "3ds/types.h"

#define BOTTOM_SCREEN_WIDTH 320
#define BOTTOM_SCREEN_HEIGHT 240

// Max number of titleGame entries loadTitles() will collect across all
// media types
#define MAX_TITLES 64


// Games are listed as full-width rows (icon on the left, name on the
// right), stacked vertically and scrollable, like the DS System Menu's
// PICTOCHAT / DS Download Play buttons.

// Left/right margin around each row
#define LIST_MARGIN_X            28.f

// Number of rows visible on screen at once; more can be scrolled to
#define LIST_VISIBLE_ROWS        3.f

// Size of the icon itself inside each row
#define LIST_ICON_SIZE           48.f

// Padding between a row's border and its icon (applies on all four sides)
#define LIST_ICON_PADDING        8.f

// Gap between the icon's right edge and the start of the game name text
#define LIST_TEXT_GAP            10.f

// The scale of the game name text drawn inside each row
#define TEXT_ROW_SCALE_X            0.4f
#define TEXT_ROW_SCALE_Y            0.4f

// App icon sizes, as stored in the SMDH (see SMDH_ApplicationTitle in
// utils.h). Both icons are RGB565 (2 bytes/pixel); the content-size consts
// below are derived from these dimensions rather than hardcoded, so they
// can't drift out of sync with the actual icon dimensions.
#define LARGE_ICON_H 48
#define LARGE_ICON_W 48
#define SMALL_ICON_H 24
#define SMALL_ICON_W 24
#define ICON_RGB565_BYTES_PER_PIXEL 2
#define LARGE_ICON_RGB565_CONTENT_SIZE \
	(LARGE_ICON_W * LARGE_ICON_H * ICON_RGB565_BYTES_PER_PIXEL)
#define SMALL_ICON_RGB565_CONTENT_SIZE \
	(SMALL_ICON_W * SMALL_ICON_H * ICON_RGB565_BYTES_PER_PIXEL)

#define LIST_TOP_OFFSET_Y 6.f

// Vertical gap left between consecutive rows (and above the first one)
#define LIST_ROW_GAP_Y            4.f

// Height of each row: icon plus its top/bottom padding
#define LIST_ROW_H               (LIST_ICON_PADDING + LIST_ICON_SIZE + LIST_ICON_PADDING)

// Width of each row: full screen width minus the left/right margins
#define LIST_ROW_W               (BOTTOM_SCREEN_WIDTH - (LIST_MARGIN_X * 2.f))

// Colors (R, G, B)
// NDS-style grid background: a fine 2px/2px dither, with a coarse grid
// of square cells tiled across the whole screen (see the grid_x_lines /
// grid_y_lines computation in main.c), matching ds.css's `.ds-grid`
// (https://github.com/notpx/ds.css)
#define COL_GRID_DITHER_LIGHT   0xFFFFFF
#define COL_GRID_DITHER_DARK    0xE3E3E3
#define COL_GRID_LINE           0xC3C3C3
#define GRID_LINE_W              2.f

// Pitch of the coarse grid squares, matching ds.css's .ds-grid 32px tile
#define GRID_CELL_PX             67.f
#define GRID_X_OFFSET 25.f
#define GRID_Y_OFFSET 9.f
#define COL_ROW_FILL            0xFFFFFF
#define COL_TEXT                0x222222

// Flat black border used on DS-style buttons/rows (see PICTOCHAT / DS
// Download Play buttons on the real DS System Menu)
#define COL_ROW_BORDER          0x000000

// Thickness of the flat border drawn around each row
#define ROW_BORDER_W            2.f

// The top edge is drawn thicker than the other three sides, matching the
// real DS System Menu's button chrome
#define ROW_BORDER_TOP_W        3.f

// The selected row is marked with blue L-shaped corner brackets drawn just
// outside its border, matching the real DS System Menu's selection
// highlight (see the "Pokemon Black Version" row on the DS Home Menu),
// rather than recoloring the row's own border.
#define COL_SELECTION_CORNER       0x3c6773
#define SELECTION_CORNER_LEN       14.f
#define SELECTION_CORNER_THICKNESS 3.f
#define SELECTION_CORNER_OUTSET    3.f

// Which media types loadTitles() scans for launchable titles
#define SHOULD_ITERATE_GAMECARD true
#define SHOULD_ITERATE_NAND true
#define SHOULD_ITERATE_SDCARD true

// Max length (incl. null terminator) of a title's display name, as stored
// in titleGame::name
#define MAX_TITLE_NAME 255