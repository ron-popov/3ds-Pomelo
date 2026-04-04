#include <3ds.h>
#include <citro2d.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define SCREEN_WIDTH 400
#define SCREEN_HEIGHT 240

#define BOTTOM_SCREEN_WIDTH 320
#define BOTTOM_SCREEN_HEIGHT 240

#define MAX_TITLE_NAME 255

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
    char file_name[8];
    u8 file_offset[4];
    u8 file_size[4];    
} ExeFS;

bool getTitleName(u64 titleId, FS_MediaType mediaType, char *nameOut, size_t nameLen) {
    SMDH smdh;
    ExeFS exefs;
    
    // Build the archive path (mediaType + titleId)
    u32 archPath[3] = {
        (u32)mediaType,
        (u32)(titleId >> 32),
        (u32)(titleId & 0xFFFFFFFF)
    };
    FS_Path archivePath = { PATH_BINARY, sizeof(archPath), archPath };

    printf("Opening Archive\n");
    
    FS_Archive archive;
    Result res = FSUSER_OpenArchive(&archive, ARCHIVE_SAVEDATA_AND_CONTENT, archivePath);
    if (R_FAILED(res)) return false;

    // svcSleepThread(4888000000); // 4000.888ms

    printf("Opened Archive\n");
    printf("Opening file in archive\n");

    // svcSleepThread(4888000000); // 4000.888ms

    // Open the SMDH (stored as exefs:/icon)
    // u32 filePathData[5] = { 0x101 };
    u32 filePathData[5] = {0};
    filePathData[0] = 0x00;        // is_save_data
    filePathData[1] = 0x00;        // content_id (in mikage), also known as NCSDPartitionId
    filePathData[2] = 0x02;        // sub_file_type, used by the function NCCHOpenExeFSSection
    filePathData[3] = 0x6e6f6369;        // name of the section to read (this spells "icon")
    // filePathData[3] = 0x6f676f6c;        // name of the section to read (this spells "logo")
    // filePathData[3] = 0x6e6e6e6e;        // name of the section to read (testing)
    filePathData[4] = 0x00000000;        // name of the section to read (part2)

    // This is comments from mikage
    // // Forward this call to ArchiveProgramDataFromTitleId::OpenFile using
    // // a new path:
    //     // * Word 0: NCCH (0) or save data (1)
    //     // * Word 1: TMD content index or NCSD partition index
    //     // * Word 2: 0 for romfs (and for save data), 1 for exefs code section, 2 for exefs non-code section
    //     // * Words 3+4: ExeFS section name
    // HLE::BinaryPathType new_file_path;
    // new_file_path.resize(20);
    // // First word is zero (=NCCH access)
    // // Second word is zero (=first content)
    // // The remaining data is copied verbatimly

    // FS_Path filePath = fsMakePath(PATH_BINARY, "");
    FS_Path filePath = { PATH_BINARY, sizeof(filePathData), filePathData };

    Handle fileHandle;
    printf("filepath.size %ld\n", filePath.size);

    // svcSleepThread(4888000000); // 4000.888ms

    res = FSUSER_OpenFile(&fileHandle, archive, filePath, FS_OPEN_READ, 0);
    if (R_FAILED(res)) {
        FSUSER_CloseArchive(archive);
        return false;
    }

    printf("Opened file in archive\n");

    // svcSleepThread(9999999999999999); // sleep for ~4 seconds


    // Read the SMDH data
    u32 bytesRead;
    res = FSFILE_Read(fileHandle, &bytesRead, 0, &smdh, sizeof(SMDH));
    FSFILE_Close(fileHandle);
    FSUSER_CloseArchive(archive);

    // if (R_FAILED(res) || bytesRead != sizeof(SMDH)) return false;
    if (R_FAILED(res)) {
        return false;
    }

    // printf("ExeFS first filename %s\n", exefs.file_name);

    printf("First 4 bytes are %#016lx\n", smdh.magic);

    // // Validate SMDH magic ("SMDH")
    // if (smdh.magic != 0x48444D53) {
    //     printf("File magic does not match SMDH\n");
    //     return false;
    // }

    printf("File magic is correct\n");

    // Pick language (use English = 1, or CFG_LANGUAGE_EN)
    u8 lang = 1; // CFG_LANGUAGE_EN
    
    // The short description is a UTF-16 string (0x40 u16 chars)
    // Convert to UTF-8 for easier use
    // utf16_to_utf8((uint8_t*)nameOut, smdh.titles[lang].shortDescription, nameLen - 1);
    // nameOut[nameLen - 1] = '\0';

    return true;
}



int main(int argc, char* argv[]) {
//---------------------------------------------------------------------------------
	// Init libs
	gfxInitDefault();
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
	C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
	C2D_Prepare();
	consoleInit(GFX_TOP, NULL);

	// Create screens
	C3D_RenderTarget* bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    Result temp_res;


    printf("rpopov custom homemenu!\n");

    // Initialize "application manager" service - it is used to fetch the list of installed titles
    temp_res = amInit();
    if (temp_res != 0) {
        printf("amInit Result %ld\n", temp_res);
    } 

    // // Get the list of installed titles, from the "application manager"
    // // installed in NAND
    // u32 title_count_nand = 0; 
    // temp_res = AM_GetTitleCount(MEDIATYPE_NAND, &title_count_nand);
    // if (temp_res != 0) {
    //     printf("Result 0x%lx\n", temp_res);
    // }

    // printf("Title Count in NAND %d\n", (int)title_count_nand);

    // // installed in SDCARD
    // u32 title_count_sdcard = 0;
    // temp_res = AM_GetTitleCount(MEDIATYPE_SD, &title_count_sdcard);
    // if (temp_res != 0) {
    //     printf("Result 0x%lx\n", temp_res);
    // }

    // printf("Title Count in SDCARD %d\n", (int)title_count_sdcard);

    // installed in GAMECARD
    u32 title_count_gamecard = 0;
    temp_res = AM_GetTitleCount(MEDIATYPE_GAME_CARD, &title_count_gamecard);
    if (temp_res != 0) {
        printf("Result 0x%lx\n", temp_res);
    }

    printf("Title Count in GAMECARD %d\n", (int)title_count_gamecard);

    // Get gamecard title id
    u32 title_found_gamecard = 0;
    u64* gamecard_title_id = new u64[1];
    temp_res = AM_GetTitleList(&title_found_gamecard, MEDIATYPE_GAME_CARD, 1, gamecard_title_id);
    if (temp_res != 0) {
        printf("AM_GetTitleList GAMECARD Result 0x%lx\n", temp_res);
    }

    printf("Gamecard has title id %#018llx\n", gamecard_title_id[0]);


    char* title_name_gamecard = new char[MAX_TITLE_NAME];
    bool title_name_gamecard_result = getTitleName(gamecard_title_id[0], MEDIATYPE_GAME_CARD, title_name_gamecard, MAX_TITLE_NAME);
    if (title_name_gamecard_result) {
        printf("title %#018llx - %s\n", gamecard_title_id[0], title_name_gamecard);
    } else {
        printf("title %#018llx - failed to get name\n", gamecard_title_id[0]);
    }

    // // Fetch info for each installed title
    // u32 titles_found_nand = 0;
    // u64* title_ids = new u64[128];
    // temp_res = AM_GetTitleList(&titles_found_nand, MEDIATYPE_NAND, 128, title_ids);
    // if (temp_res != 0) {
    //     printf("AM_GetTitleList Result 0x%lx\n", temp_res);
    // }

    // printf("Found %lu title ids in NAND\n", titles_found_nand);


    // for(u32 i = 0; i < titles_found_nand; i++){
    //     // bool getTitleName(u64 titleId, FS_MediaType mediaType, char *nameOut, size_t nameLen)
    //     char* title_name = new char[MAX_TITLE_NAME];
    //     bool title_name_result = getTitleName(title_ids[i], MEDIATYPE_NAND, title_name, MAX_TITLE_NAME);
    //     if (title_name_result) {
    //         printf("%02lu title %#018llx - %s\n", i, title_ids[i], title_name);
    //     } else {
    //         printf("%02lu title %#018llx - failed to get name\n", i, title_ids[i]);
    //     }
    // }


	while(aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysHeld();
        
        // TODO: This causes a weird error in mikage, keep this and debug sometime
        if(kDown & KEY_B)
			break; // break in order to return to hbmenu
    }

    amExit();

	// Deinit libs
	C2D_Fini();
	C3D_Fini();
	gfxExit();
	return 0;
}
