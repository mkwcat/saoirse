#include "main.h"
#include <cstring>
#include <cstdio>
#include <types.h>
#include <util.h>
#include <ios.h>
#include <stdarg.h>
#include <hollywood.h>

#define PRINT_BUFFER_SIZE 256

u32 stdoutQueueData[8];
static s32 stdoutFd = -1;
static s32 printBufQueue = -1;
static u32 printBufQueueData;
char logBuffer[PRINT_BUFFER_SIZE];
static bool logEnabled = true;

static const char* logColors[3] = {
    "\x1b[37;1m", "\x1b[33;1m", "\x1b[31;1m" };

void peli::Log(LogL level, const char* format, ...)
{
    if (!logEnabled)
        return;

    if (static_cast<s32>(level) >= 3)
        abort();
    while (stdoutFd < 0 && (stdoutFd = IOS_Open("/dev/stdout", 0)) < 0)
        usleep(1000);

    IOSRequest* req;
    const s32 ret = IOS_ReceiveMessage(printBufQueue, (u32*) &req, 0);
    if (ret < 0)
        exitClr(YUV_CYAN);

    /* Use the temporary log buffer then memcpy into the request
     * output to work around a hardware bug */
    const s32 pos = snprintf(logBuffer, PRINT_BUFFER_SIZE - 1,
        "%s[IOS] ", logColors[static_cast<s32>(level)]);
    va_list args;
    va_start(args, format);
    vsnprintf(logBuffer + pos, PRINT_BUFFER_SIZE - pos - 1, format, args);
    va_end(args);

    memcpy(req->ioctl.io, logBuffer, PRINT_BUFFER_SIZE);
    IOS_FlushDCache(req->ioctl.io, PRINT_BUFFER_SIZE);
    IOS_ResourceReply(req, IOS_SUCCESS);
}

void usleep(u32 usec)
{
    u32 queueData;
    const s32 queue = IOS_CreateMessageQueue(&queueData, 1);
    if (queue < 0) {
        peli::Log(LogL::ERROR, "usleep: failed to create message queue: %d", queue);
        abort();
    }

    const s32 timer = IOS_CreateTimer(usec, 0, queue, 1);
    if (timer < 0) {
        peli::Log(LogL::ERROR, "usleep: failed to create timer: %d", timer);
        abort();
    }

    u32 msg;
    const s32 ret = IOS_ReceiveMessage(queue, &msg, 0);
    if (ret < 0 || msg != 1) {
        peli::Log(LogL::ERROR, "usleep: IOS_ReceiveMessage failure: %d", ret);
        abort();
    }

    IOS_DestroyTimer(timer);
    IOS_DestroyMessageQueue(queue);
}

static void Log_IPCRequest(IOSRequest* req)
{
    switch (req->cmd)
    {
        case IOS_OPEN:
            IOS_ResourceReply(req, 0);
            break;

        case IOS_CLOSE:
            logEnabled = false;
            peli::Log(LogL::INFO, "Closing log...");
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

u8 mainHeapData[0x1000] ATTRIBUTE_ALIGN(32);
s32 mainHeap = -1;

extern "C" s32 Log_StartRM(void* arg)
{
    s32 ret = IOS_CreateHeap(mainHeapData, sizeof(mainHeapData));
    if (ret < 0)
        exitClr(YUV_YELLOW);
    mainHeap = ret;

#if 0
    /*
     * IPC mask. Not really a "mask", actually a hash lookup table.
     * Forbids the following:
     *   > /dev/boot2
     *   > /
     *   > /dev/di
     *   > /dev/scruffy
     * Planned to be used to overload certain resource managers when accessed
     * from PowerPC, as can be done with a single instruction patch in the
     * IOS_Open syscall.
     */
    u8 ipcMask[12] = {
        0x00, 0xF0, 0x00, 0x18, 0x73, 0x40, 0x01, 0x8C, 0x8C, 0x09, 0x00, 0x00
    };
    ret = IOS_SetIpcAccessRights(ipcMask);
#endif

    ret = IOS_CreateMessageQueue(&printBufQueueData, 1);
    if (ret < 0)
        exitClr(YUV_DARK_RED);
    printBufQueue = ret;

    s32 queue = IOS_CreateMessageQueue(stdoutQueueData, 8);
    if (queue < 0)
        exitClr(YUV_WHITE);
    
    ret = IOS_RegisterResourceManager("/dev/stdout", queue);
    if (ret < 0)
        exitClr(YUV_WHITE);
    
    while (1) {
        IOSRequest* req;
        if (IOS_ReceiveMessage(queue, (u32*) &req, 0) < 0)
            exitClr(YUV_WHITE);
        
        Log_IPCRequest(req);
    }
    return 0;
}

void* operator new(std::size_t size) {
    return IOS_Alloc(mainHeap, size);
}

void* operator new[](std::size_t size) {
    return IOS_Alloc(mainHeap, size);
}

void* operator new(std::size_t size, std::align_val_t align) {
    return IOS_AllocAligned(mainHeap, size, static_cast<u32>(align));
}

void* operator new[](std::size_t size, std::align_val_t align) {
    return IOS_AllocAligned(mainHeap, size, static_cast<u32>(align));
}

void kwrite32(u32 address, u32 value)
{
    const s32 queue = IOS_CreateMessageQueue((u32*) address, 1);
    if (queue < 0)
        exitClr(YUV_PINK);
    
    const s32 ret = IOS_SendMessage(queue, value, 0);
    if (ret < 0)
        exitClr(YUV_PINK);
    IOS_DestroyMessageQueue(queue);
}

void exitClr(u32 color)
{
    /* write to HW_VISOLID */
    kwrite32(static_cast<u32>(ACRReg::VISOLID) + HW_BASE_TRUSTED, color | 1);
    while (1) { }
}

void abort()
{
    peli::Log(LogL::ERROR, "Abort was called!");
    IOS_CancelThread(0, 0);
    while (1) { }
}

void __assert_fail(const char* expr, const char* file, s32 line)
{
    peli::Log(LogL::ERROR,
        "Assertion failed:\n\n%s\nfile %s, line %d", expr, file, line);
    abort();
}