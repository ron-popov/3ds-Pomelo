#include <3ds.h>
// #include <citro2d.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <wchar.h>

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
    printf("  Module  : %lu\n", R_MODULE(res));
    printf("  Level   : %lu\n", R_LEVEL(res));
    printf("  Summary : %lu\n", R_SUMMARY(res));
    printf("  Desc    : %lu\n", R_DESCRIPTION(res));
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
    switch(hook) {
        case APTHOOK_ONSUSPEND:
            printf("Got apt hook APTHOOK_ONSUSPEND\n");
            break;
        case APTHOOK_ONRESTORE:
            printf("Got apt hook APTHOOK_ONRESTORE\n");
            break;
        case APTHOOK_ONSLEEP:
            printf("Got apt hook APTHOOK_ONSLEEP\n");
            break;
        case APTHOOK_ONWAKEUP:
            printf("Got apt hook APTHOOK_ONWAKEUP\n");
            break;
        case APTHOOK_ONEXIT:
            printf("Got apt hook APTHOOK_ONEXIT\n");
            break;
        case APTHOOK_COUNT:
            printf("Got apt hook APTHOOK_COUNT\n");
            break;
        default:
            printf("Got apt hook UNKNOWN\n");
            break;
    }
}

// This handles messages from other applets
void aptMessageCallback(void* user, NS_APPID sender, void* msg, size_t msgsize) {
    printf("Got message from other system applet\n");
}

int main(int argc, char* argv[]) {

    gfxInitDefault();

    consoleDebugInit(debugDevice_NULL);

	consoleInit(GFX_TOP, &topScreen);
    consoleInit(GFX_BOTTOM, &bottomScreen);

    consoleSelect(&topScreen);

    Result temp_res;
    titleGame gamecardGame = {
        .titleId = 0x00,
        .mediaType = MEDIATYPE_GAME_CARD,
        .name = "Gamecard - No Game Inserted"
    };

    printf("rpopov custom homemenu!\n");
    debug_printf("rpopov custom homemenu! - DEBUG MESSAGE");

    // Register apt hook
    aptHook(&homemenuAptHookCookie, aptCallback, NULL);
    aptSetMessageCallback(&aptMessageCallback, NULL);

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

    // Find game in gamecard and save it's info, it will be launched later
    {
        debug_printf("Searching for title installed on gamecard");
        // Get gamecard title id
        u32 title_found_gamecard = 0;
        u64 gamecard_title_id[1];

        temp_res = AM_GetTitleList(&title_found_gamecard, MEDIATYPE_GAME_CARD, 1, gamecard_title_id);
        if (temp_res != 0) {
            print_error_code_verbose("AM_GetTitleList GAMECARD", temp_res);
        }

        if (title_found_gamecard) {
            debug_printf("Found title %#018llx on gamecard", gamecard_title_id[0]);
            gamecardGame.titleId = gamecard_title_id[0];
            gamecardGame.mediaType = MEDIATYPE_GAME_CARD;
        } else {
            debug_printf("Didn't find title in gamecard");
            return 0;
        }
    }

    // 00040000/0afd5f00

    // Override with hardcoded title-id installed using "install_cia"
    gamecardGame.titleId = 0x0004001000021000;
    gamecardGame.mediaType = MEDIATYPE_NAND;

    // Print title name
    {
        debug_printf("Getting title name for %#018llx", gamecardGame.titleId);
        // Get gamecard title name
        char title_name_gamecard[MAX_TITLE_NAME];
        temp_res = getTitleName(gamecardGame.titleId, gamecardGame.mediaType, title_name_gamecard, MAX_TITLE_NAME);

        if (temp_res) {
            
            strncpy(gamecardGame.name, title_name_gamecard, MAX_TITLE_NAME);

            printf("Gamecard found : %s\n", gamecardGame.name);
            debug_printf("Gamecard found : %s", gamecardGame.name);
        } else {
            printf("Gamecard title %#018llx - failed to get name\n", gamecardGame.titleId);
            debug_printf("Gamecard title %#018llx - failed to get name", gamecardGame.titleId);
            return 0;
        }
    }

    // Close handle to am system module
    {
        amExit();    
    }

    // Launch a bunch of titles required for the homescreen
    // I took this list from the real home menu
    {
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
            debug_printf("Launching system module %#018llx", titleIdsToLaunch[i]);
            u32 proc_id_out = 0;
            temp_res = NS_LaunchTitle(titleIdsToLaunch[i], 0x00, &proc_id_out);
            if (R_FAILED(temp_res)) {
                debug_printf("Failed launching system module %#018llx", titleIdsToLaunch[i]);
                char *error_message = (char*)malloc(256);
                sprintf(error_message, "launch required title (%#018llx)", titleIdsToLaunch[i]);
                print_error_code_verbose(error_message, temp_res);
            } else {
                debug_printf("Launched system module %#018llx", titleIdsToLaunch[i]);
            }
        }

        nsExit();
    }

    bool shouldLaunch = true;

	while(aptMainLoop()) {
        if (!shouldLaunch) {
            continue;
        }

        shouldLaunch = false;

        printf("Launching title in gamecard slot\n");

        // Start game using apt:startapplication
        {
            FS_ProgramInfo gameToLaunchProgramInfo = {
                .programId = gamecardGame.titleId,
                .mediaType = gamecardGame.mediaType
            };

            temp_res = APT_PrepareToStartApplication(&gameToLaunchProgramInfo, 0x00);
            if (R_FAILED(temp_res)) {
                print_error_code_verbose("APT_PrepareToStartApplication", temp_res);
                printf("Continuing even tho error\n");
                // break;
            } else {
                printf("Successfully ran APT_PrepareToStartApplication\n");
            }

            u8 parameter[0x300] = {0};
            
            temp_res = APT_StartApplication(0x300, 0x00, true, &parameter, NULL);
            if (R_FAILED(temp_res)) {
                print_error_code_verbose("APT_StartApplication", temp_res);
                break;
            } else {
                printf("Successfully ran APT_StartApplication\n");
            }
        }

        // Query whether an application is already registered/running
        while (true) {
            bool registered = 0;
            APT_IsRegistered(APPID_APPLICATION, &registered);

            if (registered) {
                printf("Is App Registered %d\n", registered);
                printf("Terminateing GFX\n");

                // Terminate gfx
                gfxExit();

                printf("Terminated GFX\n");
                printf("Waking Up Application\n");
                
                // Waking up application
                temp_res = APT_WakeupApplication();
                if (R_FAILED(temp_res)) {
                    print_error_code_verbose("APT_WakeupApplication", temp_res);
                    break;
                } else {
                    printf("Successfully ran APT_WakeupApplication\n");
                }

                // Now the process goes into sleep mode
                // Just run aptMainLoop again and again, to handle events

                break; // Break from APT_IsRegistered Loop
            }
        }
    }

    gfxExit();
    aptExit();
	return 0;

    // TODO: Reaching here causes a weird error in mikage, keep this and debug sometime
    // This is because svc index 0x51 - svcUnbindInterrupt, is not implemented
}

