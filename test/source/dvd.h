#pragma once

#include "os.h"
#include <gccore.h>


enum class DiIoctl : u8
{
    ReadDiskID = 0x70,
    EncryptedRead = 0x71,

    WaitForCoverClose = 0x79,
    GetCoverStatus = 0x88,
    Reset = 0x8A,
    UnencryptedRead = 0x8D,
    RequestDiscStatus = 0xDB
};

enum class DiErr : s32
{
    FileNotFound = -6,
    LibError = -2,
    NoAccess = -1,

    /* DIP driver errors */
    OK = (1 << 0),
    DriveError = (1 << 1),
    CoverClosed = (1 << 2)
};

namespace DVDLow
{

struct DVDCommand
{
    s32 id;
    Queue<DiErr> reply_queue{1};

    union {    
        struct {
            DiIoctl command;
            u8 pad[3];
            u32 args[7];
        } input;
        u8 input_buf[32];
    };

    union {
        u32 output[8];
        u8 output_buf[32];
    };

    DiErr syncReply();
    DiErr syncReplyAssertRet(DiErr expected);
};

const char* PrintErr(DiErr err);
void ResetAsync(DVDCommand& block, bool spinup);
void ReadDiskIDAsync(DVDCommand& block, void* data);

void UnencryptedReadAsync(DVDCommand& block, void* data, u32 len, u32 offset);
void EncryptedReadAsync(DVDCommand& block, void* data, u32 len, u32 offset);

void GetCoverStatusAsync(DVDCommand& block, u32* result);
void WaitForCoverCloseAsync(DVDCommand& block);

}

namespace DVD
{

DVDLow::DVDCommand* GetCommand();
void ReleaseCommand(DVDLow::DVDCommand*);

struct UniqueCommand
{
    UniqueCommand() : m_cmd(GetCommand()) { }
    ~UniqueCommand() { ReleaseCommand(m_cmd); }
    UniqueCommand(const UniqueCommand& from) = delete;
    
    DVDLow::DVDCommand* cmd() {
        return m_cmd;
    }

protected:
    DVDLow::DVDCommand* const m_cmd;
};

struct DiskID
{
    char gameID[4];
    u16 groupID;
    u8 discNum;
    u8 discVer;
    u8 discStreamFlag;
    u8 discStreamSize;
    u8 pad[0xE];
    u32 discMagic;
    u32 discMagicGC;
};

void Init();
bool OpenCacheFile();
DiErr ResetDrive(bool spinup);
DiErr ReadDiskID(DiskID* out);
DiErr ReadCachedDiskID(DiskID* out);
bool IsInserted();


}
