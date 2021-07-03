#pragma once

#include <types.h>

typedef u32 sec_t;

bool sdio_Deinitialize(void);
bool sdio_Startup(void);
bool sdio_Shutdown(void);
bool sdio_ReadSectors(sec_t sector, sec_t numSectors,void* buffer);
bool sdio_WriteSectors(sec_t sector, sec_t numSectors,const void* buffer);
bool sdio_ClearStatus(void);
bool sdio_IsInserted(void);