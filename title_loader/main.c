#include <types.h>
#include <ios.h>
#include <stdarg.h>
#include <string.h>
#include <vsprintf.h>
#include "log.h"

void _exit(u32 color);

void _main(u32 arg)
{
    stdoutInit();
    printf(WARN, "IOS logging initialized! %08X\n", 0x12345678);
    while (1) { }
}

void _exit(u32 color)
{
    /* write to HW_VISOLID */
    const s32 queue = IOS_CreateMessageQueue((u32*) 0x0D800024, 1);
    if (queue < 0)
        return;
    
    const s32 ret = IOS_SendMessage(queue, color | 1, 0);
    IOS_DestroyMessageQueue(queue);

    while (1) { }
}