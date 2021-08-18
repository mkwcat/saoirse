/* IOS system call definitions */
#pragma once

#include <util.h>
#include <types.h>

#define IOS_SUCCESS       0
#define IOS_EACCES        -1
#define IOS_EEXIST        -2
#define IOS_EINVAL        -4
#define IOS_EMAX          -5
#define IOS_ENOENT        -6
#define IOS_EQUEUEFULL    -8
#define IOS_EIO           -12
#define IOS_ENOMEM        -22

EXTERN_C_START

/* <----------
 * IOS Thread
 * ----------> */
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

/* <----------
 * IOS Message
 * ----------> */
s32 IOS_CreateMessageQueue(u32* buf, u32 msg_count);
s32 IOS_DestroyMessageQueue(s32 queue_id);
s32 IOS_SendMessage(s32 queue_id, u32 message, u32 flags);
s32 IOS_JamMessage(s32 queue_id, u32 message, u32 flags);
s32 IOS_ReceiveMessage(s32 queue_id, u32* message, u32 flags);

/* <----------
 * IOS Timer
 * ----------> */
s32 IOS_CreateTimer(s32 usec, s32 repeat_usec, s32 queue, u32 msg);
s32 IOS_RestartTimer(s32 timer, s32 usec, s32 repeat_usec);
s32 IOS_StopTimer(s32 timer);
s32 IOS_DestroyTimer(s32 timer);
u32 IOS_GetTime();

/* <----------
 * IOS Memory
 * ----------> */
s32 IOS_CreateHeap(void *ptr, s32 length);
s32 IOS_DestroyHeap(s32 heap);
void* IOS_Alloc(s32 heap, u32 length);
void* IOS_AllocAligned(s32 heap, u32 length, u32 align);
s32 IOS_Free(s32 heap, void* ptr);

/* <----------
 * IPC (Inter-process communication)
 * ----------> */
#define IOS_OPEN       1
#define IOS_CLOSE      2
#define IOS_READ       3
#define IOS_WRITE      4
#define IOS_SEEK       5
#define IOS_IOCTL      6
#define IOS_IOCTLV     7
#define IOS_IPC_REPLY  8

#define IOS_OPEN_NONE  0
#define IOS_OPEN_READ  1
#define IOS_OPEN_WRITE 2
#define IOS_OPEN_RW    (IOS_OPEN_READ | IOS_OPEN_WRITE)

#define IOS_SEEK_SET   0
#define IOS_SEEK_CUR   1
#define IOS_SEEK_END   2

typedef struct
{
    void* data;
    u32 len;
} IOVector;

typedef struct
{
    u32 cmd;
    s32 result;
    union {
        s32 fd;
        s32 handle;
    };
    union
    {
        struct {
            char* path;
            u32 mode;
            s32 fd;
            u32 uid;
            u16 gid;
        } open;

        struct {
            void* data;
            u32 len;
        } read, write;

        struct {
            s32 where;
            s32 whence;
        } seek;

        struct {
            u32 cmd;
            void* in;
            u32 in_len;
            void* io;
            u32 io_len;
        } ioctl;

        struct {
            u32 cmd;
            u32 in_count;
            u32 io_count;
            IOVector* vec;
        } ioctlv;

        u32 args[5];
    };
} IOSRequest;

s32 IOS_Open(const char* path, u32 mode);
s32 IOS_OpenAsync(
    const char* path, u32 mode, s32 queue_id, IOSRequest* msg);
s32 IOS_Close(s32 fd);
s32 IOS_CloseAsync(
    s32 fd, s32 queue_id, IOSRequest* msg);

s32 IOS_Seek(s32 fd, s32 where, s32 whence);
s32 IOS_SeekAsync(
    s32 fd, s32 where, s32 whence, s32 queue_id, IOSRequest* msg);
s32 IOS_Read(s32 fd, void* buf, s32 len);
s32 IOS_ReadAsync(
    s32 fd, void* buf, s32 len, s32 queue_id, IOSRequest* msg);
s32 IOS_Write(s32 fd, const void* buf, s32 len);
s32 IOS_WriteAsync(
    s32 fd, const void* buf, s32 len, s32 queue_id, IOSRequest* msg);

s32 IOS_Ioctl(s32 fd, u32 command, void* in, u32 in_len, void* io, u32 io_len);
s32 IOS_IoctlAsync(
    s32 fd, u32 command, void* in, u32 in_len, void* io, u32 io_len,
    s32 queue_id, IOSRequest* msg);

s32 IOS_Ioctlv(s32 fd, u32 command, u32 in_cnt, u32 out_cnt, IOVector* vec);
s32 IOS_IoctlvAsync(
    s32 fd, u32 command, u32 in_cnt, u32 out_cnt, IOVector* vec,
    s32 queue_id, IOSRequest* msg);

s32 IOS_RegisterResourceManager(const char* device, s32 queue_id);
s32 IOS_ResourceReply(const IOSRequest* request, s32 reply);

/* <----------
 * ARM Cache
 * ----------> */
void IOS_InvalidateDCache(void *address, u32 size);
void IOS_FlushDCache(const void *address, u32 size);

EXTERN_C_END
