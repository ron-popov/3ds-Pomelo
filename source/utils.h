#ifndef POMELO_UTILS
#define POMELO_UTILS


#include <3ds.h>
#include <stdio.h>
#include <string.h>

#include "log.h"

#define MAX_TITLE_NAME 255

// Structs for parsing title names from
typedef struct {
    u16 shortDescription[0x40];  // The title name (UTF-16)
    u16 longDescription[0x80];   // Subtitle / longer description
    u16 publisher[0x40];         // Publisher name
} SMDH_ApplicationTitle;

typedef struct {
    u32 magic;                          // 0x48444D53 = "SMDH"
    u16 version;
    u16 reserved_1;
    SMDH_ApplicationTitle titles[16];   // One per language
    u8 application_settings[0x30]; // not used, only for filler purposes
    u8 reserved_2[0x08]; // not used, only for filler purposes
    u8 icon_graphics[0x1680]; // not used, only for filler purposes
    // ... icon data follows
} SMDH; // struct size must be 0x36c0 - mikage doesn't allow to read partial sections

typedef struct {
    u64 titleId;
    FS_MediaType mediaType;
    char name[MAX_TITLE_NAME];
    u8* small_icon_buffer;
    u8* large_icon_buffer;
} titleGame;

// Parses title names from the "icon" file in the ExeFS of the title
bool loadTitleGame(u64 titleId, FS_MediaType mediaType, titleGame* titleGameOut);

// Filters out system and un-startable titles
bool shouldDisplayTitle(u64 title_id);

// Sleep using timers, and not using svcSleepThread
// svcSleepThread is not working on real hardware when we are running as the real homemenu
void hardwareTimerSleep(u8 seconds);

// Check if we are currently running in mikage or real hardware
bool isRunningInEmulator(void);

void systemModelName(u8 systemModel, char* nameOut);

#endif