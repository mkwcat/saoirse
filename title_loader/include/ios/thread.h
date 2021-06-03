#ifndef _IOS_THREAD_H
#define _IOS_THREAD_H

#include <types.h>

#ifdef __cplusplus
    extern "C" {
#endif

typedef s32 (*IOSThreadProc)(void* arg);

s32 IOS_CreateThread(IOSThreadProc proc, void* arg, u32* stack_top,
    u32 stacksize, s32 priority, bool detached);
s32 IOS_JoinThread(s32 threadid, void** value);
s32 IOS_CancelThread(s32 threadid, void* value);
s32 IOS_GetThreadId(void);
s32 IOS_GetProcessId(void);
s32 IOS_StartThread(s32 threadid);
s32 IOS_SuspendThread(s32 threadid);
void IOS_YieldThread(void);
u32 IOS_GetThreadPriority(s32 threadid);
s32 IOS_SetThreadPriority(s32 threadid, u32 priority);

#ifdef __cplusplus
    }
#endif

#endif // _IOS_THREAD_H