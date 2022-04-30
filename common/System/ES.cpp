#include "ES.hpp"

ES* ES::sInstance = nullptr;

ES::ESError ES::GetDeviceID(u32* out)
{
    IOS::OVector<1> vec;
    vec.out[0].data = reinterpret_cast<void*>(out);
    vec.out[0].len = sizeof(u32);

    s32 ret = m_rm.ioctlv(ESIoctl::GetDeviceID, vec);
    return static_cast<ESError>(ret);
}

ES::ESError ES::LaunchTitle(u64 titleID, const TicketView* view)
{
    IOS::IVector<2> vec;
    vec.in[0].data = reinterpret_cast<void*>(&titleID);
    vec.in[0].len = sizeof(u64);
    vec.in[1].data = reinterpret_cast<const void*>(view);
    vec.in[1].len = sizeof(TicketView);

    s32 ret = m_rm.ioctlv(ESIoctl::LaunchTitle, vec);
    return static_cast<ESError>(ret);
}

ES::ESError ES::GetOwnedTitlesCount(u32* outCount)
{
    IOS::OVector<1> vec;
    vec.out[0].data = reinterpret_cast<void*>(outCount);
    vec.out[0].len = sizeof(u32);

    s32 ret = m_rm.ioctlv(ESIoctl::GetOwnedTitlesCount, vec);
    return static_cast<ESError>(ret);
}

ES::ESError ES::GetTitlesCount(u32* outCount)
{
    IOS::OVector<1> vec;
    vec.out[0].data = reinterpret_cast<void*>(outCount);
    vec.out[0].len = sizeof(u32);

    s32 ret = m_rm.ioctlv(ESIoctl::GetTitlesCount, vec);
    return static_cast<ESError>(ret);
}

ES::ESError ES::GetTitles(u32 count, u64* outTitles)
{
    IOS::IOVector<1, 1> vec;
    vec.in[0].data = reinterpret_cast<void*>(&count);
    vec.in[0].len = sizeof(u32);
    vec.out[0].data = reinterpret_cast<void*>(outTitles);
    vec.out[0].len = sizeof(u64) * count;

    s32 ret = m_rm.ioctlv(ESIoctl::GetTitles, vec);
    return static_cast<ESError>(ret);
}

ES::ESError ES::GetTitleContentsCount(u64 titleID, u32* outCount)
{
    IOS::IOVector<1, 1> vec;
    vec.in[0].data = reinterpret_cast<void*>(&titleID);
    vec.in[0].len = sizeof(u64);
    vec.out[0].data = reinterpret_cast<void*>(outCount);
    vec.out[0].len = sizeof(u32);

    s32 ret = m_rm.ioctlv(ESIoctl::GetTitleContentsCount, vec);
    return static_cast<ESError>(ret);
}

ES::ESError ES::GetTitleContents(u64 titleID, u32 count, u32* outContents)
{
    IOS::IOVector<2, 1> vec;
    vec.in[0].data = reinterpret_cast<void*>(&titleID);
    vec.in[0].len = sizeof(u64);
    vec.in[1].data = reinterpret_cast<void*>(&count);
    vec.in[1].len = sizeof(u32);
    vec.out[0].data = reinterpret_cast<void*>(outContents);
    vec.out[0].len = sizeof(u32) * count;

    s32 ret = m_rm.ioctlv(ESIoctl::GetTitleContents, vec);
    return static_cast<ESError>(ret);
}

ES::ESError ES::GetNumTicketViews(u64 titleID, u32* outCount)
{
    IOS::IOVector<1, 1> vec;
    vec.in[0].data = reinterpret_cast<void*>(&titleID);
    vec.in[0].len = sizeof(u64);
    vec.out[0].data = reinterpret_cast<void*>(outCount);
    vec.out[0].len = sizeof(u32);

    s32 ret = m_rm.ioctlv(ESIoctl::GetNumTicketViews, vec);
    return static_cast<ESError>(ret);
}

ES::ESError ES::GetTicketViews(u64 titleID, u32 count, TicketView* outViews)
{
    IOS::IOVector<2, 1> vec;
    vec.in[0].data = reinterpret_cast<void*>(&titleID);
    vec.in[0].len = sizeof(u64);
    vec.in[1].data = reinterpret_cast<void*>(&count);
    vec.in[1].len = sizeof(u32);
    vec.out[0].data = reinterpret_cast<void*>(outViews);
    vec.out[0].len = sizeof(TicketView) * count;

    s32 ret = m_rm.ioctlv(ESIoctl::GetTicketViews, vec);
    return static_cast<ESError>(ret);
}

ES::ESError ES::GetTMDViewSize(u64 titleID, u32* outSize)
{
    IOS::IOVector<1, 1> vec;
    vec.in[0].data = reinterpret_cast<void*>(&titleID);
    vec.in[0].len = sizeof(u64);
    vec.out[0].data = reinterpret_cast<void*>(outSize);
    vec.out[0].len = sizeof(u32);

    s32 ret = m_rm.ioctlv(ESIoctl::GetTMDViewSize, vec);
    return static_cast<ESError>(ret);
}

ES::ESError ES::GetTMDView(u64 titleID, void* out, u32 outLen)
{
    IOS::IOVector<1, 1> vec;
    vec.in[0].data = reinterpret_cast<void*>(&titleID);
    vec.in[0].len = sizeof(u64);
    vec.out[0].data = out;
    vec.out[0].len = outLen;

    s32 ret = m_rm.ioctlv(ESIoctl::GetTMDView, vec);
    return static_cast<ESError>(ret);
}

ES::ESError ES::GetDataDir(u64 titleID, char* outPath)
{
    IOS::IOVector<1, 1> vec;
    vec.in[0].data = reinterpret_cast<void*>(&titleID);
    vec.in[0].len = sizeof(u64);
    vec.out[0].data = outPath;
    vec.out[0].len = 30;

    s32 ret = m_rm.ioctlv(ESIoctl::GetDataDir, vec);
    return static_cast<ESError>(ret);
}

ES::ESError ES::GetDeviceCert(void* out)
{
    IOS::OVector<1> vec;
    vec.out[0].data = out;
    vec.out[0].len = 0x180;

    s32 ret = m_rm.ioctlv(ESIoctl::GetDeviceCert, vec);
    return static_cast<ESError>(ret);
}

ES::ESError ES::GetTitleID(u64* outTitleID)
{
    IOS::OVector<1> vec;
    vec.out[0].data = reinterpret_cast<void*>(outTitleID);
    vec.out[0].len = sizeof(u64);

    s32 ret = m_rm.ioctlv(ESIoctl::GetTitleID, vec);
    return static_cast<ESError>(ret);
}