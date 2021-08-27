#pragma once

#include <types.h>

typedef u32 sec_t;

namespace SDCard
{

bool Deinitialize(void);
bool Open(void);
bool Startup(void);
bool Shutdown(void);
bool ReadSectors(sec_t sector, sec_t numSectors, void* buffer);
bool WriteSectors(sec_t sector, sec_t numSectors, const void* buffer);
bool ClearStatus(void);
bool IsInserted(void);
bool IsInitialized(void);

}