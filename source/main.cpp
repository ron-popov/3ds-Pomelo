#include <3ds.h>
#include <citro2d.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>


#define SCREEN_WIDTH 400
#define SCREEN_HEIGHT 240

#define BOTTOM_SCREEN_WIDTH 320
#define BOTTOM_SCREEN_HEIGHT 240


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

    // Get the list of installed titles, from the "application manager"
    // installed in NAND
    u32 title_count_nand = 0; 
    temp_res = AM_GetTitleCount(MEDIATYPE_NAND, &title_count_nand);
    if (temp_res != 0) {
        printf("Result 0x%ld\n", temp_res);
    }

    printf("Title Count in NAND %d\n", (int)title_count_nand);

    // installed in SDCARD
    u32 title_count_sdcard = 0;
    temp_res = AM_GetTitleCount(MEDIATYPE_SD, &title_count_sdcard);
    if (temp_res != 0) {
        printf("Result 0x%ld\n", temp_res);
    }

    printf("Title Count in SDCARD %d\n", (int)title_count_sdcard);

    // installed in GAMECARD
    u32 title_count_gamecard = 0;
    temp_res = AM_GetTitleCount(MEDIATYPE_GAME_CARD, &title_count_gamecard);
    if (temp_res != 0) {
        printf("Result 0x%ld\n", temp_res);
    }

    printf("Title Count in GAMECARD %d\n", (int)title_count_gamecard);


    // Fetch info for each installed title
    u32 titles_found_nand = 0;
    u64* title_ids = new u64[128];
    temp_res = AM_GetTitleList(&titles_found_nand, MEDIATYPE_NAND, 128, title_ids);
    if (temp_res != 0) {
        printf("AM_GetTitleList Result 0x%ld\n", temp_res);
    }

    printf("Found %lu title ids in NAND\n", titles_found_nand);

    for(u32 i = 0; i < titles_found_nand; i++){
        printf("%02lu title %#018llx\n", i, title_ids[i]);
    }


	while(aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysHeld();
        
        // TODO: This causes a weird error in mikage, keep this and debug sometime
        if(kDown & KEY_START)
			break; // break in order to return to hbmenu
    }

    amExit();

	// Deinit libs
	C2D_Fini();
	C3D_Fini();
	gfxExit();
	return 0;
}
