#include <3ds.h>
// #include <citro2d.h>

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


#define SCREEN_WIDTH 400
#define SCREEN_HEIGHT 240

#define BOTTOM_SCREEN_WIDTH 320
#define BOTTOM_SCREEN_HEIGHT 240

#define MAX_TITLE_NAME 255
#define MAX_TITLES 64
#define MAX_TITLES_TO_DISPLAY 26

#define TITLE_ID_SYSTEM_MODULE_AM_EU 0x0004013000001502

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define SHOULD_ITERATE_NAND false
#define SHOULD_ITERATE_SDCARD true

// Homemenu heap size is different from regular app heap size
u32 __ctru_heap_size        = 0x304000;
u32 __ctru_linear_heap_size = 0xb64000;

static aptHookCookie homemenuAptHookCookie;
static PrintConsole topScreen, bottomScreen;

typedef struct {
    u64 titleId;
    FS_MediaType mediaType;
    char name[MAX_TITLE_NAME];
} titleGame;


// This handles apt callbacks to our process
void aptCallback(APT_HookType hook, void* param) {
    log_debug("Got APT callback");

    switch(hook) {
        case APTHOOK_ONSUSPEND:
            log_debug("Got apt hook APTHOOK_ONSUSPEND");
            break;
        case APTHOOK_ONRESTORE:
            log_debug("Got apt hook APTHOOK_ONRESTORE");
            break;
        case APTHOOK_ONSLEEP:
            log_debug("Got apt hook APTHOOK_ONSLEEP");
            break;
        case APTHOOK_ONWAKEUP:
            log_debug("Got apt hook APTHOOK_ONWAKEUP");
            break;
        case APTHOOK_ONEXIT:
            log_debug("Got apt hook APTHOOK_ONEXIT");
            break;
        case APTHOOK_COUNT:
            log_debug("Got apt hook APTHOOK_COUNT");
            break;
        default:
            log_debug("Got apt hook UNKNOWN");
            break;
    }
}

// This handles messages from other applets
void aptMessageCallback(void* user, NS_APPID sender, void* msg, size_t msgsize) {
    log_debug("Got message from other system applet");
}

void aptSignalCallback(APT_Signal signal) {
    switch (signal) {
        case APTSIGNAL_POWERBUTTON:
        case APTSIGNAL_POWERBUTTON2: // Shutdown the system
            log_debug("Initiating shutdown due to power button press");
            hardwareTimerSleep(2); // Give the console a moment to flush the log
            ptmSysmInit();
            PTMSYSM_ShutdownAsync(0);
            break;
        case APTSIGNAL_HOMEBUTTON2:
        case APTSIGNAL_HOMEBUTTON:
            log_debug("Got APT Home button press signal");
            break;
        default:
            log_debug("Got APT signal 0x%x", signal);
            break;
        }
}

/// libctru weak function overriding, the functions here are defined as weak in libctru
/// The fact they are defined here, overrides the libctru implementation, each function is overriden for different purposes

// This function is used to debug apt messages, we are overriding it to use our logger
void _aptDebug(int a, int b) {
    log_debug("APT Debug 0x%x 0x%x", a, b);
}

// Overriding __appInit to remove the hidInit from this section, full explanation next to hidInit call in "main"
void __appInit(void)
{
    // Initialize services
    srvInit();
    fsInit();
    archiveMountSdmc();
    aptInit();
}


/// Main Function
int main(int argc, char* argv[]) {

    // This var will be used when needing to get result from IPC call
    Result temp_res;



    // This call is here to trick the compiler into adding the decoy code to the final binary
    inject_loader_decoys();




    // Init gfx stuff
    gfxInitDefault();

	consoleInit(GFX_TOP, &topScreen);
    consoleInit(GFX_BOTTOM, &bottomScreen);

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
    titleGame *games = malloc(MAX_TITLES * sizeof(titleGame));
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

    // Iterate over gamecard games - single one
    {
        log_debug("Iterating over gamecard games");
        printf("Iterating over gamecard games\n");

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
            log_debug("Gamecard has title id %#018llx", gamecard_title_id[0]);

            // Get gamecard title name
            char title_name_gamecard[MAX_TITLE_NAME];
            temp_res = getTitleName(gamecard_title_id[0], MEDIATYPE_GAME_CARD, title_name_gamecard, MAX_TITLE_NAME);

            if (temp_res) {
                log_debug("Found Gamecard title %#018llx - %s", gamecard_title_id[0], title_name_gamecard);

                gamecardTitleGame.titleId = gamecard_title_id[0];
                strncpy(gamecardTitleGame.name, title_name_gamecard, MAX_TITLE_NAME);
            } else {
                log_debug("Gamecard title %#018llx - failed to get name", gamecard_title_id[0]);
            }
        }

        games[games_counter] = gamecardTitleGame;
        games_counter++;
    }

    // Iterate over nand titles and fetch name of each installed title
    if (SHOULD_ITERATE_NAND) {
        log_debug("Iterating over NAND titles");
        printf("Iterating over NAND games\n");

        // Get list of installed titles
        u32 titles_found_nand = 0;
        u64 title_ids[128];
        temp_res = AM_GetTitleList(&titles_found_nand, MEDIATYPE_NAND, 128, title_ids);
        if (temp_res != 0) {
            log_debug("AM_GetTitleList Failed, Result 0x%lx", temp_res);
            print_error_code_verbose("AM_GetTitleList", temp_res);
            return 0;
        }

        log_debug("Found %lu title ids in NAND", titles_found_nand);

        // Get name of each title
        for(u32 i = 0; i < titles_found_nand; i++){

            if (games_counter == MAX_TITLES){
                printf("Finished games limit in nand\n");
                log_debug("Finished games limit in nand");
                break;
            }

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

                log_debug("Found NAND title %#018llx : %s", nandTitleGame.titleId, nandTitleGame.name);

                games[games_counter] = nandTitleGame;
                games_counter++;

            } else {
                // printf("%02lu title %#018llx - failed to get name\n", i, title_ids[i]);
                log_debug("%02lu title %#018llx - failed to get name", i, title_ids[i]);
            }
        }
    }

    // Iterate over sdcard titles and fetch name of each installed title
    if (SHOULD_ITERATE_SDCARD) {
        log_debug("Iterating over sdcard titles");
        printf("Iterating over sdcard games\n");

        // Get list of installed titles
        u32 titles_found_sd = 0;
        u64 title_ids[128];
        temp_res = AM_GetTitleList(&titles_found_sd, MEDIATYPE_SD, 128, title_ids);
        if (temp_res != 0) {
            log_debug("AM_GetTitleList Failed, Result 0x%lx", temp_res);
            print_error_code_verbose("AM_GetTitleList", temp_res);
            return 0;
        }

        log_debug("Found %lu title ids in SD", titles_found_sd);

        // Get name of each title
        for(u32 i = 0; i < titles_found_sd; i++){

            if (games_counter == MAX_TITLES){
                printf("Finished games limit in sdcard\n");
                log_debug("Finished games limit in sdcard");
                break;
            }

            if (!shouldDisplayTitle(title_ids[i])){
                continue;
            }

            char title_name[MAX_TITLE_NAME] = {0};
            temp_res = getTitleName(title_ids[i], MEDIATYPE_SD, title_name, MAX_TITLE_NAME);
            if (temp_res) {
                // printf("%02lu title %#018llx - %s\n", i, title_ids[i], title_name);
                titleGame sdTitleGame = {
                    .titleId = title_ids[i],
                    .mediaType = MEDIATYPE_SD,
                    .name = ""
                };

                strncpy(sdTitleGame.name, title_name, MAX_TITLE_NAME);

                log_debug("Found SD title %#018llx : %s", sdTitleGame.titleId, sdTitleGame.name);

                games[games_counter] = sdTitleGame;
                games_counter++;

            } else {
                // printf("%02lu title %#018llx - failed to get name\n", i, title_ids[i]);
                log_debug("%02lu title %#018llx - failed to get name", i, title_ids[i]);
            }
        }
    }

    log_debug("Finished iterating");
    printf("Finished iterating\n");

    // Close handle to am system module
    {
        amExit();    
    }

    int selected_game_index = 0;
    bool is_first_run = true;

    while(true) {

        aptMainLoop();

        // If the homemenu is not in the forefront, meaning there is an application / applet running
        // don't handle key inputs, as it will result in weird behaviour
        if (GetState() == STATE_BACKGROUND) { 
            continue;
        }

        hidScanInput();
        u32 kDown = hidKeysDown();
        
        if (kDown || is_first_run) {
            is_first_run = false;

            // Handle kDown
            switch (kDown) {
                case KEY_DDOWN: // Go down in menu
                    selected_game_index++;
                    selected_game_index = MIN(selected_game_index, games_counter - 1);
                    break;
                case KEY_DUP: // Go up in menu
                    selected_game_index--;
                    selected_game_index = MAX(selected_game_index, 0);
                    break;
                case KEY_START: // Turn off console
                    log_debug("Initiating shutdown due to start button press");
                    hardwareTimerSleep(2); // Give the console a moment to flush the log
                    ptmSysmInit();
                    PTMSYSM_ShutdownAsync(0);
                    break;
                case KEY_A: // Launch selected game
                    printf("Launching title id %#018llx\n", games[selected_game_index].titleId);
                    log_debug("Launching title id %#018llx", games[selected_game_index].titleId);

                    titleGame *selectedTitleGame = &games[selected_game_index];
 
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

                    // Query whether an application is already registered/running
                    while (true) {
                        // TODO: Move this code to check user input while waiting for new app to register
                        // This should allow us to turn off the console while waiting for apps to start

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
                            }

                            SetState(STATE_BACKGROUND);

                            break; // Break from APT_IsRegistered Loop
                        }
                    }

                    break; // Break from handleStart case
            }

            // Update only if some key was pressed
            // The contents can't change otherwise
            consoleSelect(&bottomScreen);
            consoleClear();
            for (int i = 0; i < MAX_TITLES_TO_DISPLAY; i++) {
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


