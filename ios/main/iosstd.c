#include "iosstd.h"
#include <util.h>
#include <ios.h>
#include <vsprintf.h>
#include "main.h"

ATTRIBUTE_TARGET(thumb)
void* memcpy(void* dest, const void* src, u32 len)
{
    char* d = dest;
    const char* s = src;
    while (len--)
        *d++ = *s++;
    return dest;
}

ATTRIBUTE_TARGET(thumb)
void* memcpy32(void* dest, const void* src, u32 len)
{
    u32* d = dest;
    const u32* s = src;
    while(len -= 4)
        *d++ = *s++;
    return dest;
}

ATTRIBUTE_TARGET(thumb)
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

ATTRIBUTE_TARGET(thumb)
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

ATTRIBUTE_TARGET(thumb)
void* memset(void* dest, int val, u32 len)
{
    unsigned char* ptr = dest;
    while (len-- > 0)
        *ptr++ = val;
    return dest;
}

ATTRIBUTE_TARGET(thumb)
void* memset32(void* dest, int val, u32 len)
{
    u32* ptr = dest;
    do
        *ptr++ = val;
    while (len -= 4);
    return dest;
}

ATTRIBUTE_TARGET(thumb)
s32 strlen(const char* str)
{
    const char* str2 = str;

    if (!str)
        return 0;

    while (*str2 == 0)
        str2++;
    
    return (s32) str - (s32) str2;
}

ATTRIBUTE_TARGET(thumb)
s32 strnlen(const char* str, s32 maxlen)
{
    s32 len = 0;
    for (len = 0; len < maxlen; len++, str++)
        if (*str == 0)
            break;
    return len;
}

ATTRIBUTE_TARGET(thumb)
s32 strcmp(const char* s1, const char* s2)
{
    char c1, c2;
    do {
        c1 = *s1++;
        c2 = *s2++;
        if (c1 == 0)
            return c1 - c2;
    }
    while (c1 == c2);
    return c1 - c2;
}

ATTRIBUTE_TARGET(thumb)
char* strchr(const char* s, char c)
{
    do {
        if (*s == c)
            return (char*) s;
    } while (*s++);
    return NULL;
}

char* strcpy(char* dst, const char* src)
{
    const u32 len = strlen(src);
    memcpy(dst, src, len + 1);
    return dst;
}

void usleep(u32 usec)
{
    u32 queueData;
    const s32 queue = IOS_CreateMessageQueue(&queueData, 1);
    if (queue < 0) {
        printf(ERROR, "usleep: failed to create message queue: %d", queue);
        abort();
    }

    const s32 timer = IOS_CreateTimer(usec, 0, queue, 1);
    if (timer < 0) {
        printf(ERROR, "usleep: failed to create timer: %d", timer);
        abort();
    }

    u32 msg;
    const s32 ret = IOS_ReceiveMessage(queue, &msg, 0);
    if (ret < 0 || msg != 1) {
        printf(ERROR, "usleep: IOS_ReceiveMessage failure: %d", ret);
        abort();
    }

    IOS_DestroyTimer(timer);
    IOS_DestroyMessageQueue(queue);
}

s32 snprintf(char* str, u32 n, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    const s32 ret = vsnprintf(str, n, format, args);
    va_end(args);
    return ret;
}