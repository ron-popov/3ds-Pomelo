#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>

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

bool loadSMDHContent(FS_Archive exefsArchive, SMDH *p_smdh, titleGame *titleGameOut) {
	Result res;

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

	Handle smdhFileHandle;

	FS_Path filePath = {PATH_BINARY, sizeof(filePathData), filePathData};

	res = FSUSER_OpenFile(&smdhFileHandle, exefsArchive, filePath, FS_OPEN_READ,
						  0);
	if (R_FAILED(res)) {
		print_error_code_verbose("FSUSER_OpenFile", res);
		FSUSER_CloseArchive(exefsArchive);
		return false;
	}

	// Read the SMDH data
	u32 smdhBytesRead = 0;
	res = FSFILE_Read(smdhFileHandle, &smdhBytesRead, 0, p_smdh, sizeof(SMDH));
	FSFILE_Close(smdhFileHandle);

	// Validate SMDH magic ("SMDH")
	if (p_smdh->magic != 0x48444D53) {
		return false;
	}

	// Pick language (use English = 1, or CFG_LANGUAGE_EN)
	u8 lang = 1; // CFG_LANGUAGE_EN

	// Clean the name before we override it
	memset(titleGameOut->name, 0, sizeof(titleGameOut->name));
	memset(titleGameOut->publisher, 0, sizeof(titleGameOut->publisher));

	// The short description is a UTF-16 string (0x40 u16 chars)
	// Convert to UTF-8 for easier use
	utf16_to_utf8((uint8_t *)titleGameOut->name,
				  p_smdh->titles[lang].shortDescription, MAX_TITLE_NAME - 1);
	titleGameOut->name[MAX_TITLE_NAME - 1] = '\0';

	utf16_to_utf8((uint8_t *)titleGameOut->publisher,
				  p_smdh->titles[lang].publisher, MAX_TITLE_NAME - 1);
	titleGameOut->publisher[MAX_TITLE_NAME - 1] = '\0';

	// Remove non ascii chars
	remove_non_ascii(titleGameOut->name);
	remove_non_ascii(titleGameOut->publisher);

	// PICA200 requires power-of-two dimensions, so allocate 64x64 for a 48x48
	// icon
	if (!C3D_TexInit(&titleGameOut->large_icon_tex, 64, 64, GPU_RGB565))
		return false;

	// The icon is a 48px by 48px morton encoded icon, however, c3d can only
	// have powers of two texture Which means we need to convert the 48px morton
	// icon to a 64px morton texture This function takes care of that
	uint16_t *reencoded_texture_data = linearAlloc(64 * 64 * sizeof(uint16_t));
	copy_icon_to_tex64(reencoded_texture_data,
					   (uint16_t *)p_smdh->large_icon_rgb565);

	// Load the data into the texture
	C3D_TexUpload(&titleGameOut->large_icon_tex, reencoded_texture_data);

	linearFree(reencoded_texture_data);

	// Flush tex icon
	C3D_TexFlush(&titleGameOut->large_icon_tex);

	// Don't blur
	C3D_TexSetFilter(&titleGameOut->large_icon_tex, GPU_NEAREST, GPU_NEAREST);

	return true;
}

bool loadBannerContent(FS_Archive exefsArchive, titleGame *titleGameOut) {
	Result res;
	CBMD cbmd;

	u32 bannerFilePathData[5] = {0};
	bannerFilePathData[0] = 0x00; // is_save_data
	bannerFilePathData[1] =
		0x00; // content_id (in mikage), also known as NCSDPartitionId
	bannerFilePathData[2] =
		0x02; // sub_file_type, used by the function NCCHOpenExeFSSection
	bannerFilePathData[3] = 0x6e6e6162; // name of the section to read (this
										// spells "banner")
	bannerFilePathData[4] = 0x00007265; // name of the section to read (part2)

	FS_Path bannerFilePath = {PATH_BINARY, sizeof(bannerFilePathData),
							  bannerFilePathData};

	Handle cmbdFileHandle;
	res = FSUSER_OpenFile(&cmbdFileHandle, exefsArchive, bannerFilePath,
						  FS_OPEN_READ, 0);
	if (R_FAILED(res)) {
		log_debug("Failed opening banner file for title id %#018llx", titleGameOut->titleId);
		print_error_code_verbose("FSUSER_OpenFile Banner File", res);
		return false;
	} else {
		log_debug("Found banner file for title id %#018llx", titleGameOut->titleId);
	}

	u32 cbmdBytesRead = 0;
	res = FSFILE_Read(cmbdFileHandle, &cbmdBytesRead, 0, &cbmd, sizeof(CBMD));
	log_debug("Read 0x%lx bytes from cbmd file, res 0x%lx", cbmdBytesRead, res);
	log_debug("CBMD Magic 0x%lx, Common CGFX in 0x%lx", cbmd.magic,
			  cbmd.cgfx_offset_common);

	if (cbmd.magic != 0x444d4243) {
		log_debug("CBMD file is invalid");
		FSFILE_Close(cmbdFileHandle);
		return false;
	}

	// We need to find the size of the common cgfx file
	// We will do that by first checking if there is another regional cgfx file in the same cbmd file
	// If there is one, we want to find the lowest offset one
	u32 next_cgfx_offset = 0;
	for (int cgfx_index = 0; cgfx_index < REGIONAL_CGFX_COUNT; cgfx_index++) {
		u32 regional_cgfx_offset = cbmd.cgfx_offset_regional[cgfx_index];
		if (regional_cgfx_offset != 0) {
			next_cgfx_offset = MIN(next_cgfx_offset, regional_cgfx_offset);
		}
	}

	u32 common_cgfx_size = 0;
	if (next_cgfx_offset != 0) {
		// If we have another cgfx file in the same cbmd, the size of the common
		// Is the offset of the regional cgfx file with the smallest offset, minus the offset of the common one
		common_cgfx_size = next_cgfx_offset - cbmd.cgfx_offset_common;
		log_debug("Calculated common cgfx size using regional offset calc - "
				  "0x%lx bytes",
				  common_cgfx_size);
	} else {
		// It meanst the common cgfx is the only one
		// The size of the common cgfx file is the size of the file, minus the offset of the common one
		u64 cbmd_file_size = 0;
		res = FSFILE_GetSize(cmbdFileHandle, &cbmd_file_size);
		if (R_FAILED(res)) {
			log_debug("Couldn't get size of cbmd file for cgfx calc");
			FSFILE_Close(cmbdFileHandle);
			return false;
		}

		log_debug("CBMD File Size - 0x%llx", cbmd_file_size);

		common_cgfx_size = cbmd_file_size - cbmd.cgfx_offset_common;
		log_debug("Calculated common cgfx size using cmbd file size calc - "
				  "0x%lx bytes",
				  common_cgfx_size);
	}

	FSFILE_Close(cmbdFileHandle);

	return true;
}

// Get the name of a title, from the "icon" file in the ExeFS section of the
// title
bool loadTitleMetadata(u64 titleId, FS_MediaType mediaType,
					   titleGame *titleGameOut) {
	log_debug("Get name of title %#018llx (media 0x%x)", titleId, mediaType);

	const FS_ProgramInfo archiveProgramInfo = {.programId = titleId,
											   .mediaType = mediaType};
	FS_Path archivePath = {PATH_BINARY, sizeof(archiveProgramInfo),
						   (void *)&archiveProgramInfo};


	FS_Archive exefsArchive;
	Result res =
		FSUSER_OpenArchive(&exefsArchive, ARCHIVE_SAVEDATA_AND_CONTENT, archivePath);
	if (R_FAILED(res)) {
		print_error_code_verbose("FSUSER_OpenArchive ExeFS", res);
		return false;
	}

	// Load content from the smdh file - game icon + name + publisher
	SMDH *smdh = malloc(sizeof(SMDH));
	bool load_smdh_res = loadSMDHContent(exefsArchive, smdh, titleGameOut);
	free(smdh);

	if (!load_smdh_res) {
		log_debug("failed getting smdh content for title %#018llx", titleId);
		FSUSER_CloseArchive(exefsArchive);
		return false;
	}

	// Load content from banner file - 3d model for top screen
	bool load_banner_res = loadBannerContent(exefsArchive, titleGameOut);

	if (!load_banner_res) {
		log_debug("failed getting banner content for title %#018llx", titleId);
		FSUSER_CloseArchive(exefsArchive);
		return false;
	}

	FSUSER_CloseArchive(exefsArchive);

	if (R_FAILED(res)) {
		return false;
	}

	return true;
}

bool loadTitlesFromMediaType(FS_MediaType mediaType, u8 maxTitleCount,
							 titleGame **games, u8 *games_counter) {
	Result temp_res;

	log_debug("Iterating over titles (media type 0x%x)", mediaType);

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
		strncpy(loadedTitleGame->publisher, "", MAX_TITLE_NAME);

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
		goto error;
	}

	if (SHOULD_ITERATE_SDCARD &&
		!loadTitlesFromMediaType(MEDIATYPE_SD, 128, games, games_counter)) {
		log_debug("Failed iterating over SDCARD titles, exiting");
		goto error;
	}

	if (SHOULD_ITERATE_NAND &&
		!loadTitlesFromMediaType(MEDIATYPE_NAND, 128, games, games_counter)) {
		log_debug("Failed iterating over NAND titles, exiting");
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
