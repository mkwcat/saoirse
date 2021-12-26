#include "patch.h"
#include "main.h"
#include <ios.h>
#include <string.h>
#include <util.h>

char* iosOpenStrncpy(char* dest, const char* src, u32 num, s32* pid)
{
    strncpy(dest, src, num);

    if (*pid != 15) {
        // Not PPCBOOT pid

        if (dest[0] == '@') {
            // Set to PPCBOOT pid
            *pid = 15;
            dest[0] = '/';
        }

        return dest;
    }

    if (src[0] != '/') {
        if (src[0] == '$' || src[0] == '~') {
            // This is our proxy character!
            dest[0] = 0;
        }
        return dest;
    }

    if (!strncmp(src, "/dev/", 5)) {
        if (!strcmp(src, "/dev/flash") || !strcmp(src, "/dev/boot2")) {
            // No
            dest[0] = 0;
            return dest;
        }
        if (!strcmp(src, "/dev/fs")) {
            dest[0] = '$';
            return dest;
        }
        if (!strncmp(src, "/dev/di", 7)) {
            dest[0] = '~';
        }
        return dest;
    }

    // ISFS path
    dest[0] = '$';
    return dest;
}

static u32 findSyscallTable()
{
    u32 undefinedHandler = read32(0xFFFF0024);
    if (read32(0xFFFF0004) != 0xE59FF018 || undefinedHandler < 0xFFFF0040 ||
        undefinedHandler >= 0xFFFFF000 || (undefinedHandler & 3) ||
        read32(undefinedHandler) != 0xE9CD7FFF) {
        peli::Log(LogL::ERROR, "findSyscallTable: Invalid undefined handler");
        abort();
    }

    for (s32 i = 0x300; i < 0x700; i += 4) {
        if (read32(undefinedHandler + i) == 0xE6000010 &&
            validJumptablePtr(read32(undefinedHandler + i + 4)) &&
            validJumptablePtr(read32(undefinedHandler + i + 8)))
            return read32(undefinedHandler + i + 8);
    }

    return 0;
}

ATTRIBUTE_TARGET(arm)
__attribute__((noinline)) void invalidateICacheLine(u32 addr)
{
    asm volatile("\tmcr p15, 0, %0, c7, c5, 1\n" ::"r"(addr));
}

/* [TODO] Perhaps hardcode patches for specific IOS versions and use the search
 * as a fallback? */
void patchIOSOpen()
{
    peli::Log(LogL::WARN, "The search for IOS_Open syscall");

    u32 jumptable = findSyscallTable();
    if (jumptable == 0) {
        peli::Log(LogL::ERROR, "Could not find syscall table");
        abort();
    }

    u32 addr = jumptable + 0x1C * 4;
    assert(validJumptablePtr(addr));
    addr = read32(addr);
    assert(validKernelCodePtr(addr));
    addr &= ~1; // remove thumb bit

    /* Search backwards */
    for (int i = 0; i < 0x180; i += 2) {
        if (read16(addr - i) == 0x1C6A && read16(addr - i - 2) == 0x58D0) {
            write16(
                addr - i + 2,
                thumbBLHi(addr - i + 2, (u32)toUncached(&iosOpenStrncpyHook)));
            write16(
                addr - i + 4,
                thumbBLLo(addr - i + 2, (u32)toUncached(&iosOpenStrncpyHook)));

            peli::Log(LogL::WARN, "Patched %08X = %04X%04X", addr - i + 2,
                      read16(addr - i + 2), read16(addr - i + 4));

            // IOS automatically aligns flush
            IOS_FlushDCache((void*)(addr - i + 2), 4);
            invalidateICacheLine(round_down(addr - i + 2, 32));
            invalidateICacheLine(round_down(addr - i + 2, 32) + 32);
            return;
        }
    }

    peli::Log(LogL::ERROR, "Could not find IOS_Open instruction to patch");
}