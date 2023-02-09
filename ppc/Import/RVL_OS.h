#pragma once

#include <Import/Sel.h>
#include <System/Util.h>

EXTERN_C_START

SelImport("OSReport", void OSReport(const char* format, ...));

typedef struct {
    u8 _00[0x18 - 0x00];
} OSMutex;

static_assert(sizeof(OSMutex) == 0x18);

SelImport("OSInitMutex", void OSInitMutex(OSMutex* mutex));
SelImport("OSLockMutex", void OSLockMutex(OSMutex* mutex));
SelImport("OSUnlockMutex", void OSUnlockMutex(OSMutex* mutex));
SelImport("OSTryLockMutex", BOOL OSTryLockMutex(OSMutex* mutex));

EXTERN_C_END
