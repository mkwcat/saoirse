#include "FS.h"
#include <ios.h>
#include <ff.h>
#include <diskio.h>
#include <types.h>
#include "wiisd.h"
#include <log.h>
#include <main.h>
#include <string.h>

/* <----------
 * FatFS Disk I/O Support
 * ----------> */

#define DRV_SDCARD 0

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

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff)
{
    switch (cmd)
    {
        case CTRL_SYNC:
            if (pdrv == DRV_SDCARD) {
                /* /dev/sdio must handle the AHB buffers itself */
                return RES_OK;
            }
            return RES_NOTRDY;
        
        case GET_SECTOR_SIZE:
            if (pdrv == DRV_SDCARD) {
                /* Always 512 */
                *(WORD*) buff = 512;
                return RES_OK;
            }
            return RES_NOTRDY;
        
        default:
            printf(ERROR, "disk_ioctl: unknown command: %d", cmd);
            return RES_PARERR;
    }
}

/* todo */
DWORD get_fattime()
{
    return 0;
}


/* <----------
 * IPC Filesystem Interface
 * ----------> */

#define FS_MAX_PATH 1024

#define DEVICE_STORAGE_PATH "/dev/storage"

#define IOCTL_FOPEN      1
#define IOCTL_FCLOSE     2
#define IOCTL_FREAD      3
#define IOCTL_FWRITE     4
#define IOCTL_FLSEEK     5
#define IOCTL_FTRUNCATE  6
#define IOCTL_FSYNC      7
#define IOCTL_FTELL      8
#define IOCTL_FEOF       9
#define IOCTL_FSIZE      10
#define IOCTL_FERROR     11

static s32 FsQueue;
static u32 __FsQueueData[8];
static bool FsStarted = false;

static inline
void FS_BeginFile(const void* input, FIL* fp)
{
    ASSERT(fp);
    memcpy((void*) fp, input, sizeof(FIL));
}

static inline
void FS_ReplyFile(void* output, FIL* fp)
{
    ASSERT(fp);
    memcpy(output, (void*) fp, sizeof(FIL));
}

static
s32 FS_FileCommand(IOSRequest* req)
{
    if (req->ioctlv.in_count < 1 || req->ioctlv.vec[0].len != sizeof(FIL)) {
        printf(ERROR, "FS_FileCommand: Invalid input");
        return IOS_EINVAL;
    }
    if ((u32) req->ioctlv.vec[0].data & 3) {
        printf(ERROR, "FS_FileCommand: FIL must be aligned to 4 byte boundary");
        return IOS_EINVAL;
    }
    FIL fp;
    FS_BeginFile(req->ioctlv.vec[0].data, &fp);

    switch (req->ioctl.cmd)
    {
        case IOCTL_FCLOSE:
            if (req->ioctlv.in_count != 1 && req->ioctlv.io_count != 0)
                return IOS_EINVAL;
            return f_close(&fp);
        
        case IOCTL_FREAD:
            if (req->ioctlv.in_count != 1 && req->ioctlv.io_count != 1)
                return IOS_EINVAL;
            UINT read;
            f_read(&fp, req->ioctlv.vec[1].data, req->ioctlv.vec[1].len, &read);
            return (s32) read;
        
        case IOCTL_FWRITE:
            if (req->ioctlv.in_count != 2 && req->ioctlv.io_count != 0)
                return IOS_EINVAL;
            UINT wrote;
            f_write(&fp,
                req->ioctlv.vec[1].data, req->ioctlv.vec[1].len, &wrote);
            return (s32) wrote;
        
        case IOCTL_FLSEEK:
            if (req->ioctlv.in_count != 2 && req->ioctlv.io_count != 0)
                return IOS_EINVAL;
            if (req->ioctlv.vec[1].len != sizeof(u32))
                return IOS_EINVAL;
            return f_lseek(&fp, *(FSIZE_t*) req->ioctlv.vec[1].data);
        
        case IOCTL_FTRUNCATE:
            if (req->ioctlv.in_count != 1 && req->ioctlv.io_count != 0)
                return IOS_EINVAL;
            return f_truncate(&fp);
        
        case IOCTL_FSYNC:
            if (req->ioctlv.in_count != 1 && req->ioctlv.io_count != 0)
                return IOS_EINVAL;
            return f_sync(&fp);
        
        case IOCTL_FTELL:
            if (req->ioctlv.in_count != 1 && req->ioctlv.io_count != 0)
                return IOS_EINVAL;
            return f_tell(&fp);
        
        case IOCTL_FEOF:
            if (req->ioctlv.in_count != 1 && req->ioctlv.io_count != 0)
                return IOS_EINVAL;
            return f_eof(&fp);
        
        case IOCTL_FSIZE:
            if (req->ioctlv.in_count != 1 && req->ioctlv.io_count != 0)
                return IOS_EINVAL;
            return f_size(&fp);
        
        case IOCTL_FERROR:
            if (req->ioctlv.in_count != 1 && req->ioctlv.io_count != 0)
                return IOS_EINVAL;
            return f_error(&fp);
    }

    return IOS_EINVAL;
}

static
s32 FS_ReqOpen(IOSRequest* req)
{
    if (strcmp(req->open.path, DEVICE_STORAGE_PATH))
        return IOS_ENOENT;
    if (req->open.mode != IOS_OPEN_NONE)
        return IOS_EINVAL;
    return 0;
}

static
s32 FS_ReqClose(IOSRequest* req)
{
    return 0;
}

static
s32 FS_ReqIoctlv(IOSRequest* req)
{
    switch (req->ioctlv.cmd)
    {
        case IOCTL_FOPEN: {
            if (req->ioctlv.in_count != 2 && req->ioctlv.io_count != 1)
                return IOS_EINVAL;
            if (req->ioctlv.vec[0].len == 0
             || req->ioctlv.vec[0].len > FS_MAX_PATH
             || req->ioctlv.vec[1].len != sizeof(BYTE))
                return IOS_EINVAL;
            if (strnlen((char*) req->ioctlv.vec[0].data, req->ioctlv.vec[0].len)
                == req->ioctlv.vec[0].len)
                return IOS_EINVAL;
            if (req->ioctlv.vec[2].len != sizeof(FIL)
             || (u32) req->ioctlv.vec[2].data & 3)
                return IOS_EINVAL;

            FIL fp;
            memset((void*) &fp, 0, sizeof(FIL));
            /* 
             * Hmm, what if someone changes the path by the time we get here?
             * ... probably worrying about nothing, just noting this for later
             */
            FRESULT fret = f_open(&fp, (char*) req->ioctlv.vec[0].data,
                *(BYTE*) req->ioctlv.vec[1].data);
            FS_ReplyFile(req->ioctlv.vec[0].data, &fp);
            return fret;
        }

        case IOCTL_FCLOSE: case IOCTL_FREAD: case IOCTL_FWRITE:
        case IOCTL_FLSEEK: case IOCTL_FTRUNCATE: case IOCTL_FSYNC:
        case IOCTL_FTELL: case IOCTL_FEOF: case IOCTL_FSIZE:
        case IOCTL_FERROR:
            return FS_FileCommand(req);     
    }
}

static
s32 FS_IPCRequest(IOSRequest* req)
{
    switch (req->cmd)
    {
        case IOS_OPEN: return FS_ReqOpen(req);
        case IOS_CLOSE: return FS_ReqClose(req);
        case IOS_IOCTLV: return FS_ReqIoctlv(req);
        default:
            printf(ERROR,
                "FS_IPCRequest: Received unhandled command: %d", req->cmd);
            return IOS_EINVAL;
    }
}

static
s32 FS_StartRM(void* arg)
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

static FATFS fatfs;

void FS_Init()
{
    /*
     * I don't think there's a static_assert kind of thing in C,
     * but anyway, unaligned would break because of a hardware bug (and we don't
     * have a proper memcpy implementation to work around it) 
     */
    ASSERT(sizeof(FIL) & 3 == 0);

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