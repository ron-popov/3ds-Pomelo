#include "log.h"


// Print debug messages
// When running in mikage, will output to mikage stdout
// When running on real hardware, will write to a file in the SDMC
void log_debug_mikage(const char* buffer) {
    svcOutputDebugString(buffer, strlen(buffer));
}

// Decode and prints the error code itself and it's components
void print_error_code_verbose(char* desc, Result res) {
    log_debug("%s Result 0x%lx", desc, res);
    log_debug("  Module  : %lu | Level   : %lu", R_MODULE(res), R_LEVEL(res));
    log_debug("  Summary : %lu | Desc    : %lu", R_SUMMARY(res), R_DESCRIPTION(res));
}

// Write log message to SDMC
void log_debug_hardware(const char* text) {
  if (!text) return;

    Handle fileHandle = 0;
    u64 fileSize = 0;
    u32 bytesWritten = 0;
    u32 textLength = strlen(text);

    char* text_with_newline = malloc(textLength + 1);
    sprintf(text_with_newline, "%s\n", text);

    if (textLength == 0) return;

    // --- 1. Generate the Hardware Timestamp ---
    time_t unixTime = time(NULL);
    struct tm* timeStruct = localtime(&unixTime);
    char timeBuffer[32];

    // Format: [YYYY-MM-DD HH:MM:SS] 
    u32 timeLength = snprintf(timeBuffer, sizeof(timeBuffer), "[%04d-%02d-%02d %02d:%02d:%02d] ", 
                              timeStruct->tm_year + 1900, timeStruct->tm_mon + 1, timeStruct->tm_mday,
                              timeStruct->tm_hour, timeStruct->tm_min, timeStruct->tm_sec);

    // --- 2. Mount and Open the Log File ---
    FS_Path archivePath = fsMakePath(PATH_EMPTY, "");
    FS_Path filePath = fsMakePath(PATH_ASCII, "/pomelo_debug.log");

    Result res = FSUSER_OpenFileDirectly(&fileHandle, ARCHIVE_SDMC, archivePath, 
                                         filePath, FS_OPEN_WRITE | FS_OPEN_CREATE, 0);
    if (R_FAILED(res)) return;

    // --- 3. Write Data to SD Card ---
    res = FSFILE_GetSize(fileHandle, &fileSize);
    if (R_SUCCEEDED(res)) {
        // Step A: Write the timestamp to the file buffer (Note: flag is 0, no physical flush yet)
        FSFILE_Write(fileHandle, &bytesWritten, fileSize, 
                     (const void*)timeBuffer, timeLength, 0);
        
        // Step B: Write the actual log message, offset by the timestamp's length.
        // FS_WRITE_FLUSH is passed here to push BOTH writes to the physical SD card at once.
        FSFILE_Write(fileHandle, &bytesWritten, fileSize + timeLength, 
                     (const void*)text_with_newline, textLength + 1, FS_WRITE_FLUSH);
    }
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