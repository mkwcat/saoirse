// Patch.cpp - IOS kernel patching
//   Written by Palapeli
//
// SPDX-License-Identifier: MIT

#include "Patch.hpp"
#include <Debug/Log.hpp>
#include <IOS/Syscalls.h>
#include <IOS/System.hpp>
#include <System/Config.hpp>
#include <System/OS.hpp>
#include <System/Types.h>
#include <System/Util.h>
#include <cstring>

// clang-format off
ATTRIBUTE_TARGET(arm)
ATTRIBUTE_NOINLINE
ASM_FUNCTION(void InvalidateICacheLine(u32 addr),
    // r0 = addr
    mcr     p15, 0, r0, c7, c5, 1;
    bx      lr
)

ATTRIBUTE_TARGET(thumb)
ASM_FUNCTION(static void IOSOpenStrncpyTrampoline(),
    // Overwrite first parameter
    str     r0, [sp, #0x14];
    ldr     r3, =IOSOpenStrncpy;
    mov     r12, r3;
    mov     r3, r10; // pid
    bx      r12;
)

// clang-format on

extern "C" char* IOSOpenStrncpy(char* dest, const char* src, u32 num, s32 pid)
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
        if (!strcmp(src, "/dev/sao_loader")) {
            // Disallow opening the loader file RM
            dest[0] = 0;
            return dest;
        }

        if (!strcmp(src, "/dev/flash") || !strcmp(src, "/dev/boot2")) {
            // No
            dest[0] = 0;
            return dest;
        }
        if (!strcmp(src, "/dev/fs")) {
            dest[0] = '$';
            return dest;
        }
        if (!strcmp(src, "/dev/es")) {
            dest[0] = '~';
            return dest;
        }
        if (!strncmp(src, "/dev/di", 7)) {
            dest[0] = '~';
        }
        return dest;
    }

    // ISFS path
    if (Config::s_instance->IsISFSPathReplaced(src))
        dest[0] = '$';

    return dest;
}

constexpr bool ValidJumptablePtr(u32 address)
{
    return address >= 0xFFFF0040 && !(address & 3);
}

constexpr bool ValidKernelCodePtr(u32 address)
{
    return address >= 0xFFFF0040 && (address & 2) != 2;
}

template <class T>
static inline T* ToUncached(T* address)
{
    return reinterpret_cast<T*>(reinterpret_cast<u32>(address) | 0x80000000);
}

constexpr u16 ThumbBLHi(u32 src, u32 dest)
{
    s32 diff = dest - (src + 4);
    return ((diff >> 12) & 0x7FF) | 0xF000;
}

constexpr u16 ThumbBLLo(u32 src, u32 dest)
{
    s32 diff = dest - (src + 4);
    return ((diff >> 1) & 0x7FF) | 0xF800;
}

static inline bool IsPPCRegion(const void* ptr)
{
    const u32 address = reinterpret_cast<u32>(ptr);
    return (address < 0x01800000) ||
           (address >= 0x10000000 && address < 0x13400000);
}

static u32 FindSyscallTable()
{
    u32 undefinedHandler = read32(0xFFFF0024);
    if (read32(0xFFFF0004) != 0xE59FF018 || undefinedHandler < 0xFFFF0040 ||
        undefinedHandler >= 0xFFFFF000 || (undefinedHandler & 3) ||
        read32(undefinedHandler) != 0xE9CD7FFF) {
        PRINT(IOS, ERROR, "FindSyscallTable: Invalid undefined handler");
        abort();
    }

    for (s32 i = 0x300; i < 0x700; i += 4) {
        if (read32(undefinedHandler + i) == 0xE6000010 &&
            ValidJumptablePtr(read32(undefinedHandler + i + 4)) &&
            ValidJumptablePtr(read32(undefinedHandler + i + 8)))
            return read32(undefinedHandler + i + 8);
    }

    return 0;
}

void PatchIOSOpen()
{
    PRINT(IOS, WARN, "The search for IOS_Open syscall");

    u32 jumptable = FindSyscallTable();
    if (jumptable == 0) {
        PRINT(IOS, ERROR, "Could not find syscall table");
        abort();
    }

    u32 addr = jumptable + 0x1C * 4;
    assert(ValidJumptablePtr(addr));
    addr = read32(addr);
    assert(ValidKernelCodePtr(addr));
    addr &= ~1; // Remove thumb bit

    // Search backwards for the bytes to patch
    for (int i = 0; i < 0x180; i += 2) {
        if (read16(addr - i) == 0x1C6A && read16(addr - i - 2) == 0x58D0) {
            write16(
              addr - i + 2, ThumbBLHi(addr - i + 2,
                              (u32) ToUncached(&IOSOpenStrncpyTrampoline)));
            write16(
              addr - i + 4, ThumbBLLo(addr - i + 2,
                              (u32) ToUncached(&IOSOpenStrncpyTrampoline)));

            PRINT(IOS, WARN, "Patched %08X = %04X%04X", addr - i + 2,
              read16(addr - i + 2), read16(addr - i + 4));

            // IOS automatically aligns flush
            IOS_FlushDCache((void*) (addr - i + 2), 4);
            InvalidateICacheLine(round_down(addr - i + 2, 32));
            InvalidateICacheLine(round_down(addr - i + 2, 32) + 32);
            return;
        }
    }

    PRINT(IOS, ERROR, "Could not find IOS_Open instruction to patch");
}

static bool CheckImportKeyFunction(u32 addr)
{
    if (read16(addr) == 0xB5F0 && read16(addr + 0x12) == 0x2600 &&
        read16(addr + 0x14) == 0x281F && read16(addr + 0x16) == 0xD806) {
        return true;
    }
    return false;
}

static u32 FindImportKeyFunction()
{
    // Check known addresses

    if (CheckImportKeyFunction(0x13A79C58)) {
        return 0x13A79C58 + 1;
    }

    if (CheckImportKeyFunction(0x13A79918)) {
        return 0x13A79918 + 1;
    }

    for (int i = 0; i < 0x1000; i += 2) {
        u32 addr = 0x13A79500 + i;
        if (CheckImportKeyFunction(addr)) {
            return addr + 1;
        }
    }

    return 0;
}

const u8 KoreanCommonKey[] = {
  0x63,
  0xb8,
  0x2b,
  0xb4,
  0xf4,
  0x61,
  0x4e,
  0x2e,
  0x13,
  0xf2,
  0xfe,
  0xfb,
  0xba,
  0x4c,
  0x9b,
  0x7e,
};

void ImportKoreanCommonKey()
{
    u32 func = FindImportKeyFunction();

    if (func == 0) {
        PRINT(IOS, ERROR, "Could not find import key function");
        return;
    }

    PRINT(IOS, WARN, "Found import key function at 0x%08X", func);

    // Call function by address
    (*(void (*)(int keyIndex, const u8* key, u32 keySize)) func)(
      11, KoreanCommonKey, sizeof(KoreanCommonKey));
}

constexpr u32 MakePPCBranch(u32 src, u32 dest)
{
    return ((dest - src) & 0x03FFFFFC) | 0x48000000;
}

bool IsWiiU()
{
    // Read LT_CHIPREVID; this will read zero on a normal Wii.
    // Note: this _does_ work without system mode, since the hardware registers
    // are mapped as read-only.
    return (read32(0x0D8005A0) >> 16) == 0xCAFE;
}

bool ResetEspresso(u32 entry)
{
    // Credit: marcan
    // https://fail0verflow.com/blog/2013/espresso/

    PRINT(IOS, WARN, "Resetting Espresso...");

    DASSERT(IsWiiU());
    if (!IsWiiU()) {
        PRINT(IOS, ERROR, "This reset can only be used on Wii U!");
        return false;
    }

    DASSERT(in_mem1(entry));
    if (!in_mem1(entry)) {
        PRINT(
          IOS, ERROR, "Invalid entry point: 0x%08X! Must be in MEM1!", entry);
        return false;
    }

    // Disable this until the PPC has started up again.
    bool ipcLogEnabledSave = Log::ipcLogEnabled;
    Log::ipcLogEnabled = false;

    // Reading the TMD for the boot index would be the proper way to get this
    // path, but that's more work! I don't think this path has ever changed and
    // I really doubt that it ever will.
    // Regardless, this should be fixed at some point.
    s32 ret = IOS_LaunchElf("/title/00000001/00000200/content/00000003.app");
    if (ret != IOSError::OK) {
        PRINT(IOS, ERROR, "IOS_LaunchElf fail: %d", ret);
        return false;
    }

    PRINT(IOS, INFO, "Now watching for decryption...");

    while (true) {
        IOS_InvalidateDCache((void*) 0x01330100, sizeof(u32));

        // Check if the first instruction has been decrypted. The block has
        // already been hashed by this point.
        if (read32(0x01330100) == 0x7C9F42A6) {
            PRINT(IOS, INFO, "Decrypted!");

            write32(0x01330100, MakePPCBranch(0x01330100, entry));
            IOS_FlushDCache((void*) 0x01330100, sizeof(u32));

            PRINT(IOS, WARN, "Patched %08X = %08X", 0x01330100,
              MakePPCBranch(0x01330100, entry));
            break;
        }
    }

    // PPC has started up again so reenable the IPC log.
    Log::ipcLogEnabled = ipcLogEnabledSave;
    return true;
}
