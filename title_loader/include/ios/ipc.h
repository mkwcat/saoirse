#ifndef _IOS_IPC_H
#define _IOS_IPC_H

#include <types.h>

#ifdef __cplusplus
    extern "C" {
#endif

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
        u32 req_cmd;
    };
    union
    {
        struct {
            char* path;
            u32 mode;
            s32 fd;
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
    const char* path, u32 mode, s32 queue_id, void* usrdata);
s32 IOS_Close(s32 fd);
s32 IOS_CloseAsync(
    s32 fd, s32 queue_id, void* usrdata);

s32 IOS_Seek(s32 fd, s32 where, s32 whence);
s32 IOS_SeekAsync(
    s32 fd, s32 where, s32 whence, s32 queue_id, void* usrdata);
s32 IOS_Read(s32 fd, void* buf, s32 len);
s32 IOS_ReadAsync(
    s32 fd, void* buf, s32 len, s32 queue_id, void* usrdata);
s32 IOS_Write(s32 fd, const void* buf, s32 len);
s32 IOS_WriteAsync(
    s32 fd, const void* buf, s32 len, s32 queue_id, void* usrdata);

s32 IOS_Ioctl(s32 fd, u32 command, void* in, u32 in_len, void* io, u32 io_len);
s32 IOS_IoctlAsync(
    s32 fd, u32 command, void* in, u32 in_len, void* io, u32 io_len,
    s32 queue_id, void* usrdata);

s32 IOS_Ioctlv(s32 fd, u32 command, u32 in_cnt, u32 out_cnt, IOVector* vec);
s32 IOS_IoctlvAsync(
    s32 fd, u32 command, u32 in_cnt, u32 out_cnt, IOVector* vec,
    s32 queue_id, void* usrdata);

#ifdef IOS
s32 IOS_RegisterResourceManager(const char* device, s32 queue_id);
s32 IOS_ResourceReply(const IOSRequest* request, s32 reply);
#endif

#ifdef __cplusplus
    }
#endif

#endif // _IOS_IPC_H