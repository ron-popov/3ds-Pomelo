#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "apt_callbacks.h"
#include "consts.h"
#include "draw.h"
#include "log.h"
#include "patch_fixes.h"
#include "state.h"
#include "utils.h"

// Homemenu heap size is different from regular app heap size
u32 __ctru_heap_size = 0x304000;
u32 __ctru_linear_heap_size = 0xb64000;

static aptHookCookie homemenuAptHookCookie;
static PrintConsole topScreen;

// Get the name of a title, from the "icon" file in the ExeFS section of the
// title
bool loadTitleMetadata(u64 titleId, FS_MediaType mediaType,
					   titleGame *titleGameOut) {
	SMDH *smdh = malloc(sizeof(SMDH));

	log_debug("Get name of title %#018llx (media 0x%x)", titleId, mediaType);
	// printf("Get name of title %#018llx (media 0x%x)\n", titleId, mediaType);

	const FS_ProgramInfo archiveProgramInfo = {.programId = titleId,
											   .mediaType = mediaType};
	FS_Path archivePath = {PATH_BINARY, sizeof(archiveProgramInfo),
						   (void *)&archiveProgramInfo};

	// log_debug("Opening FSUSER archive");

	FS_Archive archive;
	Result res =
		FSUSER_OpenArchive(&archive, ARCHIVE_SAVEDATA_AND_CONTENT, archivePath);
	if (R_FAILED(res)) {
		print_error_code_verbose("FSUSER_OpenArchive", res);
		goto cleanup_fail;
	}

	// printf("Ran FSUSER_OpenArchive\n");

	// This is a comment from mikage
	// a new path:
	// * Word 0: NCCH (0) or save data (1)
	// * Word 1: TMD content index or NCSD partition index
	// * Word 2: 0 for romfs (and for save data), 1 for exefs code section, 2
	// for exefs non-code section
	// * Words 3+4: ExeFS section name

	// Open the SMDH (stored as exefs:/icon)
	// Build the file path (file that inside the archive)
	u32 filePathData[5] = {0};
	filePathData[0] = 0x00; // is_save_data
	filePathData[1] =
		0x00; // content_id (in mikage), also known as NCSDPartitionId
	filePathData[2] =
		0x02; // sub_file_type, used by the function NCCHOpenExeFSSection
	filePathData[3] =
		0x6e6f6369; // name of the section to read (this spells "icon")
	filePathData[4] = 0x00000000; // name of the section to read (part2)

	FS_Path filePath = {PATH_BINARY, sizeof(filePathData), filePathData};

	Handle fileHandle;

	// log_debug("Opening FSUSER file");

	res = FSUSER_OpenFile(&fileHandle, archive, filePath, FS_OPEN_READ, 0);
	if (R_FAILED(res)) {
		print_error_code_verbose("FSUSER_OpenFile", res);
		FSUSER_CloseArchive(archive);
		goto cleanup_fail;
	}

	// log_debug("Reading FSUSER file");

	// Read the SMDH data
	u32 bytesRead;
	res = FSFILE_Read(fileHandle, &bytesRead, 0, smdh, sizeof(SMDH));
	FSFILE_Close(fileHandle);
	FSUSER_CloseArchive(archive);

	if (R_FAILED(res)) {
		goto cleanup_fail;
	}

	// Validate SMDH magic ("SMDH")
	if (smdh->magic != 0x48444D53) {
		printf("File magic does not match SMDH\n");
		goto cleanup_fail;
	}

	// Pick language (use English = 1, or CFG_LANGUAGE_EN)
	u8 lang = 1; // CFG_LANGUAGE_EN

	// Clean the name before we override it
	memset(titleGameOut->name, 0, sizeof(titleGameOut->name));

	// The short description is a UTF-16 string (0x40 u16 chars)
	// Convert to UTF-8 for easier use
	utf16_to_utf8((uint8_t *)titleGameOut->name,
				  smdh->titles[lang].shortDescription, MAX_TITLE_NAME - 1);
	titleGameOut->name[MAX_TITLE_NAME - 1] = '\0';

	// Remove non ascii chars
	remove_non_ascii(titleGameOut->name);

	// log_debug("Initiating tex");

	// PICA200 requires power-of-two dimensions, so allocate 64x64 for a 48x48
	// icon
	if (!C3D_TexInit(&titleGameOut->large_icon_tex, 64, 64, GPU_RGB565))
		goto cleanup_fail;

	// log_debug("Reencoding texture");

	// The icon is a 48px by 48px morton encoded icon, however, c3d can only
	// have powers of two texture Which means we need to convert the 48px morton
	// icon to a 64px morton texture This function takes care of that
	uint16_t *reencoded_texture_data = linearAlloc(64 * 64 * sizeof(uint16_t));
	copy_icon_to_tex64(reencoded_texture_data,
					   (uint16_t *)smdh->large_icon_rgb565);

	// log_debug("Copying tex content");

	// Load the data into the texture
	C3D_TexUpload(&titleGameOut->large_icon_tex, reencoded_texture_data);

	linearFree(reencoded_texture_data);

	// Flush tex icon
	// log_debug("Flushing tex");
	C3D_TexFlush(&titleGameOut->large_icon_tex);

	// Don't blur
	// log_debug("Setting filter to tex");
	C3D_TexSetFilter(&titleGameOut->large_icon_tex, GPU_NEAREST, GPU_NEAREST);

	free(smdh);
	return true;

cleanup_fail:
	log_debug("Something failed during getTitleName");
	free(smdh);
	return false;
}

bool loadTitlesFromMediaType(FS_MediaType mediaType, u8 maxTitleCount,
							 titleGame **games, u8 *games_counter) {
	Result temp_res;

	log_debug("Iterating over titles (media type 0x%x)", mediaType);
	printf("Iterating over games (media type 0x%x)\n", mediaType);

	// Get list of installed titles
	u32 titles_found_count = 0;
	u64 *title_ids = malloc(maxTitleCount * sizeof(u64));
	temp_res = AM_GetTitleList(&titles_found_count, mediaType, maxTitleCount,
							   title_ids);
	if (temp_res != 0) {
		log_debug("AM_GetTitleList Failed, Result 0x%lx", temp_res);
		print_error_code_verbose("AM_GetTitleList", temp_res);
		goto error;
	}

	log_debug("Found %lu title ids", titles_found_count);

	// Get name of each title
	for (u32 i = 0; i < titles_found_count; i++) {

		if (*games_counter == MAX_TITLES) {
			printf("Finished games limit\n");
			log_debug("Finished games limit");
			return false;
		}

		if (!shouldDisplayTitle(title_ids[i])) {
			// log_debug("Skipping title %#018llx", title_ids[i]);
			continue;
		}

		titleGame *loadedTitleGame = malloc(sizeof(titleGame));

		loadedTitleGame->titleId = title_ids[i];
		loadedTitleGame->mediaType = mediaType;
		strncpy(loadedTitleGame->name, "", MAX_TITLE_NAME);

		temp_res = loadTitleMetadata(title_ids[i], mediaType, loadedTitleGame);
		if (temp_res) {
			log_debug("Loaded name for title %#018llx : %s",
					  loadedTitleGame->titleId, loadedTitleGame->name);

			games[*games_counter] = loadedTitleGame;
			(*games_counter)++;

		} else {
			log_debug("%02lu title %#018llx - failed to get name", i,
					  title_ids[i]);
		}
	}

	return true;

error:
	free(title_ids);
	return false;
}

bool loadTitles(titleGame **games, u8 *games_counter) {

	// Get handle to AM system module
	log_debug("Getting handle to AM system module");

	// Initialize "application manager" system module - it is used to fetch the
	// list of installed titles
	Result temp_res = amInit();
	if (temp_res != 0) {
		print_error_code_verbose("amInit", temp_res);
		return false;
	}

	// Load the games into the games list
	if (SHOULD_ITERATE_GAMECARD &&
		!loadTitlesFromMediaType(MEDIATYPE_GAME_CARD, 1, games,
								 games_counter)) {
		log_debug("Failed iterating over GAMECARD titles, exiting");
		printf("Failed iterating over GAMECARD titles, exiting\n");
		goto error;
	}

	if (SHOULD_ITERATE_SDCARD &&
		!loadTitlesFromMediaType(MEDIATYPE_SD, 128, games, games_counter)) {
		log_debug("Failed iterating over SDCARD titles, exiting");
		printf("Failed iterating over SDCARD titles, exiting\n");
		goto error;
	}

	if (SHOULD_ITERATE_NAND &&
		!loadTitlesFromMediaType(MEDIATYPE_NAND, 128, games, games_counter)) {
		log_debug("Failed iterating over NAND titles, exiting");
		printf("Failed iterating over NAND titles, exiting\n");
		goto error;
	}

	amExit();
	return true;

error:
	amExit();
	return false;
}

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

	consoleInit(GFX_TOP, &topScreen);
	consoleSelect(&topScreen);

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
			systemModelName(system_model, (char *)&system_name);

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
	titleGame *games[MAX_TITLES];
	u8 games_counter = 0;

	// Start doing gfx stuff
	int selected_game_index = 0;
	bool is_first_run = true;
	int scroll_offset = 0;

	C3D_RenderTarget *bottomRenderTarget = NULL;
	C2D_TextBuf titleNameTextBuf = NULL;
	C2D_Font pomeloFont = NULL;

	float LIST_ROW_GAP_Y = (BOTTOM_SCREEN_HEIGHT -
							(LIST_ROW_H * LIST_VISIBLE_ROWS)) /
						   (LIST_VISIBLE_ROWS + 1);

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

			// Initialize text buffer
			titleNameTextBuf = C2D_TextBufNew(4096); // glyph buffer

			// Load default font
			// pomeloFont = C2D_FontLoadSystem(CFG_REGION_USA);

			// Load font file from disk
			// TODO: Inject this into the binary, probably romfs?
			// pomeloFont = C2D_FontLoad("sdmc:/monogram.bcfnt");
			pomeloFont = C2D_FontLoad("sdmc:/nitrods-font.bcfnt");

			log_debug("Finished iterating");
			printf("Finished iterating\n");

			SetState(STATE_FOREGROUND);

		} else if (state == STATE_FOREGROUND) {
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
				printf("Launching title id %#018llx\n",
					   games[selected_game_index]->titleId);
				log_debug("Launching title id %#018llx",
						  games[selected_game_index]->titleId);

				bool registered = 0;
				APT_IsRegistered(APPID_APPLICATION, &registered);
				if (registered) {
					log_debug("Previous app is still running");
					printf("Previous app is still running\n");
					continue;
				}

				u64 programId;
				u8 mediaType;
				bool pRegistered, pLoadState;
				APT_AppletAttr pAttributes;

				temp_res =
					APT_GetAppletInfo(APPID_APPLICATION, &programId, &mediaType,
									  &pRegistered, &pLoadState, &pAttributes);
				if (R_FAILED(temp_res)) {
					print_error_code_verbose("APT_GetAppletInfo failed\n",
											 temp_res);
				} else {
					log_debug("GetAppletInfo (APP) - 0x%llx\n0x%x | 0x%x | "
							  "0x%x | 0x%x\n",
							  programId, mediaType, pRegistered, pLoadState,
							  pAttributes);
				}

				titleGame *selectedTitleGame = games[selected_game_index];

				FS_ProgramInfo selectedGameProgramInfo = {
					.programId = selectedTitleGame->titleId,
					.mediaType = selectedTitleGame->mediaType};

				// Start game using apt:startapplication
				log_debug("Calling APT_PrepareToStartApplication");
				printf("Calling APT_PrepareToStartApplication\n");
				temp_res = APT_PrepareToStartApplication(
					&selectedGameProgramInfo, 0x00);
				if (R_FAILED(temp_res)) {
					print_error_code_verbose("APT_PrepareToStartApplication",
											 temp_res);
					printf("Continuing even tho error\n");
					// break;
				} else {
					printf("Successfully ran APT_PrepareToStartApplication\n");
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
				printf("Calling APT_StartApplication\n");
				temp_res =
					APT_StartApplication(0x300, 0x00, true, &parameter, NULL);
				if (R_FAILED(temp_res)) {
					print_error_code_verbose("APT_StartApplication", temp_res);
					break;
				} else {
					printf("Successfully ran APT_StartApplication\n");
					log_debug("Successfully ran APT_StartApplication");
				}

				SetState(STATE_WAIT_TO_REGISTER);

				is_first_run = true;

				// I used to have aptWaitForWakeUp(TR_ENABLE) over here, which
				// kinda helped the shutdown to work while pomelo is in the
				// background, I didn't see it get triggered, instead the
				// regular flow just worked Maybe the aptWaitForWakeUp caused
				// the regular flow to work while in the background
			}

			// Auto-scroll to keep selection visible
			if (selected_game_index < scroll_offset)
				scroll_offset = selected_game_index;
			if (selected_game_index >= scroll_offset + LIST_VISIBLE_ROWS)
				scroll_offset = selected_game_index - LIST_VISIBLE_ROWS + 1;

			// Render UI using citro2d
			// Render the scene
			C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
			C2D_TargetClear(bottomRenderTarget,
							rgb_to_C2D_Color32(COL_GRID_DITHER_LIGHT));
			C2D_SceneBegin(bottomRenderTarget);
			C2D_Pomelo_DrawNdsGridDither(BOTTOM_SCREEN_WIDTH,
										 BOTTOM_SCREEN_HEIGHT,
										 rgb_to_C2D_Color32(COL_GRID_DITHER_DARK));

			// Align the coarse grid lines to the row boxes' own edges,
			// instead of an independent fixed tiling, so the background
			// "squares" always line up with the games' rows
			float grid_x_lines[] = {LIST_MARGIN_X,
									BOTTOM_SCREEN_WIDTH - LIST_MARGIN_X};
			float grid_y_lines[(int)LIST_VISIBLE_ROWS * 2];
			for (int row = 0; row < LIST_VISIBLE_ROWS; row++) {
				float row_top =
					LIST_ROW_GAP_Y + row * (LIST_ROW_GAP_Y + LIST_ROW_H);
				grid_y_lines[row * 2] = row_top;
				grid_y_lines[row * 2 + 1] = row_top + LIST_ROW_H;
			}
			C2D_Pomelo_DrawNdsGridLines(
				grid_x_lines, 2, grid_y_lines, (int)LIST_VISIBLE_ROWS * 2,
				BOTTOM_SCREEN_WIDTH, BOTTOM_SCREEN_HEIGHT,
				rgb_to_C2D_Color32(COL_GRID_LINE), GRID_LINE_W);

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

				bool is_selected = game_index == selected_game_index;
				u32 fill_clr = rgb_to_C2D_Color32(COL_ROW_FILL);
				u32 border_clr = rgb_to_C2D_Color32(
					is_selected ? COL_ROW_SELECTED_BORDER : COL_ROW_BORDER);
				float border_w =
					is_selected ? ROW_SELECTED_BORDER_W : ROW_BORDER_W;
				float top_border_w = is_selected ? ROW_SELECTED_BORDER_TOP_W
												 : ROW_BORDER_TOP_W;

				float row_start_x = LIST_MARGIN_X;
				float row_start_y =
					LIST_ROW_GAP_Y + row * (LIST_ROW_GAP_Y + LIST_ROW_H);

				C2D_Pomelo_DrawNdsIconCell(row_start_x, row_start_y,
										   LIST_ROW_W, LIST_ROW_H, fill_clr,
										   border_clr, border_w,
										   top_border_w);

				float icon_x = row_start_x + LIST_ICON_PADDING;
				float icon_y = row_start_y + LIST_ICON_PADDING;

				C2D_Image image = {.tex =
									   &games[game_index]->large_icon_tex,
								   .subtex = &icon_subtex};

				C2D_DrawImageAt(image, icon_x, icon_y, 1.0f, NULL, 1.0f,
								1.0f);

				C2D_Text row_text;
				C2D_TextFontParse(&row_text, pomeloFont, titleNameTextBuf,
								  games[game_index]->name);
				C2D_TextOptimize(&row_text);

				float text_w, text_h;
				C2D_TextGetDimensions(&row_text, TEXT_ROW_SCALE,
									  TEXT_ROW_SCALE, &text_w, &text_h);

				float text_x = icon_x + LIST_ICON_SIZE + LIST_TEXT_GAP;
				float text_y =
					row_start_y + (LIST_ROW_H - text_h) / 2.f;

				C2D_DrawText(&row_text, C2D_WithColor, text_x, text_y, 0,
							 TEXT_ROW_SCALE, TEXT_ROW_SCALE,
							 rgb_to_C2D_Color32(COL_TEXT));
			}

			C3D_FrameEnd(0);

		} else if (state == STATE_WAIT_TO_REGISTER) { // Wait for the launched
													  // app to wakeup
			gspWaitForVBlank();

			bool registered = 0;
			APT_IsRegistered(APPID_APPLICATION, &registered);

			if (registered) {
				log_debug("Is App Registered %d", registered);
				log_debug("Terminating GFX");

				printf("Is App Registered %d\n", registered);
				printf("Terminating GFX\n");

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
				printf("Waking Up Application\n");

				// Waking up application
				temp_res = APT_WakeupApplication();
				if (R_FAILED(temp_res)) {
					print_error_code_verbose("APT_WakeupApplication", temp_res);
				} else {
					log_debug("Successfully ran APT_WakeupApplication");
					printf("Successfully ran APT_WakeupApplication\n");

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
