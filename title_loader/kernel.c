#include <ios.h>
#include <string.h>
#include <gctypes.h>


static u32 __kwrite32(void* _0, u32* ptr, u32 value)
{
    *ptr = value;
    return value;
}

static u16 __kwrite16(void* _0, u16* ptr, u16 value)
{
    *ptr = value;
    return value;
}

static u8 __kwrite8(void* _0, u8* ptr, u8 value)
{
    *ptr = value;
    return value;
}

static u32 __kread32(void* _0, u32* ptr)
{
    return *ptr;
}

static u16 __kread16(void* _0, u16* ptr)
{
    return *ptr;
}

static u8 __kread8(void* _0, u8* ptr)
{
    return *ptr;
}

static u32 __kor32(void* _0, u32* ptr, u32 mask)
{
    *ptr |= mask;
    return *ptr;
}

static u16 __kor16(void* _0, u16* ptr, u16 mask)
{
    *ptr |= mask;
    return *ptr;
}

static u8 __kor8(void* _0, u8* ptr, u8 mask)
{
    *ptr |= mask;
    return *ptr;
}

static u32 __kand32(void* _0, u32* ptr, u32 mask)
{
    *ptr &= mask;
    return *ptr;
}

static u16 __kand16(void* _0, u16* ptr, u16 mask)
{
    *ptr &= mask;
    return *ptr;
}

static u8 __kand8(void* _0, u8* ptr, u8 mask)
{
    *ptr &= mask;
    return *ptr;
}

static void* __kmemcpy(void* _0, void* dst, const void* src, u32 len)
{
    return memcpy(dst, src, len);
}

static s32 __kmemclear(void* _0, void* dst, u32 len)
{
    memclear(dst, len);
    return IOS_SUCCESS;
}


u32 IOS_Write32(u32 address, u32 value)
{
    return IOS_KernelExec((void*) &__kwrite32, address, value);
}

u16 IOS_Write16(u32 address, u16 value)
{
    return IOS_KernelExec((void*) &__kwrite16, address, value);
}

u8 IOS_Write8(u32 address, u8 value)
{
    return IOS_KernelExec((void*) &__kwrite8, address, value);
}

u32 IOS_Read32(u32 address)
{
    return IOS_KernelExec((void*) &__kread32, address);
}

u16 IOS_Read16(u32 address)
{
    return IOS_KernelExec((void*) &__kread16, address);
}

u8 IOS_Read8(u32 address)
{
    return IOS_KernelExec((void*) &__kread8, address);
}

u32 IOS_Set32(u32 address, u32 mask)
{
    return IOS_KernelExec((void*) &__kor32, address, mask);
}

u16 IOS_Set16(u16 address, u16 mask)
{
    return IOS_KernelExec((void*) &__kor16, address, mask);
}

u8 IOS_Set8(u8 address, u8 mask)
{
    return IOS_KernelExec((void*) &__kor8, address, mask);
}

u32 IOS_Clear32(u32 address, u32 mask)
{
    return IOS_KernelExec((void*) &__kand32, address, ~mask);
}

u16 IOS_Clear16(u16 address, u16 mask)
{
    return IOS_KernelExec((void*) &__kand16, address, ~mask);
}

u8 IOS_Clear8(u8 address, u8 mask)
{
    return IOS_KernelExec((void*) &__kand8, address, ~mask);
}

void* IOS_memcpy(u32 dst, u32 src, u32 len)
{
    return (void*) IOS_KernelExec((void*) &__kmemcpy, dst, src, len);
}

s32 IOS_memclear(u32 dst, u32 len)
{
    return IOS_KernelExec((void*) &__kmemclear, dst, len);
}