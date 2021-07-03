#include <ff.h>
#include <diskio.h>
#include <types.h>
#include "wiisd.h"
#include <log.h>
#include <main.h>

FATFS fatfs;

#define DRV_SDCARD 0

s32 FS_Init()
{
    if (!sdio_Startup()) {
        /* This probably returns false if no SD Card is inserted, so we
         * should maybe make a "configure" kind of thing from PPC side */
        printf(ERROR, "sdio_Startup failed");
        abort();
    }

    s32 ret = f_mount(&fatfs, "0:", 1);
    printf(INFO, "f_mount: %d", ret);

    // test sd card
    FIL fp;
    FRESULT fret = f_open(&fp, "/test_file.txt", FA_READ|FA_OPEN_EXISTING);
    if (fret != FR_OK) {
        printf(ERROR, "f_open failed: %d", fret);
        abort();
    }

    char str[sizeof("This is a test file.")];
    UINT throwaway;
    fret = f_read(&fp, str, sizeof(str) - 1, &throwaway);
    if (fret != FR_OK) {
        printf(ERROR, "f_read failed: %d", fret);
        abort();
    }

    str[sizeof(str) - 1] = 0;
    printf(WARN, "str: %s", str);
    return 0;
}


DSTATUS disk_status(BYTE pdrv)
{
    //printf(INFO, "disk_status called, pdrv = %d", pdrv);
    return 0;
}

DSTATUS disk_initialize(BYTE pdrv)
{
    //printf(INFO, "disk_initialize called, pdrv = %d", pdrv);
    return 0;
}

DRESULT disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count)
{
    //printf(INFO, "disk_read: pdrv = %d, sector = %d, count = %d", pdrv, sector, count);
    if (pdrv == DRV_SDCARD)
    {
        if (!sdio_ReadSectors(sector, count, buff)) {
            printf(ERROR, "sdio_ReadSectors failed");
            return RES_ERROR;
        }
        return RES_OK;
    }
    return RES_PARERR;
}

DRESULT disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count)
{
    //printf(INFO, "disk_write called, pdrv = %d", pdrv);
    return RES_NOTRDY;
}

/* todo */
DWORD get_fattime()
{
    return 0;
}


