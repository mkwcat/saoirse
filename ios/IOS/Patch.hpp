// Patch.hpp - IOS kernel patching
//   Written by Palapeli
//
// SPDX-License-Identifier: MIT

#pragma once

#include <System/Types.h>

void PatchIOSOpen();
void ImportKoreanCommonKey();
bool IsWiiU();
bool ResetEspresso(u32 entry);
