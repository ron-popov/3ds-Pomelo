#include "utils.h"

#define LARGE_ICON_H 48
#define LARGE_ICON_W 48
#define SMALL_ICON_H 24
#define SMALL_ICON_W 24


// Converts a single RGB565 pixel to BGR8 (3 bytes: B, G, R)
static inline void rgb565_pixel_to_bgr8(uint16_t pixel, uint8_t *out) {
    uint8_t r = (pixel >> 11) & 0x1F;
    uint8_t g = (pixel >> 5)  & 0x3F;
    uint8_t b =  pixel        & 0x1F;

    // Scale up to 8 bits
    // R/B: 5-bit -> 8-bit  (x << 3) | (x >> 2)
    // G:   6-bit -> 8-bit  (x << 2) | (x >> 4)
    out[0] = (b << 3) | (b >> 2);  // B
    out[1] = (g << 2) | (g >> 4);  // G
    out[2] = (r << 3) | (r >> 2);  // R
}

// Converts a full RGB565 tiled icon to a flat BGR8 buffer.
// src:    RGB565 Morton-tiled pixel data (as stored in SMDH)
// dst:    output buffer, must be width * height * 3 bytes
// width/height: icon dimensions (48 or 24)
void smdh_icon_rgb565_to_bgr8(const uint16_t *src, uint8_t *dst,
                               int width, int height) {
    int tiles_x = width  / 8;
    int tiles_y = height / 8;

    for (int ty = 0; ty < tiles_y; ty++) {
        for (int tx = 0; tx < tiles_x; tx++) {
            int tile_index = ty * tiles_x + tx;

            for (int i = 0; i < 64; i++) {
                // Decode Morton index back to local (lx, ly) within tile
                int lx = 0, ly = 0;
                for (int bit = 0; bit < 3; bit++) {
                    lx |= ((i >> (2 * bit))     & 1) << bit;
                    ly |= ((i >> (2 * bit + 1)) & 1) << bit;
                }

                // Absolute pixel position in the image
                int px = tx * 8 + lx;
                int py = ty * 8 + ly;

                uint16_t pixel = src[tile_index * 64 + i];
                uint8_t *out   = dst + (py * width + px) * 3;
                rgb565_pixel_to_bgr8(pixel, out);
            }
        }
    }
}

bool is_ascii_char(char c) {
    return (c >= 0x20 && c <= 0x7E);
}

void remove_non_ascii(char *str) {
    char *src = str; // Pointer to read from
    char *dst = str; // Pointer to write to

    while (*src != '\0') {
        // ASCII characters have values between 0 and 127
        // Casting to unsigned char prevents issues with negative values
        if (is_ascii_char(*src)) {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0'; // Null-terminate the cleaned string
}

// Get the name of a title, from the "icon" file in the ExeFS section of the title
bool loadTitleGame(u64 titleId, FS_MediaType mediaType, titleGame* titleGameOut) {
    SMDH *smdh = malloc(sizeof(SMDH));
    
    log_debug("Get name of title %#018llx (media 0x%x)", titleId, mediaType);

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
        goto cleanup_fail;
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

    res = FSUSER_OpenFile(&fileHandle, archive, filePath, FS_OPEN_READ, 0);
    if (R_FAILED(res)) {
        print_error_code_verbose("FSUSER_OpenFile",  res);
        FSUSER_CloseArchive(archive);
        goto cleanup_fail;
    }

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
    utf16_to_utf8((uint8_t*)titleGameOut->name, smdh->titles[lang].shortDescription, MAX_TITLE_NAME - 1);
    titleGameOut->name[MAX_TITLE_NAME - 1] = '\0';

    // Remove non ascii chars
    remove_non_ascii(titleGameOut->name);

    // Encoded using bgr8, meaning we need 3 bytes per pixel
    titleGameOut->large_icon_bgr8 = malloc(LARGE_ICON_H * LARGE_ICON_W * 3); 

    // Copy large icon
    smdh_icon_rgb565_to_bgr8((uint16_t*)&smdh->large_icon_rgb565, (uint8_t*)titleGameOut->large_icon_bgr8, LARGE_ICON_H, LARGE_ICON_W);

    free(smdh);
    return true;

    cleanup_fail:
    free(smdh);
    return false;
}

// Should title be displayed in the homemenu and launchable
bool shouldDisplayTitle(u64 title_id) {
    // Title categories blacklist
    u32 title_id_upper = (u32)((title_id >> 32) & 0xFFFFFFFF);

    switch (title_id_upper) {
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
        case 0x00048004: // DSiWare Ports
            return false;
        case 0x00048005: // DSi System Applications
            return false;
        case 0x0004800F: // DSi System Data Archives
            return false;
        default:
            break;
    }

    // Specific title ids blacklist
    switch(title_id) {
        case 0x0004001000021a00: // System transfer - name in "icon" is "???"
            return false;
        case 0x0004001000021f00: // System updates in safe mode - no name
            return false;
        case 0x000400102002cf00: // Homemenu
            return false;
        case 0x0000000000000000: // Empty title id
            return false;
        default:
            break;
    }
    

    return true;
}



// Hardware sleep, implemented using timers cause svcSleepThread doesn't work for some reason
void hardwareTimerSleep(u8 seconds) {
    Handle timer;
    svcCreateTimer(&timer, RESET_ONESHOT);
    svcSetTimer(timer, seconds * 1000000000, 0);
    svcWaitSynchronization(timer, -1); // Wait for the timer event
    svcCloseHandle(timer);
}


// Checks if we are currently running in an emulator?
bool isRunningInEmulator(void) {
    Handle fileHandle = 0;
    
    // Map paths to the root of the raw SDMC archive
    FS_Path archivePath = fsMakePath(PATH_EMPTY, "");
    FS_Path filePath = fsMakePath(PATH_ASCII, "/boot.firm");
    
    // Attempt to open /boot.firm strictly in read-only mode.
    // We use OpenFileDirectly to hit the SD card without needing a pre-mounted archive.
    Result res = FSUSER_OpenFileDirectly(&fileHandle, ARCHIVE_SDMC, archivePath, 
                                         filePath, FS_OPEN_READ, 0);
    
    // If the result succeeds, the file exists on the SD card.
    if (R_SUCCEEDED(res)) {
        // Instantly close the handle to prevent memory leaks
        FSFILE_Close(fileHandle);
        return false;  // Real CFW Hardware detected, pomelo can't run on real hardware without CFW
    }
    
    // If it fails (file not found, or IPC rejected), we are likely in an emulator.
    return true; 
}


void systemModelName(u8 systemModel, char* nameOut) {

    switch(systemModel) {
        case 0x00:
            strcpy(nameOut, "Old 3ds"); break;
        case 0x01:
            strcpy(nameOut, "Old 3ds XL"); break;
        case 0x02:
            strcpy(nameOut, "New 3ds"); break;
        case 0x03:
            strcpy(nameOut, "Old 2ds"); break;
        case 0x04:
            strcpy(nameOut, "New 3ds XL"); break;
        case 0x05:
            strcpy(nameOut, "New 2ds XL"); break;
        default:
            strcpy(nameOut, "Unknown Systme Model"); break;
    }
}
