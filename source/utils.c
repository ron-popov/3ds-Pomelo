#include <citro2d.h>
#include <malloc.h>

#include "utils.h"
#include "log.h"

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

static void copy_icon_to_tex64(uint16_t* dst, const uint16_t* src) {
    // Both 48x48 and 64x64 use the same 8x8 Morton tile format; only the
    // tile grid differs (6x6 vs 8x8), so tiles can be copied directly.
    for (int ty = 0; ty < 8; ty++) {
        for (int tx = 0; tx < 8; tx++) {
            uint16_t* dst_tile = dst + (ty * 8 + tx) * 64;
            if (tx < 6 && ty < 6)
                memcpy(dst_tile, src + (ty * 6 + tx) * 64, 64 * sizeof(uint16_t));
            else
                memset(dst_tile, 0, 64 * sizeof(uint16_t));
        }
    }
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


void logMemInfo() {
    // Get heap usage
    struct mallinfo mi = mallinfo();
    float heap_used_kb = mi.uordblks / 1024;

    // Get linear heap usage
    float linear_free_kb = linearSpaceFree() / 1024;

    // Get from 3ds the memory usage and total
    float total_used_kb  = osGetMemRegionUsed(MEMREGION_APPLICATION) / 1024;
    float total_available_kb = osGetMemRegionSize(MEMREGION_APPLICATION) / 1024;

    log_debug("Heap Used %.1fkb / %.1fkb | Linear Heap Free %.1fkb / %.1fkb | Total Mem Used %.1fkb / %.1fkb", 
        heap_used_kb, (0x304000f / 1024.f),
        linear_free_kb, (0xb64000f / 1024.f),
        total_used_kb, total_available_kb);
}