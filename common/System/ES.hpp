// ES.hpp - ES interface
//   Written by Palapeli
//
// SPDX-License-Identifier: MIT

#pragma once

#include <System/OS.hpp>
#include <System/Types.h>
#include <System/Util.h>

class ES
{
public:
    static ES* s_instance;

    enum class ESError : s32 {
        OK = 0,
        InvalidPubKeyType = -1005,
        ReadError = -1009,
        WriteError = -1010,
        InvalidSigType = -1012,
        MaxOpen = -1016,
        Invalid = -1017,
        DeviceIDMatch = -1020,
        HashMatch = -1022,
        NoMemory = -1024,
        NoAccess = -1026,
        IssuerNotFound = -1027,
        TicketNotFound = -1028,
        InvalidTicket = -1029,
        OutdatedBoot2 = -1031,
        TicketLimit = -1033,
        OutdatedTitle = -1035,
        RequiredIOSNotInstalled = -1036,
        WrongTMDContentCount = -1037,
        NoTMD = -1039,
    };

    enum class ESIoctl {
        AddTicket = 0x01,
        AddTitleStart = 0x02,
        AddContentStart = 0x03,
        AddContentData = 0x04,
        AddContentFinish = 0x05,
        AddTitleFinish = 0x06,
        GetDeviceID = 0x07,
        LaunchTitle = 0x08,
        OpenContent = 0x09,
        ReadContent = 0x0A,
        CloseContent = 0x0B,
        GetOwnedTitlesCount = 0x0C,
        GetOwnedTitles = 0x0D,
        GetTitlesCount = 0x0E,
        GetTitles = 0x0F,
        GetTitleContentsCount = 0x10,
        GetTitleContents = 0x11,
        GetNumTicketViews = 0x12,
        GetTicketViews = 0x13,
        GetTMDViewSize = 0x14,
        GetTMDView = 0x15,
        GetConsumption = 0x16,
        DeleteTitle = 0x17,
        DeleteTicket = 0x18,
        DIGetTMDViewSize = 0x19,
        DIGetTMDView = 0x1A,
        DIGetTicketView = 0x1B,
        DIVerify = 0x1C,
        GetDataDir = 0x1D,
        GetDeviceCert = 0x1E,
        ImportBoot = 0x1F,
        GetTitleID = 0x20,
        SetUID = 0x21,
        DeleteTitleContent = 0x22,
        SeekContent = 0x23,
        OpenTitleContent = 0x24,
        LaunchBC = 0x25,
        ExportTitleInit = 0x26,
        ExportContentBegin = 0x27,
        ExportContentData = 0x28,
        ExportContentEnd = 0x29,
        ExportTitleDone = 0x2A,
        AddTmd = 0x2B,
        Encrypt = 0x2C,
        Decrypt = 0x2D,
        GetBoot2Version = 0x2E,
        AddTitleCancel = 0x2F,
        Sign = 0x30,
        VerifySign = 0x31,
        GetStoredContentCount = 0x32,
        GetStoredContent = 0x33,
        GetStoredTmdSize = 0x34,
        GetStoredTmd = 0x35,
        GetSharedContentCount = 0x36,
        GetSharedContents = 0x37,
        DeleteSharedContent = 0x38,
        GetDiTmdSize = 0x39,
        GetDiTmd = 0x3A,
        DiVerifyWithTicketView = 0x3B,
        SetupStreamKey = 0x3C,
        DeleteStreamKey = 0x3D,
    };

    enum class SigType : u32 {
        RSA_2048 = 0x00010001,
        RSA_4096 = 0x00010000,
    };

    enum class Region : u16 {
        Japan = 0,
        USA = 1,
        Europe = 2,
        None = 3,
        Korea = 4,
    };

    enum AccessFlag {
        Flag_Hardware = 0x1,
        Flag_DVDVideo = 0x2,
    };

    struct TMDContent {
        enum Flags {
            Flag_Default = 0x1,
            Flag_Normal = 0x4000,
            Flag_DLC = 0x8000,
        };

        u32 cid;
        u16 index;
        u16 flags;
        u64 size;
        u8 hash[0x14];
    } ATTRIBUTE_PACKED;

    struct TMDHeader {
        SigType sigType;
        u8 sigBlock[256];
        u8 fill1[60];
        char issuer[64];
        u8 version;
        u8 caCRLVersion;
        u8 signerCRLVersion;
        u8 vWiiTitle;
        u64 iosTitleID;
        u64 titleID;
        u32 titleType;
        u16 groupID;
        u16 zero;
        Region region;
        u8 ratings[16];
        u8 reserved[12];
        u8 ipcMask[12];
        u8 reserved2[18];
        u32 accessRights;
        u16 titleVersion;
        u16 numContents;
        u16 bootIndex;
        u16 fill2;
    } ATTRIBUTE_PACKED;

    static_assert(sizeof(TMDHeader) == 0x1E4);

    struct TMD {
        TMDHeader header;

        TMDContent* getContents()
        {
            return reinterpret_cast<TMDContent*>(this + 1);
        }

        u32 size() const
        {
            return sizeof(TMDHeader) + sizeof(TMDContent) * header.numContents;
        }
    } ATTRIBUTE_PACKED;

    template <u16 TNumContents>
    struct TMDFixed : TMD {
        TMDContent contents[TNumContents];

        u32 size() const
        {
            return sizeof(TMDHeader) + sizeof(TMDContent) * TNumContents;
        }
    } ATTRIBUTE_PACKED;

    struct TicketLimit {
        u32 tag;
        u32 value;
    } ATTRIBUTE_PACKED;

    static_assert(sizeof(TicketLimit) == 0x8);

    struct TicketInfo {
        u64 ticketID;
        u32 consoleID;
        u64 titleID;
        u16 unknown_0x1E4;
        u16 ticketTitleVersion;
        u16 permittedTitlesMask;
        u32 permitMask;
        bool allowTitleExport;
        u8 commonKeyIndex;
        u8 reserved[0x30];
        u8 cidxMask[0x40];
        u16 fill4;
        TicketLimit limits[8];
        u16 fill8;
    } ATTRIBUTE_PACKED;

    static_assert(sizeof(TicketInfo) == 0xD4);

    struct Ticket {
        SigType sigType;
        u8 sigBlock[0x100];
        u8 fill1[0x3C];
        char issuer[64];
        u8 fill2[0x3F];
        u8 titleKey[16];
        u8 fill3;
        TicketInfo info;
    } ATTRIBUTE_PACKED;

    static_assert(sizeof(Ticket) == 0x2A4);

    struct TicketView {
        u32 view;
        TicketInfo info;
    } ATTRIBUTE_PACKED;

    static_assert(sizeof(TicketView) == 0xD8);

public:
    ESError GetDeviceID(u32* out);
    ESError LaunchTitle(u64 titleID, const TicketView* view);
    ESError GetOwnedTitlesCount(u32* outCount);
    ESError GetTitlesCount(u32* outCount);
    ESError GetTitles(u32 count, u64* outTitles);
    ESError GetTitleContentsCount(u64 titleID, u32* outCount);
    ESError GetTitleContents(u64 titleID, u32 count, u32* outContents);
    ESError GetNumTicketViews(u64 titleID, u32* outCount);
    ESError GetTicketViews(u64 titleID, u32 count, TicketView* outViews);
    ESError GetTMDViewSize(u64 titleID, u32* outSize);
    ESError GetTMDView(u64 titleID, void* out, u32 outLen);
    ESError DIGetTicketView(const Ticket* inTicket, TicketView* outView);
    ESError DIGetTicketView(TicketView* outView);
    ESError GetDataDir(u64 titleID, char* outPath);
    ESError GetDeviceCert(void* out);
    ESError GetTitleID(u64* outTitleID);

private:
    IOS::ResourceCtrl<ESIoctl> m_rm{"/dev/es"};
};
