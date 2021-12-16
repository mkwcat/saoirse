#pragma once

#include <types.h>

#ifndef ATTRIBUTE_ALIGN
#define ATTRIBUTE_ALIGN(v) __attribute__((aligned(v)))
#endif
#ifndef ATTRIBUTE_PACKED
#define ATTRIBUTE_PACKED __attribute__((packed))
#endif
#ifndef ATTRIBUTE_TARGET
#define ATTRIBUTE_TARGET(t) __attribute__((target(#t)))
#endif
#ifndef ATTRIBUTE_SECTION
#define ATTRIBUTE_SECTION(t) __attribute__((section(#t)))
#endif

#ifdef __cplusplus
#define EXTERN_C_START extern "C" {
#define EXTERN_C_END }
#else
#define EXTERN_C_START
#define EXTERN_C_END
#endif

#define LIBOGC_SUCKS_BEGIN                                                     \
    _Pragma("GCC diagnostic push")                                             \
        _Pragma("GCC diagnostic ignored \"-Wpedantic\"")

#define LIBOGC_SUCKS_END _Pragma("GCC diagnostic pop")

#ifdef __cplusplus
template <typename T> static inline T round_up(T num, unsigned int align)
{
    u32 raw = (u32)num;
    return (T)((raw + align - 1) & -align);
}

template <typename T> static inline T round_down(T num, unsigned int align)
{
    u32 raw = (u32)num;
    return (T)(raw & -align);
}

template <class T> static inline bool aligned(T addr, unsigned int align)
{
    return !(reinterpret_cast<unsigned int>(addr) & (align - 1));
}

#include <cstddef>

template <class T1, class T2>
constexpr bool check_bounds(T1 bounds, size_t bound_len, T2 buffer, size_t len)
{
    size_t low = reinterpret_cast<size_t>(bounds);
    size_t high = low + bound_len;
    size_t inside = reinterpret_cast<size_t>(buffer);
    size_t insidehi = inside + len;

    return (high >= low) && (insidehi >= inside) && (inside >= low) &&
           (insidehi <= high);
}

#endif

#ifndef TARGET_IOS
LIBOGC_SUCKS_BEGIN
#include <ogc/machine/processor.h>
LIBOGC_SUCKS_END
#else

static inline void write32(u32 address, u32 value) { *(vu32*)address = value; }

static inline u32 read32(u32 address) { return *(vu32*)address; }

static inline void mask32(u32 address, u32 clear, u32 set)
{
    *(vu32*)address = ((*(vu32*)address) & ~clear) | set;
}

static inline void write16(u32 address, u16 value) { *(vu16*)address = value; }

static inline u16 read16(u32 address) { return *(vu16*)address; }

static inline u32 bswap32(u32 val)
{
    return ((val & 0xFF) << 24) | ((val & 0xFF00) << 8) |
           ((val & 0xFF0000) >> 8) | ((val & 0xFF000000) >> 24);
}

#endif