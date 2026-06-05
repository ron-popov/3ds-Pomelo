#include "utils.h"

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
bool getTitleName(u64 titleId, FS_MediaType mediaType, char *nameOut, size_t nameLen) {
    SMDH smdh;
    
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


    // Remove non ascii chars
    remove_non_ascii(nameOut);

    return true;
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
        return false;  // Real CFW Hardware detected
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
