// SDCard.hpp
//
// SPDX-License-Identifier: MIT

#pragma once
#include <System/Types.h>

typedef u32 sec_t;

class SDCard
{
public:
    static bool Open();
    static bool Startup();
    static bool Shutdown();
    static s32 ReadSectors(sec_t sector, sec_t numSectors, void* buffer);
    static s32 WriteSectors(sec_t sector, sec_t numSectors, const void* buffer);
    static bool ClearStatus();
    static bool IsInserted();
    static bool IsInitialized();
};
