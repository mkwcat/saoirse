#include "DCache.hpp"

#include <System/Util.h>

namespace DCache
{

#ifdef TARGET_PPC

// PPC cache functions

void Store(const void* start, size_t size)
{
    if (size == 0) {
        return;
    }
    size_t address = reinterpret_cast<size_t>(start);
    size = round_up(size, 0x20);
    do {
        asm("dcbst %y0" : : "Z"(*reinterpret_cast<u8*>(address)));
        address += 0x20;
        size -= 0x20;
    } while (size > 0);
    asm("sync");
}

void Flush(const void* start, size_t size)
{
    if (size == 0) {
        return;
    }
    size_t address = reinterpret_cast<size_t>(start);
    size = round_up(size, 0x20);
    do {
        asm("dcbf %y0" : : "Z"(*reinterpret_cast<u8*>(address)));
        address += 0x20;
        size -= 0x20;
    } while (size > 0);
    asm("sync");
}

void Invalidate(void* start, size_t size)
{
    if (size == 0) {
        return;
    }
    size_t address = reinterpret_cast<size_t>(start);
    size = round_up(size, 0x20);
    do {
        asm("dcbi %y0" : : "Z"(*reinterpret_cast<u8*>(address)));
        address += 0x20;
        size -= 0x20;
    } while (size > 0);
}

#else

// IOS cache functions

#  include <IOS/Syscalls.h>

void Store(const void* start, size_t size)
{
    if (size == 0) {
        return;
    }

    // No store on IOS
    IOS_FlushDCache(start, size);
}

void Flush(const void* start, size_t size)
{
    if (size == 0) {
        return;
    }

    IOS_FlushDCache(start, size);
}

void Invalidate(void* start, size_t size)
{
    if (size == 0) {
        return;
    }

    IOS_InvalidateDCache(start, size);
}

#endif

} // namespace DCache
