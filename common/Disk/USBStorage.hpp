#pragma once
#include <System/Types.hpp>

class USBStorage
{
    USBStorage(u32 sector_size, u32 sector_count, u8 lun, u8 ep_in, u8 ep_out,
               u16 vid, u16 pid, u32 tag, u32 interface, s32 usb_fd);

    enum
    {
        Error_OK = 0,
        Error_NoInterface = -10000,
        Error_Sense = -10001,
        Error_ShortWrite = -10002,
        Error_ShortRead = -10003,
        Error_Signature = -10004,
        Error_Tag = -10005,
        Error_Status = -10006,
        Error_DataResidue = -10007,
        Error_Timedout = -10008,
        Error_Init = -10009,
        Error_Processing = -10010
    };

    bool ReadSectors(u32 sector, u32 numSectors, void* buffer);
    bool WriteSectors(u32 sector, u32 numSectors, const void* buffer);

    u32 s_size;
    u32 s_cnt;

private:
    s32 __send_cbw(u8 lun, u32 len, u8 flags, const u8* cb, u8 cbLen);
    s32 __read_csw(u8* status, u32* dataResidue);
    s32 __cycle(u8 lun, u8* buffer, u32 len, u8* cb, u8 cbLen, u8 write,
                u8* _status, u32* _dataResidue);
    s32 __usbstorage_reset();

    bool __inited = false;
    bool __mounted = false;

    u8 __lun = 0;
    u8 __ep_in = 0;
    u8 __ep_out = 0;
    u16 __vid = 0;
    u16 __pid = 0;
    u32 __tag = 0;
    u32 __interface = 0;
    s32 __usb_fd = -1;
};