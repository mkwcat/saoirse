#include "gctypes.h"
#include "ios.h"

#define HW_VISOLID 0x0D800024
#define YUV_YELLOW ((0 << 24) | (148 << 16) | (225 << 8))

void saoMain(u32 arg)
{
    /* So we know we got here, set the screen color to yellow. */
    s32 queue = IOS_CreateMessageQueue((u32*) HW_VISOLID, 0x40000000);
    if (queue >= 0) {
        IOS_SendMessage(queue, YUV_YELLOW | 1, 0);
        IOS_DestroyMessageQueue(queue);
    }

    /* Basic test! */
    s32 fd = IOS_Open("/dev/es", 0);
}

void saoExit(void)
{
    while (1) { }
}