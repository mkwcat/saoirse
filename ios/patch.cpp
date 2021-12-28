#include "patch.h"
#include "main.h"
#include <ios.h>
#include <string.h>
#include <util.h>

char* iosOpenStrncpy(char* dest, const char* src, u32 num, s32 pid)
{
    strncpy(dest, src, num);

    if (pid != 15) {
        // Not PPCBOOT pid
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

static bool checkImportKeyFunction(u32 addr)
{
    if (read16(addr) == 0xB5F0 && read16(addr + 0x12) == 0x2600 &&
        read16(addr + 0x14) == 0x281F && read16(addr + 0x16) == 0xD806) {
        return true;
    }
    return false;
}

static u32 findImportKeyFunction()
{
    // Check known addresses

    if (checkImportKeyFunction(0x13A79C58)) {
        return 0x13A79C58 + 1;
    }

    if (checkImportKeyFunction(0x13A79918)) {
        return 0x13A79918 + 1;
    }

    for (int i = 0; i < 0x1000; i += 2) {
        u32 addr = 0x13A79500 + i;
        if (checkImportKeyFunction(addr)) {
            return addr + 1;
        }
    }

    return 0;
}

const u8 koreanCommonKey[] = {
    0x63, 0xb8, 0x2b, 0xb4, 0xf4, 0x61, 0x4e, 0x2e,
    0x13, 0xf2, 0xfe, 0xfb, 0xba, 0x4c, 0x9b, 0x7e,
};

void importKoreanCommonKey()
{
    u32 func = findImportKeyFunction();

    if (func == 0) {
        peli::Log(LogL::ERROR, "Could not find import key function");
        return;
    }

    peli::Log(LogL::WARN, "Found import key function at 0x%08X", func);

    // Call function by address
    (*(void (*)(int keyIndex, const u8* key, u32 keySize))func)(
        11, koreanCommonKey, sizeof(koreanCommonKey));
}