#include "main.h"
#include "DI.h"
#include "log.h"
#include <types.h>
#include <ios.h>
#include <stdarg.h>
#include <string.h>
#include <vsprintf.h>
#include <rvl/hollywood.h>

void kwrite32(u32 address, u32 value)
{
    const s32 queue = IOS_CreateMessageQueue((u32*) address, 1);
    if (queue < 0)
        _exit(YUV_PINK);
    
    const s32 ret = IOS_SendMessage(queue, value, 0);
    if (ret < 0)
        _exit(YUV_PINK);
    IOS_DestroyMessageQueue(queue);
}

static void fixThreadPrio()
{
    s32 tid = IOS_GetThreadId();
    kwrite32(0XFFFE0000 + 0xB0 * tid + 0x48, 127);
}

u32 _main(u32 arg)
{
    kwrite32(HW_VISOLID, YUV_YELLOW);
    fixThreadPrio();
    stdoutInit();
    printf(WARN, "IOS logging initialized");

    DI_CreateThread();
    /* We idle here because we crash the idle thread
     * ... which should be fixed at some point */
    IOS_SetThreadPriority(0, 0);
    while (1) { }
    return YUV_YELLOW;
}

void _exit(u32 color)
{
    /* write to HW_VISOLID */
    kwrite32(HW_VISOLID, color | 1);
    while (1) { }
}

void abort()
{
    printf(ERROR, "Abort was called!");
    IOS_CancelThread(0, 0);
    while (1) { }
}