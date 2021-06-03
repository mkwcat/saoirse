#ifndef _IOS_HEAP_H
#define _IOS_HEAP_H

#include <types.h>

#ifdef __cplusplus
    extern "C" {
#endif

#ifdef IOS

s32 IOS_CreateHeap(void *ptr, s32 length);
s32 IOS_DestroyHeap(s32 heap);
void* IOS_Alloc(s32 heap, u32 length);
void* IOS_AllocAligned(s32 heap, u32 length, u32 align);
s32 IOS_Free(s32 heap, void* ptr);

#endif // IOS

#ifdef __cplusplus
    }
#endif

#endif // _IOS_HEAP_H