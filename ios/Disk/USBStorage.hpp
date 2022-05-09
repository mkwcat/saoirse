// USBStorage.hpp
//
// Copyright (C) 2022 Team Saoirse
// SPDX-License-Identifier: MIT

#pragma once
#include <Disk/USB.hpp>
#include <System/Types.h>

class USBStorage
{
public:
    USBStorage(USB* usb, USB::DeviceInfo info);

private:
    enum class USBStorageError
    {
        OK = 0,
        USBHalted = int(USB::USBError::Halted),
    };

    bool GetLunCount(u8* lunCount);
    bool SCSITransfer(bool isWrite, u32 size, void* data, u8 lun, u8 cbSize,
                      void* cb);
    bool TestUnitReady(u8 lun);
    bool Inquiry(u8 lun, u8* type);
    bool InitLun(u8 lun);
    bool RequestSense(u8 lun);
    bool FindLun(u8 lunCount, u8* lun);
    bool ReadCapacity(u8 lun, u32* blockSize);

public:
    bool Init();

    u32 SectorSize();
    bool ReadSectors(u32 firstSector, u32 sectorCount, void* buffer);
    bool WriteSectors(u32 firstSector, u32 sectorCount, const void* buffer);
    bool Sync();

    u32 GetDevID() const
    {
        return m_info.devId;
    }

private:
    USB* m_usb;
    USB::DeviceInfo m_info;
    bool m_valid = false;

    u32 m_id;
    u8 m_interface;
    u8 m_outEndpoint;
    u8 m_inEndpoint;
    u8* m_buffer;
    u32 m_maxPacketSize;
    u32 m_tag = 0;
    u8 m_lun;
    u32 m_blockSize;
};