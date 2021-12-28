#include <types.h>

constexpr bool validJumptablePtr(u32 address)
{
    return address >= 0xFFFF0040 && !(address & 3);
}

constexpr bool validKernelCodePtr(u32 address)
{
    return address >= 0xFFFF0040 && (address & 2) != 2;
}

template <class T> constexpr T toUncached(T address)
{
    return reinterpret_cast<T>(reinterpret_cast<u32>(address) | 0x80000000);
}

constexpr u16 thumbBLHi(u32 src, u32 dest)
{
    s32 diff = dest - (src + 4);
    return ((diff >> 12) & 0x7FF) | 0xF000;
}

constexpr u16 thumbBLLo(u32 src, u32 dest)
{
    s32 diff = dest - (src + 4);
    return ((diff >> 1) & 0x7FF) | 0xF800;
}

void patchIOSOpen();
void importKoreanCommonKey();
extern "C" void iosOpenStrncpyHook();
extern "C" char* iosOpenStrncpy(char* dest, const char* src, u32 num, s32 pid);