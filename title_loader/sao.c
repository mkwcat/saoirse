#include <gctypes.h>
#include <ios.h>
#include <string.h>

#define HW_VISOLID 0x0D800024
#define YUV_BLUE ((255 << 24) | (107 << 16) | (29 << 8))

#define SYSCALL_ADDRESS (0xFFFF93D0 + 0x70 * 4)
s32 kernelExec(void* func, ...);


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

void saoMain(u32 arg)
{
    IOS_SetThreadPriority(0, 71);

    if (saoPatchKernel() < 0)
        return;

    /* Test kernel write - set the screen to blue
     * remove this once we need a video console */
    IOS_Write32(HW_VISOLID, YUV_BLUE | 1);
}

void saoExit(void)
{
    while (1) { }
}