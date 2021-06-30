#pragma once

#include <stdint.h>

void SetupGlobals(int fst_expand);

uint32_t GetArenaLow();
void SetArenaLow(uint32_t low);
uint32_t GetArenaHigh();
void SetArenaHigh(uint32_t high);