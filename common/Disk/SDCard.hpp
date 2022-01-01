#pragma once
#include <System/Types.hpp>

typedef u32 sec_t;

namespace SDCard
{

bool Deinitialize(void);
bool Open(void);
bool Startup(void);
bool Shutdown(void);
s32 ReadSectors(sec_t sector, sec_t numSectors, void* buffer);
s32 WriteSectors(sec_t sector, sec_t numSectors, const void* buffer);
bool ClearStatus(void);
bool IsInserted(void);
bool IsInitialized(void);

}