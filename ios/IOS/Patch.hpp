// Patch.hpp - IOS kernel patching
//   Written by Palapeli
//
// Copyright (C) 2022 Team Saoirse
// SPDX-License-Identifier: MIT

#pragma once

#include <System/Types.h>

void PatchIOSOpen();
void ImportKoreanCommonKey();
bool IsWiiU();
bool ResetEspresso(u32 entry);