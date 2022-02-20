// SDCard.hpp
//
// Copyright (C) 2022 Team Saoirse
// SPDX-License-Identifier: MIT

#pragma once
#include <System/Types.h>

typedef u32 sec_t;

namespace SDCard
{

bool Open(void);
bool Startup(void);
bool Shutdown(void);
s32 ReadSectors(sec_t sector, sec_t numSectors, void* buffer);
s32 WriteSectors(sec_t sector, sec_t numSectors, const void* buffer);
bool ClearStatus(void);
bool IsInserted(void);
bool IsInitialized(void);

}