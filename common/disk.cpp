#include <diskio.h>
#include <sdcard.h>

#ifdef TARGET_IOS
#include <main.h>
#define DiskLog(level, ...) peli::Log(level, __VA_ARGS__)
#else
#include <irse.h>
#define DiskLog(level, ...) irse::Log(LogS::DiskIO, level, __VA_ARGS__)
#endif

constexpr BYTE DRV_SDCARD = 0;

DSTATUS disk_status(BYTE pdrv)
{
    // DiskLog(LogL::INFO, "disk_status: drv: %d", pdrv);
    if (pdrv == DRV_SDCARD) {
        if (!SDCard::IsInitialized() || !SDCard::IsInserted()) {
            DiskLog(LogL::WARN, "disk_status returning STA_NODISK");
            return STA_NODISK;
        }
        return 0;
    }
    return STA_NOINIT;
}

DSTATUS disk_initialize(BYTE pdrv)
{
    // DiskLog(LogL::INFO, "disk_initialize: drv: %d", pdrv);
    if (pdrv == DRV_SDCARD) {
        if (!SDCard::Startup()) {
            /* No way to differentiate between error and not inserted */
            DiskLog(LogL::WARN,
                    "disk_initialize: SDCard::Startup returned false");
            return STA_NODISK;
        }
        return 0;
    }
    DiskLog(LogL::ERROR, "disk_initialize: unknown pdrv (%d)", pdrv);
    return STA_NOINIT;
}

DRESULT disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count)
{
    // DiskLog(LogL::INFO, "disk read sec: %d, cnt: %d", sector, count);
    if (pdrv == DRV_SDCARD) {
        if (disk_status(pdrv) != 0)
            return RES_ERROR;

        s32 ret = SDCard::ReadSectors(sector, count, buff);
        if (ret < 0) {
            DiskLog(LogL::ERROR, "disk_read: SDCard::ReadSectors failed: %d",
                    ret);
            return RES_ERROR;
        }
        return RES_OK;
    }
    return RES_NOTRDY;
}

DRESULT disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count)
{
    if (pdrv == DRV_SDCARD) {
        if (disk_status(pdrv) != 0)
            return RES_ERROR;

        s32 ret = SDCard::WriteSectors(sector, count, buff);
        if (ret < 0) {
            DiskLog(LogL::ERROR, "disk_write: SDCard::WriteSectors failed: %d",
                    ret);
            return RES_ERROR;
        }
        return RES_OK;
    }
    return RES_NOTRDY;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff)
{
    switch (cmd) {
    case CTRL_SYNC:
        if (pdrv == DRV_SDCARD)
            return RES_OK;
        return RES_NOTRDY;

    case GET_SECTOR_SIZE:
        if (pdrv == DRV_SDCARD) {
            /* Always 512 */
            *(WORD*)buff = 512;
            return RES_OK;
        }
        return RES_NOTRDY;

    default:
        DiskLog(LogL::ERROR, "disk_ioctl: unknown command: %d", cmd);
        return RES_PARERR;
    }
}

/* todo */
DWORD get_fattime()
{
    return 0;
}

FATFS fatfs;

namespace FSServ
{

bool MountSDCard()
{
    /* Mount SD Card */
    FRESULT fret = f_mount(&fatfs, "0:", 0);
    if (fret != FR_OK) {
        DiskLog(LogL::ERROR, "MountSDCard: f_mount SD Card failed: %d", fret);
        return false;
    }

    fret = f_chdir("0:/saoirse");
    if (fret != FR_OK) {
        DiskLog(LogL::ERROR, "MountSDCard: failed to change directory: %d",
                fret);
        return false;
    }

    return true;
}

bool UnmountSDCard()
{
    return f_unmount("0:") == FR_OK;
}

} // namespace FSServ