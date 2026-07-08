#pragma once

#include "3ds/types.h"

#define TOP_SCREEN_WIDTH 400
#define TOP_SCREEN_HEIGHT 240

#define BOTTOM_SCREEN_WIDTH 320
#define BOTTOM_SCREEN_HEIGHT 240

#define MAX_TITLES 64
#define MAX_TITLES_TO_DISPLAY 26


// Number of game icon columns
// (dropped from 5 to 4 to make room for a bigger GRID_CELL_BORDER without
// shrinking the fixed 48px icon)
#define GRID_COLS               4.f

// Number of rows displayed, you will have more rows than that, that can be scrolled to
#define GRID_VISIBLE_ROWS       2.f

// The height of the header where the game name will be displayed
#define GRID_HEADER_H           40.f

// Size of the icon itself inside the grid cell
#define GRID_CELL_ICON_SIZE     48.f

// The border around the icon
// (doubled from 4 to 8; only possible without cramping gaps because
// GRID_COLS was dropped to 4 above)
#define GRID_CELL_BORDER        8.f

// The scale of the header text
#define TEXT_HEADER_SCALE       0.8f

// App icon sizes
#define LARGE_ICON_H 48
#define LARGE_ICON_W 48
#define SMALL_ICON_H 24
#define SMALL_ICON_W 24
#define LARGE_ICON_RGB565_CONTENT_SIZE 0x1200

// The size of the icon plus it's border, width and heigh are the same
#define GRID_CELL_H             (GRID_CELL_BORDER + GRID_CELL_ICON_SIZE + GRID_CELL_BORDER)
#define GRID_CELL_W             (GRID_CELL_BORDER + GRID_CELL_ICON_SIZE + GRID_CELL_BORDER)

// Colors (R, G, B)
// NDS-style grid background: a fine 2px/2px dither plus 2px border lines
// every 32px, matching ds.css's `.ds-grid` (https://github.com/notpx/ds.css)
#define COL_GRID_DITHER_LIGHT   0xFFFFFF
#define COL_GRID_DITHER_DARK    0xE3E3E3
#define COL_GRID_LINE           0xC3C3C3
#define COL_CELL                0xD4CFC5
#define COL_CELL_SELECTED       0xFFFFFF
#define COL_TEXT                0x222222
#define COL_BORDER_SELECTED     0x636e72

// Thickness of the DS-style bevel border drawn around each icon cell
#define CELL_BEVEL_BORDER_W     2.f

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