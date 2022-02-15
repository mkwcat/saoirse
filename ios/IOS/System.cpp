#include "System.hpp"
#include <Debug/Log.hpp>
#include <Disk/Disk.hpp>
#include <Disk/SDCard.hpp>
#include <FAT/ff.h>
#include <IOS/EmuDI.hpp>
#include <IOS/EmuFS.hpp>
#include <IOS/IPCLog.hpp>
#include <IOS/Patch.hpp>
#include <IOS/Syscalls.h>
#include <System/Hollywood.hpp>
#include <System/OS.hpp>
#include <System/Types.h>
#include <System/Util.h>
#include <cstdio>
#include <cstring>

constexpr u32 SystemHeapSize = 0x4000; // 16 KB
s32 systemHeap = -1;

// Common ARM C++ init
void StaticInit()
{
    typedef void (*func_ptr)(void);
    extern func_ptr _init_array_start[], _init_array_end[];

    for (func_ptr* ctor = _init_array_start; ctor != _init_array_end; ctor++) {
        (*ctor)();
    }
}

void* operator new(std::size_t size)
{
    void* block = IOS_Alloc(systemHeap, size);
    assert(block != nullptr);
    return block;
}

void* operator new[](std::size_t size)
{
    void* block = IOS_Alloc(systemHeap, size);
    assert(block != nullptr);
    return block;
}

void* operator new(std::size_t size, std::align_val_t align)
{
    void* block = IOS_AllocAligned(systemHeap, size, static_cast<u32>(align));
    assert(block != nullptr);
    return block;
}

void* operator new[](std::size_t size, std::align_val_t align)
{
    void* block = IOS_AllocAligned(systemHeap, size, static_cast<u32>(align));
    assert(block != nullptr);
    return block;
}

void operator delete(void* ptr)
{
    IOS_Free(systemHeap, ptr);
}

void operator delete[](void* ptr)
{
    IOS_Free(systemHeap, ptr);
}

void operator delete(void* ptr, std::size_t size)
{
    IOS_Free(systemHeap, ptr);
}

void operator delete[](void* ptr, std::size_t size)
{
    IOS_Free(systemHeap, ptr);
}

void abort()
{
    PRINT(IOS, ERROR, "Abort was called!");
    // TODO: Application exit
    IOS_CancelThread(0, 0);
    while (true)
        ;
}

void AbortColor(u32 color)
{
    // Write to HW_VISOLID
    KernelWrite(static_cast<u32>(ACRReg::VISOLID) + HW_BASE_TRUSTED, color | 1);
    IOS_CancelThread(0, 0);
    while (true)
        ;
}

void AssertFail(const char* expr, const char* file, s32 line)
{
    PRINT(IOS, ERROR, "Assertion failed:\n\n%s\nfile %s, line %d", expr, file,
          line);
    abort();
}

void usleep(u32 usec)
{
    u32 queueData;
    const s32 queue = IOS_CreateMessageQueue(&queueData, 1);
    if (queue < 0) {
        PRINT(IOS, ERROR, "[usleep] Failed to create message queue: %d", queue);
        abort();
    }

    const s32 timer = IOS_CreateTimer(usec, 0, queue, 1);
    if (timer < 0) {
        PRINT(IOS, ERROR, "[usleep] Failed to create timer: %d", timer);
        abort();
    }

    u32 msg;
    const s32 ret = IOS_ReceiveMessage(queue, &msg, 0);
    if (ret < 0 || msg != 1) {
        PRINT(IOS, ERROR, "[usleep] IOS_ReceiveMessage failed: %d", ret);
        abort();
    }

    IOS_DestroyTimer(timer);
    IOS_DestroyMessageQueue(queue);
}

void KernelWrite(u32 address, u32 value)
{
    const s32 queue = IOS_CreateMessageQueue((u32*)address, 0x40000000);
    if (queue < 0)
        AbortColor(YUV_PINK);

    const s32 ret = IOS_SendMessage(queue, value, 0);
    if (ret < 0)
        AbortColor(YUV_PINK);
    IOS_DestroyMessageQueue(queue);
}

static void OpenTestFile()
{
    // We must attempt to open a file first for FatFS to function properly.
    FIL testFile;
    FRESULT fret = f_open(&testFile, "0:/", FA_READ);
    PRINT(IOS, INFO, "[OpenTestFile] Test open result: %d", fret);
}

bool OpenLogFile()
{
    PRINT(IOS, INFO, "Opening log file");

    FRESULT fret = f_open(&Log::logFile, "0:/saoirse_log.txt",
                          FA_CREATE_ALWAYS | FA_WRITE);
    if (fret != FR_OK) {
        PRINT(IOS, ERROR, "Failed to open log file! %d", fret);
        return false;
    }

    Log::fileLogEnabled = true;
    PRINT(IOS, INFO, "Log file opened");
    return true;
}

s32 SystemThreadEntry([[maybe_unused]] void* arg)
{
    ImportKoreanCommonKey();
    IOS::Resource::MakeIPCToCallbackThread();
    StaticInit();

    PRINT(IOS, INFO, "Wait for start request...");
    IPCLog::sInstance->WaitForStartRequest();
    PRINT(IOS, INFO, "Starting up game IOS...");

    PatchIOSOpen();

    if (!SDCard::Open()) {
        PRINT(IOS, ERROR, "SDCard::Open returned false");
        abort();
    }
    if (FSServ::MountSDCard()) {
        OpenTestFile();
        PRINT(IOS, INFO, "SD card mounted");
    }

    // TODO: Fix reentrant code so we can enable this
    if (false) {
        OpenLogFile();
    }

    new Thread(EmuFS::ThreadEntry, nullptr, nullptr, 0x800, 80);
    new Thread(EmuDI::ThreadEntry, nullptr, nullptr, 0x800, 80);

    return 0;
}

extern "C" void Entry([[maybe_unused]] void* arg)
{
    static u8 systemHeapData[SystemHeapSize] ATTRIBUTE_ALIGN(32);

    // Create system heap
    s32 ret = IOS_CreateHeap(systemHeapData, sizeof(systemHeapData));
    if (ret < 0)
        AbortColor(YUV_YELLOW);
    systemHeap = ret;

    IPCLog::sInstance = new IPCLog();
    Log::ipcLogEnabled = true;

    IOS_SetThreadPriority(0, 40);

    static u8 SystemThreadStack[0x800] ATTRIBUTE_ALIGN(32);

    ret = IOS_CreateThread(
        SystemThreadEntry, nullptr,
        reinterpret_cast<u32*>(SystemThreadStack + sizeof(SystemThreadStack)),
        sizeof(SystemThreadStack), 80, true);
    if (ret < 0)
        AbortColor(YUV_YELLOW);

    // Set new thread CPSR with system mode enabled
    u32 cpsr = 0x1F | ((u32)(SystemThreadEntry)&1 ? 0x20 : 0);
    KernelWrite(0xFFFE0000 + ret * 0xB0, cpsr);

    ret = IOS_StartThread(ret);
    if (ret < 0)
        AbortColor(YUV_YELLOW);

    IPCLog::sInstance->Run();
}