#ifndef _STRING_H
#define _STRING_H

#include <gctypes.h>

void* memcpy(void* dst, const void* src, u32 len);
void memclear(void* ptr, u32 len);

#endif /* _STRING_H */