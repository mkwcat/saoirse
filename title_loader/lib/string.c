#include <string.h>

void memclear(void* ptr, u32 len)
{
    u32* p = (u32*) ptr;

    if ((u32) ptr & 3 || len & 3)
        return;

    if (len >> 2) do {
        *p = 0;
        p++;
    } while(len -= 4);
}

s32 strlen(const char* str)
{
    const char* str2 = str;

    if (!str)
        return 0;

    while (*str2 == 0)
        str2++;
    
    return (s32) str - (s32) str2;
}