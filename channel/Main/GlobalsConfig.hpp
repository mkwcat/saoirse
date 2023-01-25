// GlobalsConfig.hpp
//   Written by riidefi
//   Based on BrainSlug, by Chadderz
//
// SPDX-License-Identifier: MIT

#pragma once
#include <stdint.h>

void SetupGlobals(int fst_expand);

uint32_t GetArenaLow();
void SetArenaLow(uint32_t low);
uint32_t GetArenaHigh();
void SetArenaHigh(uint32_t high);

namespace mem
{
inline void* AllocFromArenaHigh(uint32_t size)
{
    const uint32_t current_High = GetArenaHigh();
    SetArenaHigh(current_High + size);

    return reinterpret_cast<void*>(current_High);
}

struct arena_high {
};
} // namespace mem

inline void* operator new(uint32_t size, mem::arena_high)
{
    return mem::AllocFromArenaHigh(size);
}

inline void* operator new[](uint32_t size, mem::arena_high)
{
    return mem::AllocFromArenaHigh(size);
}
