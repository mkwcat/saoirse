#pragma once
#include <gctypes.h>

namespace ES
{

enum class SigType : u32
{
   RSA_2048 = 0x00010001,
   RSA_4096 = 0x00010000
};

struct TMDContent
{
    u32 cid;
    u16 index;
    u16 type;
    u64 size;
    u8 hash[0x14];
};

struct TMDHeader
{
    SigType sigType;
    u8 sigBlock[256];
    u8 fill1[60];
    char issuer[64];
    u8 version;
    u8 caCRLVersion;
    u8 signerCRLVersion;
    u8 vWiiTitle;
    u64 sysVersion;
    u64 titleID;
    u32 titleType;
    u16 groupID;
    u16 zero;
    u16 region;
    u8 ratings[16];
    u8 reserved[12];
    u8 ipcMask[12];
    u8 reserved2[18];
    u32 accessRights;
    u16 titleVersion;
    u16 numContents;
    u16 bootIndex;
    u16 fill2;
} __attribute__((packed));

struct TMD : TMDHeader
{
    TMDContent* contents() { return reinterpret_cast<TMDContent*>(this + 1); }
};

template<u16 TNumContents>
struct TMDFixed : TMD 
{
    TMDContent contents[TNumContents];
};

}