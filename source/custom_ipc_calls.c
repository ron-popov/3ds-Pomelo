#include <stdlib.h>
#include <stdio.h>

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

	// I checked which static buffer ids the real homemenu is using when performing this calls
	// 0x00 - For paramter
	// 0x02 - For hmac
	cmdbuf[4]=IPC_Desc_StaticBuffer(parameterSize,0x00);
	cmdbuf[5]=(u32)parameter;

	cmdbuf[6]=IPC_Desc_StaticBuffer(hmacSize,0x02);
	cmdbuf[7]=(u32)hmac;

	return aptSendCommand(cmdbuf);
}

Result NS_CardUpdateInitialize() {
	// svcCreateMemoryBlock(&swkbdSharedMemHandle, (u32)swkbdSharedMem, sharedMemSize, MEMPERM_READ|MEMPERM_WRITE, MEMPERM_READ|MEMPERM_WRITE);
	// using CardUpdateInitialize = IPC::IPCCommand<0x7>::add_uint32::add_handle<IPC::HandleType::SharedMemoryBlock>::response;

	u32 sharedMemSize = 0x100;
	char* sharedMem = malloc(sharedMemSize);
	Handle sharedMemHandle;

	// Result shared_mem_result = svcCreateMemoryBlock(&sharedMemHandle, (u32)sharedMem, 
	// 	sharedMemSize, MEMPERM_READ|MEMPERM_WRITE, MEMPERM_READ|MEMPERM_WRITE);

	Result shared_mem_result = svcCreateMemoryBlock(&sharedMemHandle, (u32)sharedMem, 
		sharedMemSize, MEMPERM_READ, MEMPERM_READ);

	if (R_FAILED(shared_mem_result)) {
		printf("Failed creating shared memory block\n");
		return shared_mem_result;
	}

	// Get handle to ns service
	Handle nsHandle;
	Result ns_handle_result = srvGetServiceHandle(&nsHandle, "ns:s");

	if (R_FAILED(ns_handle_result)) {
		printf("Failed getting handle to NS\n");
		return ns_handle_result;
	}

	Result ret = 0;
	u32 *cmdbuf = getThreadCommandBuffer();

	cmdbuf[0] = 0x00070042;
	cmdbuf[1] = sharedMemSize;
	cmdbuf[2] = 0;
	cmdbuf[3] = sharedMemHandle;

	ret = svcSendSyncRequest(nsHandle);

	nsExit();

	if(R_FAILED(ret)) {
		return ret;
	} else {
		return (Result)cmdbuf[1];	
	}
	
}


Result CUSTOM_APT_Initialize(NS_APPID appId, u32 attr, Handle* signalEvent, Handle* resumeEvent)
{
	u32 cmdbuf[16];
	cmdbuf[0]=IPC_MakeHeader(0x2,2,0); // 0x20080
	cmdbuf[1]=appId;
	cmdbuf[2]=attr;

	Result ret = aptSendCommand(cmdbuf);
	if (R_SUCCEEDED(ret))
	{
		if(signalEvent) *signalEvent=cmdbuf[3];
		if(resumeEvent) *resumeEvent=cmdbuf[4];
	}

	return ret;
}



Result CUSTOM_APT_Enable(u32 attr)
{
	u32 cmdbuf[16];
	cmdbuf[0]=IPC_MakeHeader(0x3,1,0); // 0x30040
	cmdbuf[1]=attr;

	return aptSendCommand(cmdbuf);
}