/*-------------------------------------------------------------

usbstorage.c -- Bulk-only USB mass storage support

Copyright (C) 2008
Sven Peter (svpe) <svpe@gmx.net>
Copyright (C) 2009-2010
tueidj, rodries, Tantric
Stripped down nintendont port by FIX94
Modified by Palapeli for Saoirse (2022)

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.	This notice may not be removed or altered from any source
distribution.

-------------------------------------------------------------*/

#include "USBStorage.hpp"
#include "USB.hpp"
#include <System/OS.hpp>
#include <System/Types.h>
#include <System/Util.h>
#include <cstring>

#ifdef TARGET_IOS
#include <IOS/System.hpp>
#else
#include <unistd.h>
#endif

#define ROUNDDOWN32(v) (((u32)(v)-0x1f) & ~0x1f)

#define HEAP_SIZE (18 * 1024)
#define TAG_START 0x0BADC0DE

#define CBW_SIZE 31
#define CBW_SIGNATURE 0x43425355
#define CBW_IN (1 << 7)
#define CBW_OUT 0

#define CSW_SIZE 13
#define CSW_SIGNATURE 0x53425355

#define SCSI_TEST_UNIT_READY 0x00
#define SCSI_REQUEST_SENSE 0x03
#define SCSI_INQUIRY 0x12
#define SCSI_START_STOP 0x1B
#define SCSI_READ_CAPACITY 0x25
#define SCSI_READ_10 0x28
#define SCSI_WRITE_10 0x2A

#define SCSI_SENSE_REPLY_SIZE 18
#define SCSI_SENSE_NOT_READY 0x02
#define SCSI_SENSE_MEDIUM_ERROR 0x03
#define SCSI_SENSE_HARDWARE_ERROR 0x04

#define USB_CLASS_MASS_STORAGE 0x08
#define MASS_STORAGE_RBC_COMMANDS 0x01
#define MASS_STORAGE_ATA_COMMANDS 0x02
#define MASS_STORAGE_QIC_COMMANDS 0x03
#define MASS_STORAGE_UFI_COMMANDS 0x04
#define MASS_STORAGE_SFF8070_COMMANDS 0x05
#define MASS_STORAGE_SCSI_COMMANDS 0x06
#define MASS_STORAGE_BULK_ONLY 0x50

#define USBSTORAGE_GET_MAX_LUN 0xFE
#define USBSTORAGE_RESET 0xFF

#define USB_ENDPOINT_BULK 0x02

#define USBSTORAGE_CYCLE_RETRIES 3

#define INVALID_LUN -2

#define MAX_TRANSFER_SIZE_V5 (16 * 1024)

#define DEVLIST_MAXSIZE 8

Mutex transferMutex;
static u8* cbw_buffer = (u8*)IOS::Alloc(32);
static u8* transferbuffer = (u8*)IOS::Alloc(MAX_TRANSFER_SIZE_V5);

s32 USBStorage::__send_cbw(u8 lun, u32 len, u8 flags, const u8* cb, u8 cbLen)
{
    s32 retval = USBStorage::Error_OK;

    if (cbLen == 0 || cbLen > 16)
        return IOSError::Invalid;

    write32(((u32)cbw_buffer), bswap32(CBW_SIGNATURE));
    write32(((u32)cbw_buffer) + 4, bswap32(++__tag));
    write32(((u32)cbw_buffer) + 8, bswap32(len));
    cbw_buffer[12] = flags;
    cbw_buffer[13] = lun;
    cbw_buffer[14] = (cbLen > 6 ? 10 : 6);

    memcpy(cbw_buffer + 15, cb, cbLen);

    retval = USB::sInstance->WriteBlkMsg(__usb_fd, __ep_out, CBW_SIZE,
                                         (void*)cbw_buffer);

    if (retval == CBW_SIZE)
        return USBStorage::Error_OK;
    else if (retval > 0)
        return USBStorage::Error_ShortWrite;

    return retval;
}

s32 USBStorage::send_cbw(u8 lun, u32 len, u8 flags, const u8* cb, u8 cbLen)
{
    transferMutex.lock();
    auto ret = __send_cbw(lun, len, flags, cb, cbLen);
    transferMutex.unlock();
    return ret;
}

s32 USBStorage::__read_csw(u8* status, u32* dataResidue)
{
    s32 retval = USBStorage::Error_OK;
    u32 signature, tag, _dataResidue, _status;

    retval =
        USB::sInstance->WriteBlkMsg(__usb_fd, __ep_in, CSW_SIZE, cbw_buffer);
    if (retval > 0 && retval != CSW_SIZE)
        return USBStorage::Error_ShortRead;
    else if (retval < 0)
        return retval;

    signature = bswap32(read32(((u32)cbw_buffer)));
    tag = bswap32(read32(((u32)cbw_buffer) + 4));
    _dataResidue = bswap32(read32(((u32)cbw_buffer) + 8));
    _status = cbw_buffer[12];

    if (signature != CSW_SIGNATURE)
        return USBStorage::Error_Signature;

    if (dataResidue != NULL)
        *dataResidue = _dataResidue;
    if (status != NULL)
        *status = _status;

    if (tag != __tag)
        return USBStorage::Error_Tag;

    return USBStorage::Error_OK;
}

s32 USBStorage::read_csw(u8* status, u32* dataResidue)
{
    transferMutex.lock();
    auto ret = __read_csw(status, dataResidue);
    transferMutex.unlock();
    return ret;
}

s32 USBStorage::__cycle(u8 lun, u8* buffer, u32 len, u8* cb, u8 cbLen, u8 write,
                        u8* _status, u32* _dataResidue)
{
    s32 retval = USBStorage::Error_OK;

    u8 status = 0;
    u32 dataResidue = 0;
    u32 max_size = MAX_TRANSFER_SIZE_V5;
    u8 ep = write ? __ep_out : __ep_in;
    s8 retries = USBSTORAGE_CYCLE_RETRIES + 1;

    do {
        u8* _buffer = buffer;
        u32 _len = len;
        retries--;

        if (retval == USBStorage::Error_Timedout)
            break;

        retval = send_cbw(lun, len, (write ? CBW_OUT : CBW_IN), cb, cbLen);

        while (_len > 0 && retval >= 0) {
            u32 thisLen = _len > max_size ? max_size : _len;

            if (!aligned(buffer, 32) || !in_mem2(buffer)) {
                if (write)
                    memcpy(transferbuffer, _buffer, thisLen);
                retval = USB::sInstance->WriteBlkMsg(__usb_fd, ep, thisLen,
                                                     transferbuffer);
                if (!write && retval > 0)
                    memcpy(_buffer, transferbuffer, retval);
            } else {
                retval =
                    USB::sInstance->WriteBlkMsg(__usb_fd, ep, thisLen, _buffer);
            }
            if (static_cast<u32>(retval) == thisLen) {
                _len -= retval;
                _buffer += retval;
            } else if (retval != USBStorage::Error_Timedout) {
                retval = USBStorage::Error_DataResidue;
            }
        }

        if (retval >= 0) {
            retval = read_csw(&status, &dataResidue);
        }

        if (retval < 0) {
            if (__usbstorage_reset() == USBStorage::Error_Timedout)
                retval = USBStorage::Error_Timedout;
        }
    } while (retval < 0 && retries > 0);

    if (_status != NULL)
        *_status = status;
    if (_dataResidue != NULL)
        *_dataResidue = dataResidue;

    return retval;
}

s32 USBStorage::cycle(u8 lun, u8* buffer, u32 len, u8* cb, u8 cbLen, u8 write,
                      u8* _status, u32* _dataResidue)
{
    transferMutex.lock();
    auto ret =
        __cycle(lun, buffer, len, cb, cbLen, write, _status, _dataResidue);
    transferMutex.unlock();
    return ret;
}

s32 USBStorage::__usbstorage_reset()
{
    s32 retval = USB::sInstance->WriteCtrlMsg(
        __usb_fd,
        (USB::CtrlType::Dir_Host2Device | USB::CtrlType::Type_Class |
         USB::CtrlType::Rec_Interface),
        USBSTORAGE_RESET, 0, __interface, 0, NULL);

    usleep(60 * 1000);
    // from http://www.usb.org/developers/devclass_docs/usbmassbulk_10.pdf
    USB::sInstance->ClearHalt(__usb_fd, __ep_in);
    usleep(10000);
    USB::sInstance->ClearHalt(__usb_fd, __ep_out);
    usleep(10000);
    return retval;
}

bool USBStorage::ReadSectors(u32 sector, u32 numSectors, void* buffer)
{
    if (!__mounted)
        return false;

    u8 status = 0;
    s32 retval;
    u8 cmd[] = {
        SCSI_READ_10, __lun << 5, sector >> 24,    sector >> 16, sector >> 8,
        sector,       0,          numSectors >> 8, numSectors,   0};

    retval = cycle(__lun, reinterpret_cast<u8*>(buffer), numSectors * s_size,
                   cmd, sizeof(cmd), 0, &status, NULL);
    if (retval > 0 && status != 0)
        retval = USBStorage::Error_Status;

    return retval >= 0;
}

bool USBStorage::WriteSectors(u32 sector, u32 numSectors, const void* buffer)
{
    if (!__mounted)
        return false;

    u8 status = 0;
    s32 retval;
    u8 cmd[] = {
        SCSI_WRITE_10, __lun << 5, sector >> 24,    sector >> 16, sector >> 8,
        sector,        0,          numSectors >> 8, numSectors,   0};

    retval = cycle(__lun, (u8*)buffer, numSectors * s_size, cmd, sizeof(cmd), 1,
                   &status, NULL);
    if (retval > 0 && status != 0)
        retval = USBStorage::Error_Status;

    return retval >= 0;
}

USBStorage::USBStorage(u32 sector_size, u32 sector_count, u8 lun, u8 ep_in,
                       u8 ep_out, u16 vid, u16 pid, u32 tag, u32 interface,
                       s32 usb_fd)
{
    s_size = sector_size;
    s_cnt = sector_count;

    __lun = lun;
    __vid = vid;
    __pid = pid;
    __tag = tag;
    __interface = interface;
    __usb_fd = usb_fd;
    __ep_in = ep_in;
    __ep_out = ep_out;

    __inited = true;
    __mounted = true;
}