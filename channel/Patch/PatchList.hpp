// PatchList.hpp - Game patch list
//   Written by Palapeli
//
// SPDX-License-Identifier: MIT

#pragma once

#include <System/Types.h>
#include <vector>

class PatchList
{
public:
    enum class PatchError {
        OK,
        MemoryOutOfBounds,
        EmptyPatch,
    };

    enum class BranchType {
        B,
        BL,
    };

    struct MemoryPatch {
        u32 address;
        std::vector<u8> bytes;
    };

    static bool IsValidMemPatchRegion(u32 start, u32 end);

    // For setting up patches
    PatchError ImportMemoryPatch(u32 address, const u8* start, const u8* end);
    PatchError ImportPoke32(u32 address, u32 value);
    PatchError ImportPokeBranch(u32 src, u32 dest);

    // For applying patches
    void CopyToGameMem(u32 address, const u8* start, const u8* end);
    PatchError ApplyPatches();

protected:
    std::vector<MemoryPatch> m_patches;
};
