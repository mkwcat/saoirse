#pragma once

#include <os.h>
#include <es.h>
#include <util.h>
#include <dip.h>
#include "File.hpp"

enum class DiIoctl : u8
{
    ReadDiskID = 0x70,
    EncryptedRead = 0x71,

    WaitForCoverClose = 0x79,
    GetCoverStatus = 0x88,
    Reset = 0x8A,
    OpenPartition = 0x8B,
    UnencryptedRead = 0x8D,
    RequestDiscStatus = 0xDB,

    Proxy_PatchDVD = 0x00,
    Proxy_StartGame = 0x01
};

enum class DiErr : s32
{
    FileNotFound = -6,
    LibError = -2,
    NoAccess = -1,

    /* DIP driver errors */
    OK = (1 << 0),
    DriveError = (1 << 1),
    CoverClosed = (1 << 2),
    Timeout = (1 << 4),
    Security = (1 << 5),
    Verify = (1 << 6),
    Invalid = (1 << 7)
};

namespace DVDLow
{

struct DVDCommand
{
    s32 id;
    Queue<DiErr> reply_queue{1};
    IOS::Vector* vec = nullptr;

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

    void sendIoctl(DiIoctl cmd, void* out, u32 outLen);
    void sendIoctlv(DiIoctl cmd, u32 inputCnt, u32 outputCnt);
    DiErr syncReply() { return this->reply_queue.receive(); }
    DiErr syncReplyAssertRet(DiErr expected);
    void beginIoctlv(u32 inputCnt, u32 outputCnt) {
        vec = new IOS::Vector[inputCnt * outputCnt];
    }
    void endIoctlv() { delete vec; vec = nullptr; }
};

const char* PrintErr(DiErr err);
void ResetAsync(DVDCommand& block, bool spinup);
void ReadDiskIDAsync(DVDCommand& block, void* data);
void OpenPartitionAsync(DVDCommand& block, u32 offset, ES::TMDFixed<512>* tmd);

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
void InitProxy();
void Deinit();
bool OpenCacheFile();
DiErr ResetDrive(bool spinup);
DiErr ReadDiskID(DiskID* out);
DiErr ReadCachedDiskID(DiskID* out);
bool IsInserted();

}

namespace DVDProxy
{

s32 ApplyPatches(DIP::DVDPatch* patches, u32 patchCount);
void StartGame();

}