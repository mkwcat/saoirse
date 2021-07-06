#include "es.h"

#include <ios.h>
#include <types.h>

static s32 __esFd = -1;

s32 ES_InitLib(void)
{
    if (__esFd < 0)
        __esFd = IOS_Open(ES_PATH, IOS_OPEN_NONE);
    return __esFd;
}

s32 ES_CloseLib(void)
{
    s32 ret = IOS_Close(__esFd);
    if (ret >= IOS_SUCCESS)
        __esFd = -1;
    return ret;
}

s32 ES_LaunchTitle(u64 titleID, const TicketView* view)
{
    IOVector vec[2];

    vec[0].data = (void*) &titleID;
    vec[0].len  = sizeof(u64);
    vec[1].data = (void*) view;
    vec[1].len  = sizeof(TicketView);

    return IOS_Ioctlv(__esFd, IOCTL_ES_LAUNCH, 2, 0, vec);
}

s32 ES_GetNumTicketViews(u64 titleID, u32* cnt)
{
    IOVector vec[2];

    vec[0].data = (void*) &titleID;
    vec[0].len  = sizeof(u64);
    vec[1].data = (void*) cnt;
    vec[1].len  = sizeof(u32);

    return IOS_Ioctlv(__esFd, IOCTL_ES_GETVIEWCNT, 1, 1, vec);
}

s32 ES_GetTicketViews(u64 titleID, TicketView* views, u32 cnt)
{
    IOVector vec[3];

    vec[0].data = (void*) &titleID;
    vec[0].len  = sizeof(u64);
    vec[1].data = (void*) &cnt;
    vec[1].len  = sizeof(u32);
    vec[2].data = (void*) views;
    vec[2].len  = sizeof(TicketView) * cnt;

    return IOS_Ioctlv(__esFd, IOCTL_ES_GETVIEWS, 2, 1, vec);
}