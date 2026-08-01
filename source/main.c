#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "apt_callbacks.h"
#include "c3d/renderqueue.h"
#include "consts.h"
#include "draw.h"
#include "log.h"
#include "patch_fixes.h"
#include "state.h"
#include "utils.h"
#include "load_title.h"

// Homemenu heap size is different from regular app heap size
u32 __ctru_heap_size = 0x304000;
u32 __ctru_linear_heap_size = 0xb64000;

static aptHookCookie homemenuAptHookCookie;
static PrintConsole topScreen;


/// Main Function
int main(int argc, char *argv[]) {

	// This var will be used when needing to get result from IPC call
	Result temp_res;

	// This call is here to trick the compiler into adding the decoy code to the
	// final binary
	inject_loader_decoys();

	// Init gfx stuff
	gfxInitDefault();

	// Init rendering stuff
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
	C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
	C2D_Prepare();

	// Setup debug logging stuff
	consoleDebugInit(debugDevice_NULL);
	log_debug("Starting Pomelo!");


	log_debug("Compiled using C Version %ld", __STDC_VERSION__);

	// Check if we are running on real hardware or mikage
	bool is_mikage = isRunningInEmulator();
	if (is_mikage) {
		log_debug("Running in mikage");

		log_debug("System Model : %s", "Emulator");
	} else {
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
			systemModelName(system_model, (char *)&system_name);

			log_debug("System Model Id : %x", system_model);

			log_debug("System Model : %s", system_name);
		}

		cfguExit();
	}

	// Configure APT stuff and hooks
	aptHook(&homemenuAptHookCookie, aptCallback, NULL);
	aptSetMessageCallback(&aptMessageCallback, NULL);
	aptSetSignalCallback(&aptSignalCallback);

	// Start doing homemenu stuff - enumerating titles and their names
	titleGame *games[MAX_TITLES];
	u8 games_counter = 0;

	// Start doing gfx stuff
	int selected_game_index = 0;
	bool is_first_run = true;
	int scroll_offset = 0;

	C3D_RenderTarget *bottomRenderTarget = NULL;
	C3D_RenderTarget *topRenderTarget = NULL;
	C2D_TextBuf titleNameTextBuf = NULL;
	C2D_Font pomeloFont = NULL;

	SetState(STATE_STARTING_POMELO);

	log_debug("Starting apt loop");

	while (true) {

		aptMainLoop();

		enum HomemenuState state = GetState();

		if (state == STATE_NONE) { // There is no state, that is weird
			log_debug("Invalid state reached");
			svcBreak(USERBREAK_USER);
		} else if (state == STATE_STARTING_POMELO) {
			if (!loadTitles((titleGame **)&games, &games_counter)) {
				return 0;
			}

			// Init render target
			bottomRenderTarget = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
			if (bottomRenderTarget == NULL) {
				log_debug("Failed initializing bottom screen for rendering!");
				return 0;
			}

			// Init render target
			topRenderTarget = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
			if (topRenderTarget == NULL) {
				log_debug("Failed initializing top screen for rendering!");
				return 0;
			}

			// Initialize text buffer
			titleNameTextBuf = C2D_TextBufNew(4096); // glyph buffer

			// Load default font
			// pomeloFont = C2D_FontLoadSystem(CFG_REGION_USA);

			// Load font file from disk
			// TODO: Inject this into the binary, probably romfs?
			// pomeloFont = C2D_FontLoad("sdmc:/monogram.bcfnt");
			pomeloFont = C2D_FontLoad("sdmc:/nitrods-font.bcfnt");

			log_debug("Finished iterating");

			SetState(STATE_FOREGROUND);

		} else if (state == STATE_FOREGROUND) {
			bool should_render_screen = true;

			gspWaitForVBlank();
			hidScanInput();
			u32 kDown = hidKeysDown();

			if (kDown == 0x00 &&
				!is_first_run) { // If no keys were pressed and it's
								 // not the first run, skip this flow
				continue;
			}

			is_first_run = false;

			// Handle kDown
			// Games are listed as a single scrollable column now, so
			// up/left and down/right are equivalent ways to move by one row
			switch (kDown) {
			case KEY_DDOWN:
			case KEY_DRIGHT:
				selected_game_index =
					MIN(selected_game_index + 1, (int)games_counter - 1);
				break;
			case KEY_DUP:
			case KEY_DLEFT:
				selected_game_index = MAX(selected_game_index - 1, 0);
				break;
			case KEY_START: // Turn off console
				log_debug("Initiating shutdown due to start button press");
				hardwareTimerSleep(
					2); // Give the console a moment to flush the log
				ptmSysmInit();
				PTMSYSM_ShutdownAsync(0);
				break;
			case KEY_A: // Launch selected game
				log_debug("Launching title id %#018llx",
						  games[selected_game_index]->titleId);

				bool registered = 0;
				APT_IsRegistered(APPID_APPLICATION, &registered);
				if (registered) {
					log_debug("Previous app is still running");
					continue;
				}

				titleGame *selectedTitleGame = games[selected_game_index];

				FS_ProgramInfo selectedGameProgramInfo = {
					.programId = selectedTitleGame->titleId,
					.mediaType = selectedTitleGame->mediaType};

				// Start game using apt:startapplication
				log_debug("Calling APT_PrepareToStartApplication");
				temp_res = APT_PrepareToStartApplication(
					&selectedGameProgramInfo, 0x00);
				if (R_FAILED(temp_res)) {
					print_error_code_verbose("APT_PrepareToStartApplication",
											 temp_res);
					// break;
				} else {
					log_debug("Successfully ran APT_PrepareToStartApplication");
				}

				// Clearing memory allocations of all games
				// Some games will hang during boot if this memory is not
				// released
				for (int i = 0; i < games_counter; i++) {
					log_debug("Freeing texture index 0x%x", i);
					C3D_TexDelete(&games[i]->large_icon_tex);
					log_debug("Freeing game index 0x%x", i);
					free(games[i]);
				}

				games_counter = 0;
				selected_game_index = 0;

				u8 parameter[0x300] = {0};

				log_debug("Calling APT_StartApplication");
				temp_res =
					APT_StartApplication(0x300, 0x00, true, &parameter, NULL);
				if (R_FAILED(temp_res)) {
					print_error_code_verbose("APT_StartApplication", temp_res);
					break;
				} else {
					log_debug("Successfully ran APT_StartApplication");
				}

				
				// Clear the bottom screen via a raw GSP framebuffer write
				// I tried using citro3d but it didn't really work :(
				for (int i = 0; i < 2; i++) {
					u16 fbWidth, fbHeight;
					u8 *fb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &fbWidth,
											   &fbHeight);
					unsigned bpp =
						gspGetBytesPerPixel(gfxGetScreenFormat(GFX_BOTTOM));
					memset(fb, 0, (size_t)fbWidth * fbHeight * bpp);
					gfxFlushBuffers();
					gfxSwapBuffers();
					gspWaitForVBlank();
				}

				SetState(STATE_WAIT_TO_REGISTER);

				is_first_run = true;
				should_render_screen = false;
			}

			if (should_render_screen) {
				C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

				// --- Render top screen
				C3D_FrameDrawOn(topRenderTarget);

				renderTitleBanner(games[selected_game_index], topRenderTarget);

				// --- Render bottom screen

				// Auto-scroll to keep selection visible
				if (selected_game_index < scroll_offset) scroll_offset =
					selected_game_index;
				if (selected_game_index >= scroll_offset + LIST_VISIBLE_ROWS)
					scroll_offset = selected_game_index - LIST_VISIBLE_ROWS + 1;
	
				// Render UI using citro2d
				// Render the scene
				C2D_TargetClear(bottomRenderTarget,
								rgb_to_C2D_Color32(COL_GRID_DITHER_LIGHT));
				C2D_SceneBegin(bottomRenderTarget);
				drawBottomScreenGridBackground();
	
				// Reset the glyph buffer every frame: up to LIST_VISIBLE_ROWS
				// texts get parsed below, and they'd otherwise keep
				// accumulating in the buffer frame after frame
				C2D_TextBufClear(titleNameTextBuf);
	
				// Draw each visible game as a full-width row: icon on the
				// left, name on the right, like the DS System Menu's
				// PICTOCHAT / DS Download Play buttons
				for (int row = 0; row < LIST_VISIBLE_ROWS; row++) {
					int game_index = row + scroll_offset;
	
					if (game_index >= games_counter)
						break;
	
					drawGameRow(row, games[game_index],
							   game_index == selected_game_index,
							   titleNameTextBuf, pomeloFont);
				}

				C3D_FrameEnd(0);
			}
			

		} else if (state == STATE_WAIT_TO_REGISTER) { // Wait for the launched
													  // app to wakeup
			gspWaitForVBlank();

			bool registered = 0;
			APT_IsRegistered(APPID_APPLICATION, &registered);

			if (registered) {
				log_debug("Is App Registered %d", registered);
				log_debug("Terminating GFX");


				if (pomeloFont != NULL) {
					C2D_FontFree(pomeloFont);
				}
				if (titleNameTextBuf != NULL) {
					C2D_TextBufDelete(titleNameTextBuf);
				}
				if (bottomRenderTarget != NULL) {
					C3D_RenderTargetDelete(bottomRenderTarget);
				}

				C2D_Fini();
				C3D_Fini();
				gfxExit();

				log_debug("Waking Up Application");

				// Waking up application
				temp_res = APT_WakeupApplication();
				if (R_FAILED(temp_res)) {
					print_error_code_verbose("APT_WakeupApplication", temp_res);
				} else {
					log_debug("Successfully ran APT_WakeupApplication");
					SetState(STATE_BACKGROUND);
				}
			}
		} else if (state == STATE_BACKGROUND) {
			// Another app is currently running;
			// block until APT
			// wakes us back up (e.g. the launched game exits)

			log_debug("Waiting for wakeup");
			APT_Command wakeup_cmd = aptWaitForWakeUp(TR_APPJUMP);
			log_debug("Woke up from background with APT command 0x%x",
					  wakeup_cmd);
			SetState(STATE_CLOSING_APP);
		} else if (state == STATE_CLOSING_APP) {
			// The running app is responsible for also reading the home button signal
			// And closing itself

			gfxInitDefault();

			// Init rendering stuff
			C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
			C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
			C2D_Prepare();

			consoleInit(GFX_TOP, &topScreen);
			consoleSelect(&topScreen);

			SetState(STATE_STARTING_POMELO);
		}

	} // End of pomelo main loop

	gfxExit();
	aptExit();
	return 0;
}
