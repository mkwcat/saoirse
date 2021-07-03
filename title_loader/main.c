#include "main.h"
#include "DI.h"
#include "FS.h"
#include "log.h"
#include <types.h>
#include <util.h>
#include <ios.h>
#include <stdarg.h>
#include <string.h>
#include <vsprintf.h>
#include <rvl/hollywood.h>

static u8 heapArea[0x2000] ATTRIBUTE_ALIGN(32);
s32 heap = -1;

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
    IOS_SetThreadPriority(0, 80);
    stdoutInit();
    printf(WARN, "IOS logging initialized");

    s32 ret = IOS_CreateHeap(heapArea, sizeof(heapArea));
    if (ret < 0) {
        printf(ERROR, "IOS_CreateHeap failed: %d", ret);
        abort();
    }
    heap = ret;

    DI_CreateThread();
    printf(INFO, "call FS_Init()");
    FS_Init();
    IOS_CancelThread(0, 0);
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