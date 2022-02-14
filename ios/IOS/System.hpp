#pragma once
#include <System/Types.h>
#include <System/Util.h>

#define YUV_RED ((84 << 24) | (255 << 16) | (76 << 8))
#define YUV_DARK_RED ((106 << 24) | (192 << 16) | (38 << 8))
#define YUV_GREEN ((43 << 24) | (21 << 16) | (149 << 8))
#define YUV_DARK_GREEN ((85 << 24) | (74 << 16) | (75 << 8))
#define YUV_BLUE ((255 << 24) | (107 << 16) | (29 << 8))
#define YUV_DARK_BLUE ((192 << 24) | (117 << 16) | (14 << 8))
#define YUV_PINK ((170 << 24) | (181 << 16) | (180 << 8))
#define YUV_PURPLE ((170 << 24) | (181 << 16) | (52 << 8))
#define YUV_CYAN ((149 << 24) | (64 << 16) | (89 << 8))
#define YUV_YELLOW ((0 << 24) | (148 << 16) | (225 << 8))
#define YUV_DARK_YELLOW ((64 << 24) | (138 << 16) | (113 << 8))
#define YUV_WHITE ((128 << 24) | (128 << 16) | (255 << 8))
#define YUV_GRAY ((128 << 24) | (128 << 16) | (128 << 8))

#define YUV_0 YUV_RED
#define YUV_1 YUV_DARK_RED
#define YUV_2 YUV_GREEN
#define YUV_3 YUV_DARK_GREEN
#define YUV_4 YUV_BLUE
#define YUV_5 YUV_DARK_BLUE
#define YUV_6 YUV_PINK
#define YUV_7 YUV_PURPLE
#define YUV_8 YUV_CYAN
#define YUV_9 YUV_YELLOW
#define YUV_10 YUV_DARK_YELLOW
#define YUV_11 YUV_WHITE
#define YUV_12 YUV_GRAY

void AbortColor(u32 color);
void AssertFail(const char* expr, const char* file, s32 line);
void KernelWrite(u32 address, u32 value);

EXTERN_C_START
void abort();
void usleep(u32 usec);
EXTERN_C_END

#define assert(expr)                                                           \
    (((expr) ? (void)0 : AssertFail(#expr, __FILE__, __LINE__)))
#define ASSERT assert
