#pragma once

#define TOP_SCREEN_WIDTH 400
#define TOP_SCREEN_HEIGHT 240

#define BOTTOM_SCREEN_WIDTH 320
#define BOTTOM_SCREEN_HEIGHT 240

#define MAX_TITLES 64
#define MAX_TITLES_TO_DISPLAY 26


// Number of game icon columns
#define GRID_COLS               5.f

// Number of rows displayed, you will have more rows than that, that can be scrolled to
#define GRID_VISIBLE_ROWS       3.f

// The height of the header where the game name will be displayed
#define GRID_HEADER_H           40.f

// Size of the icon itself inside the grid cell
#define GRID_CELL_ICON_SIZE     48.f

// The border around the icon
#define GRID_CELL_BORDER        4.f

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
#define COL_BG                  0xE8E4DC
#define COL_CELL                0xD4CFC5
#define COL_CELL_SELECTED       0xFFFFFF
#define COL_TEXT                0x222222
#define COL_BORDER              0xB8B3A8
#define COL_BORDER_SELECTED     0x636e72

#define TITLE_ID_SYSTEM_MODULE_AM_EU 0x0004013000001502

#define SHOULD_ITERATE_GAMECARD false
#define SHOULD_ITERATE_NAND false
#define SHOULD_ITERATE_SDCARD false

#define MAX_TITLE_NAME 255