// Disk.cpp - SD Card/USB I/O
//   Written by Palapeli
//
// Copyright (C) 2022 Team Saoirse
// SPDX-License-Identifier: MIT

#include <Debug/Log.hpp>
#include <Disk/SDCard.hpp>
#include <FAT/diskio.h>
#include <FAT/ff.h>
#include <IOS/DeviceMgr.hpp>
#include <System/OS.hpp>

constexpr BYTE DRV_SDCARD = 0;

DSTATUS disk_status(BYTE pdrv)
{
    // PRINT(DiskIO, INFO, "disk_status: drv: %d", pdrv);
    if (pdrv == DRV_SDCARD) {
        if (!SDCard::IsInitialized() || !SDCard::IsInserted()) {
            DeviceMgr::sInstance->SetError(DeviceMgr::DRVToDeviceKind(pdrv));
            DeviceMgr::sInstance->ForceUpdate();
            PRINT(DiskIO, WARN, "disk_status returning STA_NODISK");
            return STA_NODISK;
        }
        return 0;
    }
    return STA_NOINIT;
}

DSTATUS disk_initialize(BYTE pdrv)
{
    // PRINT(DiskIO, INFO, "disk_initialize: drv: %d", pdrv);
    if (pdrv == DRV_SDCARD) {
        if (!SDCard::Startup()) {
            /* No way to differentiate between error and not inserted */
            DeviceMgr::sInstance->SetError(DeviceMgr::DRVToDeviceKind(pdrv));
            DeviceMgr::sInstance->ForceUpdate();
            PRINT(DiskIO, WARN,
                  "disk_initialize: SDCard::Startup returned false");
            return STA_NODISK;
        }
        return 0;
    }
    PRINT(DiskIO, ERROR, "disk_initialize: unknown pdrv (%d)", pdrv);
    return STA_NOINIT;
}

DRESULT disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count)
{
    // PRINT(DiskIO, INFO, "disk read sec: %d, cnt: %d", sector, count);
    if (pdrv == DRV_SDCARD) {
        if (disk_status(pdrv) != 0)
            return RES_ERROR;

        s32 ret = SDCard::ReadSectors(sector, count, buff);
        if (ret < 0) {
            DeviceMgr::sInstance->SetError(DeviceMgr::DRVToDeviceKind(pdrv));
            DeviceMgr::sInstance->ForceUpdate();
            PRINT(DiskIO, ERROR, "disk_read: SDCard::ReadSectors failed: %d",
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
            DeviceMgr::sInstance->SetError(DeviceMgr::DRVToDeviceKind(pdrv));
            DeviceMgr::sInstance->ForceUpdate();
            PRINT(DiskIO, ERROR, "disk_write: SDCard::WriteSectors failed: %d",
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
        PRINT(DiskIO, ERROR, "disk_ioctl: unknown command: %d", cmd);
        return RES_PARERR;
    }
}

/* todo */
DWORD get_fattime()
{
    return 0;
}

#if FF_USE_LFN == 3

void* ff_memalloc(UINT msize)
{
    return new u8[msize];
}

void ff_memfree(void* mblock)
{
    u8* data = reinterpret_cast<u8*>(mblock);
    delete data;
}

#endif

#if FF_FS_REENTRANT

int ff_cre_syncobj([[maybe_unused]] BYTE vol, FF_SYNC_t* sobj)
{
    Mutex* mutex = new Mutex;
    *sobj = reinterpret_cast<FF_SYNC_t>(mutex);
    return 1;
}

// Lock sync object
int ff_req_grant(FF_SYNC_t sobj)
{
    Mutex* mutex = reinterpret_cast<Mutex*>(sobj);
    mutex->lock();
    return 1;
}

void ff_rel_grant(FF_SYNC_t sobj)
{
    Mutex* mutex = reinterpret_cast<Mutex*>(sobj);
    mutex->unlock();
}

// Delete a sync object
int ff_del_syncobj(FF_SYNC_t sobj)
{
    Mutex* mutex = reinterpret_cast<Mutex*>(sobj);
    delete mutex;
    return 1;
}

#endif