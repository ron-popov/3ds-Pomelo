#pragma once

#include <3ds.h>
#include <string.h>

Result APT_PrepareToStartApplication(FS_ProgramInfo* info, u8 flags);

Result APT_StartApplication(u32 parameterSize, u32 hmacSize, u32 paused, void* parameter, void* hmac);

Result NS_CardUpdateInitialize();

Result CUSTOM_APT_Initialize(NS_APPID appId, u32 attr, Handle* signalEvent, Handle* resumeEvent);

Result CUSTOM_APT_Enable(u32 attr);