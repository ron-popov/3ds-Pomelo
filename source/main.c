#include <3ds.h>
#include <citro3d.h>
#include <citro2d.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <wchar.h>

#include "log.h"
#include "utils.h"
#include "patch_fixes.h"
#include "state.h"
#include "draw.h"
#include "consts.h"
#include "apt_callbacks.h"

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

// Homemenu heap size is different from regular app heap size
u32 __ctru_heap_size        = 0x304000;
u32 __ctru_linear_heap_size = 0xb64000;

static aptHookCookie homemenuAptHookCookie;
static PrintConsole topScreen;

bool loadTitles(FS_MediaType mediaType, titleGame** games, u8* games_counter) {
    Result temp_res;

    log_debug("Iterating over titles (media type 0x%x)", mediaType);
    printf("Iterating over games (media type 0x%x)\n", mediaType);

    // Get list of installed titles
    u32 titles_found_count = 0;
    u64 title_ids[128];
    temp_res = AM_GetTitleList(&titles_found_count, mediaType, 128, title_ids);
    if (temp_res != 0) {
        log_debug("AM_GetTitleList Failed, Result 0x%lx", temp_res);
        print_error_code_verbose("AM_GetTitleList", temp_res);
        return false;
    }

    log_debug("Found %lu title ids", titles_found_count);

    // Get name of each title
    for(u32 i = 0; i < titles_found_count; i++) {

        if (*games_counter == MAX_TITLES){
            printf("Finished games limit\n");
            log_debug("Finished games limit");
            return false;
        }

        if (!shouldDisplayTitle(title_ids[i])){
            log_debug("Skipping title %#018llx", title_ids[i]);
            continue;
        }

        titleGame* loadedTitleGame = malloc(sizeof(titleGame));

        loadedTitleGame->titleId = title_ids[i];
        loadedTitleGame->mediaType = mediaType;
        strncpy(loadedTitleGame->name, "", MAX_TITLE_NAME);

        temp_res = loadTitleGame(title_ids[i], MEDIATYPE_NAND, loadedTitleGame);
        if (temp_res) {
            log_debug("Loaded name for title %#018llx : %s", loadedTitleGame->titleId, loadedTitleGame->name);

            games[*games_counter] = loadedTitleGame;
            (*games_counter)++;

        } else {
            log_debug("%02lu nand title %#018llx - failed to get name", i, title_ids[i]);
        }
    }

    return true;
}

/// Main Function
int main(int argc, char* argv[]) {

    // This var will be used when needing to get result from IPC call
    Result temp_res;

    // This call is here to trick the compiler into adding the decoy code to the final binary
    inject_loader_decoys();


    // Init gfx stuff
    gfxInitDefault();

    // Init rendering stuff
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
	C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
	C2D_Prepare();

    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

	consoleInit(GFX_TOP, &topScreen);
    consoleSelect(&topScreen);

    SetState(STATE_FOREGROUND);


    // Setup debug logging stuff
    consoleDebugInit(debugDevice_NULL);
    log_debug("Starting Pomelo!");

    printf("Starting Pomelo!\n\n");

    log_debug("Compiled using C Version %ld", __STDC_VERSION__);

    // Check if we are running on real hardware or mikage
    bool is_mikage = isRunningInEmulator();
    if (is_mikage) {
        printf("Running in Mikage\n");
        log_debug("Running in mikage");

        printf("System Model : %s\n", "Emulator");
        log_debug("System Model : %s", "Emulator");
    } else {
        printf("Running on Real Hardware\n");
        log_debug("Running on Real Hardware");

        temp_res = cfguInit();
        if (R_FAILED(temp_res)) {
            print_error_code_verbose("cfguInit for system model", temp_res);
            return 0;
        }

        u8 system_model;
        temp_res = CFGU_GetSystemModel(&system_model);
        if (R_FAILED(temp_res)) {
            print_error_code_verbose("GetSystemModel", temp_res);
        } else {
            char system_name[32];
            systemModelName(system_model, (char*)&system_name);

            log_debug("System Model Id : %x", system_model);

            printf("System Model : %s\n", system_name);
            log_debug("System Model : %s", system_name);
        }

        cfguExit();
    }





    // Configure APT stuff and hooks
    aptHook(&homemenuAptHookCookie, aptCallback, NULL);
    aptSetMessageCallback(&aptMessageCallback, NULL);
    aptSetSignalCallback(&aptSignalCallback);





    // Start doing homemenu stuff - enumerating titles and their names
    titleGame* games[MAX_TITLES];
    u8 games_counter = 0;
    

    // Launch a bunch of titles required for the homescreen
    // I took this list from the real home menu
    {
        log_debug("Launching system modules");
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
                log_debug("Failed launching system module %#018llx", titleIdsToLaunch[i]);
                char *error_message = (char*)malloc(256);
                sprintf(error_message, "launch required title (%#018llx)", titleIdsToLaunch[i]);
                print_error_code_verbose(error_message, temp_res);
                free(error_message);
            }
        }

        log_debug("Finished launching system modules");

        nsExit();
    }

    // This must come after launching the titles, because one of the titles that is being launcher
    // is the InfraRed module, which HID requires, hidInit hangs if the IR module is not running
    hidInit();

    // Run the "am" system module title, before getting it's handle
    // It is used to iterate the installed titles
    {
        log_debug("Launching AM system module");
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
        log_debug("Getting handle to AM system module");
        // Initialize "application manager" system module - it is used to fetch the list of installed titles
        temp_res = amInit();
        if (temp_res != 0) {
            print_error_code_verbose("amInit", temp_res);
        } 
    }

    if (!loadTitles(MEDIATYPE_NAND, (titleGame**)&games, &games_counter)) {
        log_debug("Failed iterating over NAND titles, exiting");
        printf("Failed iterating over NAND titles, exiting\n");
        return 0;
    }

    // Iterate over gamecard games - single one
    // if (SHOULD_ITERATE_GAMECARD) {
    //     log_debug("Iterating over gamecard games");
    //     printf("Iterating over gamecard games\n");

    //     // Get gamecard title id
    //     u32 title_found_gamecard = 0;
    //     u64 gamecard_title_id[1];
    //     temp_res = AM_GetTitleList(&title_found_gamecard, MEDIATYPE_GAME_CARD, 1, gamecard_title_id);
    //     if (temp_res != 0) {
    //         print_error_code_verbose("AM_GetTitleList GAMECARD", temp_res);
    //     }

    //     titleGame gamecardTitleGame = {
    //         .titleId = 0x00,
    //         .mediaType = MEDIATYPE_GAME_CARD,
    //         .name = "Gamecard - No Game Inserted"
    //     };

    //     if (title_found_gamecard) {
    //         log_debug("Gamecard has title id %#018llx", gamecard_title_id[0]);
    //         gamecardTitleGame.titleId = gamecard_title_id[0];

    //         // Get gamecard title name
    //         temp_res = loadTitleGame(gamecard_title_id[0], MEDIATYPE_GAME_CARD, &gamecardTitleGame);

    //         if (temp_res) {
    //             log_debug("Found Gamecard title %#018llx - %s", gamecardTitleGame.titleId, gamecardTitleGame.name);
    //         } else {
    //             log_debug("Gamecard title %#018llx - failed to get name", gamecardTitleGame.titleId);
    //         }
    //     }

    //     games[games_counter] = gamecardTitleGame;
    //     games_counter++;
    // }

    // Iterate over nand titles and fetch name of each installed title
    // if (SHOULD_ITERATE_NAND) {
    //     log_debug("Iterating over NAND titles");
    //     printf("Iterating over NAND games\n");

    //     // Get list of installed titles
    //     u32 titles_found_nand = 0;
    //     u64 title_ids[128];
    //     temp_res = AM_GetTitleList(&titles_found_nand, MEDIATYPE_NAND, 128, title_ids);
    //     if (temp_res != 0) {
    //         log_debug("AM_GetTitleList Failed, Result 0x%lx", temp_res);
    //         print_error_code_verbose("AM_GetTitleList", temp_res);
    //         return 0;
    //     }

    //     log_debug("Found %lu title ids in NAND", titles_found_nand);

    //     // Get name of each title
    //     for(u32 i = 0; i < titles_found_nand; i++){

    //         if (games_counter == MAX_TITLES){
    //             printf("Finished games limit in nand\n");
    //             log_debug("Finished games limit in nand");
    //             break;
    //         }

    //         if (!shouldDisplayTitle(title_ids[i])){
    //             log_debug("Skipping nand title %#018llx", title_ids[i]);
    //             continue;
    //         }

    //         titleGame* nandTitleGame = malloc(sizeof(titleGame));

    //         nandTitleGame->titleId = title_ids[i];
    //         nandTitleGame->mediaType = MEDIATYPE_NAND;
    //         strncpy(nandTitleGame->name, "", MAX_TITLE_NAME);

    //         temp_res = loadTitleGame(title_ids[i], MEDIATYPE_NAND, nandTitleGame);
    //         if (temp_res) {
    //             log_debug("Found NAND title %#018llx : %s", nandTitleGame->titleId, nandTitleGame->name);

    //             games[games_counter] = nandTitleGame;
    //             games_counter++;

    //         } else {
    //             log_debug("%02lu nand title %#018llx - failed to get name", i, title_ids[i]);
    //         }

    //         printf("CmdBuf Usage (Before Split) %.2f\n", C3D_GetCmdBufUsage());
    //         C3D_FrameSplit(0);
    //         printf("CmdBuf Usage (After Split) %.2f\n", C3D_GetCmdBufUsage());

    //         if (games_counter == 1) {
    //             break;
    //         }
    //     }
    // }

    // Iterate over sdcard titles and fetch name of each installed title
    // if (SHOULD_ITERATE_SDCARD) {
    //     log_debug("Iterating over sdcard titles");
    //     printf("Iterating over sdcard games\n");

    //     // Get list of installed titles
    //     u32 titles_found_sd = 0;
    //     u64 title_ids[128];
    //     temp_res = AM_GetTitleList(&titles_found_sd, MEDIATYPE_SD, 128, title_ids);
    //     if (temp_res != 0) {
    //         log_debug("AM_GetTitleList Failed, Result 0x%lx", temp_res);
    //         print_error_code_verbose("AM_GetTitleList", temp_res);
    //         return 0;
    //     }

    //     log_debug("Found %lu title ids in SD", titles_found_sd);

    //     // Get name of each title
    //     for(u32 i = 0; i < titles_found_sd; i++){

    //         if (games_counter == MAX_TITLES){
    //             printf("Finished games limit in sdcard\n");
    //             log_debug("Finished games limit in sdcard");
    //             break;
    //         }

    //         if (!shouldDisplayTitle(title_ids[i])){
    //             // log_debug("Skipping sdcard title %#018llx", title_ids[i]);
    //             continue;
    //         }

    //         titleGame sdTitleGame = {
    //             .titleId = title_ids[i],
    //             .mediaType = MEDIATYPE_SD,
    //             .name = ""
    //         };
    //         temp_res = loadTitleGame(title_ids[i], MEDIATYPE_SD, &sdTitleGame);
    //         if (temp_res) {
    //             log_debug("Found SD title %#018llx : %s", sdTitleGame.titleId, sdTitleGame.name);

    //             games[games_counter] = sdTitleGame;
    //             games_counter++;

    //         } else {
    //             log_debug("%02lu sdcard title %#018llx - failed to get name", i, title_ids[i]);
    //         }
    //     }
    // }

    log_debug("Finished iterating");
    printf("Finished iterating\n");

    // Close handle to am system module
    {
        amExit();    
    }

    int selected_game_index = 0;
    bool is_first_run = true;
    int scroll_offset = 0;

    // Init render target
    C3D_RenderTarget* bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    if (bottom == NULL) {
        log_debug("Failed initializing bottom screen for rendering!");
        return 0;
    }

    // Initialize text buffer
    C2D_TextBuf buf = C2D_TextBufNew(4096); // glyph buffer

    // Load font
    // C2D_Font font = C2D_FontLoadSystem(CFG_REGION_USA);
    // C2D_Font font = C2D_FontLoadFromMem(&font, 96*13); // system font
    // or load a custom font:
    C2D_Font font = C2D_FontLoad("sdmc:/monogram.bcfnt"); // TODO: Inject this into the binary?

    float GRID_CELL_GAP_W = (BOTTOM_SCREEN_WIDTH - (GRID_CELL_W * GRID_COLS)) / (GRID_COLS + 1);
    float GRID_CELL_GAP_H = (BOTTOM_SCREEN_HEIGHT - GRID_HEADER_H - (GRID_CELL_H * GRID_VISIBLE_ROWS)) / (GRID_VISIBLE_ROWS + 1);


    log_debug("Starting apt loop");

    while(true) {

        gspWaitForVBlank();
        aptMainLoop();
        
        switch (GetState()) {
            case STATE_NONE:
                log_debug("Invalid state reached");
                svcBreak(USERBREAK_USER);
                break;
            case STATE_BACKGROUND:
                // We want to continue running in the background to handle APT events, such as shutdown
                continue;
            case STATE_WAIT_TO_REGISTER:
                bool registered = 0;
                APT_IsRegistered(APPID_APPLICATION, &registered);

                if (registered) {
                    log_debug("Is App Registered %d", registered);
                    log_debug("Terminating GFX");

                    printf("Is App Registered %d\n", registered);
                    printf("Terminating GFX\n");

                    // Terminate gfx
                    gfxExit();

                    log_debug("Waking Up Application");
                    printf("Waking Up Application\n");

                    // Waking up application
                    temp_res = APT_WakeupApplication();
                    if (R_FAILED(temp_res)) {
                        print_error_code_verbose("APT_WakeupApplication", temp_res);
                        break;
                    } else {
                        log_debug("Successfully ran APT_WakeupApplication");
                        printf("Successfully ran APT_WakeupApplication\n");

                        SetState(STATE_BACKGROUND);
                    }
                }
            case STATE_FOREGROUND:
                hidScanInput();
                u32 kDown = hidKeysDown();
                
                if (kDown == 0x00 && !is_first_run) { // If no keys were pressed and it's not the first run, skip this flow
                    break;
                }

                is_first_run = false;

                log_debug("Button press 0x%lx", kDown);
                printf("Button press 0x%lx\n", kDown);

                // Handle kDown
                switch (kDown) {
                    case KEY_DDOWN: // Go down in menu
                        selected_game_index = MIN(selected_game_index + GRID_COLS, (int)games_counter - 1);
                        break;
                    case KEY_DUP: // Go up in menu
                        selected_game_index = MAX(selected_game_index - GRID_COLS, 0);
                        break;
                    case KEY_DLEFT:
                        selected_game_index = MAX(selected_game_index - 1, 0);
                        break;
                    case KEY_DRIGHT:
                        selected_game_index = MIN(selected_game_index + 1, (int)games_counter - 1);
                        break;
                    case KEY_START: // Turn off console
                        log_debug("Initiating shutdown due to start button press");
                        hardwareTimerSleep(2); // Give the console a moment to flush the log
                        ptmSysmInit();
                        PTMSYSM_ShutdownAsync(0);
                        break;
                    case KEY_A: // Launch selected game
                        printf("Launching title id %#018llx\n", games[selected_game_index]->titleId);
                        log_debug("Launching title id %#018llx", games[selected_game_index]->titleId);

                        titleGame *selectedTitleGame = games[selected_game_index];
    
                        FS_ProgramInfo selectedGameProgramInfo = {
                            .programId = selectedTitleGame->titleId,
                            .mediaType = selectedTitleGame->mediaType
                        };

                        // Start game using apt:startapplication
                        {
                            log_debug("Calling APT_PrepareToStartApplication");
                            printf("Calling APT_PrepareToStartApplication\n");
                            temp_res = APT_PrepareToStartApplication(&selectedGameProgramInfo, 0x00);
                            if (R_FAILED(temp_res)) {
                                print_error_code_verbose("APT_PrepareToStartApplication", temp_res);
                                printf("Continuing even tho error\n");
                                // break;
                            } else {
                                printf("Successfully ran APT_PrepareToStartApplication\n");
                                log_debug("Successfully ran APT_PrepareToStartApplication");
                            }

                            u8 parameter[0x300] = {0};
                            
                            log_debug("Calling APT_StartApplication");
                            printf("Calling APT_StartApplication\n");
                            temp_res = APT_StartApplication(0x300, 0x00, true, &parameter, NULL);
                            if (R_FAILED(temp_res)) {
                                print_error_code_verbose("APT_StartApplication", temp_res);
                                break;
                            } else {
                                printf("Successfully ran APT_StartApplication\n");
                                log_debug("Successfully ran APT_StartApplication");
                            }

                            SetState(STATE_WAIT_TO_REGISTER);
                        }

                        // I used to have aptWaitForWakeUp(TR_ENABLE) over here, which kinda helped the shutdown to work
                        // while pomelo is in the background, I didn't see it get triggered, instead the regular flow just worked
                        // Maybe the aptWaitForWakeUp caused the regular flow to work while in the background

                        break; // Break from handleStart case
                }

                // Auto-scroll to keep selection visible
                {
                    int sel_row = selected_game_index / GRID_COLS;
                    if (sel_row < scroll_offset)
                        scroll_offset = sel_row;
                    if (sel_row >= scroll_offset + GRID_VISIBLE_ROWS)
                        scroll_offset = sel_row - GRID_VISIBLE_ROWS + 1;
                }
                
                // Render UI using citro2d
                {
                    log_debug("Starting to render");

                    // Render the scene 
                    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
                    C2D_TargetClear(bottom, rgb_to_C2D_Color32(COL_BG));
                    C2D_SceneBegin(bottom);

                    // Draw seperating line from header to grid
                    C2D_Pomelo_DrawRectangleSingleColor(
                        0, 0, 
                        BOTTOM_SCREEN_WIDTH, GRID_HEADER_H,
                        rgb_to_C2D_Color32(COL_CELL)
                    );

                    // Draw header text
                    C2D_Text header_text;
                    C2D_TextFontParse(&header_text, font, buf, "Mario Kart 7");
                    C2D_TextOptimize(&header_text);

                    C2D_DrawText(&header_text, C2D_WithColor, GRID_CELL_GAP_W + 2, 8, 0, TEXT_HEADER_SCALE, TEXT_HEADER_SCALE, rgb_to_C2D_Color32(COL_TEXT));

                    // Draw grid of cells, not including icons, just the background color of the cells
                    for (int grid_x = 0; grid_x < GRID_COLS; grid_x++) {
                        for (int grid_y = 0; grid_y < GRID_VISIBLE_ROWS; grid_y++) {
                            int game_index = grid_y * GRID_COLS + grid_x;

                            if (game_index >= games_counter)
                                break;

                            u32 cell_color = game_index == selected_game_index ? rgb_to_C2D_Color32(COL_CELL_SELECTED) : rgb_to_C2D_Color32(COL_CELL);

                            float cell_start_x = GRID_CELL_GAP_W + grid_x * (GRID_CELL_GAP_W + GRID_CELL_W);
                            float cell_Start_y = GRID_HEADER_H + GRID_CELL_GAP_H + grid_y * (GRID_CELL_GAP_H + GRID_CELL_H);

                            C2D_Pomelo_DrawRectangleSingleColor(
                                cell_start_x, cell_Start_y,
                                GRID_CELL_W, GRID_CELL_H, 
                                cell_color
                            );

                            C2D_Image image = {
                                .tex = &games[game_index]->large_icon_tex,
                                .subtex = &icon_subtex
                            };

                            C2D_DrawImageAt(image, cell_start_x + GRID_CELL_BORDER, cell_Start_y + GRID_CELL_BORDER, 1.0f, NULL, 1.0f, 1.0f);
                        }
                    }



                    C3D_FrameEnd(0);
                    log_debug("Finished rendering");
                }
            }
        }

    gfxExit();
    aptExit();
    return 0;
}




