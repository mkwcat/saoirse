#include <ios.h>
#include "ff.h"
#include "diskio.h"
#include <types.h>
#include "wiisd.h"
#include <main.h>
#include <iosstd.h>
#include <EfsFile.h>

/* <----------
 * FatFS Disk I/O Support
 * ----------> */

#define DRV_SDCARD 0

DSTATUS disk_status(BYTE pdrv)
{
    if (pdrv == DRV_SDCARD)
    {
        if (!sdio_IsInitialized() || !sdio_IsInserted()) {
            return STA_NODISK;
        }
        return 0;
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
    }
    printf(ERROR, "disk_initialize: unknown pdrv (%d)", pdrv);
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
 * IPC Filesystem Resource Manager
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

static s32 FsQueue;
static u32 __FsQueueData[8];
static bool FsStarted = false;
FATFS fatfs;

static inline
bool FS_BeginFile(const void* input, u32 in_len, FIL* fp)
{
    ASSERT(fp);
    if (in_len != sizeof(EfsFile))
        return false;
    EfsFile* vfp = (EfsFile*) input;
    memset((void*) fp, 0, sizeof(EfsFile));

    fp->obj.fs = &fatfs;
    fp->obj.id = fatfs.id;
    fp->obj.attr = vfp->fat.obj_attr;
    fp->obj.stat = vfp->fat.obj_stat;
    fp->obj.sclust = vfp->fat.obj_sclust;
    fp->obj.objsize = vfp->fat.obj_size;
    fp->fptr = vfp->fat.fptr;
    fp->sect = vfp->fat.sect;
    fp->clust = vfp->fat.clust;
#if !FF_FS_READONLY
    fp->dir_sect = 0;
    /* Invalid pointer */
    fp->dir_ptr = (BYTE*) 0xCECECECE;
#endif
    return true;
}

static inline
bool FS_ReplyFile(void* output, u32 out_len, const FIL* fp)
{
    ASSERT(fp);
    if (out_len != sizeof(EfsFile))
        return false;
    EfsFile* vfp = (EfsFile*) output;
    memset(output, 0, sizeof(EfsFile));

    vfp->dev = 0;
    vfp->fs = 0;
    vfp->fat.obj_attr = fp->obj.attr;
    vfp->fat.obj_stat = fp->obj.stat;
    vfp->fat.obj_sclust = fp->obj.sclust;
    vfp->fat.obj_size = fp->obj.objsize;
    vfp->fat.fptr = fp->fptr;
    vfp->fat.sect = fp->sect;
    vfp->fat.clust = fp->clust;
    return true;
}

static
s32 FS_FileCommand(IOSRequest* req, FIL* fp)
{
    switch (req->ioctl.cmd)
    {
        case IOCTL_FCLOSE:
            if (req->ioctlv.in_count != 1 && req->ioctlv.io_count != 1)
                return IOS_EINVAL;
            return f_close(fp);
        
        case IOCTL_FREAD:
            if (req->ioctlv.in_count != 1 && req->ioctlv.io_count != 3)
                return IOS_EINVAL;
            if (req->ioctlv.vec[3].len != sizeof(UINT)
             || (u32) req->ioctlv.vec[3].data & 3)
                return IOS_EINVAL;
            UINT* read = (UINT*) req->ioctlv.vec[3].data;
            return f_read(fp,
                req->ioctlv.vec[2].data, req->ioctlv.vec[2].len, read);
        
        case IOCTL_FWRITE:
            if (req->ioctlv.in_count != 2 && req->ioctlv.io_count != 2)
                return IOS_EINVAL;
            if (req->ioctlv.vec[3].len != sizeof(UINT)
             || (u32) req->ioctlv.vec[3].data & 3)
                return IOS_EINVAL;
            UINT* wrote = (UINT*) req->ioctlv.vec[3].data;
            return f_write(fp,
                req->ioctlv.vec[1].data, req->ioctlv.vec[1].len, wrote);
        
        case IOCTL_FLSEEK:
            if (req->ioctlv.in_count != 2 && req->ioctlv.io_count != 1)
                return IOS_EINVAL;
            if (req->ioctlv.vec[1].len != sizeof(u32)
             || (u32) req->ioctlv.vec[1].data & 3)
                return IOS_EINVAL;
            return f_lseek(fp, *(FSIZE_t*) req->ioctlv.vec[1].data);
        
        case IOCTL_FTRUNCATE:
            if (req->ioctlv.in_count != 1 && req->ioctlv.io_count != 1)
                return IOS_EINVAL;
            return f_truncate(fp);
        
        case IOCTL_FSYNC:
            if (req->ioctlv.in_count != 1 && req->ioctlv.io_count != 1)
                return IOS_EINVAL;
            return f_sync(fp);
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
                == (s32) req->ioctlv.vec[0].len)
                return IOS_EINVAL;
            if (req->ioctlv.vec[2].len != sizeof(EfsFile)
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
            return FS_ReplyFile(req->ioctlv.vec[2].data,
                                req->ioctlv.vec[2].len, &fp)
                   ? fret : IOS_EINVAL;
        }

        case IOCTL_FCLOSE: case IOCTL_FREAD: case IOCTL_FWRITE:
        case IOCTL_FLSEEK: case IOCTL_FTRUNCATE: case IOCTL_FSYNC: {
            if (req->ioctlv.in_count < 1
             || req->ioctlv.vec[0].len != sizeof(EfsFile)) {
                printf(ERROR, "FS_FileCommand: Invalid input");
                return IOS_EINVAL;
            }
            if (req->ioctlv.io_count < 1
             || req->ioctlv.vec[req->ioctlv.in_count].len != sizeof(EfsFile)) {
                printf(ERROR, "FS_FileCommand: Invalid output");
                return IOS_EINVAL;
            }
            if ((u32) req->ioctlv.vec[0].data & 3
             || (u32) req->ioctlv.vec[req->ioctlv.in_count].data & 3) {
                printf(ERROR,
                    "FS_FileCommand: FIL must be aligned to 4 byte boundary");
                return IOS_EINVAL;
            }
            FIL fp;
            if (!FS_BeginFile(req->ioctlv.vec[0].data,
                              req->ioctlv.vec[0].len, &fp))
                return IOS_EINVAL;
            const FRESULT fret = FS_FileCommand(req, &fp);
            return FS_ReplyFile(req->ioctlv.vec[req->ioctlv.in_count].data,
                                req->ioctlv.vec[req->ioctlv.in_count].len, &fp)
                   ? fret : IOS_EINVAL;
        }

        default:
            printf(ERROR,
                "FS_ReqIoctlv: Unhandled command: %d", req->ioctlv.cmd);
            return IOS_EINVAL;
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

s32 FS_StartRM(void* arg)
{
    printf(INFO, "Starting FS...");

    if (!sdio_Open()) {
        printf(ERROR, "FS_Init: sdio_Open returned false");
        abort();
    }

    /* Mount SD Card */
    FRESULT fret = f_mount(&fatfs, "0:", 0);
    if (fret != FR_OK) {
        /* FatFS won't try to initialize the drive yet, so there will be no
         * "not inserted" error */
        printf(ERROR, "FS_Init: f_mount SD Card failed: %d", fret);
    }

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


/* <----------
 * Filesystem Client
 * ----------> */

s32 storageFd = -1;

s32 FS_CliInit()
{
    if (storageFd < 0)
        storageFd = IOS_Open("/dev/storage", 0);
    return storageFd;
}

FRESULT FS_Open(FIL* fp, const TCHAR* path, u8 mode)
{
    IOVector vec[2 + 1];

    vec[0].data = (void*) path;
    vec[0].len = strlen(path) + 1;

    BYTE _mode = (BYTE) mode;
    vec[1].data = &_mode;
    vec[1].len = sizeof(BYTE);

    vec[2].data = fp;
    vec[2].len = sizeof(FIL);
    return IOS_Ioctlv(storageFd, IOCTL_FOPEN, 2, 1, vec);
}

FRESULT FS_Close(FIL* fp)
{
    IOVector vec[1 + 1];

    vec[0].data = fp;
    vec[0].len = sizeof(FIL);
    vec[1].data = fp;
    vec[1].len = sizeof(FIL);
    return IOS_Ioctlv(storageFd, IOCTL_FCLOSE, 1, 1, vec);
}

FRESULT FS_Read(FIL* fp, void* data, u32 len, u32* read)
{
    IOVector vec[1 + 3];

    vec[0].data = fp;
    vec[0].len = sizeof(FIL);
    vec[1].data = fp;
    vec[1].len = sizeof(FIL);

    vec[2].data = data;
    vec[2].len = len;

    UINT _read;
    vec[3].data = &_read;
    vec[3].len = sizeof(UINT);

    FRESULT ret = IOS_Ioctlv(storageFd, IOCTL_FREAD, 1, 3, vec);
    *read = (u32) _read;
    return ret;
}

FRESULT FS_Write(FIL* fp, const void* data, u32 len, u32* wrote)
{
    IOVector vec[2 + 2];

    vec[0].data = fp;
    vec[0].len = sizeof(FIL);
    vec[2].data = fp;
    vec[2].len = sizeof(FIL);

    vec[1].data = (void*) data;
    vec[1].len = len;

    UINT _wrote;
    vec[3].data = &_wrote;
    vec[3].len = sizeof(UINT);

    FRESULT ret = IOS_Ioctlv(storageFd, IOCTL_FWRITE, 2, 2, vec);
    *wrote = (u32) _wrote;
    return ret;
}

FRESULT FS_LSeek(FIL* fp, u32 offset)
{
    IOVector vec[2 + 1];

    vec[0].data = fp;
    vec[0].len = sizeof(FIL);
    vec[2].data = fp;
    vec[2].len = sizeof(FIL);

    FSIZE_t _offset = offset;
    vec[1].data = &_offset;
    vec[1].len = sizeof(FSIZE_t);

    return IOS_Ioctlv(storageFd, IOCTL_FLSEEK, 2, 1, vec);
}

FRESULT FS_Truncate(FIL* fp)
{
    IOVector vec[1 + 1];

    vec[0].data = fp;
    vec[0].len = sizeof(FIL);
    vec[1].data = fp;
    vec[1].len = sizeof(FIL);

    return IOS_Ioctlv(storageFd, IOCTL_FTRUNCATE, 1, 1, vec);
}

FRESULT FS_Sync(FIL* fp)
{
    IOVector vec[1 + 1];

    vec[0].data = fp;
    vec[0].len = sizeof(FIL);
    vec[1].data = fp;
    vec[1].len = sizeof(FIL);

    return IOS_Ioctlv(storageFd, IOCTL_FSYNC, 1, 1, vec);
}