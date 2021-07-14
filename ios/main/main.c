#include "main.h"
#include <iosstd.h>
#include <types.h>
#include <util.h>
#include <ios.h>
#include <stdarg.h>
#include <vsprintf.h>
#include <hollywood.h>

#define PRINT_BUFFER_SIZE 256

u32 stdoutQueueData[8];
static s32 stdoutFd = -1;
static s32 printBufQueue = -1;
static u32 printBufQueueData;
char logBuffer[PRINT_BUFFER_SIZE];

static const char* logColors[3] = {
    "\x1b[37;1m", "\x1b[33;1m", "\x1b[31;1m" };

void printf(s32 level, const char* format, ...)
{
    if (level >= 3)
        abort();
    while (stdoutFd < 0 && (stdoutFd = IOS_Open("/dev/stdout", 0)) < 0)
        usleep(1000);

    IOSRequest* req;
    const s32 ret = IOS_ReceiveMessage(printBufQueue, (u32*) &req, 0);
    if (ret < 0)
        _exit(YUV_CYAN);

    /* Use the temporary log buffer then memcpy into the request
     * output to work around a hardware bug */
    const s32 pos = snprintf(logBuffer, PRINT_BUFFER_SIZE - 1,
        "%s[IOS] ", logColors[level]);
    va_list args;
    va_start(args, format);
    vsnprintf(logBuffer + pos, PRINT_BUFFER_SIZE - pos - 1, format, args);
    va_end(args);

    memcpy32(req->ioctl.io, logBuffer, PRINT_BUFFER_SIZE);
    IOS_FlushDCache(req->ioctl.io, PRINT_BUFFER_SIZE);
    IOS_ResourceReply(req, IOS_SUCCESS);
}

static void Log_IPCRequest(IOSRequest* req)
{
    switch (req->cmd)
    {
        case IOS_OPEN:
        case IOS_CLOSE:
            IOS_ResourceReply(req, 0);
            break;
        
        case IOS_IOCTL:
            if (req->ioctl.cmd == 0) {
                /* Read from console */
                if (req->ioctl.io_len != PRINT_BUFFER_SIZE) {
                    IOS_ResourceReply(req, IOS_EINVAL);
                    break;
                }
                /* Will reply on next printf */
                IOS_SendMessage(printBufQueue, (u32) req, 0);
                break;
            }
            IOS_ResourceReply(req, IOS_EINVAL);
            break;

        default:
            IOS_ResourceReply(req, IOS_EINVAL);
            break;
    }
}

s32 Log_StartRM(void* arg)
{
    s32 ret = IOS_CreateMessageQueue(&printBufQueueData, 1);
    if (ret < 0)
        _exit(YUV_DARK_RED);
    printBufQueue = ret;

    s32 queue = IOS_CreateMessageQueue(stdoutQueueData, 8);
    if (queue < 0)
        _exit(YUV_WHITE);
    
    ret = IOS_RegisterResourceManager("/dev/stdout", queue);
    if (ret < 0)
        _exit(YUV_WHITE);
    
    while (1) {
        IOSRequest* req;
        if (IOS_ReceiveMessage(queue, (u32*) &req, 0) < 0)
            _exit(YUV_WHITE);
        
        Log_IPCRequest(req);
    }
    return 0;
}

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

void __assert_fail(const char* expr, const char* file, s32 line)
{
    printf(ERROR,
        "Assertion failed:\n\n%s\nfile %s, line %d", expr, file, line);
    abort();
}