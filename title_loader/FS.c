#include "FS.h"
#include <ios.h>
#include <ff.h>
#include <diskio.h>
#include <types.h>
#include "wiisd.h"
#include <log.h>
#include <main.h>
#include <string.h>

#define DRV_SDCARD 0

#define DEVICE_STORAGE_PATH "/dev/storage"

static FATFS fatfs;
static s32 FsQueue;
static u32 __FsQueueData[8];
static bool FsStarted = false;

/* ---------->
 * FatFS Disk I/O Support
 * <---------- */

DSTATUS disk_status(BYTE pdrv)
{
    if (pdrv == DRV_SDCARD)
    {
        if (!sdio_IsInitialized() || !sdio_IsInserted())
            return STA_NODISK;
    }
    return STA_NOINIT;
}

DSTATUS disk_initialize(BYTE pdrv)
{
    if (pdrv == DRV_SDCARD)
    {
        if (!sdio_Startup()) {
            /* No way to differentiate between error and not inserted */
            printf(WARN, "disk_initialize: sdio_Startup returned false");
            return STA_NODISK;
        }
        return 0;
    } else {
        printf(ERROR, "disk_initialize: unknown pdrv (%d)", pdrv);
    }
    return STA_NOINIT;
}

DRESULT disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count)
{
    if (pdrv == DRV_SDCARD)
    {
        if (disk_status(pdrv) != 0)
            return RES_ERROR;
        if (!sdio_ReadSectors(sector, count, buff)) {
            printf(ERROR, "disk_read: sdio_ReadSectors failed");
            return RES_ERROR;
        }
        return RES_OK;
    }
    return STA_NOINIT;
}

DRESULT disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count)
{
    if (pdrv == DRV_SDCARD)
    {
        if (disk_status(pdrv) != 0)
            return RES_ERROR;
        if (!sdio_WriteSectors(sector, count, buff)) {
            printf(ERROR, "disk_write: sdio_WriteSectors failed");
            return RES_ERROR;
        }
        return RES_OK;
    }
    return STA_NOINIT;
}

/* todo */
DWORD get_fattime()
{
    return 0;
}


/* ---------->
 * IPC Filesystem Interface
 * <---------- */

static s32 FS_ReqOpen(IOSRequest* req)
{
    if (strcmp(req->open.path, DEVICE_STORAGE_PATH))
        return IOS_ENOENT;
    if (req->open.mode != IOS_OPEN_NONE)
        return IOS_EINVAL;
    return 0;
}

static s32 FS_ReqClose(IOSRequest* req)
{
    return 0;
}

static s32 FS_ReqIoctl(IOSRequest* req)
{
    switch (req->ioctl.cmd)
    {
        // todo
    }
}

static s32 FS_IPCRequest(IOSRequest* req)
{
    switch (req->cmd)
    {
        case IOS_OPEN: return FS_ReqOpen(req);
        case IOS_CLOSE: return FS_ReqClose(req);
        case IOS_IOCTL: return FS_ReqIoctl(req);
        default:
            printf(ERROR,
                "FS_IPCRequest: Received unhandled command: %d", req->cmd);
            return IOS_EINVAL;
    }
}

static s32 FS_StartRM(void* arg)
{
    s32 ret = IOS_CreateMessageQueue(__FsQueueData, 8);
    if (ret < 0) {
        printf(ERROR, "FS_StartRM: IOS_CreateMessageQueue failed: %d", ret);
        abort();
    }
    FsQueue = ret;

    ret = IOS_RegisterResourceManager(DEVICE_STORAGE_PATH, FsQueue);
    if (ret != IOS_SUCCESS) {
        printf(ERROR,
            "FS_StartRM: IOS_RegisterResourceManager failed: %d", ret);
        abort();
    }

    FsStarted = true;
    while (true) {
        IOSRequest* req;
        ret = IOS_ReceiveMessage(FsQueue, (u32*) &req, 0);
        if (ret != IOS_SUCCESS) {
            printf(ERROR, "FS_StartRM: IOS_ReceiveMessage failed: %d", ret);
            abort();
        }

        ret = FS_IPCRequest(req);
        IOS_ResourceReply(req, ret);
    }
    return 0;
}

void FS_Init()
{
    if (!sdio_Open()) {
        printf(ERROR, "FS_Init: sdio_Open returned false");
        abort();
    }

    /* Mount SD Card */
    FRESULT fret = f_mount(&fatfs, "0:", 0);
    if (fret != FR_OK) {
        /* FatFS shouldn't try to initialize the drive yet, so there will be no
         * "not inserted" error */
        printf(ERROR, "FS_Init: f_mount SD Card failed: %d", fret);
    }

    /* Should maybe create a thread */
    FS_StartRM(NULL);
}