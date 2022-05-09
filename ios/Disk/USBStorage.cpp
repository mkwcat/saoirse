// USBStorage.cpp - USB Mass Storage I/O
//
// This file is from MKW-SP
// Copyright (C) 2021-2022 Pablo Stebler
// SPDX-License-Identifier: MIT
//
// Modified by Palapeli for Saoirse

// Resources:
// - https://www.usb.org/sites/default/files/usbmassbulk_10.pdf
// -
// https://www.seagate.com/files/staticfiles/support/docs/manual/Interface%20manuals/100293068j.pdf
// -
// https://www.downtowndougbrown.com/2018/12/usb-mass-storage-with-embedded-devices-tips-and-quirks/
// - https://github.com/devkitPro/libogc/blob/master/libogc/usbstorage.c

#include "USBStorage.hpp"
#include <Debug/Log.hpp>
#include <Disk/USB.hpp>
#include <System/Util.h>
#include <algorithm>
#include <cstring>

enum
{
    MSC_GET_MAX_LUN = 0xfe,
};

enum
{
    CSW_SIZE = 0x1f,
    CBW_SIZE = 0xd,
};

enum
{
    SCSI_TEST_UNIT_READY = 0x0,
    SCSI_REQUEST_SENSE = 0x3,
    SCSI_INQUIRY = 0x12,
    SCSI_READ_CAPACITY_10 = 0x25,
    SCSI_READ_10 = 0x28,
    SCSI_WRITE_10 = 0x2a,
    SCSI_SYNCHRONIZE_CACHE_10 = 0x35,
};

enum
{
    SCSI_TYPE_DIRECT_ACCESS = 0x0,
};

USBStorage::USBStorage(USB* usb, USB::DeviceInfo info)
{
    m_buffer = (u8*)IOS::Alloc(0x4000);
    m_usb = usb;
    m_info = info;
}

bool USBStorage::GetLunCount(u8* lunCount)
{
    u8 requestType = USB::CtrlType::Rec_Interface;
    requestType |= USB::CtrlType::ReqType_Class;
    requestType |= USB::CtrlType::Dir_Device2Host;
    if (m_usb->WriteCtrlMsg(m_id, requestType, MSC_GET_MAX_LUN, 0, m_interface,
                            0x1, m_buffer) != USB::USBError::OK) {
        PRINT(IOS_USB, ERROR, "WriteCtrlMsg failed");
        return false;
    }
    *lunCount = read8(m_buffer) + 1;
    return *lunCount >= 1 && *lunCount <= 16;
}

bool USBStorage::SCSITransfer(bool isWrite, u32 size, void* data, u8 lun,
                              u8 cbSize, void* cb)
{
    assert(!!size == !!data);
    assert(lun <= 16);
    assert(cbSize >= 1 && cbSize <= 16);
    assert(cb);

    memset(m_buffer, 0, CSW_SIZE);

    m_tag++;

    write32_le(m_buffer + 0x0, 0x43425355);
    write32_le(m_buffer + 0x4, m_tag);
    write32_le(m_buffer + 0x8, size);
    write8(m_buffer + 0xC, !isWrite << 7);
    write8(m_buffer + 0xD, lun);
    write8(m_buffer + 0xE, cbSize);
    memcpy(m_buffer + 0xF, cb, cbSize);

    if (m_usb->WriteBulkMsg(m_id, m_outEndpoint, CSW_SIZE, m_buffer) !=
        USB::USBError::OK) {
        PRINT(IOS_USB, ERROR, "WriteBulkMsg failed");
        return false;
    }

    u32 remainingSize = size;
    while (remainingSize > 0) {
        u32 chunkSize = std::min<u32>(remainingSize, 0x4000);
        if (isWrite) {
            memcpy(m_buffer, data, chunkSize);
        }
        if (m_usb->WriteBulkMsg(m_id, isWrite ? m_outEndpoint : m_inEndpoint,
                                chunkSize, m_buffer) != USB::USBError::OK) {
            PRINT(IOS_USB, ERROR, "WriteBulkMsg (2) failed");
            return false;
        }
        if (!isWrite) {
            memcpy(data, m_buffer, chunkSize);
        }
        remainingSize -= chunkSize;
        data += chunkSize;
    }

    memset(m_buffer, 0, CBW_SIZE);

    if (m_usb->WriteBulkMsg(m_id, m_inEndpoint, CBW_SIZE, m_buffer) !=
        USB::USBError::OK) {
        PRINT(IOS_USB, ERROR, "WriteBulkMsg (3) failed");
        return false;
    }

    if (read32_le(m_buffer + 0x0) != 0x53425355) {
        return false;
    }
    if (read32_le(m_buffer + 0x4) != m_tag) {
        return false;
    }
    if (read32_le(m_buffer + 0x8) != 0) {
        return false;
    }
    if (read8(m_buffer + 0xC) != 0) {
        return false;
    }

    return true;
}

bool USBStorage::TestUnitReady(u8 lun)
{
    u8 cmd[6] = {0};
    write8(cmd + 0x0, SCSI_TEST_UNIT_READY);

    return SCSITransfer(false, 0, NULL, lun, sizeof(cmd), cmd);
}

bool USBStorage::Inquiry(u8 lun, u8* type)
{
    u8 response[36] = {0};
    u8 cmd[6] = {0};
    write8(cmd + 0x0, SCSI_INQUIRY);
    write8(cmd + 0x1, lun << 5);
    write8(cmd + 0x4, sizeof(response));

    if (!SCSITransfer(false, sizeof(response), response, lun, sizeof(cmd),
                      cmd)) {
        return false;
    }

    *type = response[0] & 0x1f;
    return true;
}

bool USBStorage::InitLun(u8 lun)
{
    if (!TestUnitReady(lun)) {
        return false;
    }

    u8 type;
    if (!Inquiry(lun, &type)) {
        return false;
    }
    return type == SCSI_TYPE_DIRECT_ACCESS;
}

bool USBStorage::RequestSense(u8 lun)
{
    u8 response[18] = {0};
    u8 cmd[6] = {0};
    write8(cmd + 0x0, SCSI_REQUEST_SENSE);
    write8(cmd + 0x4, sizeof(response));

    if (!SCSITransfer(false, sizeof(response), response, lun, sizeof(cmd),
                      cmd)) {
        return false;
    }

    PRINT(IOS_USB, INFO, "USBStorage: Sense key: %x", response[0x2] & 0xf);
    return true;
}

bool USBStorage::FindLun(u8 lunCount, u8* lun)
{
    for (*lun = 0; *lun < lunCount; (*lun)++) {
        for (u32 i = 0; i < 5; i++) {
            if (InitLun(*lun)) {
                return true;
            }

            // This can clear a UNIT ATTENTION condition
            RequestSense(*lun);

            usleep(i * 10);
        }
    }

    return false;
}

bool USBStorage::ReadCapacity(u8 lun, u32* blockSize)
{
    u8 response[8] = {0};
    u8 cmd[10] = {0};
    write8(cmd, SCSI_READ_CAPACITY_10);

    if (!SCSITransfer(false, sizeof(response), response, lun, sizeof(cmd),
                      cmd)) {
        return false;
    }

    *blockSize = read32(response + 0x4);
    return true;
}

bool USBStorage::Init()
{
    u8 numEndpoints = m_info.interface.numEndpoints;
    assert(numEndpoints <= 16);
    bool outFound = false;
    bool inFound = false;
    for (u32 i = 0; i < numEndpoints; i++) {
        const USB::EndpointDescriptor* endpointDescriptor = &m_info.endpoint[i];

        u8 transferType =
            endpointDescriptor->attributes & USB::CtrlType::TransferType_Mask;
        if (transferType != USB::CtrlType::TransferType_Bulk) {
            continue;
        }

        u8 direction =
            endpointDescriptor->endpointAddr & USB::CtrlType::Dir_Mask;
        if (!outFound && direction == USB::CtrlType::Dir_Host2Device) {
            m_outEndpoint = endpointDescriptor->endpointAddr;
            outFound = true;
            m_maxPacketSize = endpointDescriptor->maxPacketSize;
        }
        if (!inFound && direction == USB::CtrlType::Dir_Device2Host) {
            m_inEndpoint = endpointDescriptor->endpointAddr;
            inFound = true;
        }
    }
    if (!outFound || !inFound) {
        return false;
    }

    u16 vendorId = m_info.device.vid;
    u16 productId = m_info.device.pid;
    PRINT(IOS_USB, INFO, "USBStorage: Found device %x:%x", vendorId, productId);
    m_id = m_info.devId;
    m_interface = m_info.interface.ifNum;

    PRINT(IOS_USB, INFO, "USBStorage: Max packet size: %d", m_maxPacketSize);

    u8 lunCount;
    if (!GetLunCount(&lunCount)) {
        return false;
    }
    PRINT(IOS_USB, INFO, "USBStorage: Device has %d logical unit(s)", lunCount);

    if (!FindLun(lunCount, &m_lun)) {
        return false;
    }
    PRINT(IOS_USB, INFO, "USBStorage: Using logical unit %d", m_lun);

    if (!ReadCapacity(m_lun, &m_blockSize)) {
        return false;
    }
    PRINT(IOS_USB, INFO, "USBStorage: Block size: %d bytes", m_blockSize);

    if (!TestUnitReady(m_lun)) {
        return false;
    }

    m_valid = true;
    return true;
}

u32 USBStorage::SectorSize()
{
    return m_blockSize;
}

bool USBStorage::ReadSectors(u32 firstSector, u32 sectorCount, void* buffer)
{
    assert(sectorCount <= UINT16_MAX);

    for (u32 i = 0; i < 1; i++) {
        u8 cmd[10] = {0};
        write8(cmd + 0x0, SCSI_READ_10);
        write16(cmd + 0x2, firstSector >> 16);
        write16(cmd + 0x4, firstSector & 0xFFFF);
        write8(cmd + 0x7, sectorCount >> 8);
        write8(cmd + 0x8, sectorCount & 0xFF);

        u32 size = sectorCount * m_blockSize;
        if (SCSITransfer(false, size, buffer, m_lun, sizeof(cmd), cmd)) {
            return true;
        }

        usleep(i * 10);
    }

    return false;
}

bool USBStorage::WriteSectors(u32 firstSector, u32 sectorCount,
                              const void* buffer)
{
    assert(sectorCount <= UINT16_MAX);

    for (u32 i = 0; i < 1; i++) {
        u8 cmd[10] = {0};
        write8(cmd + 0x0, SCSI_WRITE_10);
        write16(cmd + 0x2, firstSector >> 16);
        write16(cmd + 0x4, firstSector & 0xFFFF);
        write8(cmd + 0x7, sectorCount >> 8);
        write8(cmd + 0x8, sectorCount & 0xFF);

        u32 size = sectorCount * m_blockSize;
        if (SCSITransfer(true, size, const_cast<void*>(buffer), m_lun,
                         sizeof(cmd), cmd)) {
            return true;
        }

        usleep(i * 10);
    }

    return false;
}

bool USBStorage::Sync()
{
    u8 cmd[10] = {0};
    write8(cmd, SCSI_SYNCHRONIZE_CACHE_10);

    return SCSITransfer(false, 0, NULL, m_lun, sizeof(cmd), cmd);
}