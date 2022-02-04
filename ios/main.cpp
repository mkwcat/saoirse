#include "main.h"
#include "IPCLog.hpp"
#include "patch.h"
#include <Debug/Log.hpp>
#include <Disk/Disk.hpp>
#include <Disk/SDCard.hpp>
#include <FAT/ff.h>
#include <System/Hollywood.hpp>
#include <System/OS.hpp>
#include <System/Types.h>
#include <System/Util.h>
#include <cstdio>
#include <cstring>
#include <ios.h>
#include <stdarg.h>

u8 mainThreadStack[0x400] ATTRIBUTE_ALIGN(32);

u8 mainHeapData[0x4000] ATTRIBUTE_ALIGN(32);
s32 mainHeap = -1;

extern "C" s32 DI_StartRM(void* arg);
extern "C" s32 FS_StartRM(void* arg);

void usleep(u32 usec)
{
    u32 queueData;
    const s32 queue = IOS_CreateMessageQueue(&queueData, 1);
    if (queue < 0) {
        PRINT(IOS, ERROR, "usleep: failed to create message queue: %d", queue);
        abort();
    }

    const s32 timer = IOS_CreateTimer(usec, 0, queue, 1);
    if (timer < 0) {
        PRINT(IOS, ERROR, "usleep: failed to create timer: %d", timer);
        abort();
    }

    u32 msg;
    const s32 ret = IOS_ReceiveMessage(queue, &msg, 0);
    if (ret < 0 || msg != 1) {
        PRINT(IOS, ERROR, "usleep: IOS_ReceiveMessage failure: %d", ret);
        abort();
    }

    IOS_DestroyTimer(timer);
    IOS_DestroyMessageQueue(queue);
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
    PRINT(IOS, INFO, "Test open result: %d", fret);
}

s32 mainThreadProc(void* arg)
{
    IOS::Resource::makeIpcToCallbackThread();
    cppInit();

    patchIOSOpen();
    importKoreanCommonKey();

    PRINT(IOS, INFO, "Wait for start request...");
    IPCLog::sInstance->waitForStartRequest();
    PRINT(IOS, INFO, "Starting up game IOS...");

    if (!SDCard::Open()) {
        PRINT(IOS, ERROR, "FS_StartRM: SDCard::Open returned false");
        abort();
    }
    if (FSServ::MountSDCard()) {
        OpenTestFile();
        PRINT(IOS, INFO, "SD card mounted");
    }

#if 0
    PRINT(IOS, INFO, "Opening log file");
    FRESULT fret = f_open(&Log::logFile, "0:/saoirse_log.txt",
                          FA_CREATE_ALWAYS | FA_WRITE);
    if (fret != FR_OK) {
        PRINT(IOS, ERROR, "Failed to open log file! %d", fret);
        abort();
    }
    Log::fileLogEnabled = true;
    PRINT(IOS, INFO, "Log file opened");
#endif

    new Thread(FS_StartRM, nullptr, nullptr, 0x800, 80);
    new Thread(DI_StartRM, nullptr, nullptr, 0x800, 80);

    return 0;
}

static void saoMain()
{
    s32 ret = IOS_CreateHeap(mainHeapData, sizeof(mainHeapData));
    if (ret < 0)
        exitClr(YUV_YELLOW);
    mainHeap = ret;

    IPCLog::sInstance = new IPCLog();
    Log::ipcLogEnabled = true;

    IOS_SetThreadPriority(0, 40);

    ret = IOS_CreateThread(
        mainThreadProc, nullptr,
        reinterpret_cast<u32*>(mainThreadStack + sizeof(mainThreadStack)),
        sizeof(mainThreadStack), 127, true);
    if (ret < 0)
        exitClr(YUV_YELLOW);
    /* Patch for system mode */
    u32 cpsr = 0x1F | ((u32)(mainThreadProc)&1 ? 0x20 : 0);
    kwrite32(0xFFFE0000 + ret * 0xB0, cpsr);
    ret = IOS_StartThread(ret);
    if (ret < 0)
        exitClr(YUV_YELLOW);

    IPCLog::sInstance->run();
}

void* operator new(std::size_t size)
{
    void* block = IOS_Alloc(mainHeap, size);
    assert(block != nullptr);
    return block;
}

void* operator new[](std::size_t size)
{
    void* block = IOS_Alloc(mainHeap, size);
    assert(block != nullptr);
    return block;
}

void* operator new(std::size_t size, std::align_val_t align)
{
    void* block = IOS_AllocAligned(mainHeap, size, static_cast<u32>(align));
    assert(block != nullptr);
    return block;
}

void* operator new[](std::size_t size, std::align_val_t align)
{
    void* block = IOS_AllocAligned(mainHeap, size, static_cast<u32>(align));
    assert(block != nullptr);
    return block;
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
    PRINT(IOS, ERROR, "Abort was called!");
    IOS_CancelThread(0, 0);
    while (1) {
    }
}

void __assert_fail(const char* expr, const char* file, s32 line)
{
    PRINT(IOS, ERROR, "Assertion failed:\n\n%s\nfile %s, line %d", expr, file,
          line);
    abort();
}

extern "C" s32 Log_StartRM([[maybe_unused]] void* arg)
{
    saoMain();
    return 0;
}