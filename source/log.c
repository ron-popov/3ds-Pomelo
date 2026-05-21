#include "log.h"

// Write log message to SDMC
void log_debug_hardware(const char* text) {
    if (!text) return;
    
    Handle fileHandle = 0;
    u64 fileSize = 0;
    u32 bytesWritten = 0;
    u32 textLength = strlen(text);

    if (textLength == 0) return;

    // Map low-level paths to target the raw SDMC hardware archive
    FS_Path archivePath = fsMakePath(PATH_EMPTY, "");
    FS_Path filePath = fsMakePath(PATH_ASCII, "/pomelo_debug.log");

    // CRITICAL FIX: Use OpenFileDirectly to hit the SD card using its raw Archive ID
    // without needing a pre-mounted FS_Archive struct.
    Result res = FSUSER_OpenFileDirectly(&fileHandle, ARCHIVE_SDMC, archivePath, 
                                         filePath, FS_OPEN_WRITE | FS_OPEN_CREATE, 0);
    if (R_FAILED(res)) return;

    // Seek to the end of the file for appending
    res = FSFILE_GetSize(fileHandle, &fileSize);
    if (R_SUCCEEDED(res)) {
        // Write the exact string length directly to the SD card and force a physical cache flush
        FSFILE_Write(fileHandle, &bytesWritten, fileSize, 
                     (const void*)text, textLength, FS_WRITE_FLUSH);
    }

    // Instantly release the handle
    FSFILE_Close(fileHandle);
}


// Print debug messages
// When running in mikage, will output to mikage stdout
// When running on real hardware, will write to a file in the SDMC
void log_debug_mikage(const char* buffer) {
    svcOutputDebugString(buffer, strlen(buffer));
}

void log_debug(const char* format, ...) {
    char buffer[512]; // Temporary buffer for the formatted string
    va_list args;
    
    va_start(args, format);
    // vsnprintf prevents buffer overflows by limiting write size
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    log_debug_mikage((const char*)&buffer);
    log_debug_hardware((const char*)&buffer);
}

// Decode and prints the error code itself and it's components
void print_error_code_verbose(char* desc, Result res) {
    log_debug("%s Result 0x%lx\n", desc, res);
    log_debug("  Module  : %lu | Level   : %lu\n", R_MODULE(res), R_LEVEL(res));
    log_debug("  Summary : %lu | Desc    : %lu\n", R_SUMMARY(res), R_DESCRIPTION(res));
}