#include <System/Util.h>
#include <algorithm>
#include <string.h>

constexpr u32 ReadShorts(u16*& src)
{
    u32 value = (src[0] << 16) | src[1];
    src += 2;
    return value;
}

constexpr u32 ReadBytes(u8*& src)
{
    u32 value = (src[0] << 24) | (src[1] << 16) | (src[2] << 8) | src[3];
    src += 4;
    return value;
}

void* memcpy(void* __restrict dst, const void* __restrict src, size_t len)
{
    const u32 dstAddr = u32(dst);
    const u32 dstRounded = round_down(dstAddr, 4);
    const u32 dstEndAddr = dstAddr + len;
    const u32 dstEndRounded = round_down(dstEndAddr, 4);

    // Do main rounded copy in words

    u32* dstRoundUp = (u32*) round_up(dst, 4);
    u32 dstRoundUpLen = dstEndRounded - round_up(dstAddr, 4);
    u32 srcAlignAddr = u32(src) + round_up(dstAddr, 4) - dstAddr;

    if (aligned(srcAlignAddr, 4)) {
        // Copy reading words
        u32* srcFixed = (u32*) srcAlignAddr;

        // Copy 4 words at a time
        while (dstRoundUpLen >= 16) {
            *dstRoundUp++ = *srcFixed++;
            *dstRoundUp++ = *srcFixed++;
            *dstRoundUp++ = *srcFixed++;
            *dstRoundUp++ = *srcFixed++;
            dstRoundUpLen -= 16;
        }

        // Copy one word at a time
        while (dstRoundUpLen >= 4) {
            *dstRoundUp++ = *srcFixed++;
            dstRoundUpLen -= 4;
        }
    } else if (aligned(srcAlignAddr, 2)) {
        // Copy reading shorts
        u16* srcFixed = (u16*) srcAlignAddr;

        // Copy 4 words at a time
        while (dstRoundUpLen >= 16) {
            *dstRoundUp++ = ReadShorts(srcFixed);
            *dstRoundUp++ = ReadShorts(srcFixed);
            *dstRoundUp++ = ReadShorts(srcFixed);
            *dstRoundUp++ = ReadShorts(srcFixed);
            dstRoundUpLen -= 16;
        }

        // Copy one word at a time
        while (dstRoundUpLen >= 4) {
            *dstRoundUp++ = ReadShorts(srcFixed);
            dstRoundUpLen -= 4;
        }
    } else {
        // Copy reading bytes
        u8* srcFixed = (u8*) srcAlignAddr;

        // Copy 4 words at a time
        while (dstRoundUpLen >= 16) {
            *dstRoundUp++ = ReadBytes(srcFixed);
            *dstRoundUp++ = ReadBytes(srcFixed);
            *dstRoundUp++ = ReadBytes(srcFixed);
            *dstRoundUp++ = ReadBytes(srcFixed);
            dstRoundUpLen -= 16;
        }

        // Copy one word at a time
        while (dstRoundUpLen >= 4) {
            *dstRoundUp++ = ReadBytes(srcFixed);
            dstRoundUpLen -= 4;
        }
    }

    // Write the leading bytes
    if (dstRounded != dstAddr) {
        u8* srcU8 = (u8*) src;
        u32 srcData = ReadBytes(srcU8);

        srcData >>= ((dstAddr % 4) * 8);
        u32 mask = 0xFFFFFFFF >> ((dstAddr % 4) * 8);
        if (dstEndAddr - dstRounded < 4)
            mask &= ~(0xFFFFFFFF >> ((dstEndAddr - dstRounded) * 8));
        mask32(dstRounded, mask, srcData & mask);
    }

    // Write the trailing bytes
    if (dstEndAddr != dstEndRounded &&
        // Check if this was covered by the leading bytes copy
        (dstEndRounded != dstRounded || dstRounded == dstAddr)) {
        u8* srcU8 = ((u8*) src) + dstEndRounded - dstAddr;
        u32 srcData = ReadBytes(srcU8);

        u32 mask = ~(0xFFFFFFFF >> ((dstEndAddr - dstEndRounded) * 8));
        mask32(dstEndRounded, mask, srcData & mask);
    }

    return dst;
}

void* memset(void* dst, int value0, size_t len)
{
    u8 value = value0;

    const u32 dstAddr = u32(dst);
    const u32 dstRounded = round_down(dstAddr, 4);
    const u32 dstEndAddr = dstAddr + len;
    const u32 dstEndRounded = round_down(dstEndAddr, 4);

    // Do main rounded set in words

    u32* dstRoundUp = (u32*) round_up(dst, 4);
    u32 dstRoundUpLen = dstEndRounded - round_up(dstAddr, 4);
    u32 valueFixed = (value << 24) | (value << 16) | (value << 8) | value;

    // Copy 4 words at a time
    while (dstRoundUpLen >= 16) {
        *dstRoundUp++ = valueFixed;
        *dstRoundUp++ = valueFixed;
        *dstRoundUp++ = valueFixed;
        *dstRoundUp++ = valueFixed;
        dstRoundUpLen -= 16;
    }

    // Copy one word at a time
    while (dstRoundUpLen >= 4) {
        *dstRoundUp++ = valueFixed;
        dstRoundUpLen -= 4;
    }

    // Write the leading bytes
    if (dstRounded != dstAddr) {
        u32 srcData = valueFixed;

        srcData >>= ((dstAddr % 4) * 8);
        u32 mask = 0xFFFFFFFF >> ((dstAddr % 4) * 8);
        if (dstEndAddr - dstRounded < 4)
            mask &= ~(0xFFFFFFFF >> ((dstEndAddr - dstRounded) * 8));
        mask32(dstRounded, mask, srcData & mask);
    }

    // Write the trailing bytes
    if (dstEndAddr != dstEndRounded &&
        // Check if this was covered by the leading bytes copy
        (dstEndRounded != dstRounded || dstRounded == dstAddr)) {
        u32 srcData = valueFixed;

        u32 mask = ~(0xFFFFFFFF >> ((dstEndAddr - dstEndRounded) * 8));
        mask32(dstEndRounded, mask, srcData & mask);
    }

    return dst;
}

int memcmp(const void* s1, const void* s2, size_t n)
{
    const u8* su1 = (const u8*) s1;
    const u8* su2 = (const u8*) s2;

    size_t i = 0;
    for (; i < n && su1[i] == su2[i]; i++) {
    }

    return i < n ? su1[i] - su2[i] : 0;
}

size_t strlen(const char* s)
{
    const char* f = s;
    while (*s != '\0') {
        s++;
    }
    return s - f;
}

int strcmp(const char* s1, const char* s2)
{
    size_t i = 0;
    for (; s1[i] == s2[i]; i++) {
        if (s1[i] == '\0') {
            break;
        }
    }

    return s1[i] - s2[i];
}

int strncmp(const char* s1, const char* s2, size_t n)
{
    size_t i = 0;
    for (; i < n && s1[i] == s2[i]; i++) {
        if (s1[i] == '\0') {
            break;
        }
    }

    return i < n ? s1[i] - s2[i] : 0;
}

char* strchr(const char* s, int c)
{
    while (*s != '\0') {
        if (*s == c) {
            // This function is also a const-cast
            return (char*) s;
        }

        s++;
    }

    return NULL;
}

char* strcpy(char* __restrict dst, const char* __restrict src)
{
    return (char*) memcpy(dst, src, strlen(src) + 1);
}

char* strncpy(char* __restrict dst, const char* __restrict src, size_t n)
{
    return (char*) memcpy(dst, src, std::min<size_t>(strlen(src) + 1, n));
}
