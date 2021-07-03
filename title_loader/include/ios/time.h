#pragma once

#ifdef __cplusplus
    extern "C" {
#endif

s32 IOS_CreateTimer(s32 usec, s32 repeat_usec, s32 queue, u32 msg);
s32 IOS_RestartTimer(s32 timer, s32 usec, s32 repeat_usec);
s32 IOS_StopTimer(s32 timer);
s32 IOS_DestroyTimer(s32 timer);
u32 IOS_GetTime();

#ifdef __cplusplus
    }
#endif