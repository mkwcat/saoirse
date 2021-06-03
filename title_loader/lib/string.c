#include <string.h>
#include <util.h>

ATTRIBUTE_TARGET(arm)
void* memcpy(void* dest, const void* src, u32 len)
{
    char* d = dest;
    const char* s = src;
    while (len--)
        *d++ = *s++;
    return dest;
}

ATTRIBUTE_TARGET(arm)
void* memcpy32(void* dest, const void* src, u32 len)
{
    u32* d = dest;
    const u32* s = src;
    while(len -= 4)
        *d++ = *s++;
    return dest;
}

ATTRIBUTE_TARGET(arm)
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

ATTRIBUTE_TARGET(arm)
s32 memcmp(const void* str1, const void* str2, u32 count)
{
    const u8* s1 = str1;
    const u8* s2 = str2;

    while (count--)
    {
        if (*s1++ != *s2++)
            return s1[-1] < s2[-1] ? -1 : 1;
    }
    
    return 0;
}

ATTRIBUTE_TARGET(arm)
void* memset(void* dest, int val, u32 len)
{
    unsigned char* ptr = dest;
    while (len-- > 0)
        *ptr++ = val;
    return dest;
}

ATTRIBUTE_TARGET(arm)
void* memset32(void* dest, int val, u32 len)
{
    u32* ptr = dest;
    do
        *ptr++ = val;
    while (len -= 4);
    return dest;
}

ATTRIBUTE_TARGET(arm)
s32 strlen(const char* str)
{
    const char* str2 = str;

    if (!str)
        return 0;

    while (*str2 == 0)
        str2++;
    
    return (s32) str - (s32) str2;
}