#include "DI.h"
#include "main.h"
#include "log.h"
#include <ios.h>
#include <string.h>
#include <stdbool.h>

#define DI_PROXY_PATH "/dev/di/proxy"

static s32 DiMsgQueue = -1;
static u32 __diMsgData[8];
static bool DiStarted = false;


/* Handle patched DI IOCTLs, returns false to forward to real DI */
bool DI_DoNewIOCTL(IOSRequest* req)
{
    // no requests handled yet
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

static u32 DiStack[0x1000 / 4];

void DI_CreateThread()
{
    const s32 tid = IOS_CreateThread(DI_ThreadEntry, NULL,
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