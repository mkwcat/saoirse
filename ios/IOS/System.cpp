// System.cpp - Saoirse IOS system
//   Written by Palapeli
//
// SPDX-License-Identifier: MIT

#include "System.hpp"
#include <DVD/DI.hpp>
#include <Debug/Log.hpp>
#include <Disk/DeviceMgr.hpp>
#include <Disk/SDCard.hpp>
#include <EmuDI/EmuDI.hpp>
#include <FAT/ff.h>
#include <IOS/EmuES.hpp>
#include <IOS/EmuFS.hpp>
#include <IOS/IPCLog.hpp>
#include <IOS/Patch.hpp>
#include <IOS/Syscalls.h>
#include <System/AES.hpp>
#include <System/Config.hpp>
#include <System/ES.hpp>
#include <System/Hollywood.hpp>
#include <System/OS.hpp>
#include <System/SHA.hpp>
#include <System/Types.h>
#include <System/Util.h>
#include <cstdio>
#include <cstring>

constexpr u32 SystemHeapSize = 0x40000; // 256 KB
s32 System::s_heapId = -1;

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
    void* block = IOS_Alloc(System::GetHeap(), size);
    assert(block != nullptr);
    return block;
}

void* operator new[](std::size_t size)
{
    void* block = IOS_Alloc(System::GetHeap(), size);
    assert(block != nullptr);
    return block;
}

void* operator new(std::size_t size, std::align_val_t align)
{
    void* block =
      IOS_AllocAligned(System::GetHeap(), size, static_cast<u32>(align));
    assert(block != nullptr);
    return block;
}

void* operator new[](std::size_t size, std::align_val_t align)
{
    void* block =
      IOS_AllocAligned(System::GetHeap(), size, static_cast<u32>(align));
    assert(block != nullptr);
    return block;
}

void operator delete(void* ptr)
{
    IOS_Free(System::GetHeap(), ptr);
}

void operator delete[](void* ptr)
{
    IOS_Free(System::GetHeap(), ptr);
}

void operator delete(void* ptr, std::size_t size)
{
    IOS_Free(System::GetHeap(), ptr);
}

void operator delete[](void* ptr, std::size_t size)
{
    IOS_Free(System::GetHeap(), ptr);
}

void operator delete(void* ptr, std::size_t size, std::align_val_t align)
{
    IOS_Free(System::GetHeap(), ptr);
}

void operator delete[](void* ptr, std::size_t size, std::align_val_t align)
{
    IOS_Free(System::GetHeap(), ptr);
}

void abort()
{
    PRINT(IOS, ERROR, "Abort was called! Thread: %d", IOS_GetThreadId());
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

extern "C" void __AssertFail(
  const char* expr, const char* file, s32 line, u32 lr)
{
    PRINT(IOS, ERROR, "Assertion failed:\n\n%s\nfile %s, line %d, LR: %08X",
      expr, file, line, lr);
    abort();
}

// clang-format off
ASM_ARM_FUNCTION(void AssertFail(const char* expr, const char* file, s32 line),
    mov     r3, lr;
    b       __AssertFail;
)

// clang-format on

void usleep(u32 usec)
{
    if (usec == 0)
        return;

    u32 queueData;
    const s32 queue = IOS_CreateMessageQueue(&queueData, 1);
    if (queue < 0) {
        PRINT(IOS, ERROR, "Failed to create message queue: %d", queue);
        abort();
    }

    const s32 timer = IOS_CreateTimer(usec, 0, queue, 1);
    if (timer < 0) {
        PRINT(IOS, ERROR, "Failed to create timer: %d", timer);
        abort();
    }

    u32 msg;
    const s32 ret = IOS_ReceiveMessage(queue, &msg, 0);
    if (ret < 0 || msg != 1) {
        PRINT(IOS, ERROR, "IOS_ReceiveMessage failed: %d", ret);
        abort();
    }

    IOS_DestroyTimer(timer);
    IOS_DestroyMessageQueue(queue);
}

bool s_timerStarted = false;
u8 s_timerIndex = 0;
u64 s_baseEpoch = 0;

struct {
    u32 m_timer = 0;
    u64 m_tick = 0;
} s_timerCtx[2];

#define diff_ticks(tick0, tick1)                                               \
  (((u64) (tick1) < (u64) (tick0))                                             \
      ? ((u64) -1 - (u64) (tick0) + (u64) (tick1))                             \
      : ((u64) (tick1) - (u64) (tick0)))

static s32 TimerThreadEntry([[maybe_unused]] void* arg)
{
    // 32 minute interval
    static constexpr u32 TimerInterval = 1000 * (60 * 32);

    while (true) {
        usleep(TimerInterval);

        u8 prev = s_timerIndex;
        u8 next = prev ^ 1;

        u32 prevTimer = s_timerCtx[prev].m_timer;
        u32 nextTimer = ACRReadTrusted(ACRReg::TIMER);

        s_timerCtx[next].m_tick =
          s_timerCtx[prev].m_tick + diff_ticks(prevTimer, nextTimer);
        s_timerCtx[next].m_timer = nextTimer;
        s_timerIndex = next;
    }
}

void System::SetTime(u32 hwTimerVal, u64 epoch)
{
    s_timerCtx[s_timerIndex].m_timer = hwTimerVal;
    s_timerCtx[s_timerIndex].m_tick = 0;
    s_baseEpoch = epoch;

    if (!s_timerStarted) {
        s_timerStarted = true;
        new Thread(TimerThreadEntry, nullptr, nullptr, 0x400, 1);
    }
}

u64 System::GetTime()
{
    u8 i = s_timerIndex;
    u64 timeNow = s_timerCtx[i].m_tick + diff_ticks(s_timerCtx[i].m_timer,
                                           ACRReadTrusted(ACRReg::TIMER));

    return s_baseEpoch + (timeNow / 1898614);
}

/**
 * Memcpy with only word writes to work around a Wii hardware bug.
 */
void* System::UnalignedMemcpy(void* dest, const void* src, size_t len)
{
    const u32 destAddr = u32(dest);
    const u32 destRounded = round_down(destAddr, 4);
    const u32 destEndAddr = destAddr + len;
    const u32 destEndRounded = round_down(destEndAddr, 4);

    // Do main rounded copy (optimized memcpy will copy in words anyway)
    memcpy(round_up(dest, 4), src + round_up(destAddr, 4) - destAddr,
      destEndRounded - round_up(destAddr, 4));

    // Write the leading bytes
    if (destRounded != destAddr) {
        u32 srcData;
        memcpy(&srcData, src, 4);

        srcData >>= ((destAddr % 4) * 8);
        u32 mask = 0xFFFFFFFF >> ((destAddr % 4) * 8);
        if (destEndAddr - destRounded < 4)
            mask &= ~(0xFFFFFFFF >> ((destEndAddr - destRounded) * 8));
        mask32(destRounded, mask, srcData & mask);
    }

    // Write the trailing bytes
    if (destEndAddr != destEndRounded &&
        // Check if this was covered by the leading bytes copy
        (destEndRounded != destRounded || destRounded == destAddr)) {
        u32 srcData;
        memcpy(&srcData, src + (destEndRounded - destAddr), 4);

        u32 mask = ~(0xFFFFFFFF >> ((destEndAddr - destEndRounded) * 8));
        mask32(destEndRounded, mask, srcData & mask);
    }

    return dest;
}

void KernelWrite(u32 address, u32 value)
{
    const s32 queue = IOS_CreateMessageQueue((u32*) address, 0x40000000);
    if (queue < 0)
        AbortColor(YUV_PINK);

    const s32 ret = IOS_SendMessage(queue, value, 0);
    if (ret < 0)
        AbortColor(YUV_PINK);
    IOS_DestroyMessageQueue(queue);
}

s32 SystemThreadEntry([[maybe_unused]] void* arg)
{
    SHA::s_instance = new SHA();
    AES::s_instance = new AES();
    DI::s_instance = new DI();
    ES::s_instance = new ES();

    ImportKoreanCommonKey();
    IOS::Resource::MakeIPCToCallbackThread();
    StaticInit();

    DeviceMgr::s_instance = new DeviceMgr();

    PRINT(IOS, INFO, "Wait for start request...");
    IPCLog::s_instance->WaitForStartRequest();
    PRINT(IOS, INFO, "Starting up game IOS...");

    PatchIOSOpen();

    new Thread(EmuFS::ThreadEntry, nullptr, nullptr, 0x2000, 80);
    new Thread(EmuDI::ThreadEntry, nullptr, nullptr, 0x2000, 80);
    new Thread(EmuES::ThreadEntry, nullptr, nullptr, 0x2000, 80);

    return 0;
}

extern "C" void Entry([[maybe_unused]] void* arg)
{
    static u8 systemHeapData[SystemHeapSize] ATTRIBUTE_ALIGN(32);

    // Create system heap
    s32 ret = IOS_CreateHeap(systemHeapData, sizeof(systemHeapData));
    if (ret < 0)
        AbortColor(YUV_YELLOW);
    System::SetHeap(ret);

    Config::s_instance = new Config();
    IPCLog::s_instance = new IPCLog();
    Log::ipcLogEnabled = true;

    IOS_SetThreadPriority(0, 40);

    static u8 SystemThreadStack[0x800] ATTRIBUTE_ALIGN(32);

    ret = IOS_CreateThread(SystemThreadEntry, nullptr,
      reinterpret_cast<u32*>(SystemThreadStack + sizeof(SystemThreadStack)),
      sizeof(SystemThreadStack), 80, true);
    if (ret < 0)
        AbortColor(YUV_YELLOW);

    // Set new thread CPSR with system mode enabled
    u32 cpsr = 0x1F | ((u32) (SystemThreadEntry) &1 ? 0x20 : 0);
    KernelWrite(0xFFFE0000 + ret * 0xB0, cpsr);

    ret = IOS_StartThread(ret);
    if (ret < 0)
        AbortColor(YUV_YELLOW);

    IPCLog::s_instance->Run();
}
