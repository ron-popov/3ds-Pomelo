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

#define TITLE_ID_SYSTEM_MODULE_AM_EU 0x0004013000001502

void print_error_code_verbose(char* desc, Result res) {
    printf("%s Result 0x%lx\n", desc, res);
    printf("  Module  : %lu\n", R_MODULE(res));
    printf("  Level   : %lu\n", R_LEVEL(res));
    printf("  Summary : %lu\n", R_SUMMARY(res));
    printf("  Desc    : %lu\n", R_DESCRIPTION(res));
}




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

bool getTitleName(u64 titleId, FS_MediaType mediaType, char *nameOut, size_t nameLen) {
    SMDH smdh;
    
    // Build the archive path (mediaType + titleId)
    u32 archPath[3] = {
        (u32)mediaType,
        (u32)(titleId >> 32),
        (u32)(titleId & 0xFFFFFFFF)
    };
    FS_Path archivePath = { PATH_BINARY, sizeof(archPath), archPath };

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




int main(int argc, char* argv[]) {
	// Init libs
	gfxInitDefault();
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
	C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
	C2D_Prepare();
	consoleInit(GFX_TOP, NULL);

	// Create screens
	// C3D_RenderTarget* bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    Result temp_res;

    printf("rpopov custom homemenu!\n");

    // Get handle to PM system module
    {
        // printf("initializing the PMApp system module handle\n");

        // Initialize "Process Application Manager" system module
        // It is useful for launching more titles / system modules
        temp_res = pmAppInit();
        if (temp_res != 0) {
            print_error_code_verbose("pmappInit", temp_res);
        } 

        // printf("initialized the PMApp system module handle\n");        
    }

    // Get all running processes using SVCGetProcessList
    {
        // The kernel supports at most 64 processes simultaneously
        u32  pids[64];
        s32  count = 0;

        // printf("First process pid (before) : %#016lx\n", pids[0]);

        temp_res = svcGetProcessList(&count, pids, 64);
        if (R_FAILED(temp_res)) {
            print_error_code_verbose("svcGetProcessList", temp_res);
        }

        // printf("Total processes: %ld\n", (long)count);        
    }

    // Run the "am" system module title, before getting it's handle
    {
        // printf("Launching AM system module\n");

        const FS_ProgramInfo amProgramInfo = {
            .programId = TITLE_ID_SYSTEM_MODULE_AM_EU, 
            .mediaType = MEDIATYPE_NAND
        };

        temp_res = PMAPP_LaunchTitle(&amProgramInfo, 0x00);
        if (R_FAILED(temp_res)) {
            print_error_code_verbose("launch application manager", temp_res);
        }

        // printf("Launched AM system module\n");
    }


    // Get handle to AM system module
    {
        // printf("initializing the AM system module handle\n");

        // Initialize "application manager" system module - it is used to fetch the list of installed titles
        temp_res = amInit();
        if (temp_res != 0) {
            print_error_code_verbose("amInit", temp_res);
        } 

        // printf("initialized the am service handle\n");        
    }

    // Get number of title installed in NAND using AM module
    {
        printf("getting titles installed in nand\n");

        // Title count installed in NAND
        u32 title_count_nand = 0;
        temp_res = AM_GetTitleCount(MEDIATYPE_NAND, &title_count_nand);
        if (temp_res != 0) {
            print_error_code_verbose("AM_GetTitleCount", temp_res);
        }

        printf("Title Count in NAND %d\n", (int)title_count_nand);        
    }

    {        
        // Get gamecard title id
        u32 title_found_gamecard = 0;
        u64 gamecard_title_id[1];
        temp_res = AM_GetTitleList(&title_found_gamecard, MEDIATYPE_GAME_CARD, 1, gamecard_title_id);
        if (temp_res != 0) {
            print_error_code_verbose("AM_GetTitleList GAMECARD", temp_res);
        }

        printf("Gamecard has title id %#018llx\n", gamecard_title_id[0]);
    }



    // // Get gamecard title name
    // char title_name_gamecard[MAX_TITLE_NAME];
    // temp_res = getTitleName(gamecard_title_id[0], MEDIATYPE_GAME_CARD, title_name_gamecard, MAX_TITLE_NAME);
    // if (temp_res) {
    //     printf("title %#018llx - %s\n", gamecard_title_id[0], title_name_gamecard);
    // } else {
    //     printf("title %#018llx - failed to get name\n", gamecard_title_id[0]);
    // }

    // Get homemenu title (hardcoded tid) title name
    // u64 homemenu_tid = 0x0004003000008F02; // us
    u64 homemenu_tid = 0x0004003000009802; // eu
    char title_name_homemenu[MAX_TITLE_NAME];
    temp_res = getTitleName(homemenu_tid, MEDIATYPE_NAND, title_name_homemenu, MAX_TITLE_NAME);
    if (temp_res) {
        printf("title %#018llx - %s\n", homemenu_tid, title_name_homemenu);
    } else {
        printf("title %#018llx - failed to get name\n", homemenu_tid);
    }

    // // Fetch info for each installed title
    // u32 titles_found_nand = 0;
    // u64 title_ids[128];
    // temp_res = AM_GetTitleList(&titles_found_nand, MEDIATYPE_NAND, 128, title_ids);
    // if (temp_res != 0) {
    //     printf("AM_GetTitleList Result 0x%lx\n", temp_res);
    // }

    // printf("Found %lu title ids in NAND\n", titles_found_nand);


    // for(u32 i = 0; i < 10; i++){
    //     svcSleepThread(4888000000); // sleep for ~4 seconds

    //     char title_name[MAX_TITLE_NAME];
    //     temp_res = getTitleName(title_ids[i], MEDIATYPE_NAND, title_name, MAX_TITLE_NAME);
    //     if (temp_res) {
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
