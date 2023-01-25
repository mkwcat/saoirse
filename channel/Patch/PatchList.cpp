// PatchList.cpp - Game patch list
//   Written by Palapeli
//
// SPDX-License-Identifier: MIT

#include "PatchList.hpp"
#include <Debug/Log.hpp>
#include <cassert>
#include <cstring>
#include <ogc/cache.h>
#include <ogc/irq.h>
#include <utility>

bool PatchList::IsValidMemPatchRegion(u32 start, u32 end)
{
    if (end <= start)
        return false;

    if (start >= 0x80000100 && end <= 0x80900000)
        return true;

    return false;
}

PatchList::PatchError PatchList::ImportMemoryPatch(u32 address, const u8* start,
                                                   const u8* end)
{
    if (start == end) {
        PRINT(PatchList, ERROR, "Memory patch has no data! (0x08X)", address);
        return PatchError::EmptyPatch;
    }

    if (!IsValidMemPatchRegion(address, address + (end - start))) {
        PRINT(PatchList, ERROR,
              "Out of bounds memory patch region! (0x08X - 0x%08X)", address,
              address + (end - start));
        return PatchError::MemoryOutOfBounds;
    }

    m_patches.push_back({
        .address = address,
        .bytes = std::vector(start, end),
    });

    return PatchError::OK;
}

PatchList::PatchError PatchList::ImportPoke32(u32 address, u32 value)
{
    return ImportMemoryPatch(address, (u8*)&value, ((u8*)&value) + 4);
}

PatchList::PatchError PatchList::ImportPokeBranch(u32 src, u32 dest)
{
    u32 value = ((dest - src) & 0x03FFFFFF) | 0x48000000;
    return ImportPoke32(src, value);
}

void PatchList::CopyToGameMem(u32 address, const u8* start, const u8* end)
{
    assert(IsValidMemPatchRegion(address, address + (end - start)));

    memcpy(reinterpret_cast<void*>(address), start, end - start);
}

PatchList::PatchError PatchList::ApplyPatches()
{
    // 0x80001800 is where the exit stub should be, so disable interrupts to
    // make sure nobody calls that while we're clearing it.
    u32 level = IRQ_Disable();
    DCZeroRange(reinterpret_cast<void*>(0x80001800), 0x80003000 - 0x80001800);
    DCFlushRange(reinterpret_cast<void*>(0x80001800), 0x80003000 - 0x80001800);
    IRQ_Restore(level);

    for (auto patch : m_patches) {
        CopyToGameMem(patch.address, &patch.bytes.begin()[0],
                      &patch.bytes.end()[0]);
    }

    return PatchError::OK;
}
