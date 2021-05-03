#include <gctypes.h>
#include <string.h>
#include <ios.h>
#include <es.h>
#include <rvl/hollywood.h>
#include <debug/debugPrint.h>

#define STATEFLAGS_PATH "/title/00000001/00000002/data/state.dat"
#define SYSMENU_ID 0x000100014C554C5A // LULZ

#define YUV_BLUE ((255 << 24) | (107 << 16) | (29 << 8))

#define SYSCALL_ADDRESS (0xFFFF93D0 + 0x70 * 4)
s32 kernelExec(void* func, ...);

u32 block1[6];
u32 block2[6];

/* Patch the Kernel for the IOS_KernelExec syscall */
s32 saoPatchKernel(void)
{
    s32 ret, queue;

    queue = IOS_CreateMessageQueue((u32*) SYSCALL_ADDRESS, 1);
    if (queue < 0)
        return queue;
    
    ret = IOS_SendMessage(queue, (u32) &kernelExec, 0);
    IOS_DestroyMessageQueue(queue);
    return ret;
}

void saoMapRegion(void* _0, u32* block)
{
    ((void (*)(u32*)) 0xFFFF5179)(block);
}

void saoMain(u32 arg)
{
    IOS_SetThreadPriority(0, 80);

    if (saoPatchKernel() < 0)
        return;

    //IOS_Write32(HW_VISOLID, ((84 << 24) | (255 << 16) | (76 << 8)) | 1);

    /* Map uncached MEM1 */
    block1[0] = 0x00000000; // physical address
    block1[1] = 0x80000000; // virtual address
    block1[2] = 0x04000000; // length
    block1[3] = 15; // domain (shared)
    block1[4] = 3; // access (read/write)
    block1[5] = 0; // is cached
    IOS_KernelExec((void*) &saoMapRegion, block1);

    /* Map uncached MEM2 */
    block2[0] = 0x10000000; // physical address
    block2[1] = 0x90000000; // virtual address
    block2[2] = 0x04000000; // length
    block2[3] = 15; // domain (shared)
    block2[4] = 3; // access (read/write)
    block2[5] = 0; // is cached
    IOS_KernelExec((void*) &saoMapRegion, block2);

    ES_OpenLib();

    /* Test kernel write - set the screen to blue
     * remove this once we need a video console */

    DebugPrint_Printf(4, 0, "Star CONSOLE %d\n", 5);
}

void saoExit(void)
{
    u32 msg[8];
    u32 cnt;
    TicketView view;

    while (1) { } // temporary

    if (ES_GetNumTicketViews(SYSMENU_ID, &cnt) < 0 || cnt != 1
     || ES_GetTicketViews(SYSMENU_ID, &view, cnt) < 0
     || ES_LaunchTitle(SYSMENU_ID, &view) < 0
    ) {
        /* Launch system menu failed! */
        IOS_Clear32(HW_RESETS, RSTBINB);
    }

    while (1) { }
}