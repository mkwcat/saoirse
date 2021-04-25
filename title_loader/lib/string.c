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