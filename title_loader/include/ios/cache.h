#ifndef _IOS_CACHE_H
#define _IOS_CACHE_H

#include <types.h>

void IOS_InvalidateDCache(void *address, u32 size);
void IOS_FlushDCache(const void *address, u32 size);

#endif // _IOS_CACHE_H