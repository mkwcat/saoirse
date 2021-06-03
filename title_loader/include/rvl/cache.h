#pragma once

#ifdef IOS
#define DCFlushRange IOS_FlushDCache
#define DCInvalidateRange IOS_InvalidateDCache
#else
void DCFlushRange(void* address, u32 len);
void DCInvalidateRange(void* address, u32 len);
void ICFlushRange(void* address, u32 len);
void ICInvalidateRange(void* address, u32 len);
#endif