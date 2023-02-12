#pragma once

#include <System/Types.h>

#ifdef TARGET_IOS
constexpr u32 MemBase = 0x00000000;
#else
constexpr u32 MemBase = 0x80000000;
#endif

enum {
    SEL_ADDRESS = MemBase + 0x00900000,
    SEL_MAXLEN = 0x00100000,

    IOS_BOOT_MSG_ADDRESS = MemBase + 0x10000100,
    IOS_BOOT_MSG_MAXLEN = 0x20,

    IOS_FILE_INFO_ADDRESS = IOS_BOOT_MSG_ADDRESS + IOS_BOOT_MSG_MAXLEN,
    IOS_FILE_INFO_MAXLEN = 0x20,

    CONSOLE_DATA_ADDRESS = IOS_FILE_INFO_ADDRESS + IOS_FILE_INFO_MAXLEN,
    CONSOLE_DATA_MAXLEN = 0x20,

    IOS_BOOT_ADDRESS = MemBase + 0x10000200,
    IOS_BOOT_MAXLEN = 0x00040000,

    IOS_BOOT_STACK = IOS_BOOT_ADDRESS + IOS_BOOT_MAXLEN,
    IOS_BOOT_STACK_MAXLEN = 0x2000,

    CONSOLE_XFB_ADDRESS = IOS_BOOT_STACK + IOS_BOOT_STACK_MAXLEN,
    CONSOLE_XFB_MAXLEN = 320 * 574 * sizeof(unsigned int),

    BOOT_ARC_ADDRESS = CONSOLE_XFB_ADDRESS + CONSOLE_XFB_MAXLEN,
    BOOT_ARC_MAXLEN = 0x00100000,
};

struct Boot_ConsoleData {
    u16 xfbWidth;
    u16 xfbHeight;
    u32 lock;
    s32 ppcRow;
    s32 iosRow;

    bool xfbInit;

    static constexpr u32 PPC_LOCK = 0x1;
    static constexpr u32 IOS_LOCK = 0x2;
};

static_assert(sizeof(Boot_ConsoleData) < CONSOLE_DATA_MAXLEN);
