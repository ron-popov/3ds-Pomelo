#pragma once

#include <3ds.h>
#include <string.h>

Result APT_PrepareToStartApplication(FS_ProgramInfo* info, u8 flags);

Result APT_StartApplication(u32 parameterSize, u32 hmacSize, u32 paused, void* parameter, void* hmac);