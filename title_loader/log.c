#include "log.h"
#include "main.h"
#include <ios.h>
#include <stdarg.h>
#include <string.h>
#include <vsprintf.h>

u32 stdoutQueueData[8];
static s32 printBufQueue = -1;
static u32 printBufQueueData;
char logBuffer[256];

static void sendPrint();

void printf(s32 level, const char* format, ...)
{
    IOSRequest* req;
    /* This will block until an IOCTL request is available */
    const s32 ret = IOS_ReceiveMessage(printBufQueue, (u32*) &req, 0);
    if (ret < 0 || req == NULL)
        _exit(YUV_WHITE);

    va_list args;
	va_start(args, format);
    /* Use the temporary log buffer then memcpy into the request
     * output to work around a hardware bug */
    vsnprintf(logBuffer, PRINT_BUFFER_SIZE - 1, format, args);
    va_end(args);

    memcpy32(req->ioctl.io, logBuffer, PRINT_BUFFER_SIZE);
    IOS_FlushDCache(req->ioctl.io, PRINT_BUFFER_SIZE);
    IOS_ResourceReply(req, level);
}

static void stdoutCommand(IOSRequest* req)
{
    switch (req->cmd)
    {
        case IOS_OPEN:
        case IOS_CLOSE:
            IOS_ResourceReply(req, 0);
            break;
        
        case IOS_IOCTL:
            if (req->ioctl.io_len != PRINT_BUFFER_SIZE) {
                IOS_ResourceReply(req, -4);
                return;
            }
            /* Will reply on next printf */
            IOS_SendMessage(printBufQueue, (u32) req, 0);
            break;

        default:
            IOS_ResourceReply(req, -4);
            return;
    }
}

s32 stdoutThread(void* arg)
{
    s32 queue = IOS_CreateMessageQueue(stdoutQueueData, 8);
    if (queue < 0)
        _exit(YUV_WHITE);
    
    s32 ret = IOS_RegisterResourceManager("/dev/stdout", queue);
    if (ret < 0)
        _exit(YUV_WHITE);
    
    while (1) {
        IOSRequest* req;
        if (IOS_ReceiveMessage(queue, (u32*) &req, 0) < 0)
            _exit(YUV_WHITE);
        
        stdoutCommand(req);
    }
    return 0;
}

static u32 stdoutThreadStack[0x400 / 4];

void stdoutInit()
{
    /* Return if called again */
    if (printBufQueue >= 0)
        return;

    s32 ret = IOS_CreateMessageQueue(&printBufQueueData, 1);
    if (ret < 0)
        _exit(YUV_DARK_RED);
    printBufQueue = ret;
    
    const s32 tid = IOS_CreateThread(
        stdoutThread, NULL, stdoutThreadStack + 0x400 / 4, 0x400, 80, true);
    if (tid < 0)
        _exit(YUV_DARK_BLUE);

    ret = IOS_StartThread(tid);
    if (ret < 0)
        _exit(YUV_GREEN);
    
    return;
}