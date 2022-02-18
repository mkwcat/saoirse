// IOSBoot.cpp - IOS startup code
//   Written by Palapeli
//
// Copyright (C) 2022 Team Saoirse
// SPDX-License-Identifier: MIT

#include "IOSBoot.hpp"
#include "Arch.hpp"
#include <Debug/Log.hpp>
#include <System/Hollywood.hpp>
#include <System/Util.h>
#include <new>
#include <ogc/cache.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// #define IOS_LAUNCH_FAIL_DEBUG

#ifdef IOS_LAUNCH_FAIL_DEBUG
LIBOGC_SUCKS_BEGIN
#include <ogc/pad.h>
LIBOGC_SUCKS_END
#endif

template <class T>
constexpr T SRAMMirrToReal(T address)
{
    return reinterpret_cast<T>(reinterpret_cast<u32>(address) - 0xF2B00000);
}

constexpr u32 syscall(u32 id)
{
    return 0xE6000010 | id << 5;
}

constexpr u32 VFILE_ADDR = 0x91000000;
constexpr u32 VFILE_SIZE = 0x100000;

template <u32 TSize>
struct VFile {
    static constexpr u32 MAGIC = 0x46494C45; /* FILE */

    VFile(const void* data, u32 len) : m_magic(MAGIC), m_length(len), m_pos(0)
    {
        ASSERT(len <= TSize);
        ASSERT(len >= 0x34);
        ASSERT(!memcmp(data,
                       "\x7F"
                       "ELF",
                       4));
        memcpy(m_data, data, len);
        m_data[7] = 0x61;
        m_data[8] = 1;
        IOSBoot::SafeFlushRange(reinterpret_cast<void*>(this), 32 + len);
    }

    u32 m_magic;
    u32 m_length;
    u32 m_pos;
    u32 m_pad[8 - 3];
    u8 m_data[TSize];
};

/*
 * Performs an IOS exploit and branches to the entrypoint in system mode.
 *
 * Exploit summary:
 * - IOS does not check validation of vectors with length 0.
 * - All memory regions mapped as readable are executable (ARMv5 has no
 *   'no execute' flag).
 * - NULL/0 points to the beginning of MEM1.
 * - The /dev/sha resource manager, part of IOSC, runs in system mode.
 * - It's obvious basically none of the code was audited at all.
 *
 * IOCTL 0 (SHA1_Init) writes to the context vector (1) without checking the
 * length at all. Two of the 32-bit values it initializes are zero.
 *
 * Common approach: Point the context vector to the LR on the stack and then
 * take control after return.
 * A much more stable approach taken here: Overwrite the PC of the idle thread,
 * which should always have its context start at 0xFFFE0000 in memory (across
 * IOS versions).
 */
s32 IOSBoot::Entry(u32 entrypoint)
{
    IOS::ResourceCtrl<u32> sha("/dev/sha");
    if (sha.fd() < 0)
        return sha.fd();

    PRINT(Core, INFO, "Exploit: Setting up MEM1");
    u32* mem1 = reinterpret_cast<u32*>(MEM1_BASE);
    mem1[0] = 0x4903468D; // ldr r1, =0x10100000; mov sp, r1;
    mem1[1] = 0x49034788; // ldr r1, =entrypoint; blx r1;
    /* Overwrite reserved handler to loop infinitely */
    mem1[2] = 0x49036209; // ldr r1, =0xFFFF0014; str r1, [r1, #0x20];
    mem1[3] = 0x47080000; // bx r1
    mem1[4] = 0x10100000; // temporary stack
    mem1[5] = entrypoint;
    mem1[6] = 0xFFFF0014; // reserved handler

    IOS::IOVector<1, 2> vec;
    vec.in[0].data = NULL;
    vec.in[0].len = 0;
    vec.out[0].data = reinterpret_cast<void*>(0xFFFE0028);
    vec.out[0].len = 0;
    /* Unused vector utilized for cache safety */
    vec.out[1].data = MEM1_BASE;
    vec.out[1].len = 32;

    PRINT(Core, INFO, "Exploit: Doing exploit call");
    return sha.ioctlv(0, vec);
}

void IOSBoot::SafeFlushRange(const void* data, u32 len)
{
    // The IPC function flushes the cache here on PPC, and then IOS invalidates
    // its own cache.
    // Note: Neither libogc or IOS check for the invalid fd before they do what
    // we want, but libogc _could_ change. Keep an eye on this.
    IOS_Write(-1, data, len);
}

/* Async ELF launch */
s32 IOSBoot::Launch(const void* data, u32 len)
{
    new (reinterpret_cast<void*>(VFILE_ADDR)) VFile<VFILE_SIZE>(data, len);

    u32 loaderSize;
    const void* loader = Arch::getFileStatic("ios_loader.bin", &loaderSize);

    u8* loaderMemory = reinterpret_cast<u8*>(0x90100000);
    memcpy(loaderMemory, loader, loaderSize);
    SafeFlushRange(loaderMemory, loaderSize);

    return Entry(reinterpret_cast<u32>(loaderMemory) & ~0xC0000000);
}

void IOSBoot::LaunchSaoirseIOS()
{
    u32 elfSize = 0;
    const void* elf = Arch::getFileStatic("saoirse_ios.elf", &elfSize);
    assert(elf != nullptr);

#ifdef IOS_LAUNCH_FAIL_DEBUG
    SetupPrintHook();
#endif

    PRINT(Core, INFO, "Starting up Saoirse IOS...");
    s32 ret = Launch(elf, elfSize);
    PRINT(Core, INFO, "IOSBoot::Launch result: %d", ret);

    if (ret == IOSError::OK) {
        IPCLog::sInstance = new IPCLog();
    }

#ifdef IOS_LAUNCH_FAIL_DEBUG
    sleep(1);
    ReadPrintHook();
    DebugLaunchReport();

    PAD_Init();

    while (true) {
        PAD_ScanPads();
        int buttonsDown = PAD_ButtonsDown(0);
        if (buttonsDown & PAD_BUTTON_A)
            ACRMaskTrusted(ACRReg::RESETS, 1, 0);
    }
#endif
}

static bool ValidIOSSPAddr(u32 sp)
{
    if ((sp >= 0x10000000 && (sp + 16) < 0x14000000) ||
        (sp >= 0x0D4E0000 && (sp + 16) < 0x0D500000)) {
        return true;
    }
    return false;
}

static bool ValidIOSPCAddr(u32 pc)
{
    if ((pc + 16) < 0x01800000)
        return true;

    if ((pc >= 0x13400000 && (pc + 16) < 0x14000000) ||
        (pc >= 0x0D4E0000 && (pc + 16) < 0x0D500000)) {
        return true;
    }
    return false;
}

static void DebugCodeDump(u32 pc)
{
    // If the PC is in SRAM, translate it.
    if (pc >= 0xFFFE0000 && pc <= 0xFFFFFFFF) {
        pc = SRAMMirrToReal(pc);
    }

    // Check if we're executing from an accessible memory address and then dump
    // 6 words.
    if (ValidIOSPCAddr(pc)) {
        for (int i = 0; i < 24; i += 4) {
            printf("%08X ", read32(pc + i));
        }
        printf("\n");
    }
}

static void DebugRegisterDump(u32 addr)
{
    u32 reg[16];

    for (int i = 0; i < 16; i++) {
        reg[i] = read32(addr + i * 4);
    }

    printf(
        "R0       R1       R2       R3       R4       R5       R6       R7\n");
    printf("%08X %08X %08X %08X %08X %08X %08X %08X\n", reg[0], reg[1], reg[2],
           reg[3], reg[4], reg[5], reg[6], reg[7]);
    printf(
        "R8       R9       R10      R11      R12      R13      R14      R15\n");
    printf("%08X %08X %08X %08X %08X %08X %08X %08X\n", reg[8], reg[9], reg[10],
           reg[11], reg[12], reg[13], reg[14], reg[15]);
}

static void ReportIOSReceiveMessage([[maybe_unused]] u32 sp)
{
    // TODO: I need a more complete RAM dump to research this.
}

static void ReportIOSThread(int id)
{
    const u32 threadPtr = SRAMMirrToReal(0xFFFE0000 + 0xB0 * id);

    PRINT(Core, INFO, "--- Thread %d (PID: %d) ---", id,
          read32(threadPtr + 0x54));

    PRINT(Core, INFO, "CPSR 0x%08X; State 0x%04X; PC 0x%08X; LR 0x%08X",
          read32(threadPtr + 0x00), read32(threadPtr + 0x50),
          read32(threadPtr + 0x40), read32(threadPtr + 0x3C));

    // All addresses here should be physical (we don't map anything
    // different from the physical address).
    u32 pc = read32(threadPtr + 0x40);
    u32 lr = read32(threadPtr + 0x3C);
    u32 sp = read32(threadPtr + 0x38);

    // Convert the values to readable addresses.
    pc = (pc >= 0xFFFE0000 && pc <= 0xFFFFFFFF) ? SRAMMirrToReal(pc) : pc;
    lr = (lr >= 0xFFFE0000 && lr <= 0xFFFFFFFF) ? SRAMMirrToReal(lr) : lr;
    sp = (sp >= 0xFFFE0000 && sp <= 0xFFFFFFFF) ? SRAMMirrToReal(sp) : lr;
    // Zero address if it is invalid
    pc = ValidIOSPCAddr(pc) ? pc : 0;
    lr = ValidIOSPCAddr(lr) ? lr : 0;
    sp = ValidIOSSPAddr(sp) ? sp : 0;
    // Align to word
    pc &= ~3;
    lr &= ~3;
    sp &= ~3;

    if (pc != 0) {
        PRINT(Core, INFO, "PC Dump (-8):");
        DebugCodeDump(pc - 8);
    }

    if (lr != 0) {
        PRINT(Core, INFO, "LR Dump (-8):");
        DebugCodeDump(lr - 8);
    }

    DebugRegisterDump(threadPtr + 4);

    // Check for specific contexts and dump more information.

    // Blocked in IOS_ReceiveMessage.
    if (pc != 0 && read32(pc) == 0x1BFFFF2C && read32(pc + 4) == 0xEAFFFFD7) {
        if (sp == 0) {
            PRINT(Core, INFO,
                  "Cannot give IOS_ReceiveMessage report: Invalid sp!");
        } else {
            // Valid sp, let's give report (TODO)
            PRINT(Core, INFO, "Dumping IOS_ReceiveMessage context: TODO!");
            ReportIOSReceiveMessage(sp);
        }
    }
}

void IOSBoot::DebugLaunchReport()
{
    // Warning: We should not go through IOS because if it's panicked, any IPC
    // call will never return. Logging and interacting with memory and hardware
    // will work regardless if whether or not IOS is currently functional.

    // Check VFile status
    auto vf = (VFile<VFILE_SIZE>*)VFILE_ADDR;
    PRINT(Core, INFO, "VFile::m_length = 0x%08X", vf->m_length);
    PRINT(Core, INFO, "VFile::m_pos = 0x%08X", vf->m_pos);

    if (ACRReadFlag(ACRBUSPROTBit::PPCKERN) == false) {
        PRINT(Core, WARN, "Cannot give detailed launch report: No bus access!");
        return;
    }

    // Get access to SRAM. Should already be enabled if we have PPCKERN, but
    // just to be sure.
    ACRSetFlag(ACRSRNPROTBit::AHPEN, true);
    // Disable MEM2 protection.
    MEMCRWrite(MEMCRReg::MEM_PROT_DDR, 0);

    PRINT(Core, INFO, "Idle thread state 0x%08X; PC 0x%08X; LR 0x%08X",
          read32(SRAMMirrToReal(0xFFFE0050)),
          read32(SRAMMirrToReal(0xFFFE0040)),
          read32(SRAMMirrToReal(0xFFFE003C)));

    // Our process is PID 1 (ES), so to find it we will search and report on
    // every thread with PID 1. We will skip like the first 20 threads so we
    // don't trigger on the normal ES process, and I don't actually know what
    // the total thread count is, so we'll go until 80.
    int foundcount = 0;
    for (int i = 20; i < 80; i++) {
        const u32 threadPtr = SRAMMirrToReal(0xFFFE0000 + 0xB0 * i);

        // Check the CPSR to see if the thread realistically exists.
        if (read32(threadPtr + 0) == 0) {
            continue;
        }

        // Check if PID is ES.
        if (read32(threadPtr + 0x54) != 1 && read32(threadPtr + 0x54) != 0) {
            continue;
        }

        // This is our thread. Report some information on it.
        foundcount++;
        ReportIOSThread(i);
    }

    if (foundcount == 0) {
        PRINT(Core, INFO, "The process was not started");
    }

    // Check if the module was successfully copied into memory. It's not
    // reliable to just check for certain bytes because they could change, so
    // we'll just print whatever bytes we read.
    // Also, change this address if we ever move the memory bounds.
    PRINT(Core, INFO, "Module dump:");
    DebugCodeDump(0x13620000);
}

IOSBoot::IPCLog* IOSBoot::IPCLog::sInstance;

void IOSBoot::IPCLog::startGameIOS()
{
    Queue<u32> eventWaitQueue(1);
    setEventWaitingQueue(&eventWaitQueue, 2);
    logRM.ioctl(Log::IPCLogIoctl::StartGameEvent, nullptr, 0, nullptr, 0);
    eventWaitQueue.receive();
}

bool IOSBoot::IPCLog::handleEvent(s32 result)
{
    if (result < 0) {
        PRINT(Core, ERROR, "/dev/saoirse error: %d", result);
        return false;
    }

    switch (static_cast<Log::IPCLogReply>(result)) {
    case Log::IPCLogReply::Print:
        puts(logBuffer);
        return true;

    case Log::IPCLogReply::Notice:
        PRINT(Core, INFO, "Received resource notify event");
        m_eventCount++;

        if (m_eventCount == m_triggerEventCount) {
            m_triggerEventCount = -1;
            m_eventQueue->send(0);
        }
        return true;

    case Log::IPCLogReply::Close:
        return false;
    }

    return true;
}

s32 IOSBoot::IPCLog::threadEntry(void* userdata)
{
    IPCLog* log = reinterpret_cast<IPCLog*>(userdata);

    while (true) {
        s32 result =
            log->logRM.ioctl(Log::IPCLogIoctl::RegisterPrintHook, NULL, 0,
                             log->logBuffer, sizeof(log->logBuffer));
        if (!log->handleEvent(result))
            break;
    }

    return 0;
}

IOSBoot::IPCLog::IPCLog()
{
    if (this->logRM.fd() == IOSError::NotFound) {
        // Unfortunately there isn't really a way to detect the moment the log
        // resource manager is created, so we just have to keep trying until it
        // succeeds.
        for (s32 i = 0; i < 1000; i++) {
            usleep(1000);
            new (&this->logRM) IOS::ResourceCtrl<s32>("/dev/saoirse");
            if (this->logRM.fd() != IOSError::NotFound)
                break;
        }
    }
    if (this->logRM.fd() < 0) {
        PRINT(Core, ERROR, "/dev/saoirse open error: %d", this->logRM.fd());
        // [TODO] Maybe this could be handled better? Could reload IOS and try
        // again.
        abort();
    }

    new (&m_thread)
        Thread(threadEntry, reinterpret_cast<void*>(this), nullptr, 0x800, 80);
}

/* don't judge this code; it's not meant to be seen by eyes */

void IOSBoot::SetupPrintHook()
{
    static const u8 hook_code[] = {
        0x4A, 0x04, 0x68, 0x13, 0x18, 0xD0, 0x70, 0x01, 0x21, 0x00, 0x70, 0x41,
        0x33, 0x01, 0x60, 0x13, 0x47, 0x70, 0x00, 0x00, 0x10, 0xC0, 0x00, 0x00};
    *(u32*)0x90C00000 = 4;
    DCFlushRange((void*)0x90C00000, 0x10000);

    *(u32*)0xCD4F744C = ((u32)(&hook_code) & ~0xC0000000) | 1;
}

void IOSBoot::ReadPrintHook()
{
    DCInvalidateRange((void*)0x90C00000, 0x10000);
    printf("PRINT HOOK RESULT:\n%s", (char*)0x90C00004);
}