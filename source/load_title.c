#include "load_title.h"
#include "log.h"
#include <stdlib.h>

bool loadSMDHContent(FS_Archive exefsArchive, SMDH *p_smdh,
					 titleGame *titleGameOut) {
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

	// Open CBMD (Banner) file in exefs
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
		log_debug("Failed opening banner file for title id %#018llx",
				  titleGameOut->titleId);
		print_error_code_verbose("FSUSER_OpenFile Banner File", res);
		return false;
	} else {
		log_debug("Found banner file for title id %#018llx",
				  titleGameOut->titleId);
	}

	// Read CBMD file header
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
	// We will do that by first checking if there is another regional cgfx file
	// in the same cbmd file If there is one, we want to find the lowest offset
	// one
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
		// Is the offset of the regional cgfx file with the smallest offset,
		// minus the offset of the common one
		common_cgfx_size = next_cgfx_offset - cbmd.cgfx_offset_common;
		log_debug("Calculated common cgfx size using regional offset calc - "
				  "0x%lx bytes",
				  common_cgfx_size);
	} else {
		// It meanst the common cgfx is the only one
		// The size of the common cgfx file is the size of the file, minus the
		// offset of the common one
		u64 cbmd_file_size = 0;
		res = FSFILE_GetSize(cmbdFileHandle, &cbmd_file_size);
		if (R_FAILED(res)) {
			log_debug("Couldn't get size of cbmd file for cgfx calc, res 0x%lx",
					  res);
			FSFILE_Close(cmbdFileHandle);
			return false;
		}

		log_debug("CBMD File Size - 0x%llx", cbmd_file_size);

		common_cgfx_size = cbmd_file_size - cbmd.cgfx_offset_common;
		log_debug("Calculated common cgfx size using cmbd file size calc - "
				  "0x%lx bytes",
				  common_cgfx_size);
	}

	// Read compresses common cgfx file
	cbmdBytesRead = 0;
	u8 *compresses_common_cgfx_buffer = malloc(common_cgfx_size);
	res = FSFILE_Read(cmbdFileHandle, &cbmdBytesRead, cbmd.cgfx_offset_common,
					  compresses_common_cgfx_buffer, common_cgfx_size);

	if (R_FAILED(res)) {
		log_debug("Couldn't read common cgfx file from cbmd, res 0x%lx", res);
		FSFILE_Close(cmbdFileHandle);
		free(compresses_common_cgfx_buffer);
		return false;
	}

	log_debug("Read 0x%lx bytes from cbmd file for cgfx common", cbmdBytesRead);

	// Uncompress common cgfx file - it's compresses using LZ11
	// We don't know the size of the uncompresses file, according to 3dbrew,
	// it's no more than 0x80000 https: // www.3dbrew.org/wiki/CBMD
	u8 *common_cgfx_buffer = malloc(CGFX_DECOMPRESSES_MAX_SIZE);
	if (!decompress(common_cgfx_buffer, CGFX_DECOMPRESSES_MAX_SIZE, NULL,
					compresses_common_cgfx_buffer, common_cgfx_size)) {
		log_debug("Decompression of common cgfx file failed");
		free(common_cgfx_buffer);
		free(compresses_common_cgfx_buffer);
		FSFILE_Close(cmbdFileHandle);
		return false;
	}
	free(compresses_common_cgfx_buffer);

	// Save the uncompresses buffer to titleGame
	titleGameOut->cgfx_buffer = common_cgfx_buffer;

	// Check the header value
	CGFX_HEADER *cgfx_header = (CGFX_HEADER *)common_cgfx_buffer;
	log_debug("Uncompresses cgfx header magic - 0x%lx", cgfx_header->magic);

	if (cgfx_header->magic != 0x58464743) {
		log_debug("Invalid CGFX header magic");
	} else {
		log_debug("VALID CGFX header magic!");
	}

	log_debug("CGFX Entries count - 0x%lx", cgfx_header->num_entries);

	// Parse data section, usually the first section after the cgfx headers
	DATA_HEADER *data_header =
		(DATA_HEADER *)(common_cgfx_buffer + sizeof(CGFX_HEADER));
	log_debug("Data Header magic - 0x%lx", data_header->magic);

	if (data_header->magic != 0x41544144) {
		log_debug("Invalid data header magic - 0x%lx", data_header->magic);
		FSFILE_Close(cmbdFileHandle);
		return false;
	}

	log_debug("Validated DATA header magic!");

	log_debug("Number of model entries - 0x%lx",
			  data_header->dict_entries[0].num_entries);
	log_debug("Offset of model entries - 0x%lx",
			  data_header->dict_entries[0].offset);

	// Cleanup
	FSFILE_Close(cmbdFileHandle);

	return true;
}

bool renderTitleBanner(titleGame *titleGame, C3D_RenderTarget *renderTarget) {
	log_debug("Rendering banner for title id %#018llx", titleGame->titleId);
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
	Result res = FSUSER_OpenArchive(&exefsArchive, ARCHIVE_SAVEDATA_AND_CONTENT,
									archivePath);
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
		loadedTitleGame->cgfx_buffer = NULL;
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
