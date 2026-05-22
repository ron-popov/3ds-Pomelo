#include "utils.h"


// Get the name of a title, from the "icon" file in the ExeFS section of the title
bool getTitleName(u64 titleId, FS_MediaType mediaType, char *nameOut, size_t nameLen) {
    SMDH smdh;
    
    // // Build the archive path (mediaType + titleId)
    // u32 archPath[3] = {
    //     (u32)mediaType,
    //     (u32)((titleId >> 32) & 0xFFFFFFFF),
    //     (u32)(titleId & 0xFFFFFFFF)
    // };

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
bool is_running_in_emulator(void) {
    bool is_emulator = false;
    
    // --- Check 2: The Hardware MAC Address via CFG Block ---
    u8 mac[6];
    memset(mac, 0, sizeof(mac));
    
    if (R_SUCCEEDED(cfguInit())) {
        // Use the raw block reader. Size = 6 bytes, Block ID = 0x00090001 (WLAN MAC)
        Result macRes = CFGU_GetConfigInfoBlk2(6, 0x00090001, mac);
        
        if (R_FAILED(macRes)) {
            // If the block read fails, the sysdata archives are missing -> Emulator.
            is_emulator = true;
        } else if (mac[0] == 0x00 && mac[1] == 0x00 && mac[2] == 0x00 && 
                   mac[3] == 0x00 && mac[4] == 0x00 && mac[5] == 0x00) {
            // If it succeeds but the MAC is blank -> Emulator.
            is_emulator = true;
        }
        
        cfguExit();
    }
    
    return is_emulator;
}