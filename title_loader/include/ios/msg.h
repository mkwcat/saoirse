#ifndef _IOS_MSG_H
#define _IOS_MSG_H

#include <types.h>

#ifdef __cplusplus
    extern "C" {
#endif

s32 IOS_CreateMessageQueue(u32* buf, u32 msg_count);
s32 IOS_DestroyMessageQueue(s32 queue_id);
s32 IOS_SendMessage(s32 queue_id, u32 message, u32 flags);
s32 IOS_JamMessage(s32 queue_id, u32 message, u32 flags);
s32 IOS_ReceiveMessage(s32 queue_id, u32* message, u32 flags);

#ifdef __cplusplus
    }
#endif

#endif // _IOS_MSG_H
