#include "dvd.h"
#include "irse.h"
#include "os.h"
#include "hollywood.h"
#include <cstdio>
#include <array>
#include <utility>
#include <unistd.h>
#include <stdlib.h>

static IOS::ResourceCtrl<DiIoctl> di(-1);
static IOS::File cacheFile(-1);
static bool initialized = false;

static DVDLow::DVDCommand sDvdBlocks[8];
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
    ASSERT(block != nullptr);

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

    block->endIoctlv();
    block->reply_queue.send(err);
    return 0;
}

void DVD::Init()
{
    if (initialized)
        return;
    initialized = true;

    new (&di) IOS::ResourceCtrl<DiIoctl>("/dev/di/proxy");
    ASSERT(di.fd() >= 0);

    for (s32 i = 0; i < 8; i++)
        dataQueue.send(&sDvdBlocks[i]);

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

    DVDLow::ResetAsync(*block.cmd(), spinup);
    const DiErr ret = block.cmd()->syncReply();
    if (ret != DiErr::OK)
        irse::Log(LogS::DVD, LogL::WARN, "Failed to reset drive: %s\n",
            DVDLow::PrintErr(ret));
    return ret;
}

bool DVD::IsInserted()
{
    UniqueCommand block;
    u32 status ATTRIBUTE_ALIGN(32);

    DVDLow::GetCoverStatusAsync(*block.cmd(), &status);
    block.cmd()->syncReplyAssertRet(DiErr::OK);

    return status == STATUS_INSERTED;
}

DiErr DVD::ReadDiskID(DiskID* out)
{
    UniqueCommand block;

    DVDLow::ReadDiskIDAsync(*block.cmd(), reinterpret_cast<void*>(out));
    return block.cmd()->syncReply();
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


void DVDLow::ResetAsync(DVDCommand& block, bool spinup)
{
    block.input.command = DiIoctl::Reset;
    block.input.args[0] = static_cast<u32>(spinup);

    block.sendIoctl(DiIoctl::Reset,
        reinterpret_cast<void*>(block.output_buf), sizeof(block.output_buf));
}

void DVDLow::ReadDiskIDAsync(DVDCommand& block, void* data)
{
    block.input.command = DiIoctl::ReadDiskID;
    block.sendIoctl(DiIoctl::ReadDiskID, data, 0x20);
}

void DVDLow::OpenPartitionAsync(DVDCommand& block, u32 offset, signed_blob* meta)
{
    block.input.command = DiIoctl::OpenPartition;
    block.input.args[0] = offset;

    block.beginIoctlv(3, 2);
    /* input - Ticket (optional) */
    block.vec[1].data = nullptr;
    block.vec[1].len = 0;
    /* input - Shared certs (optional) */
    block.vec[2].data = nullptr;
    block.vec[2].len = 0;
    /* output - TMD */
    block.vec[3].data = meta;
    block.vec[3].len = 0x49E4;
    /* output - ES Error */
    block.vec[4].data = block.output;
    block.vec[4].len = 32; // uhh...
    block.sendIoctlv(DiIoctl::OpenPartition, 3, 2);
}

void DVDLow::UnencryptedReadAsync(
    DVDCommand& block, void* data, u32 len, u32 offset)
{
    block.input.command = DiIoctl::UnencryptedRead;
    block.input.args[0] = len;
    block.input.args[1] = offset;

    block.sendIoctl(DiIoctl::UnencryptedRead, data, len);
}

void DVDLow::EncryptedReadAsync(
    DVDCommand& block, void* data, u32 len, u32 offset)
{
    block.input.command = DiIoctl::EncryptedRead;
    block.input.args[0] = len;
    block.input.args[1] = offset;

    block.sendIoctl(DiIoctl::EncryptedRead, data, len);
}

void DVDLow::GetCoverStatusAsync(DVDCommand& block, u32* result)
{
    block.input.command = DiIoctl::GetCoverStatus;
    if (!aligned(result, 32)) {
        block.reply_queue.send(DiErr::LibError);
        return;
    }

    block.sendIoctl(DiIoctl::GetCoverStatus,
        reinterpret_cast<void*>(result), sizeof(u32));
}

void DVDLow::WaitForCoverCloseAsync(DVDCommand& block)
{
    block.input.command = DiIoctl::WaitForCoverClose;

    block.sendIoctl(DiIoctl::WaitForCoverClose, nullptr, 0);
}

void DVDLow::DVDCommand::sendIoctl(
    DiIoctl cmd, void* out, u32 outLen)
{
    if (!initialized) {
        this->reply_queue.send(DiErr::LibError);
        return;
    }

    const s32 ret = di.ioctlAsync(cmd,
        this->input_buf, sizeof(this->input_buf),
        out, outLen, &DVD_Callback, reinterpret_cast<void*>(this));
    
    if (ret != IOSErr::OK)
        this->reply_queue.send(static_cast<DiErr>(ret));
}

void DVDLow::DVDCommand::sendIoctlv(
    DiIoctl cmd, u32 inputCnt, u32 outputCnt)
{
    if (!initialized) {
        this->reply_queue.send(DiErr::LibError);
        return;
    }

    ASSERT(inputCnt >= 1);
    this->vec[0].data = this->input_buf;
    this->vec[0].len = sizeof(this->input_buf);

    const s32 ret = di.ioctlvAsync(cmd, inputCnt, outputCnt, this->vec,
        &DVD_Callback, reinterpret_cast<void*>(this));
    
    if (ret != IOSErr::OK)
        this->reply_queue.send(static_cast<DiErr>(ret));
}

DiErr DVDLow::DVDCommand::syncReplyAssertRet(DiErr expected)
{
    const DiErr ret = this->syncReply();
    
    if (ret != expected) {
        irse::Log(LogS::DVD, LogL::ERROR,
            "SyncReplyAssertRet: ret == %s, expected %s",
            DVDLow::PrintErr(ret), DVDLow::PrintErr(expected));
        abort();
    }
    
    return ret;
}

s32 DVDProxy::ApplyPatches(Patch* patches, u32 patchCount)
{
    return di.ioctl(DiIoctl::Proxy_PatchDVD,
        patches, patchCount * sizeof(Patch), nullptr, 0);
}