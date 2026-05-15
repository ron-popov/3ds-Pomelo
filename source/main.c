#include <3ds.h>
// #include <citro2d.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <wchar.h>

// This strings is here to fix luma3ds loader trying to patch a homemenu code.bin
// trying to do some memory replacements and failing
// I reverse engineered it according to the patchCode function in luma3ds "loader"
// https://github.com/LumaTeam/Luma3DS/blob/master/sysmodules/loader/source/patcher.c

static char* PATCHING_BYPASS_REGION_FREE_PATCHING __attribute__((used)) = 
    "\x0A\x0C\x00\x10"
    "\x00\x00\x00\x00";


// This section is using asm word statements because the loader is looking specifically
// at the text section for some of the patching
__attribute__((naked)) void inject_loader_decoys(void) {
    asm volatile (
        "b 1f\n"                    // Jump to label '1' (forward)
        ""                          // SMDH REGION CHECK MANUAL START
        ".word 0xE1110000\n"
        ".word 0x0A000000\n"
        ".word 0xE1A0000D\n"
        ""                          // SMDH REGION CHECK MANUAL END
        ""                          // NDS FLASHCARD WHITELIST START
        ".word 0x00E92DAA\n"
        ".word 0xAAAAAABB\n"
        ".word 0xBBBBBBFF\n"
        ".word 0x08E5D110\n"
        ".word 0x00008D00\n"
        ""                          // NDS FLASHCARD WHITELIST END
        "1:\n"                      // Label '1'
        "bx lr\n"                   // Return
    );
}


#define SCREEN_WIDTH 400
#define SCREEN_HEIGHT 240

#define BOTTOM_SCREEN_WIDTH 320
#define BOTTOM_SCREEN_HEIGHT 240

#define MAX_TITLE_NAME 255

#define TITLE_ID_SYSTEM_MODULE_AM_EU 0x0004013000001502

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

u32 __ctru_heap_size        = 0x304000;
u32 __ctru_linear_heap_size = 0xb64000;

static aptHookCookie homemenuAptHookCookie;
static PrintConsole topScreen, bottomScreen;
static bool isForefront = true; // Is the homemenu in the forefront right now?

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
} titleGame;

void debug_printf(const char* format, ...) {
    char buffer[512]; // Temporary buffer for the formatted string
    va_list args;
    
    va_start(args, format);
    // vsnprintf prevents buffer overflows by limiting write size
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    // Pass the formatted string to your libctru wrapper
    // svcOutputDebugString(buffer, strlen(buffer));
    svcOutputDebugString(buffer, strlen(buffer));
}

void print_error_code_verbose(char* desc, Result res) {
    printf("%s Result 0x%lx\n", desc, res);
    printf("  Module  : %lu | Level   : %lu\n", R_MODULE(res), R_LEVEL(res));
    printf("  Summary : %lu | Desc    : %lu\n", R_SUMMARY(res), R_DESCRIPTION(res));
}

bool getTitleName(u64 titleId, FS_MediaType mediaType, char *nameOut, size_t nameLen) {
    SMDH smdh;
    
    // // Build the archive path (mediaType + titleId)
    // u32 archPath[3] = {
    //     (u32)mediaType,
    //     (u32)((titleId >> 32) & 0xFFFFFFFF),
    //     (u32)(titleId & 0xFFFFFFFF)
    // };

    const FS_ProgramInfo archiveProgramInfo = {
        .programId = titleId, 
        .mediaType = mediaType
    };
    FS_Path archivePath = { PATH_BINARY, sizeof(archiveProgramInfo), (void*)&archiveProgramInfo };

    // printf("About to run FSUSER_OpenArchive\n");

    FS_Archive archive;
    Result res = FSUSER_OpenArchive(&archive, ARCHIVE_SAVEDATA_AND_CONTENT, archivePath);
    if (R_FAILED(res)) {
        print_error_code_verbose("FSUSER_OpenArchive", res);
        return false;
    }

    // printf("Ran FSUSER_OpenArchive\n");

    // This is a comment from mikage
    // a new path:
        // * Word 0: NCCH (0) or save data (1)
        // * Word 1: TMD content index or NCSD partition index
        // * Word 2: 0 for romfs (and for save data), 1 for exefs code section, 2 for exefs non-code section
        // * Words 3+4: ExeFS section name

    // Open the SMDH (stored as exefs:/icon)
    // Build the file path (file that inside the archive)
    u32 filePathData[5] = {0};
    filePathData[0] = 0x00;             // is_save_data
    filePathData[1] = 0x00;             // content_id (in mikage), also known as NCSDPartitionId
    filePathData[2] = 0x02;             // sub_file_type, used by the function NCCHOpenExeFSSection
    filePathData[3] = 0x6e6f6369;       // name of the section to read (this spells "icon")
    filePathData[4] = 0x00000000;       // name of the section to read (part2)

    FS_Path filePath = { PATH_BINARY, sizeof(filePathData), filePathData };

    Handle fileHandle;

    // printf("About to run FSUSER_OpenFile\n");

    res = FSUSER_OpenFile(&fileHandle, archive, filePath, FS_OPEN_READ, 0);
    if (R_FAILED(res)) {
        print_error_code_verbose("FSUSER_OpenFile",  res);
        FSUSER_CloseArchive(archive);
        return false;
    }

    // printf("Ran FSUSER_OpenFile\n");

    // Read the SMDH data
    u32 bytesRead;
    res = FSFILE_Read(fileHandle, &bytesRead, 0, &smdh, sizeof(SMDH));
    FSFILE_Close(fileHandle);
    FSUSER_CloseArchive(archive);

    if (R_FAILED(res)) {
        return false;
    }

    // Validate SMDH magic ("SMDH")
    if (smdh.magic != 0x48444D53) {
        printf("File magic does not match SMDH\n");
        return false;
    }

    // Pick language (use English = 1, or CFG_LANGUAGE_EN)
    u8 lang = 1; // CFG_LANGUAGE_EN
    
    // The short description is a UTF-16 string (0x40 u16 chars)
    // Convert to UTF-8 for easier use
    utf16_to_utf8((uint8_t*)nameOut, smdh.titles[lang].shortDescription, nameLen - 1);
    nameOut[nameLen - 1] = '\0';

    return true;
}

bool shouldDisplayTitle(u64 title_id) {
    u32 title_id_upper = (u32)((title_id >> 32) & 0xFFFFFFFF);

    switch (title_id_upper) {
        case 0x00040010: // System Applications
            return true;
        case 0x0004001b: // System data archives
            return false;
        case 0x00040030: // System Applets
            return false;
        case 0x0004009B: // Shared data archives
            return false;
        case 0x000400DB: // System data archives
            return false;
        case 0x00040130: // System modules
            return false;
        case 0x00040138: // System Firmware
            return false;
        case 0x00040001: // Download play titles
            return true;
        case 0x0004800F: // DSi System Data Archives
            return false;
        default:
            return true;
    }
}

void aptCallback(APT_HookType hook, void* param) {
    debug_printf("Got APT callback");

    switch(hook) {
        case APTHOOK_ONSUSPEND:
            debug_printf("Got apt hook APTHOOK_ONSUSPEND");
            break;
        case APTHOOK_ONRESTORE:
            debug_printf("Got apt hook APTHOOK_ONRESTORE");
            break;
        case APTHOOK_ONSLEEP:
            debug_printf("Got apt hook APTHOOK_ONSLEEP");
            break;
        case APTHOOK_ONWAKEUP:
            debug_printf("Got apt hook APTHOOK_ONWAKEUP");
            break;
        case APTHOOK_ONEXIT:
            debug_printf("Got apt hook APTHOOK_ONEXIT");
            break;
        case APTHOOK_COUNT:
            debug_printf("Got apt hook APTHOOK_COUNT");
            break;
        default:
            debug_printf("Got apt hook UNKNOWN");
            break;
    }
}

// This handles messages from other applets
void aptMessageCallback(void* user, NS_APPID sender, void* msg, size_t msgsize) {
    debug_printf("Got message from other system applet");
}

void _aptDebug(int a, int b) {
    debug_printf("APT Debug %#x %#x\n", a, b);
}

void hardwareTimerSleep(u8 seconds) {
    Handle timer;
    svcCreateTimer(&timer, RESET_ONESHOT);
    svcSetTimer(timer, seconds * 1000000000, 0);
    svcWaitSynchronization(timer, -1); // Wait for the timer event
    svcCloseHandle(timer);
}

// Overriding __appInit to remove the hidInit from this section, full explanation next to hidInit call in "main"
void __appInit(void)
{
    // Initialize services
    srvInit();
    aptInit();
    fsInit();
    archiveMountSdmc();
}


int main(int argc, char* argv[]) {

    inject_loader_decoys();

    gfxInitDefault();

	consoleInit(GFX_TOP, &topScreen);
    consoleInit(GFX_BOTTOM, &bottomScreen);

    consoleSelect(&topScreen);

    isForefront = true;

    titleGame games[16];
    u8 games_counter = 0;

    Result temp_res;

    printf("Starting Pomelo!\n");

    consoleDebugInit(debugDevice_NULL);
    debug_printf("Starting Pomelo! - DEBUG MESSAGE");

    // Register apt hook
    aptHook(&homemenuAptHookCookie, aptCallback, NULL);
    aptSetMessageCallback(&aptMessageCallback, NULL);

    // Launch a bunch of titles required for the homescreen
    // I took this list from the real home menu
    {
        debug_printf("Launching system modules");
        // Get handle to NS system module
        // NS is used to initialize all sorts of modules the homemenu is supposed to launch
        {
            temp_res = nsInit();
            if (temp_res != 0) {
                print_error_code_verbose("nsInit", temp_res);
            } 
        }

        u64 titleIdsToLaunch[] = {
            0x4013000001d02, 0x4013000001802, 0x4013000001a02, 0x4013000001502, 0x4013000001c02, 
            0x4013000002702, 0x4013000001602, 0x4013000002002, 0x4013000003302, 0x4013000002d02, 
            0x4013000002e02, 0x4013000002902, 0x4013000002f02, 0x4013000002602, 0x4013000002402, 
            0x4013000003202, 0x4013000003402, 0x4013000003502, 0x4013000002b02, 0x4013000002c02, 
            0x4013000002802
        };

        for (int i = 0; i < sizeof(titleIdsToLaunch) / sizeof(u64); i++) {
            u32 proc_id_out = 0;
            temp_res = NS_LaunchTitle(titleIdsToLaunch[i], 0x00, &proc_id_out);
            if (R_FAILED(temp_res)) {
                debug_printf("Failed launching system module %#018llx", titleIdsToLaunch[i]);
                char *error_message = (char*)malloc(256);
                sprintf(error_message, "launch required title (%#018llx)", titleIdsToLaunch[i]);
                print_error_code_verbose(error_message, temp_res);
            }
        }

        debug_printf("Finished launching system modules");

        nsExit();
    }

    // This must come after launching the titles, because one of the titles that is being launcher
    // is the InfraRed module, which HID requires, hidInit hangs if the IR module is not running
    hidInit();

    // Run the "am" system module title, before getting it's handle
    // It is used to iterate the installed titles
    {
        debug_printf("Launching AM system module");
        // Get handle to PM system module
        // PM is used to initialize the AM module
        {
            // Initialize "Process Application Manager" system module
            // It is useful for launching more titles / system modules
            temp_res = pmAppInit();
            if (temp_res != 0) {
                print_error_code_verbose("pmappInit", temp_res);
            } 
        }

        const FS_ProgramInfo amProgramInfo = {
            .programId = TITLE_ID_SYSTEM_MODULE_AM_EU, 
            .mediaType = MEDIATYPE_NAND
        };

        temp_res = PMAPP_LaunchTitle(&amProgramInfo, 0x00);
        if (R_FAILED(temp_res)) {
            print_error_code_verbose("launch application manager", temp_res);
        }

        pmAppExit();
    }

    // Get handle to AM system module
    {
        debug_printf("Getting handle to AM system module");
        // Initialize "application manager" system module - it is used to fetch the list of installed titles
        temp_res = amInit();
        if (temp_res != 0) {
            print_error_code_verbose("amInit", temp_res);
        } 
    }

    // Iterate over gamecard games - single one
    {
        // Get gamecard title id
        u32 title_found_gamecard = 0;
        u64 gamecard_title_id[1];
        temp_res = AM_GetTitleList(&title_found_gamecard, MEDIATYPE_GAME_CARD, 1, gamecard_title_id);
        if (temp_res != 0) {
            print_error_code_verbose("AM_GetTitleList GAMECARD", temp_res);
        }

        // Get name for the title
        titleGame gamecardTitleGame = {
            .titleId = 0x00,
            .mediaType = MEDIATYPE_GAME_CARD,
            .name = "Gamecard - No Game Inserted"
        };

        if (title_found_gamecard) {
            debug_printf("Gamecard has title id %#018llx", gamecard_title_id[0]);

            // Get gamecard title name
            char title_name_gamecard[MAX_TITLE_NAME];
            temp_res = getTitleName(gamecard_title_id[0], MEDIATYPE_GAME_CARD, title_name_gamecard, MAX_TITLE_NAME);

            if (temp_res) {
                debug_printf("Found Gamecard title %#018llx - %s", gamecard_title_id[0], title_name_gamecard);

                gamecardTitleGame.titleId = gamecard_title_id[0];
                strncpy(gamecardTitleGame.name, title_name_gamecard, MAX_TITLE_NAME);
            } else {
                printf("Gamecard title %#018llx - failed to get name\n", gamecard_title_id[0]);
                debug_printf("Gamecard title %#018llx - failed to get name", gamecard_title_id[0]);
            }
        }

        games[games_counter] = gamecardTitleGame;
        games_counter++;
    }

    // Iterate over nand titles and fetch name of each installed title
    {
        // Get list of installed titles
        u32 titles_found_nand = 0;
        u64 title_ids[128];
        temp_res = AM_GetTitleList(&titles_found_nand, MEDIATYPE_NAND, 128, title_ids);
        if (temp_res != 0) {
            printf("AM_GetTitleList Result 0x%lx\n", temp_res);
        }

        debug_printf("Found %lu title ids in NAND", titles_found_nand);

        // Get name of each title
        for(u32 i = 0; i < titles_found_nand; i++){

            if (!shouldDisplayTitle(title_ids[i])){
                continue;
            }

            char title_name[MAX_TITLE_NAME] = {0};
            temp_res = getTitleName(title_ids[i], MEDIATYPE_NAND, title_name, MAX_TITLE_NAME);
            if (temp_res) {
                // printf("%02lu title %#018llx - %s\n", i, title_ids[i], title_name);
                titleGame nandTitleGame = {
                    .titleId = title_ids[i],
                    .mediaType = MEDIATYPE_NAND,
                    .name = ""
                };

                strncpy(nandTitleGame.name, title_name, MAX_TITLE_NAME);

                debug_printf("Found NAND title %#018llx : %s", nandTitleGame.titleId, nandTitleGame.name);

                games[games_counter] = nandTitleGame;
                games_counter++;

            } else {
                printf("%02lu title %#018llx - failed to get name\n", i, title_ids[i]);
                debug_printf("%02lu title %#018llx - failed to get name", i, title_ids[i]);
            }
        }
    }

    // Close handle to am system module
    {
        amExit();    
    }

    int selected_game_index = 0;
    bool is_first_run = true;

    while(aptMainLoop()) {

        // If the homemenu is not in the forefront, meaning there is an application / applet running
        // don't handle key inputs, as it will result in weird behaviour
        if (!isForefront) { 
            continue;
        }

        hidScanInput();
        u32 kDown = hidKeysDown();
        
        if (kDown || is_first_run) {
            is_first_run = false;

            // Handle kDown
            switch (kDown) {
                case KEY_B: // Go down in menu
                    selected_game_index++;
                    selected_game_index = MIN(selected_game_index, games_counter - 1);
                    break;
                case KEY_X: // Go up in menu
                    selected_game_index--;
                    selected_game_index = MAX(selected_game_index, 0);
                    break;
                case KEY_SELECT: // Exit
                    return 0;
                case KEY_START: // Launch selected game
                    printf("Launching title id %#018llx\n", games[selected_game_index].titleId);
                    debug_printf("Launching title id %#018llx", games[selected_game_index].titleId);

                    titleGame *selectedTitleGame = &games[selected_game_index];
 
                    FS_ProgramInfo selectedGameProgramInfo = {
                        .programId = selectedTitleGame->titleId,
                        .mediaType = selectedTitleGame->mediaType
                    };

                    // Start game using apt:startapplication
                    {
                        debug_printf("Calling APT_PrepareToStartApplication");
                        temp_res = APT_PrepareToStartApplication(&selectedGameProgramInfo, 0x00);
                        if (R_FAILED(temp_res)) {
                            print_error_code_verbose("APT_PrepareToStartApplication", temp_res);
                            printf("Continuing even tho error\n");
                            // break;
                        } else {
                            printf("Successfully ran APT_PrepareToStartApplication\n");
                            debug_printf("Successfully ran APT_PrepareToStartApplication");
                        }

                        u8 parameter[0x300] = {0};
                        
                        debug_printf("Calling APT_StartApplication");
                        temp_res = APT_StartApplication(0x300, 0x00, true, &parameter, NULL);
                        if (R_FAILED(temp_res)) {
                            print_error_code_verbose("APT_StartApplication", temp_res);
                            break;
                        } else {
                            printf("Successfully ran APT_StartApplication\n");
                            debug_printf("Successfully ran APT_StartApplication");
                        }
                    }

                    // Query whether an application is already registered/running
                    while (true) {
                        bool registered = 0;
                        APT_IsRegistered(APPID_APPLICATION, &registered);

                        if (registered) {
                            printf("Is App Registered %d\n", registered);
                            printf("Terminating GFX\n");

                            // Terminate gfx
                            gfxExit();

                            printf("Waking Up Application\n");
                            
                            // Waking up application
                            temp_res = APT_WakeupApplication();
                            if (R_FAILED(temp_res)) {
                                print_error_code_verbose("APT_WakeupApplication", temp_res);
                                break;
                            } else {
                                printf("Successfully ran APT_WakeupApplication\n");
                            }

                            isForefront = false;

                            break; // Break from APT_IsRegistered Loop
                        }
                    }

                    break; // Break from handleStart case
            }

            // Update only if some key was pressed
            // The contents can't change otherwise
            consoleSelect(&bottomScreen);
            consoleClear();
            for (int i = 0; i < (sizeof(games) / sizeof(titleGame)); i++) {
                titleGame game = games[i];
                if (game.name[0] == 0) {
                    // No more games to display
                    break;
                }


                if (i == selected_game_index) {
                    printf("* %s\n", game.name);
                } else {
                    printf("  %s\n", game.name);
                }                
            }
            consoleSelect(&topScreen);
        }
    }

    gfxExit();
    aptExit();
	return 0;

    // TODO: Reaching here causes a weird error in mikage, keep this and debug sometime
    // This is because svc index 0x51 - svcUnbindInterrupt, is not implemented
}

