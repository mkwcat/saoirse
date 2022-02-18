// Codehandler.hpp - Codehandler patch list
//   Written by Palapeli
//
// Copyright (C) 2022 Team Saoirse
// SPDX-License-Identifier: MIT

#pragma once

#include "PatchList.hpp"
#include <System/Types.h>

class Codehandler
{
public:
    // TODO: Use high mem region for cheat code list
    const u32 MAX_LINES = 256;

    static PatchList::PatchError ImportCodehandler(PatchList* patchList);
    static PatchList::PatchError ImportGCT(PatchList* patchList,
                                           const u8* start, const u8* end);
};