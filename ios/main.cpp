#include "main.h"
#include "patch.h"
#include <cstdio>
#include <cstring>
#include <disk.h>
#include <ff.h>
#include <hollywood.h>
#include <ios.h>
#include <os.h>
#include <sdcard.h>
#include <stdarg.h>
#include <types.h>
#include <util.h>

u8 mainThreadStack[0x400] ATTRIBUTE_ALIGN(32);
u8 DI_RMStack[0x400] ATTRIBUTE_ALIGN(32);
u8 FS_RMStack[0x800] ATTRIBUTE_ALIGN(32);

u8 mainHeapData[0x1000] ATTRIBUTE_ALIGN(32);
s32 mainHeap = -1;

extern "C" s32 DI_StartRM(void* arg);
extern "C" s32 FS_StartRM(void* arg);

#define PRINT_BUFFER_SIZE 256

u32 stdoutQueueData[8];
static s32 printBufQueue = -1;
static u32 printBufQueueData;
char logBuffer[PRINT_BUFFER_SIZE];
static bool logEnabled = false;
static bool logFileEnabled = false;
static FIL logFile;

static s32 startGameWaitQueue = -1;
static u32 startGameWaitQueueData;

static const char* logColors[3] = {"\x1b[37;1m", "\x1b[33;1m", "\x1b[31;1m"};

void peli::Log(LogL level, const char* format, ...)
{
    if (static_cast<s32>(level) >= 3)
        abort();

    IOSRequest* req;
    if (logEnabled) {
        const s32 ret = IOS_ReceiveMessage(printBufQueue, (u32*)&req, 0);
        if (ret < 0)
            exitClr(YUV_CYAN);
    }

    /* Use the temporary log buffer then memcpy into the request
     * output to work around a hardware bug */
    const s32 pos = snprintf(logBuffer, PRINT_BUFFER_SIZE - 2, "%s[IOS] ",
                             logColors[static_cast<s32>(level)]);
    va_list args;
    va_start(args, format);
    vsnprintf(logBuffer + pos, PRINT_BUFFER_SIZE - pos - 1, format, args);
    va_end(args);

    if (logEnabled) {
        memcpy(req->ioctl.io, logBuffer, PRINT_BUFFER_SIZE);
        IOS_FlushDCache(req->ioctl.io, PRINT_BUFFER_SIZE);
        IOS_ResourceReply(req, 0);
    }

    if (logFileEnabled) {
        UINT bw = 0;
        f_write(&logFile, logBuffer + 7, strlen(logBuffer) - 7, &bw);
        static const char newline = '\n';
        f_write(&logFile, &newline, 1, &bw);
        f_sync(&logFile);
    }
}

void peli::NotifyResourceStarted()
{
    if (!logEnabled)
        return;

    IOSRequest* req;
    const s32 ret = IOS_ReceiveMessage(printBufQueue, (u32*)&req, 0);
    if (ret < 0)
        exitClr(YUV_CYAN);

    IOS_ResourceReply(req, 1);
}

void usleep(u32 usec)
{
    u32 queueData;
    const s32 queue = IOS_CreateMessageQueue(&queueData, 1);
    if (queue < 0) {
        peli::Log(LogL::ERROR, "usleep: failed to create message queue: %d",
                  queue);
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
    switch (req->cmd) {
    case IOS_OPEN:
        IOS_ResourceReply(req, 0);
        break;

    case IOS_CLOSE: {
        logEnabled = false;
        usleep(10000);
        IOSRequest* req2;
        const s32 ret = IOS_ReceiveMessage(printBufQueue, (u32*)&req2, 0);
        if (ret < 0)
            exitClr(YUV_CYAN);
        IOS_ResourceReply(req2, 2);
        IOS_ResourceReply(req, 0);
        break;
    }

    case IOS_IOCTL:
        if (req->ioctl.cmd == 0) {
            /* Read from console */
            if (req->ioctl.io_len != PRINT_BUFFER_SIZE) {
                IOS_ResourceReply(req, IOS_EINVAL);
                break;
            }
            /* Will reply on next printf */
            IOS_SendMessage(printBufQueue, (u32)req, 0);
            break;
        }
        if (req->ioctl.cmd == 1) {
            // Start game IOS command
            IOS_SendMessage(startGameWaitQueue, 0, 0);
            IOS_ResourceReply(req, IOS_SUCCESS);
            break;
        }
        IOS_ResourceReply(req, IOS_EINVAL);
        break;

    default:
        IOS_ResourceReply(req, IOS_EINVAL);
        break;
    }
}

void kwrite32(u32 address, u32 value)
{
    const s32 queue = IOS_CreateMessageQueue((u32*)address, 0x40000000);
    if (queue < 0)
        exitClr(YUV_PINK);

    const s32 ret = IOS_SendMessage(queue, value, 0);
    if (ret < 0)
        exitClr(YUV_PINK);
    IOS_DestroyMessageQueue(queue);
}

/* Common ARM C++ init */
typedef void (*func_ptr)(void);
extern func_ptr _init_array_start[], _init_array_end[];

void cppInit()
{
    for (func_ptr* ctor = _init_array_start; ctor != _init_array_end; ctor++) {
        (*ctor)();
    }
}

static void OpenTestFile()
{
    /* We must attempt to open a file first for FatFS to function properly */
    FIL testFile;
    FRESULT fret = f_open(&testFile, "0:/", FA_READ);
    peli::Log(LogL::INFO, "Test open result: %d", fret);
}

s32 mainThreadProc(void* arg)
{
    patchIOSOpen();
    importKoreanCommonKey();

    s32 ret = IOS_CreateHeap(mainHeapData, sizeof(mainHeapData));
    if (ret < 0)
        exitClr(YUV_YELLOW);
    mainHeap = ret;

    cppInit();
    IOS::Resource::makeIpcToCallbackThread();

    u32 msg;
    ret = IOS_ReceiveMessage(startGameWaitQueue, &msg, 0);
    if (ret != 0) {
        peli::Log(LogL::ERROR,
                  "IOS_ReceiveMessage(startGameWaitQueue) failed! %d", ret);
        abort();
    }

    peli::Log(LogL::INFO, "Starting up game IOS...");

    if (!SDCard::Open()) {
        peli::Log(LogL::ERROR, "FS_StartRM: SDCard::Open returned false");
        abort();
    }
    if (FSServ::MountSDCard()) {
        OpenTestFile();
        peli::Log(LogL::INFO, "SD card mounted");
    }

    peli::Log(LogL::INFO, "Opening log file");
    FRESULT fret = f_open(&logFile, "0:/saoirselog.txt", FA_CREATE_ALWAYS | FA_WRITE);
    if (fret != FR_OK) {
        peli::Log(LogL::ERROR, "Failed to open log file! %d", fret);
        abort();
    }
    logFileEnabled = true;
    peli::Log(LogL::INFO, "Log file opened");

    ret = IOS_CreateThread(
        FS_StartRM, nullptr,
        reinterpret_cast<u32*>(FS_RMStack + sizeof(FS_RMStack)),
        sizeof(FS_RMStack), 80, false);
    if (ret < 0)
        exitClr(YUV_DARK_BLUE);
    ret = IOS_StartThread(ret);
    if (ret < 0)
        exitClr(YUV_DARK_BLUE);

    ret = IOS_CreateThread(
        DI_StartRM, nullptr,
        reinterpret_cast<u32*>(DI_RMStack + sizeof(DI_RMStack)),
        sizeof(DI_RMStack), 80, false);
    if (ret < 0)
        exitClr(YUV_DARK_RED);
    ret = IOS_StartThread(ret);
    if (ret < 0)
        exitClr(YUV_DARK_RED);

    return 0;
}

static void saoMain()
{
    logEnabled = true;

    s32 ret = IOS_CreateMessageQueue(&startGameWaitQueueData, 1);
    if (ret < 0)
        exitClr(YUV_DARK_RED);
    startGameWaitQueue = ret;

    ret = IOS_CreateMessageQueue(&printBufQueueData, 1);
    if (ret < 0)
        exitClr(YUV_DARK_RED);
    printBufQueue = ret;

    IOS_SetThreadPriority(0, 40);

    ret = IOS_CreateThread(
        mainThreadProc, nullptr,
        reinterpret_cast<u32*>(mainThreadStack + sizeof(mainThreadStack)),
        sizeof(mainThreadStack), 127, false);
    if (ret < 0)
        exitClr(YUV_YELLOW);
    /* Patch for system mode */
    u32 cpsr = 0x1F | ((u32)(mainThreadProc)&1 ? 0x20 : 0);
    kwrite32(0xFFFE0000 + ret * 0xB0, cpsr);
    ret = IOS_StartThread(ret);
    if (ret < 0)
        exitClr(YUV_YELLOW);

    s32 queue = IOS_CreateMessageQueue(stdoutQueueData, 8);
    if (queue < 0)
        exitClr(YUV_WHITE);

    ret = IOS_RegisterResourceManager("/dev/stdout", queue);
    if (ret < 0)
        exitClr(YUV_WHITE);

    while (1) {
        IOSRequest* req;
        if (IOS_ReceiveMessage(queue, (u32*)&req, 0) < 0)
            exitClr(YUV_WHITE);

        Log_IPCRequest(req);
    }
}

void* operator new(std::size_t size)
{
    return IOS_Alloc(mainHeap, size);
}

void* operator new[](std::size_t size)
{
    return IOS_Alloc(mainHeap, size);
}

void* operator new(std::size_t size, std::align_val_t align)
{
    return IOS_AllocAligned(mainHeap, size, static_cast<u32>(align));
}

void* operator new[](std::size_t size, std::align_val_t align)
{
    return IOS_AllocAligned(mainHeap, size, static_cast<u32>(align));
}

void operator delete(void* ptr)
{
    IOS_Free(mainHeap, ptr);
}

void operator delete[](void* ptr)
{
    IOS_Free(mainHeap, ptr);
}

void operator delete(void* ptr, std::size_t size)
{
    IOS_Free(mainHeap, ptr);
}

void operator delete[](void* ptr, std::size_t size)
{
    IOS_Free(mainHeap, ptr);
}

void exitClr(u32 color)
{
    /* write to HW_VISOLID */
    kwrite32(static_cast<u32>(ACRReg::VISOLID) + HW_BASE_TRUSTED, color | 1);
    IOS_CancelThread(0, 0);
    while (1) {
    }
}

void abort()
{
    peli::Log(LogL::ERROR, "Abort was called!");
    IOS_CancelThread(0, 0);
    while (1) {
    }
}

void __assert_fail(const char* expr, const char* file, s32 line)
{
    peli::Log(LogL::ERROR, "Assertion failed:\n\n%s\nfile %s, line %d", expr,
              file, line);
    abort();
}

extern "C" s32 Log_StartRM([[maybe_unused]] void* arg)
{
    saoMain();
    return 0;
}