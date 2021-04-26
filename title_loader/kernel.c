#include <ios.h>
#include <string.h>
#include <gctypes.h>


static s32 __kwrite32(void* _0, u32* ptr, u32 value)
{
    *ptr = value;
    return value;
}

static s32 __kwrite16(void* _0, u16* ptr, u16 value)
{
    *ptr = value;
    return value;
}

static s32 __kwrite8(void* _0, u8* ptr, u8 value)
{
    *ptr = value;
    return value;
}

static s32 __kread32(void* _0, u32* ptr)
{
    return *ptr;
}

static s32 __kread16(void* _0, u16* ptr)
{
    return *ptr;
}

static s32 __kread8(void* _0, u8* ptr)
{
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


s32 IOS_Write32(u32 address, u32 value)
{
    return IOS_KernelExec((void*) &__kwrite32, address, value);
}

s32 IOS_Write16(u32 address, u16 value)
{
    return IOS_KernelExec((void*) &__kwrite16, address, value);
}

s32 IOS_Write8(u32 address, u8 value)
{
    return IOS_KernelExec((void*) &__kwrite8, address, value);
}

s32 IOS_Read32(u32 address)
{
    return IOS_KernelExec((void*) &__kread32, address);
}

s32 IOS_Read16(u32 address)
{
    return IOS_KernelExec((void*) &__kread16, address);
}

s32 IOS_Read8(u32 address)
{
    return IOS_KernelExec((void*) &__kread8, address);
}

void* IOS_memcpy(u32 dst, const void* src, u32 len)
{
    return (void*) IOS_KernelExec((void*) &__kmemcpy, dst, src, len);
}

s32 IOS_memclear(u32 dst, u32 len)
{
    return IOS_KernelExec((void*) &__kmemclear, dst, len);
}