#pragma once

#include <types.h>

extern s32 heap;

void _exit(u32 color);
void abort();
void __assert_fail(const char* expr, const char* file, s32 line);

#define assert(expr) \
    ((void) ((expr) ? 0 : __assert_fail(#expr, __FILE__, __LINE__)))
#define ASSERT assert