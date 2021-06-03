#ifndef _ES_H
#define _ES_H

#include <types.h>

#ifdef __cplusplus
    extern "C" {
#endif

#define ES_PATH                             "/dev/es"

#define IOCTL_ES_LAUNCH                     8
#define IOCTL_ES_GETVIEWCNT                 18
#define IOCTL_ES_GETVIEWS                   19

typedef struct
{
	u32 tag;
	u32 value;
} __attribute__((packed)) TicketLimit;

typedef struct
{
    u32 view;
    u64 ticketID;
    u32 deviceType;
    u64 titleID;
    u16 accessMask;
    u8 reserved[0x3c];
    u8 cidxMask[0x40];
    u16 padding;
    TicketLimit limits[8];
} __attribute__((packed)) TicketView;

typedef struct
{
    u32 cid;
    u16 index;
    u16 type;
    u64 size;
    u8 hash[0x14];
}  __attribute__((packed)) TMDContent;

typedef struct
{
    char issuer[64];
    u8 version;
    u8 caCRLVersion;
    u8 signerCRLVersion;
    u8 vwiiTitle;
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
    u16 fill3;
    TMDContent contents[];
} __attribute__((packed)) TMD;

s32 ES_InitLib();
s32 ES_CloseLib();
s32 ES_GetNumTicketViews(u64 titleID, u32* cnt);
s32 ES_GetTicketViews(u64 titleID, TicketView* views, u32 cnt);
s32 ES_LaunchTitle(u64 titleID, const TicketView* view);

#ifdef __cplusplus
    }
#endif

#endif // _ES_H