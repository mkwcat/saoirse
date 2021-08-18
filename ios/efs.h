#pragma once

#include "ff.h"
#include <types.h>

extern s32 storageFd;
extern FATFS fatfs;

void FS_Init(s32 replyQueue);
s32 FS_CliInit();
FRESULT FS_Open(FIL* fp, const TCHAR* path, u8 mode);
FRESULT FS_Close(FIL* fp);
FRESULT FS_Read(FIL* fp, void* data, u32 len, u32* read);
FRESULT FS_Write(FIL* fp, const void* data, u32 len, u32* wrote);
FRESULT FS_LSeek(FIL* fp, u32 offset);
FRESULT FS_Truncate(FIL* fp);
FRESULT FS_Sync(FIL* fp);