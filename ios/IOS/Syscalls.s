// Syscalls.s - IOS system calls
//   Written by Palapeli
//
// SPDX-License-Identifier: MIT

    .section ".text"
    .arm
    .align  2

#define SECT(ARG) .syscall ## ARG

#define DEF_SYSCALL(name, id)                                                  \
    .global name;                                                              \
    .section SECT(.name), "ax", %progbits;                                     \
    .type   name, function;                                                    \
    .align  2;                                                                 \
name:;                                                                         \
    .word   0xE6000010 | id << 5;                                              \
    bx      lr;                                                                \
    .size   name, .- name

/* System calls start */
    DEF_SYSCALL( IOS_CreateThread,            0x00 )
    DEF_SYSCALL( IOS_JoinThread,              0x01 )
    DEF_SYSCALL( IOS_CancelThread,            0x02 )
    DEF_SYSCALL( IOS_GetThreadId,             0x03 )
    DEF_SYSCALL( IOS_GetProcessId,            0x04 )
    DEF_SYSCALL( IOS_StartThread,             0x05 )
    DEF_SYSCALL( IOS_SuspendThread,           0x06 )
    DEF_SYSCALL( IOS_YieldThread,             0x07 )
    DEF_SYSCALL( IOS_GetThreadPriority,       0x08 )
    DEF_SYSCALL( IOS_SetThreadPriority,       0x09 )

    DEF_SYSCALL( IOS_CreateMessageQueue,      0x0A )
    DEF_SYSCALL( IOS_DestroyMessageQueue,     0x0B )
    DEF_SYSCALL( IOS_SendMessage,             0x0C )
    DEF_SYSCALL( IOS_JamMessage,              0x0D )
    DEF_SYSCALL( IOS_ReceiveMessage,          0x0E )

    DEF_SYSCALL( IOS_HandleEvent,             0x0F )
    DEF_SYSCALL( IOS_UnregisterEventHandler,  0x10 )
    DEF_SYSCALL( IOS_CreateTimer,             0x11 )
    DEF_SYSCALL( IOS_RestartTimer,            0x12 )
    DEF_SYSCALL( IOS_StopTimer,               0x13 )
    DEF_SYSCALL( IOS_DestroyTimer,            0x14 )
    DEF_SYSCALL( IOS_GetTime,                 0x15 )

    DEF_SYSCALL( IOS_CreateHeap,              0x16 )
    DEF_SYSCALL( IOS_DestroyHeap,             0x17 )
    DEF_SYSCALL( IOS_Alloc,                   0x18 )
    DEF_SYSCALL( IOS_AllocAligned,            0x19 )
    DEF_SYSCALL( IOS_Free,                    0x1A )

    DEF_SYSCALL( IOS_RegisterResourceManager, 0x1B )
    DEF_SYSCALL( IOS_Open,                    0x1C )
    DEF_SYSCALL( IOS_Close,                   0x1D )
    DEF_SYSCALL( IOS_Read,                    0x1E )
    DEF_SYSCALL( IOS_Write,                   0x1F )
    DEF_SYSCALL( IOS_Seek,                    0x20 )
    DEF_SYSCALL( IOS_Ioctl,                   0x21 )
    DEF_SYSCALL( IOS_Ioctlv,                  0x22 )
    DEF_SYSCALL( IOS_OpenAsync,               0x23 )
    DEF_SYSCALL( IOS_CloseAsync,              0x24 )
    DEF_SYSCALL( IOS_ReadAsync,               0x25 )
    DEF_SYSCALL( IOS_WriteAsync,              0x26 )
    DEF_SYSCALL( IOS_SeekAsync,               0x27 )
    DEF_SYSCALL( IOS_IoctlAsync,              0x28 )
    DEF_SYSCALL( IOS_IoctlvAsync,             0x29 )
    DEF_SYSCALL( IOS_ResourceReply,           0x2A )

    DEF_SYSCALL( IOS_SetUid,                  0x2B )
    DEF_SYSCALL( IOS_GetUid,                  0x2C )
    DEF_SYSCALL( IOS_SetGid,                  0x2D )
    DEF_SYSCALL( IOS_GetGid,                  0x2E )

    DEF_SYSCALL( IOS_InvalidateDCache,        0x3F )
    DEF_SYSCALL( IOS_FlushDCache,             0x40 )
    DEF_SYSCALL( IOS_LaunchElf,               0x41 )
    DEF_SYSCALL( IOS_LaunchOS,                0x42 )
    DEF_SYSCALL( IOS_LaunchOSFromMemory,      0x43 )
    DEF_SYSCALL( IOS_SetVersion,              0x4C )
    DEF_SYSCALL( IOS_GetVersion,              0x4D )
    DEF_SYSCALL( IOS_VirtualToPhysical,       0x4F )
    DEF_SYSCALL( IOS_SetPPCACRPerms,          0x54 )
    DEF_SYSCALL( IOS_SetIpcAccessRights,      0x59 )
    DEF_SYSCALL( IOS_LaunchRM,                0x5A )
