// EmuDI.cpp - Emulated DI RM
//   Written by Palapeli
//
// Copyright (C) 2022 Team Saoirse
// SPDX-License-Identifier: MIT

#include "EmuDI.hpp"
#include <DVD/EmuDI.hpp>
#include <Debug/Log.hpp>
#include <IOS/DeviceMgr.hpp>
#include <IOS/IPCLog.hpp>
#include <IOS/Syscalls.h>
#include <IOS/System.hpp>
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

#define DI_IOCTL_READ 0x71
#define DI_PROXY_IOCTL_PATCHDVD 0x00
#define DI_PROXY_IOCTL_STARTGAME 0x01

#define DI_EOK 0x1
#define DI_ESECURITY 0x20
#define DI_EBADARGUMENT 0x80

static void OpenPatchFile(FIL* fp, DVDPatch* patch)
{
    memset(fp, 0, sizeof(FIL));

    // Get FATFS object
    auto device = DeviceMgr::DRVToDeviceKind(patch->drv);
    auto fatfs = DeviceMgr::sInstance->GetFilesystem(device);

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

static s32 RealRead(s32 fd, void* outbuf, u32 offset, u32 length)
{
    DVDCommand rblock;
    rblock.cmd = DI_IOCTL_READ;
    rblock.args[0] = length;
    rblock.args[1] = offset;
    return IOS_Ioctl(fd, DI_IOCTL_READ, &rblock, sizeof(DVDCommand), outbuf,
                     length);
}

static inline bool IsPatchedOffset(u32 wordOffset)
{
    return wordOffset & 0x80000000;
}

static s32 Read(s32 handle, u8* outbuf, u32 offset, u32 length)
{
    if (!IsPatchedOffset(offset)) {
        if (!IsPatchedOffset(offset + (length >> 2) - 1)) {
            /* Not patched read, forward to real DI */
            return RealRead(handle, outbuf, offset, length);
        }
        /*
         * Part DVD read, part SD read
         */
        const s32 ret =
            RealRead(handle, outbuf, offset, (0x80000000 - offset) << 2);
        if (ret != DI_EOK) {
            PRINT(IOS_EmuDI, ERROR, "DI_Read: Partial read failed: %d", ret);
            /* If it fails, just memset 0 the output buffer */
            memset(outbuf, 0, (0x80000000 - offset) << 2);
        }

        outbuf += (0x80000000 - offset) << 2;
        length -= (0x80000000 - offset) << 2;
    }

    for (u32 idx = SearchPatch(offset); length != 0; idx++) {
        PRINT(IOS_EmuDI, INFO, "DI_Read: Read patch %d of %d", idx,
              DiNumPatches);
        if (idx >= DiNumPatches) {
            PRINT(IOS_EmuDI, WARN, "DI_Read: Out of bounds DVD read");
            memset(outbuf, 0, length);
            return DI_EOK; /* Just success, I guess? */
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

/* Handle patched DI IOCTLs, returns false to forward to real DI */
static bool DI_DoNewIOCTL(IOSRequest* req)
{
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

    case DI_IOCTL_READ: {
        /* [TODO] Check partition */
        if (req->ioctl.in_len != sizeof(DVDCommand)) {
            IOS_ResourceReply(req, DI_ESECURITY);
            return true;
        }
        DVDCommand* block = reinterpret_cast<DVDCommand*>(req->ioctl.in);
        if (block->cmd != DI_IOCTL_READ) {
            IOS_ResourceReply(req, DI_EBADARGUMENT);
            return true;
        }

        u8* outbuf = reinterpret_cast<u8*>(req->ioctl.io);
        u32 offset = block->args[1];
        u32 length = block->args[0];
        if (length > req->ioctl.io_len) {
            PRINT(IOS_EmuDI, ERROR,
                  "DI_IOCTL_READ: Output size < read length (0x%X, 0x%x)",
                  length, req->ioctl.io_len);
            IOS_ResourceReply(req, DI_ESECURITY);
            return true;
        }
        IOS_ResourceReply(req, Read(req->handle, outbuf, offset, length & ~3));
        return true;
    }
    }

    return false;
}

static inline void ReqOpen(IOSRequest* req)
{
    s32 ret = IOS_ENOENT;
    if (!strcmp(req->open.path, "~dev/di")) {
        ret = IOS_Open("/dev/di", req->open.mode);
    }
    IOS_ResourceReply(req, ret);
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

    /* If DoNewIOCTL returns false, forward to real DI */
    const s32 ret =
        IOS_Ioctl(req->handle, req->ioctl.cmd, req->ioctl.in, req->ioctl.in_len,
                  req->ioctl.io, req->ioctl.io_len);
    IOS_ResourceReply(req, ret);
}

static inline void ReqIoctlv(IOSRequest* req)
{
    /* Probably won't be replacing any IOCTLVs */
    const s32 ret =
        IOS_Ioctlv(req->handle, req->ioctlv.cmd, req->ioctlv.in_count,
                   req->ioctlv.io_count, req->ioctlv.vec);
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
        ret = IOS_ReceiveMessage(DiMsgQueue, (u32*)&req, 0);
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