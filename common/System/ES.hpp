// ES.hpp - ES types
//   Written by Palapeli
//
// Copyright (C) 2022 Team Saoirse
// SPDX-License-Identifier: MIT

#pragma once
#include <System/Types.h>
#include <System/Util.h>

namespace ES
{

enum class ESError : s32
{
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

enum class SigType : u32
{
    RSA_2048 = 0x00010001,
    RSA_4096 = 0x00010000,
};

enum class Region : u16
{
    Japan = 0,
    USA = 1,
    Europe = 2,
    None = 3,
    Korea = 4,
};

namespace AccessFlag
{
enum
{
    Hardware = 0x1,
    DVDVideo = 0x2,
};
} // namespace AccessFlag

struct TMDContent {
    enum Flags
    {
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

} // namespace ES