#pragma once

#include "3ds/types.h"

#define TOP_SCREEN_WIDTH 400
#define TOP_SCREEN_HEIGHT 240

#define BOTTOM_SCREEN_WIDTH 320
#define BOTTOM_SCREEN_HEIGHT 240

#define MAX_TITLES 64
#define MAX_TITLES_TO_DISPLAY 26


// Games are listed as full-width rows (icon on the left, name on the
// right), stacked vertically and scrollable, like the DS System Menu's
// PICTOCHAT / DS Download Play buttons.

// Left/right margin around each row
#define LIST_MARGIN_X            12.f

// Number of rows visible on screen at once; more can be scrolled to
#define LIST_VISIBLE_ROWS        3.f

// Size of the icon itself inside each row
#define LIST_ICON_SIZE           48.f

// Padding between a row's border and its icon (applies on all four sides)
#define LIST_ICON_PADDING        8.f

// Gap between the icon's right edge and the start of the game name text
#define LIST_TEXT_GAP            10.f

// The scale of the game name text drawn inside each row
#define TEXT_ROW_SCALE           0.7f

// App icon sizes
#define LARGE_ICON_H 48
#define LARGE_ICON_W 48
#define SMALL_ICON_H 24
#define SMALL_ICON_W 24
#define LARGE_ICON_RGB565_CONTENT_SIZE 0x1200

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
#define GRID_CELL_PX             32.f
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

// The selected row's border uses ds.css's --color-ds-blue accent and a
// thicker stroke, instead of the plain black border, as its highlight
#define COL_ROW_SELECTED_BORDER 0x0059F3
#define ROW_SELECTED_BORDER_W   3.f
#define ROW_SELECTED_BORDER_TOP_W 4.f

#define TITLE_ID_SYSTEM_MODULE_AM 0x0004013000001502

#define SHOULD_ITERATE_GAMECARD true
#define SHOULD_ITERATE_NAND true
#define SHOULD_ITERATE_SDCARD true

#define MAX_TITLE_NAME 255

// List of titles ids that the homemenu has to launch during startup
static const u64 TitleIdsToLaunch[] = {
    0x4013000001d02, 0x4013000001802, 0x4013000001a02, 0x4013000001502, 0x4013000001c02, 
    0x4013000002702, 0x4013000001602, 0x4013000002002, 0x4013000003302, 0x4013000002d02, 
    0x4013000002e02, 0x4013000002902, 0x4013000002f02, 0x4013000002602, 0x4013000002402, 
    0x4013000003202, 0x4013000003402, 0x4013000003502, 0x4013000002b02, 0x4013000002c02, 
    0x4013000002802
};