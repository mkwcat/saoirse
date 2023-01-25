// EmuDI.cpp - Emulated DI RM
//   Written by Palapeli
//
// SPDX-License-Identifier: MIT

#include "EmuDI.hpp"
#include "ISO.hpp"
#include "VirtualDisc.hpp"
#include <DVD/DI.hpp>
#include <DVD/EmuDI.hpp>
#include <Debug/Log.hpp>
#include <Disk/DeviceMgr.hpp>
#include <IOS/IPCLog.hpp>
#include <IOS/Syscalls.h>
#include <IOS/System.hpp>
#include <System/ES.hpp>
#include <System/Types.h>
#include <cstring>

namespace EmuDI
{

typedef struct {
    u8 cmd;
    u8 pad[3];
    u32 args[7];
} DVDCommand;

static s32 DiMsgQueue = -1;
static u32 __diMsgData[8];
static bool DiStarted = false;
static bool GameStarted = false;

/* 200 for now */
static DVDPatch DiPatches[200];
static u32 DiNumPatches = 0;

#define DI_PROXY_IOCTL_PATCHDVD 0x00
#define DI_PROXY_IOCTL_STARTGAME 0x01

#define DI_EOK 0x1
#define DI_ESECURITY 0x20
#define DI_EBADARGUMENT 0x80

VirtualDisc* disc;
bool useVirtualDisc = false;

static DI::DIError WriteOutput(
  void* out, u32 outLen, const void* data, u32 dataLen)
{
    if (outLen < dataLen)
        return DI::DIError::Security;

    memcpy(out, data, dataLen);
    return DI::DIError::OK;
}

template <class T>
static DI::DIError WriteOutputStruct(void* out, u32 outLen, const T* data)
{
    return WriteOutput(
      out, outLen, reinterpret_cast<const void*>(data), sizeof(T));
}

/*
 * IOCTL for the emulated disc drive.
 * block: Input DVD command
 * cmd: IOCTL number
 * out: Output buffer
 * outLen: Length of the output buffer
 */
static DI::DIError EmuIoctl(
  DVDCommand* block, DI::DIIoctl cmd, void* out, u32 outLen)
{
    assert(disc != nullptr);

    switch (cmd) {
        // Stubs
    case DI::DIIoctl::Reset:
    case DI::DIIoctl::ClearCoverInterrupt:
        return DI::DIError::OK;

    case DI::DIIoctl::Inquiry: {
        if (outLen != sizeof(DI::DriveInfo)) {
            PRINT(IOS_EmuDI, ERROR,
              "Inquiry: Output buffer length does not match DriveInfo");
            return DI::DIError::Security;
        }

        return DI::sInstance->Inquiry(reinterpret_cast<DI::DriveInfo*>(out));
    }

    case DI::DIIoctl::GetStatusRegister: {
        u32 disr = 0;
        return WriteOutputStruct(out, outLen, &disr);
    }

    case DI::DIIoctl::Read: {
        u32 inByteLength = block->args[0];
        u32 inWordOffset = block->args[1];

        if (inByteLength != outLen) {
            PRINT(IOS_EmuDI, ERROR,
              "Read: Output buffer length does not match command block");
            return DI::DIError::Security;
        }

        if (!disc->ReadFromPartition(out, inWordOffset, inByteLength))
            return DI::DIError::Drive;

        return DI::DIError::OK;
    }

    case DI::DIIoctl::ReadDiskID: {
        DI::DiskID diskID;

        if (!disc->ReadDiskID(&diskID))
            return DI::DIError::Drive;

        PRINT(IOS_EmuDI, INFO, "Read DiskID: %.6s", diskID.gameID);
        return WriteOutputStruct(out, outLen, &diskID);
    }

    case DI::DIIoctl::UnencryptedRead: {
        u32 inByteLength = block->args[0];
        u32 inWordOffset = block->args[1];

        if (inByteLength != outLen) {
            PRINT(IOS_EmuDI, ERROR,
              "UnencryptedRead: Output buffer length does not match "
              "command block");
            return DI::DIError::Security;
        }

        u32 wordOffsetEnd = inWordOffset + ((inByteLength + 3) >> 2);

        // Read for modchip detection (Error #001, unauthorized device has
        // been detected)
        if (inWordOffset >= 0x460A0000 && wordOffsetEnd <= 0x460A0008)
            return DI::DIError::Drive;

        // Same as above but for dual layer discs
        if (inWordOffset >= 0x7ED40000 && wordOffsetEnd <= 0x7ED40008)
            return DI::DIError::Drive;

        // Only acceptable range
        if (wordOffsetEnd > 0x14000)
            return DI::DIError::Security;

        if (!disc->UnencryptedRead(out, inWordOffset, inByteLength))
            return DI::DIError::Drive;

        return DI::DIError::OK;
    }

    case DI::DIIoctl::ReadDiskBca: {
        // NSMBW reads this as some form of copy protection
        PRINT(IOS_EmuDI, INFO, "Read disk BCA");

        if (!aligned(out, 32))
            return DI::DIError::Security;

        if (outLen < 0x40)
            return DI::DIError::Security;

        u8 bca[0x40] = {0};
        bca[0x33] = 1;
        return WriteOutput(out, outLen, bca, 0x40);
    }

    case DI::DIIoctl::GetControlRegister: {
        u32 dicr = 0;
        return WriteOutputStruct(out, outLen, &dicr);
    }

    case DI::DIIoctl::GetCoverRegister: {
        u32 dicvr = disc->IsInserted() ? 0 : 1;
        return WriteOutputStruct(out, outLen, &dicvr);
    }

    default:
        PRINT(IOS_EmuDI, ERROR, "Unknown ioctl 0x%02X", static_cast<u32>(cmd));
        return DI::DIError::Security;
    }
}

/*
 * IOCTLV for the emulated disc drive.
 * block: Input DVD command
 * cmd: IOCTL number
 * inCount: Input vector count
 * ioCount: Output buffer count
 * vec: I/O vectors
 */
static DI::DIError EmuIoctlv(DVDCommand* block, DI::DIIoctl cmd, u32 inCount,
  u32 ioCount, IOS::Vector* vec)
{
    assert(disc != nullptr);

    switch (cmd) {
    case DI::DIIoctl::OpenPartition: {
        PRINT(IOS_EmuDI, INFO, "Open partition!");
        if (inCount != 3 || ioCount != 2) {
            PRINT(IOS_EmuDI, ERROR, "Invalid I/O vector count");
            return DI::DIError::Security;
        }

        [[maybe_unused]] const ES::Ticket* inTicket = nullptr;
        [[maybe_unused]] bool useInTicket = false;
        [[maybe_unused]] const void* inCerts = nullptr;
        [[maybe_unused]] u32 inCertsLen = 0;

        if (vec[1].len != 0) {
            if (vec[1].len < sizeof(ES::Ticket)) {
                PRINT(
                  IOS_EmuDI, ERROR, "Input ticket vector size is too short");
                return DI::DIError::Security;
            }

            inTicket = reinterpret_cast<ES::Ticket*>(vec[1].data);
            useInTicket = true;
        }

        if (vec[2].len != 0) {
            inCerts = vec[2].data;
            inCertsLen = vec[2].len;
        }

        if (vec[3].len < sizeof(ES::TMDFixed<512>)) {
            PRINT(IOS_EmuDI, ERROR, "Output TMD vector size is too short");
            return DI::DIError::Security;
        }

        auto outTmd = reinterpret_cast<ES::TMDFixed<512>*>(vec[3].data);

        if (vec[4].len < sizeof(ES::ESError)) {
            PRINT(IOS_EmuDI, ERROR, "Output ES error vector size is too short");
            return DI::DIError::Security;
        }

        [[maybe_unused]] auto esError =
          reinterpret_cast<ES::ESError*>(vec[4].data);

        PRINT(IOS_EmuDI, INFO, "All open partition params correct");
        return disc->OpenPartition(block->args[0], outTmd);
    }

    default:
        PRINT(IOS_EmuDI, ERROR, "Unknown ioctlv 0x%02X", static_cast<u32>(cmd));
        return DI::DIError::Security;
    }
}

/*
 * Open a patch file by its identifier.
 * fp: (out) FATFS file pointer
 * patch: (in) DVD patch to open
 */
static void OpenPatchFile(FIL* fp, DVDPatch* patch)
{
    memset(fp, 0, sizeof(FIL));

    // Get FATFS object
    auto devId = DeviceMgr::sInstance->DRVToDevID(patch->drv);
    auto fatfs = DeviceMgr::sInstance->GetFilesystem(devId);

    fp->obj.fs = fatfs;
    fp->obj.id = fatfs->id;
    fp->obj.sclust = patch->start_cluster;
    fp->obj.objsize = 0xFFFFFFFF;
    fp->flag = FA_READ;
    fp->fptr = patch->file_offset;
    fp->clust = patch->cur_cluster;
}

static inline u32 SearchPatch(u32 offset)
{
    for (s32 j = 0, i = DiNumPatches; i != 0; i >>= 1) {
        s32 k = j + (i >> 1);
        u32 p_start = DiPatches[k].disc_offset;
        u32 p_end = p_start + DiPatches[k].disc_length;

        if (p_start == offset)
            return k;
        if (offset > p_start) {
            if (p_end > offset)
                return k;
            /* Move right */
            j = k + 1;
            i--;
        }
        /* offset < p_start */
    }
    return DiNumPatches;
}

static s32 RealRead(void* outbuf, u32 offset, u32 length)
{
    DVDCommand rblock;
    rblock.cmd = static_cast<u8>(DI::DIIoctl::Read);
    rblock.args[0] = length;
    rblock.args[1] = offset;

    if (useVirtualDisc) {
        return static_cast<s32>(
          EmuIoctl(&rblock, DI::DIIoctl::Read, outbuf, length));
    }

    return static_cast<s32>(DI::sInstance->Read(outbuf, length, offset));
}

static inline bool IsPatchedOffset(u32 wordOffset)
{
    return wordOffset & 0x80000000;
}

static s32 Read(u8* outbuf, u32 offset, u32 length)
{
    if (!IsPatchedOffset(offset)) {
        if (!IsPatchedOffset(offset + (length >> 2) - 1)) {
            /* Not patched read, forward to real DI */
            return RealRead(outbuf, offset, length);
        }
        /*
         * Part DVD read, part SD read
         */
        const s32 ret = RealRead(outbuf, offset, (0x80000000 - offset) << 2);
        if (ret != DI_EOK) {
            PRINT(IOS_EmuDI, ERROR, "DI_Read: Partial read failed: %d", ret);
            /* If it fails, just memset 0 the output buffer */
            memset(outbuf, 0, (0x80000000 - offset) << 2);
        }

        outbuf += (0x80000000 - offset) << 2;
        length -= (0x80000000 - offset) << 2;
    }

    for (u32 idx = SearchPatch(offset); length != 0; idx++) {
        PRINT(
          IOS_EmuDI, INFO, "DI_Read: Read patch %d of %d", idx, DiNumPatches);
        if (idx >= DiNumPatches) {
            PRINT(IOS_EmuDI, WARN, "DI_Read: Out of bounds DVD read");
            memset(outbuf, 0, length);
            return DI_EOK; // Just success, I guess?
        }

        FIL f;
        OpenPatchFile(&f, &DiPatches[idx]);

        u32 read_len = DiPatches[idx].disc_length << 2;
        if (DiPatches[idx].disc_offset != offset) {
            const FRESULT fret =
              f_lseek(&f, (offset - DiPatches[idx].disc_offset) << 2);
            if (fret != FR_OK) {
                PRINT(IOS_EmuDI, ERROR, "DI_Read: FS_LSeek failed: %d", fret);
                abort();
            }
            read_len -= (offset - DiPatches[idx].disc_offset) << 2;
        }

        if (read_len > length)
            read_len = length;
        UINT read = 0;
        PRINT(IOS_EmuDI, INFO, "doing read!");
        const FRESULT fret = f_read(&f, outbuf, read_len, &read);
        if (fret != FR_OK) {
            PRINT(IOS_EmuDI, ERROR, "DI_Read: FS_Read failed: %d", fret);
            memset(outbuf + read, 0, read_len - read);
        }

        outbuf += read_len;
        length -= read_len;
        offset += read_len >> 2;
    }

    return DI_EOK;
}

/*
 * Handle DI IOCTLs for patched games, returning false forwards the command to
 * the actual disc image.
 */
static bool DI_DoNewIOCTL(IOSRequest* req)
{
    switch (static_cast<DI::DIIoctl>(req->ioctl.cmd)) {
    case DI::DIIoctl::Read: {
        /* [TODO] Check partition */
        if (req->ioctl.in_len != sizeof(DVDCommand)) {
            IOS_ResourceReply(req, DI_ESECURITY);
            return true;
        }
        DVDCommand* block = reinterpret_cast<DVDCommand*>(req->ioctl.in);
        if (block->cmd != static_cast<u8>(DI::DIIoctl::Read)) {
            IOS_ResourceReply(req, DI_EBADARGUMENT);
            return true;
        }

        u8* outbuf = reinterpret_cast<u8*>(req->ioctl.io);
        u32 offset = block->args[1];
        u32 length = block->args[0];
        if (length > req->ioctl.io_len) {
            PRINT(IOS_EmuDI, ERROR,
              "DI_IOCTL_READ: Output size < read length (0x%X, 0x%x)", length,
              req->ioctl.io_len);
            IOS_ResourceReply(req, DI_ESECURITY);
            return true;
        }
        IOS_ResourceReply(req, Read(outbuf, offset, length & ~3));
        return true;
    }

    default:
        break;
    }

    switch (req->ioctl.cmd) {
    case DI_PROXY_IOCTL_PATCHDVD: {
        /*
         * Assuming patches are valid, this could only
         * be called from secure code
         */
        if (GameStarted)
            return false;
        if (req->ioctl.in_len == 0) {
            IOS_ResourceReply(req, IOS_EINVAL);
            return true;
        }

        DiNumPatches = req->ioctl.in_len / sizeof(DVDPatch);
        if (req->ioctl.in_len > sizeof(DiPatches)) {
            PRINT(IOS_EmuDI, ERROR,
              "DI_PROXY_IOCTL_PATCHDVD: "
              "Not enough memory for DVD patches");
            IOS_ResourceReply(req, IOS_ENOMEM);
            return true;
        }
        memcpy(DiPatches, req->ioctl.in, req->ioctl.in_len);
        IOS_ResourceReply(req, IOS_SUCCESS);
        return true;
    }

    case DI_PROXY_IOCTL_STARTGAME: {
        if (GameStarted)
            return false;
        PRINT(IOS_EmuDI, WARN, "DI_PROXY_IOCTL_STARTGAME: Starting game...");
        GameStarted = true;
        IOS_ResourceReply(req, IOS_SUCCESS);
        return true;
    }

    default:
        break;
    }

    return false;
}

static inline void ReqOpen(IOSRequest* req)
{
    if (strcmp(req->open.path, "~dev/di") != 0) {
        IOS_ResourceReply(req, IOSError::NotFound);
        return;
    }

    IOS_ResourceReply(req, 0);
}

static inline void ReqClose(IOSRequest* req)
{
    const s32 ret = IOS_Close(req->handle);
    IOS_ResourceReply(req, ret);
}

static inline void ReqIoctl(IOSRequest* req)
{
    if (DI_DoNewIOCTL(req))
        return;

    // If DoNewIOCTL returns false, forward to real DI

    if (useVirtualDisc) {
        // DI emulation
        if (req->ioctl.in_len < sizeof(DVDCommand)) {
            PRINT(IOS_EmuDI, ERROR, "Wrong input command block size");
            IOS_ResourceReply(req, static_cast<s32>(DI::DIError::Security));
            return;
        }

        auto reply = EmuIoctl(reinterpret_cast<DVDCommand*>(req->ioctl.in),
          static_cast<DI::DIIoctl>(req->ioctl.cmd), req->ioctl.io,
          req->ioctl.io_len);
        IOS_ResourceReply(req, static_cast<s32>(reply));
        return;
    }

    // Real drive
    const s32 ret = IOS_Ioctl(DI::sInstance->GetFd(), req->ioctl.cmd,
      req->ioctl.in, req->ioctl.in_len, req->ioctl.io, req->ioctl.io_len);
    IOS_ResourceReply(req, ret);
}

static inline void ReqIoctlv(IOSRequest* req)
{
    // Probably won't be replacing any IOCTLVs

    if (useVirtualDisc) {
        // DI emulation
        if (req->ioctlv.in_count < 1 ||
            req->ioctlv.vec[0].len < sizeof(DVDCommand)) {
            PRINT(IOS_EmuDI, ERROR, "Wrong input command block size");
            IOS_ResourceReply(req, static_cast<s32>(DI::DIError::Security));
            return;
        }

        auto reply =
          EmuIoctlv(reinterpret_cast<DVDCommand*>(req->ioctlv.vec[0].data),
            static_cast<DI::DIIoctl>(req->ioctlv.cmd), req->ioctlv.in_count,
            req->ioctlv.io_count, req->ioctlv.vec);
        IOS_ResourceReply(req, static_cast<s32>(reply));
        return;
    }

    // Real drive
    const s32 ret = IOS_Ioctlv(DI::sInstance->GetFd(), req->ioctlv.cmd,
      req->ioctlv.in_count, req->ioctlv.io_count, req->ioctlv.vec);
    IOS_ResourceReply(req, ret);
}

void HandleRequest(IOSRequest* req)
{
    switch (req->cmd) {
    case IOS_OPEN:
        ReqOpen(req);
        break;
    case IOS_CLOSE:
        ReqClose(req);
        break;
    case IOS_IOCTL:
        ReqIoctl(req);
        break;
    case IOS_IOCTLV:
        ReqIoctlv(req);
        break;

    /* Reply from forwarded commands */
    case IOS_IPC_REPLY:
        IOS_ResourceReply(req, req->result);
        break;

    default:
        PRINT(IOS_EmuDI, ERROR, "Received unhandled command: %d", req->cmd);
        /* Real DI just... does not reply to unknown commands? [check] */
        break;
    }
}

s32 ThreadEntry([[maybe_unused]] void* arg)
{
    PRINT(IOS_EmuDI, INFO, "Starting DI...");
    PRINT(IOS_EmuDI, INFO, "EmuDI thread ID: %d", IOS_GetThreadId());

    disc = new ISO("0:/xaa", "0:/xab");
    useVirtualDisc = true;

    s32 ret = IOS_CreateMessageQueue(__diMsgData, 8);
    if (ret < 0) {
        PRINT(IOS_EmuDI, ERROR,
          "DI_ThreadEntry: IOS_CreateMessageQueue failed: %d", ret);
        abort();
    }
    DiMsgQueue = ret;

    ret = IOS_RegisterResourceManager("~dev/di", DiMsgQueue);
    if (ret != IOS_SUCCESS) {
        PRINT(IOS_EmuDI, ERROR,
          "DI_ThreadEntry: IOS_RegisterResourceManager failed: %d", ret);
        abort();
    }

    PRINT(IOS_EmuDI, INFO, "DI started");

    DiStarted = true;
    IPCLog::sInstance->Notify();
    while (1) {
        IOSRequest* req;
        ret = IOS_ReceiveMessage(DiMsgQueue, (u32*) &req, 0);
        if (ret != IOS_SUCCESS) {
            PRINT(IOS_EmuDI, ERROR,
              "DI_ThreadEntry: IOS_ReceiveMessage failed: %d", ret);
            abort();
        }

        HandleRequest(req);
    }
    return 0;
}

} // namespace EmuDI
