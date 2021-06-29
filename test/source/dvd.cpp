#include "dvd.h"
#include "irse.h"
#include "os.h"
#include <cstdio>
#include <array>
#include <utility>
#include <unistd.h>
#include <stdlib.h>

static IOS::ResourceCtrl<DiIoctl> di(-1);
static IOS::File cacheFile(-1);
static bool initialized = false;

static Queue<DVDLow::DVDCommand*> dataQueue(8);

constexpr const char* DVD_CACHE_FILE = "/title/00000001/00000002/data/cache.dat";
constexpr std::size_t DVD_CACHE_SIZE = 0xB00000;
constexpr std::ptrdiff_t DVD_CACHE_DISKID_OFFS = 0x20;

enum EStatus {
    STATUS_NOTINSERTED = 1,
    STATUS_INSERTED = 2
};

const char* DVDLow::PrintErr(DiErr err)
{
#define DI_ERR_STR(error) case DiErr::error: return #error
    switch(err) {
        DI_ERR_STR(FileNotFound);
        DI_ERR_STR(LibError);
        DI_ERR_STR(NoAccess);
        DI_ERR_STR(OK);
        DI_ERR_STR(DriveError);
        DI_ERR_STR(CoverClosed);
    }
#undef DI_ERR_STR
    return "Unknown";
}

DVDLow::DVDCommand* DVD::GetCommand()
{
    DVDLow::DVDCommand* block = dataQueue.receive();
    ASSERT(block != nullptrptr);

    return block;
}

void DVD::ReleaseCommand(DVDLow::DVDCommand* block)
{
    dataQueue.send(block);
}

static s32 DVD_Callback(s32 result, void* usrdata)
{
    DVDLow::DVDCommand* block = reinterpret_cast<DVDLow::DVDCommand*>(usrdata);
    DiErr err = static_cast<DiErr>(result);

    block->reply_queue.send(err);
    return 0;
}

static void DVD_SendIoctl(
    DiIoctl cmd, DVDLow::DVDCommand* block, void* output, u32 outputLen)
{
    if (!initialized) {
        block->reply_queue.send(DiErr::LibError);
        return;
    }

    const s32 ret = di.ioctlAsync(cmd,
        block->input_buf, sizeof(block->input_buf),
        output, outputLen, &DVD_Callback, reinterpret_cast<void*>(block));
    
    if (ret != IOSErr::OK)
        block->reply_queue.send(static_cast<DiErr>(ret));
}

void DVD::Init()
{
    if (initialized)
        return;
    initialized = true;

    new (&di) IOS::ResourceCtrl<DiIoctl>("/dev/di");
    ASSERT(di.fd() >= 0);

    DVDLow::DVDCommand* blocks = new DVDLow::DVDCommand[8];
    for (s32 i = 0; i < 8; i++)
        dataQueue.send(blocks + i);

    irse::Log(LogS::DVD, LogL::WARN, "DVD initialized");
}

bool DVD::OpenCacheFile()
{
    IOS::File f(DVD_CACHE_FILE, IOS::Mode::Read);
    if (f.fd() < 0) {
        irse::Log(LogS::DVD, LogL::ERROR,
            "Failed to open cache.dat: %d", f.fd());
        return false;
    }
    if (f.size() != DVD_CACHE_SIZE) {
        irse::Log(LogS::DVD, LogL::ERROR, "Invalid cache.dat size");
        return false;
    }

    new (&cacheFile) IOS::File(std::move(f));
    return true;
}

DiErr DVD::ResetDrive(bool spinup)
{
    UniqueCommand block;

    DVDLow::ResetAsync(block.cmd(), spinup);
    DiErr ret = DVDLow::SyncReply(block.cmd());
    if (ret != DiErr::OK)
        irse::Log(LogS::DVD, LogL::WARN, "Failed to reset drive: %s\n",
            DVDLow::PrintErr(ret));
    return ret;
}

bool DVD::IsInserted()
{
    UniqueCommand block;
    u32 status ATTRIBUTE_ALIGN(32);

    DVDLow::GetCoverStatusAsync(block.cmd(), &status);
    DVDLow::SyncReplyAssertRet(block.cmd(), DiErr::OK);

    return status == STATUS_INSERTED;
}

DiErr DVD::ReadDiskID(DiskID* out)
{
    UniqueCommand block;

    DVDLow::ReadDiskIDAsync(block.cmd(), reinterpret_cast<void*>(out));

    return DVDLow::SyncReply(block.cmd());
}

DiErr DVD::ReadCachedDiskID(DiskID* out)
{
    if (const s32 ret = cacheFile.seek(DVD_CACHE_DISKID_OFFS, SEEK_SET); 
        ret != DVD_CACHE_DISKID_OFFS)
        return DiErr::DriveError;

    if (const s32 ret = cacheFile.read(reinterpret_cast<void*>(out), 0x20);
        ret != 0x20)
        return DiErr::DriveError;
    
    return DiErr::OK;
}


void DVDLow::ResetAsync(DVDCommand* block, bool spinup)
{
    block->input.command = DiIoctl::Reset;
    block->input.args[0] = static_cast<u32>(spinup);

    DVD_SendIoctl(DiIoctl::Reset, block,
        block->output_buf, sizeof(block->output_buf));
}

void DVDLow::ReadDiskIDAsync(DVDCommand* block, void* out)
{
    block->input.command = DiIoctl::ReadDiskID;
    DVD_SendIoctl(DiIoctl::ReadDiskID, block, out, 0x20);
}

void DVDLow::UnencryptedReadAsync(
    DVDCommand* block, void* buffer, u32 length, u32 offset)
{
    block->input.command = DiIoctl::UnencryptedRead;
    block->input.args[1] = length;
    block->input.args[2] = offset;

    DVD_SendIoctl(DiIoctl::UnencryptedRead, block, buffer, length);
}

void DVDLow::GetCoverStatusAsync(DVDCommand* block, u32* result)
{
    block->input.command = DiIoctl::GetCoverStatus;

    DVD_SendIoctl(DiIoctl::GetCoverStatus, block,
        reinterpret_cast<void*>(result), sizeof(u32));
}

void DVDLow::WaitForCoverCloseAsync(DVDCommand* block)
{
    block->input.command = DiIoctl::WaitForCoverClose;

    DVD_SendIoctl(DiIoctl::WaitForCoverClose, block, nullptr, 0);
}

DiErr DVDLow::SyncReply(DVDLow::DVDCommand* block)
{
    return block->reply_queue.receive();
}

DiErr DVDLow::SyncReplyAssertRet(DVDLow::DVDCommand* cmd, DiErr expected)
{
    const DiErr ret = cmd->reply_queue.receive();
    
    if (ret != expected) {
        irse::Log(LogS::DVD, LogL::ERROR,
            "SyncReplyAssertRet: ret == %s, expected %s\n",
            DVDLow::PrintErr(ret), DVDLow::PrintErr(expected));
        abort();
    }
    
    return ret;
}