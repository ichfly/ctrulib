#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctr/types.h>
#include <ctr/srv.h>
#include <ctr/APT.h>
#include <ctr/GSP.h>
#include <ctr/GX.h>
#include <ctr/GPU.h>
#include <ctr/HID.h>
#include <ctr/SHDR.h>
#include <ctr/svc.h>
#include "costable.h"
#include "test_shbin.h"

u8* gspHeap;
u32* gxCmdBuf;

u8 currentBuffer;
u8* topLeftFramebuffers[2];
u8* topLeftFramebuffersPA[2];

Handle gspEvent, gspSharedMemHandle;

void gspGpuInit()
{
	gspInit();

	GSPGPU_AcquireRight(NULL, 0x0);
	GSPGPU_SetLcdForceBlack(NULL, 0x0);

	//set subscreen to blue
	u32 regData=0x01FF0000;
	GSPGPU_WriteHWRegs(NULL, 0x202A04, &regData, 4);

	//grab main left screen framebuffer addresses
	GSPGPU_ReadHWRegs(NULL, 0x400468, (u32*)&topLeftFramebuffersPA, 8);

	//convert PA to VA (assuming FB in VRAM)
	topLeftFramebuffers[0]=topLeftFramebuffersPA[0]+0x7000000;
	topLeftFramebuffers[1]=topLeftFramebuffersPA[1]+0x7000000;

	//setup our gsp shared mem section
	u8 threadID;
	svc_createEvent(&gspEvent, 0x0);
	GSPGPU_RegisterInterruptRelayQueue(NULL, gspEvent, 0x1, &gspSharedMemHandle, &threadID);
	svc_mapMemoryBlock(gspSharedMemHandle, 0x10002000, 0x3, 0x10000000);

	//map GSP heap
	svc_controlMemory((u32*)&gspHeap, 0x0, 0x0, 0x2000000, 0x10003, 0x3);

	//wait until we can write stuff to it
	svc_waitSynchronization1(gspEvent, 0x55bcb0);

	//GSP shared mem : 0x2779F000
	gxCmdBuf=(u32*)(0x10002000+0x800+threadID*0x200);

	currentBuffer=0;
}

void gspGpuExit()
{
	GSPGPU_UnregisterInterruptRelayQueue(NULL);

	//unmap GSP shared mem
	svc_unmapMemoryBlock(gspSharedMemHandle, 0x10002000);
	svc_closeHandle(gspSharedMemHandle);
	svc_closeHandle(gspEvent);
	
	gspExit();

	//free GSP heap
	svc_controlMemory((u32*)&gspHeap, (u32)gspHeap, 0x0, 0x2000000, MEMOP_FREE, 0x0);
}

void swapBuffers()
{
	u32 regData;
	GSPGPU_ReadHWRegs(NULL, 0x400478, &regData, 4);
	regData^=1;
	currentBuffer=regData&1;
	GSPGPU_WriteHWRegs(NULL, 0x400478, &regData, 4);
}

int main()
{
	initSrv();
	
	aptInit(APPID_APPLICATION);

	gspGpuInit();

	hidInit(NULL);

	aptSetupEventHandler();

	GPU_Init(NULL);

	u32* gpuCmd=(u32*)(&gspHeap[0x100000]);
	u32 gpuCmdSize=0x10000;

	GPU_Reset(gxCmdBuf, gpuCmd, gpuCmdSize);
	
	DVLB_s* shader=SHDR_ParseSHBIN((u32*)test_shbin,test_shbin_size);

	APP_STATUS status;
	while((status=aptGetStatus())!=APP_EXITING)
	{
		if(status==APP_RUNNING)
		{
			u32 PAD=hidSharedMem[7];
			
			u32 regData=PAD|0x01000000;
			GSPGPU_WriteHWRegs(NULL, 0x202A04, &regData, 4);

			GPUCMD_SetBuffer(gpuCmd, gpuCmdSize, 0);

				//depth buffer in VRAM, color buffer in FCRAM (gspHeap)
				//(no real reasoning behind this configuration)
				GPU_SetViewport((u32*)0x18000000,(u32*)0x20000000,0,0,240*2,400);
				SHDR_UseProgram(shader, 0);
				GPUCMD_AddSingleParam(0x0008025E, 0x00000000);

			GPUCMD_Finalize();
			GPUCMD_Run(gxCmdBuf);

			GX_SetDisplayTransfer(gxCmdBuf, (u32*)gspHeap, GX_BUFFER_DIM(480,400), (u32*)topLeftFramebuffers[currentBuffer], GX_BUFFER_DIM(480,400), 0x01001000);

			swapBuffers();
		}
		else if(status == APP_SUSPENDING)
		{
			aptReturnToMenu();
		}
		else if(status == APP_SLEEPMODE)
		{
			aptWaitStatusEvent();
		}
		svc_sleepThread(16666666);
	}

	hidExit();
	gspGpuExit();
	aptExit();
	svc_exitProcess();
	return 0;
}
