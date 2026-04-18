#include "custom_ipc_calls.h"

Result APT_PrepareToStartApplication(FS_ProgramInfo* programInfo, u8 launchFlags)
{
	// https://www.3dbrew.org/wiki/APT:PrepareToStartApplication

	u32 cmdbuf[16];

	cmdbuf[0]=0x00150140;
	memcpy(&cmdbuf[1], programInfo, sizeof(FS_ProgramInfo));
	cmdbuf[5] = launchFlags;

	return aptSendCommand(cmdbuf);
}

/**
 * Inputs:
 * - Size of parameter to send to the started application
 * - HMAC size
 * - 1 to launch paused, 0 to run immediately
 * - Parameter to send to the started application
 * - HMAC
 */
Result APT_StartApplication(u32 parameterSize, u32 hmacSize, u32 paused, void* parameter, void* hmac)
{
	u32 cmdbuf[16];

	cmdbuf[0]=0x001B00C4;
	cmdbuf[1]=parameterSize;
	cmdbuf[2]=hmacSize;
	cmdbuf[3]=paused;
	cmdbuf[4]=(u32)parameter;
	cmdbuf[5]=(u32)hmac;

	return aptSendCommand(cmdbuf);
}