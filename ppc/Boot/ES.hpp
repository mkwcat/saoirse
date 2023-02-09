#pragma once

#include "IOS.hpp"

#include <optional>

namespace IOS
{

class ES final : private Resource
{
public:
#pragma pack(push, 4)

    struct Ticket {
        u32 signatureType;
        u8 _004[0x1DC - 0x004];
        u64 titleID;
        u16 accessMask;
        u8 _1e6[0x222 - 0x1E6];
        u8 contentAccessMask[512 / 8];
        u8 _262[0x2A4 - 0x262];
    };

    static_assert(sizeof(Ticket) == 0x2A4);

    struct Content {
        u32 id;
        u16 index;
        u16 type;
        u64 size;
        u8 sha1[0x14];
    };

    static_assert(sizeof(Content) == 0x24);

    struct TMD {
        u32 signatureType;
        u8 _004[0x184 - 0x004];
        u64 iosID;
        u64 titleID;
        u32 titleType;
        u16 groupID;
        u8 _19a[0x19C - 0x19A];
        u16 region;
        u8 ratings[16];
        u8 _1ae[0x1DC - 0x1AE];
        u16 titleVersion;
        u16 numContents;
        u16 bootIndex;
        Content contents[512];
    };

    static_assert(sizeof(TMD) == 0x49E4);

    struct TicketView {
        u8 _00[0xd8 - 0x00];
    };

    static_assert(sizeof(TicketView) == 0xD8);

    struct TMDView {
        u8 _0000[0x0058 - 0x0000];
        u16 titleVersion;
        u8 _005a[0x205C - 0x005A];
    };

    static_assert(sizeof(TMDView) == 0x205C);
#pragma pack(pop)

    ES();
    ~ES();
    using Resource::ok;

    std::optional<u32> getTicketViewCount(u64 titleID);
    bool getTicketViews(u64 titleID, u32 count, TicketView* views);
    bool launchTitle(u64 titleID, TicketView* view);
};

} // namespace IOS
