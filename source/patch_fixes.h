// This strings is here to fix luma3ds loader trying to patch a homemenu code.bin
// trying to do some memory replacements and failing
// I reverse engineered it according to the patchCode function in luma3ds "loader"
// https://github.com/LumaTeam/Luma3DS/blob/master/sysmodules/loader/source/patcher.c

static char* PATCHING_BYPASS_REGION_FREE_PATCHING __attribute__((used)) = 
    "\x0A\x0C\x00\x10"
    "\x00\x00\x00\x00";


// This section is using asm word statements because the loader is looking specifically
// at the text section for some of the patching
__attribute__((naked)) void inject_loader_decoys(void) {
    asm volatile (
        "b 1f\n"                    // Jump to label '1' (forward)
        ""                          // SMDH REGION CHECK MANUAL START
        ".word 0xE1110000\n"
        ".word 0x0A000000\n"
        ".word 0xE1A0000D\n"
        ""                          // SMDH REGION CHECK MANUAL END
        ""                          // NDS FLASHCARD WHITELIST START
        ".word 0x00E92DAA\n"
        ".word 0xAAAAAABB\n"
        ".word 0xBBBBBBFF\n"
        ".word 0x08E5D110\n"
        ".word 0x00008D00\n"
        ""                          // NDS FLASHCARD WHITELIST END
        "1:\n"                      // Label '1'
        "bx lr\n"                   // Return
    );
}