#include "DI.h"
#include "FS.h"
#include "main.h"
#include "log.h"
#include <ios.h>
#include <string.h>
#include <stdbool.h>

typedef struct
{
    u32 disc_offset;
    u32 disc_length;

    /* file info */
    u8 drv;
    DWORD start_cluster;
    DWORD cur_cluster;
    u32 file_offset;
} DVDPatch;

typedef struct
{
    u8 cmd;
    u8 pad[3];
    u32 args[7];
} DVDCommand;

#define DI_PROXY_PATH "/dev/di/proxy"

static s32 DiMsgQueue = -1;
static u32 __diMsgData[8];
static bool DiStarted = false;
static bool GameStarted = false;

static DVDPatch* DiPatches = NULL;
static u32 DiNumPatches = 0;

#define DI_IOCTL_READ 0x71
#define DI_PROXY_IOCTL_PATCHDVD 0x00
#define DI_PROXY_IOCTL_STARTGAME 0x01

#define DI_EOK 0x1
#define DI_ESECURITY 0x20
#define DI_EBADARGUMENT 0x80


static
void DI_OpenPatchFile(FIL* fp, DVDPatch* patch)
{
    memset(fp, 0, sizeof(FIL));
    fp->obj.fs = NULL; // automatically filled by FS
    fp->obj.id = fatfs.id;
    fp->obj.sclust = patch->start_cluster;
    fp->obj.objsize = 0xFFFFFFFF;
    fp->flag = FA_READ;
    fp->fptr = patch->file_offset;
    fp->clust = patch->cur_cluster;
}

static inline
u32 DI_SearchPatch(u32 offset)
{
    for (s32 j = 0, i = DiNumPatches; i != 0; i >>= 1)
    {
        s32 k = j + (i >> 1);
        u32 p_start = DiPatches[k].disc_offset;
        u32 p_end = p_start + DiPatches[k].disc_length;

        if (p_start == offset)
            return k;
        if (offset > p_start) {
            if (p_end > offset)
                return k;
            /* Move right */
            j = k + 1; i--;
        }
        /* offset < p_start */
    }
    return DiNumPatches;
}

static
s32 DI_RealRead(s32 fd, void* outbuf, u32 offset, u32 length)
{
    DVDCommand rblock;
    rblock.cmd = DI_IOCTL_READ;
    rblock.args[0] = length;
    rblock.args[1] = offset;
    return IOS_Ioctl(fd, DI_IOCTL_READ,
        &rblock, sizeof(DVDCommand), outbuf, length);
}

static
s32 DI_Read(s32 handle, u8* outbuf, u32 offset, u32 length)
{
    if (!(offset & 0x80000000))
    {
        if (!((offset + (length >> 2) - 1) & 0x80000000)) {
            /* Not patched read, forward to real DI */
            return DI_RealRead(handle, outbuf, offset, length);
        }
        /* 
         * Part DVD read, part SD read
         * [TODO] see if this is actually needed, I'm assuming it's important
         * for dual layer discs?
         */
        const s32 ret = DI_RealRead(handle, outbuf, offset,
            (0x80000000 - offset) << 2);
        if (ret != DI_EOK) {
            printf(ERROR, "DI_Read: Partial read failed: %d", ret);
            /* If it fails, just memset 0 the output buffer */
            memset(outbuf, 0, (0x80000000 - offset) << 2);
        }

        outbuf += (0x80000000 - offset) << 2;
        length -= (0x80000000 - offset) << 2;
    }

    for (u32 idx = DI_SearchPatch(offset); length != 0; idx++)
    {
        printf(INFO, "DI_Read: Read patch %d of %d", idx, DiNumPatches);
        if (idx >= DiNumPatches) {
            printf(WARN, "DI_Read: Out of bounds DVD read");
            memset(outbuf, 0, length);
            return DI_EOK; /* Just success, I guess? */
        }
    
        FIL f;
        DI_OpenPatchFile(&f, &DiPatches[idx]);

        u32 read_len = DiPatches[idx].disc_length << 2;
        if (DiPatches[idx].disc_offset != offset) {
            const FRESULT fret = FS_LSeek(&f,
                (offset - DiPatches[idx].disc_offset) << 2);
            if (fret != FR_OK) {
                printf(ERROR, "DI_Read: FS_LSeek failed: %d", fret);
                abort();
            }
            read_len -= (offset - DiPatches[idx].disc_offset) << 2;
        }
        
        if (read_len > length)
            read_len = length;
        u32 read = 0;
        const FRESULT fret = FS_Read(&f, outbuf, read_len, &read);
        if (fret != FR_OK) {
            printf(ERROR, "DI_Read: FS_Read failed: %d", fret);
            memset(outbuf + read, 0, read_len - read);
        }

        outbuf += read_len;
        length -= read_len;
        offset += read_len >> 2;
    }

    return DI_EOK;
}

/* Handle patched DI IOCTLs, returns false to forward to real DI */
static
bool DI_DoNewIOCTL(IOSRequest* req)
{
    switch (req->ioctl.cmd)
    {
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
        
            DiPatches = IOS_Alloc(heap, req->ioctl.in_len);
            if (DiPatches == NULL) {
                printf(ERROR, "DI_PROXY_IOCTL_PATCHDVD: "
                    "Could not allocate memory for DVD patches");
                IOS_ResourceReply(req, IOS_ENOMEM);
                return true;
            }
            DiNumPatches = req->ioctl.in_len / sizeof(DVDPatch);
            memcpy(DiPatches, req->ioctl.in, req->ioctl.in_len);
            IOS_ResourceReply(req, IOS_SUCCESS);
            return true;
        }
        
        case DI_PROXY_IOCTL_STARTGAME: {
            if (GameStarted)
                return false;
            printf(WARN, "DI_PROXY_IOCTL_STARTGAME: Starting game...");
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
            DVDCommand* block = req->ioctl.in;
            if (block->cmd != DI_IOCTL_READ) {
                IOS_ResourceReply(req, DI_EBADARGUMENT);
                return true;
            }

            u8* outbuf = req->ioctl.io;
            u32 offset = block->args[1];
            u32 length = block->args[0];
            if (length > req->ioctl.io_len) {
                printf(ERROR,
                    "DI_IOCTL_READ: Output size < read length (0x%X, 0x%x)",
                    length, req->ioctl.io_len);
                IOS_ResourceReply(req, DI_ESECURITY);
                return true;
            }
            IOS_ResourceReply(req, 
                DI_Read(req->handle, outbuf, offset, length & ~3));
            return true;
        }
    }

    return false;
}

static inline
void DI_ReqOpen(IOSRequest* req)
{
    s32 ret = IOS_ENOENT;
    if (!strcmp(req->open.path, DI_PROXY_PATH))
        ret = IOS_Open("/dev/di", req->open.mode);
    IOS_ResourceReply(req, ret);
}

static inline
void DI_ReqClose(IOSRequest* req)
{
    const s32 ret = IOS_Close(req->handle);
    IOS_ResourceReply(req, ret);
}

static inline
void DI_ReqIoctl(IOSRequest* req)
{
    if (DI_DoNewIOCTL(req))
        return;
    
    /* If DoNewIOCTL returns false, forward to real DI */
    const s32 ret = IOS_IoctlAsync(req->handle, req->ioctl.cmd,
        req->ioctl.in, req->ioctl.in_len,
        req->ioctl.io, req->ioctl.io_len, DiMsgQueue, req);
    if (ret != IOS_SUCCESS) {
        printf(ERROR, "IOS_Ioctl(0x%02X) forward failed: %d",
            req->ioctl.cmd, ret);
        abort();
    }
}

static inline
void DI_ReqIoctlv(IOSRequest* req)
{
    /* Probably won't be replacing any IOCTLVs */
    const s32 ret = IOS_IoctlvAsync(req->handle, req->ioctlv.cmd,
        req->ioctlv.in_count, req->ioctlv.io_count, req->ioctlv.vec,
        DiMsgQueue, req);
    if (ret != IOS_SUCCESS) {
        printf(ERROR, "IOS_Ioctlv(0x%02X) forward failed: %d",
            req->ioctlv.cmd, ret);
        abort();
    }
}

void DI_HandleRequest(IOSRequest* req)
{
    switch (req->cmd)
    {
        case IOS_OPEN:
            DI_ReqOpen(req);
            break;
        case IOS_CLOSE:
            DI_ReqClose(req);
            break;
        case IOS_IOCTL:
            DI_ReqIoctl(req);
            break;
        case IOS_IOCTLV:
            DI_ReqIoctlv(req);
            break;
        
        /* Reply from forwarded commands */
        case IOS_IPC_REPLY:
            IOS_ResourceReply(req, req->result);
            break;

        default:
            printf(ERROR, "Received unhandled command: %d", req->cmd);
            /* Real DI just... does not reply to unknown commands? [check] */
            break;
    }
}

s32 DI_ThreadEntry(void* arg)
{
    printf(INFO, "Starting DI...");

    s32 ret = IOS_CreateMessageQueue(__diMsgData, 8);
    if (ret < 0) {
        printf(ERROR, "DI_ThreadEntry: IOS_CreateMessageQueue failed: %d", ret);
        abort();
    }
    DiMsgQueue = ret;

    ret = IOS_RegisterResourceManager(DI_PROXY_PATH, DiMsgQueue);
    if (ret != IOS_SUCCESS) {
        printf(ERROR,
            "DI_ThreadEntry: IOS_RegisterResourceManager failed: %d", ret);
        abort();
    }

    s32 replyQueue = (s32) arg;
    IOS_SendMessage(replyQueue, 0, 0);

    DiStarted = true;
    while (1) {
        IOSRequest* req;
        ret = IOS_ReceiveMessage(DiMsgQueue, (u32*) &req, 0);
        if (ret != IOS_SUCCESS) {
            printf(ERROR, "DI_ThreadEntry: IOS_ReceiveMessage failed: %d", ret);
            abort();
        }

        DI_HandleRequest(req);
    }
    return 0;
}

static u32 DiStack[0x400 / 4];

void DI_Init(s32 replyQueue)
{
    const s32 tid = IOS_CreateThread(DI_ThreadEntry, (void*) replyQueue,
        DiStack + sizeof(DiStack) / 4, sizeof(DiStack), 100, true);
    if (tid < 0) {
        printf(ERROR, "Failed to make DI thread: %d", tid);
        abort();
    }

    const s32 ret = IOS_StartThread(tid);
    if (ret != IOS_SUCCESS) {
        printf(ERROR, "Failed to start DI thread: %d", ret);
        abort();
    }
}