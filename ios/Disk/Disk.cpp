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
#include <limits>
#include <tuple>

constexpr BYTE DRV_SDCARD = 0;

DSTATUS disk_status(BYTE pdrv)
{
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

// From https://howardhinnant.github.io/date_algorithms.html#civil_from_days
template <class Int>
constexpr std::tuple<Int, unsigned, unsigned> civil_from_days(Int z) noexcept
{
    static_assert(
        std::numeric_limits<unsigned>::digits >= 18,
        "This algorithm has not been ported to a 16 bit unsigned integer");
    static_assert(
        std::numeric_limits<Int>::digits >= 20,
        "This algorithm has not been ported to a 16 bit signed integer");
    z += 719468;
    const Int era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097); // [0, 146096]
    const unsigned yoe =
        (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365; // [0, 399]
    const Int y = static_cast<Int>(yoe) + era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100); // [0, 365]
    const unsigned mp = (5 * doy + 2) / 153; // [0, 11]
    const unsigned d = doy - (153 * mp + 2) / 5 + 1; // [1, 31]
    const unsigned m = mp < 10 ? mp + 3 : mp - 9; // [1, 12]
    return std::tuple<Int, unsigned, unsigned>(y + (m <= 2), m, d);
}

DWORD get_fattime()
{
    struct {
        u32 year : 7;
        u32 month : 4;
        u32 day : 5;
        u32 hour : 5;
        u32 minute : 6;
        u32 second : 5;
    } fattime;

    u64 time64 = System::GetTime();
    u32 time32 = time64;

    u32 days = time64 / 86400;
    auto date = civil_from_days(days);

    fattime = {
        .year = std::get<0>(date) - 1980,
        .month = std::get<1>(date),
        .day = std::get<2>(date),
        .hour = (time32 / 60 / 60) % 24,
        .minute = (time32 / 60) % 60,
        .second = time32 % 60,
    };

    return *reinterpret_cast<DWORD*>(&fattime);
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