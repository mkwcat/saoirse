// DeviceMgr.cpp - I/O storage device manager
//   Written by Palapeli
//
// Copyright (C) 2022 Team Saoirse
// SPDX-License-Identifier: MIT

#include "DeviceMgr.hpp"
#include <Debug/Log.hpp>
#include <Disk/SDCard.hpp>
#include <IOS/IPCLog.hpp>
#include <System/Config.hpp>
#include <System/Types.h>

DeviceMgr* DeviceMgr::sInstance;

DeviceMgr::DeviceMgr()
{
    // 64 ms repeating timer
    m_timer = IOS_CreateTimer(0, 64000, m_timerQueue.id(), 0);
    assert(m_timer >= 0);

    bool ret = SDCard::Open();
    assert(ret);

    USB::sInstance = new USB(0);
    ret = USB::sInstance->Init();
    assert(ret);

    // Reset everything to default.
    for (u32 i = 0; i < DeviceCount; i++) {
        InitHandle(i);
    }

    for (u32 i = 0; i < USB::MaxDevices; i++) {
        m_usbDevices[i].inUse = false;
    }

    m_devices[0].disk = SDCard();
    m_devices[0].enabled = true;

    m_thread.create(ThreadEntry, reinterpret_cast<void*>(this), nullptr, 0x800,
                    40);
}

bool DeviceMgr::IsInserted(u32 devId)
{
    ASSERT(devId < DeviceCount);

    return m_devices[devId].inserted & !m_devices[devId].error;
}

bool DeviceMgr::IsMounted(u32 devId)
{
    ASSERT(devId < DeviceCount);

    return IsInserted(devId) && m_devices[devId].mounted;
}

void DeviceMgr::SetError(u32 devId)
{
    ASSERT(devId < DeviceCount);

    m_devices[devId].error = true;
}

FATFS* DeviceMgr::GetFilesystem(u32 devId)
{
    ASSERT(devId < DeviceCount);

    return &m_devices[devId].fs;
}

void DeviceMgr::ForceUpdate()
{
    m_timerQueue.send(0);
}

bool DeviceMgr::IsLogEnabled()
{
    if (!m_logEnabled || !IsMounted(m_logDevice))
        return false;

    return true;
}

void DeviceMgr::WriteToLog(const char* str, u32 len)
{
    if (!IsLogEnabled())
        return;

    UINT bw = 0;
    f_write(&m_logFile, str, len, &bw);
    static const char newline = '\n';
    f_write(&m_logFile, &newline, 1, &bw);
    f_sync(&m_logFile);
}

bool DeviceMgr::DeviceInit(u32 devId)
{
    ASSERT(devId < DeviceCount);
    DeviceHandle* dev = &m_devices[devId];

    if (!dev->enabled || dev->error) {
        PRINT(IOS_DevMgr, ERROR, "Device not enabled: %u", devId);
        return false;
    }

    if (std::holds_alternative<SDCard>(dev->disk)) {
        if (SDCard::Startup())
            return true;

        SetError(devId);
        PRINT(IOS_DevMgr, ERROR, "SDCard::Startup failed");
        return false;
    }

    if (std::holds_alternative<USBStorage>(dev->disk)) {
        USBStorage& disk = std::get<USBStorage>(dev->disk);
        if (disk.Init())
            return true;

        SetError(devId);
        PRINT(IOS_DevMgr, ERROR, "USBStorage::Init failed");
        return false;
    }

    PRINT(IOS_DevMgr, ERROR, "Device not recognized: %u", devId);
    return false;
}

bool DeviceMgr::DeviceRead(u32 devId, void* data, u32 sector, u32 count)
{
    ASSERT(devId < DeviceCount);
    DeviceHandle* dev = &m_devices[devId];

    if (!dev->enabled || dev->error) {
        PRINT(IOS_DevMgr, ERROR, "Device not enabled: %u", devId);
        return false;
    }

    if (std::holds_alternative<SDCard>(dev->disk)) {
        auto ret = SDCard::ReadSectors(sector, count, data);
        if (ret == IOSError::OK)
            return true;

        SetError(devId);
        PRINT(IOS_DevMgr, ERROR, "SDCard::ReadSectors failed: %08X", ret);
        return false;
    }

    if (std::holds_alternative<USBStorage>(dev->disk)) {
        USBStorage& disk = std::get<USBStorage>(dev->disk);
        if (disk.ReadSectors(sector, count, data))
            return true;

        SetError(devId);
        PRINT(IOS_DevMgr, ERROR, "USBStorage::ReadSectors failed");
        return false;
    }

    PRINT(IOS_DevMgr, ERROR, "Device not recognized: %u", devId);
    return false;
}

bool DeviceMgr::DeviceWrite(u32 devId, const void* data, u32 sector, u32 count)
{
    ASSERT(devId < DeviceCount);
    DeviceHandle* dev = &m_devices[devId];

    if (!dev->enabled || dev->error) {
        PRINT(IOS_DevMgr, ERROR, "Device not enabled: %u", devId);
        return false;
    }

    if (std::holds_alternative<SDCard>(dev->disk)) {
        auto ret = SDCard::WriteSectors(sector, count, data);
        if (ret == 0)
            return true;

        SetError(devId);
        PRINT(IOS_DevMgr, ERROR, "SDCard::WriteSectors failed: %08X", ret);
        return false;
    }

    if (std::holds_alternative<USBStorage>(dev->disk)) {
        USBStorage& disk = std::get<USBStorage>(dev->disk);
        if (disk.WriteSectors(sector, count, data))
            return true;

        SetError(devId);
        PRINT(IOS_DevMgr, ERROR, "USBStorage::WriteSectors failed");
        return false;
    }

    PRINT(IOS_DevMgr, ERROR, "Device not recognized: %u", devId);
    return false;
}

bool DeviceMgr::DeviceSync(u32 devId)
{
    ASSERT(devId < DeviceCount);
    DeviceHandle* dev = &m_devices[devId];

    if (!dev->enabled || dev->error) {
        PRINT(IOS_DevMgr, ERROR, "Device not enabled: %u", devId);
        return false;
    }

    if (std::holds_alternative<SDCard>(dev->disk)) {
        return true;
    }

    if (std::holds_alternative<USBStorage>(dev->disk)) {
        USBStorage& disk = std::get<USBStorage>(dev->disk);
        if (disk.Sync())
            return true;

        SetError(devId);
        PRINT(IOS_DevMgr, ERROR, "USBStorage::Sync failed");
        return false;
    }

    PRINT(IOS_DevMgr, ERROR, "Device not recognized: %u", devId);
    return false;
}

void DeviceMgr::Run()
{
    PRINT(IOS_DevMgr, INFO, "Entering DeviceMgr...");
    PRINT(IOS_DevMgr, INFO, "DevMgr thread ID: %d", IOS_GetThreadId());

    auto usbDevices = (USB::DeviceEntry*)IOS::Alloc(sizeof(USB::DeviceEntry) *
                                                    USB::MaxDevices);
    IOS::Request usbReq = {};
    if (!USB::sInstance->EnqueueDeviceChange(usbDevices, &m_timerQueue,
                                             &usbReq))
        USBFatal();

    while (true) {
        // Wait for 64 ms.
        auto req = m_timerQueue.receive(0);

        if (req == &usbReq) {
            PRINT(IOS_DevMgr, INFO, "USB device change");
            assert(req->cmd == IOS::Command::Reply);

            u32 count = req->result;
            USBChange(usbDevices, count);
            usbReq = {};
            if (!USB::sInstance->EnqueueDeviceChange(usbDevices, &m_timerQueue,
                                                     &usbReq))
                USBFatal();
        }

        for (u32 i = 0; i < DeviceCount; i++) {
            UpdateHandle(i);
        }
    }
}

s32 DeviceMgr::ThreadEntry(void* arg)
{
    DeviceMgr* that = reinterpret_cast<DeviceMgr*>(arg);
    that->Run();

    return 0;
}

void DeviceMgr::USBFatal()
{
    assert(!"USBFatal() was called!");
}

void DeviceMgr::USBChange(USB::DeviceEntry* devices, u32 count)
{
    if (count > USB::MaxDevices) {
        PRINT(IOS_DevMgr, ERROR, "USB GetDeviceChange error: %d", (s32)count);
        USBFatal();
        return;
    }

    // Scan for device changes
    bool foundMap[USB::MaxDevices] = {};

    for (u32 i = 0; i < USB::MaxDevices; i++) {
        if (!m_usbDevices[i].inUse)
            continue;

        // Invalidate device's intId if it errored
        u32 intId = m_usbDevices[i].intId;
        if (intId < DeviceCount && m_devices[intId].error)
            m_usbDevices[i].intId = DeviceCount;

        u32 j = 0;
        // Sometimes the first 16 bits "device index?" will change
        while (j < count && (m_usbDevices[i].usbId & 0xFFFF) !=
                                (devices[j].devId & 0xFFFF)) {
            j++;
        }

        if (j < count) {
            foundMap[j] = true;
            continue;
        }

        PRINT(IOS_DevMgr, INFO, "Device with id %X was removed",
              m_usbDevices[i].usbId);

        // Set device to not inserted
        if (intId < DeviceCount)
            m_devices[intId].inserted = false;

        m_usbDevices[i].inUse = false;
    }

    // Search for new devices
    for (u32 i = 0; i < count; i++) {
        if (foundMap[i] == true)
            continue;

        PRINT(IOS_DevMgr, INFO, "Device with id %X was added",
              devices[i].devId);

        // Search for an open handle
        u32 j = 0;
        for (; j < USB::MaxDevices; j++) {
            if (!m_usbDevices[j].inUse)
                break;
        }
        assert(j < USB::MaxDevices);

        m_usbDevices[j].inUse = true;
        m_usbDevices[j].usbId = devices[i].devId;
        m_usbDevices[j].intId = DeviceCount; // Invalid

        if (USB::sInstance->Attach(devices[i].devId) != USB::USBError::OK) {
            PRINT(IOS_DevMgr, ERROR, "Failed to attach device %X",
                  devices[i].devId);
            continue;
        }

        USB::DeviceInfo info;
        u8 alt = 0;
        for (; alt < devices[i].altSetCount; alt++)
            if (USB::sInstance->GetDeviceInfo(devices[i].devId, &info, alt) ==
                USB::USBError::OK)
                break;
        if (alt == devices[i].altSetCount) {
            PRINT(IOS_DevMgr, ERROR, "Failed to get info from device %X",
                  devices[i].devId);
            continue;
        }
        assert(info.devId == devices[i].devId);

        if (info.interface.ifClass != USB::ClassCode::MassStorage ||
            info.interface.ifSubClass != USB::SubClass::MassStorage_SCSI ||
            info.interface.ifProtocol != USB::Protocol::MassStorage_BulkOnly) {
            PRINT(IOS_DevMgr, WARN,
                  "USB device is not a (compatible) storage device (%X:%X:%X)",
                  info.interface.ifClass, info.interface.ifSubClass,
                  info.interface.ifProtocol);
            continue;
        }

        // Find open device ID
        u32 k = 0;
        for (; k < DeviceCount; k++) {
            if (!m_devices[k].enabled)
                break;
        }
        if (k >= DeviceCount) {
            PRINT(IOS_DevMgr, ERROR, "No open devices available");
            continue;
        }

        PRINT(IOS_DevMgr, INFO, "Using device %u", k);

        auto dev = &m_devices[k];
        dev->disk = USBStorage(USB::sInstance, info);

        m_usbDevices[j].intId = k;
        dev->inserted = true;
        dev->error = false;
        dev->mounted = false;
        dev->enabled = true;
    }
}

void DeviceMgr::InitHandle(u32 devId)
{
    ASSERT(devId < DeviceCount);

    m_devices[devId].enabled = false;
    m_devices[devId].inserted = false;
    m_devices[devId].error = false;
    m_devices[devId].mounted = false;
}

void DeviceMgr::UpdateHandle(u32 devId)
{
    ASSERT(devId < DeviceCount);
    DeviceHandle* dev = &m_devices[devId];

    if (!dev->enabled)
        return;

    if (std::holds_alternative<SDCard>(dev->disk)) {
        dev->inserted = SDCard::IsInserted();
    }

    // Clear error if the device has been ejected, so we can try again if it's
    // reinserted.
    if (!dev->inserted)
        dev->error = false;

    if (!dev->inserted && dev->mounted) {
        // Disable file log if it was writing to this device
        if (m_logEnabled && std::holds_alternative<SDCard>(dev->disk)) {
            m_logEnabled = false;
            m_logDevice = DeviceCount;
        }

        PRINT(IOS_DevMgr, INFO, "Unmount device %d", devId);

        dev->error = false;
        dev->mounted = false;

        // Create drv str.
        char str[16] = "0:";
        str[0] = devId + '0';

        FRESULT fret = f_unmount(str);
        if (fret != FR_OK) {
            PRINT(IOS_DevMgr, ERROR, "Failed to unmount device %d: %d", devId,
                  fret);
            dev->error = true;
            return;
        }

        PRINT(IOS_DevMgr, INFO, "Successfully unmounted device %d", devId);

        if (std::holds_alternative<USBStorage>(dev->disk)) {
            dev->enabled = false;
        }

        IPCLog::sInstance->NotifyDeviceRemoval(devId);
    }

    if (dev->inserted && !dev->mounted && !dev->error) {
        // Mount the device.
        PRINT(IOS_DevMgr, INFO, "Mount device %d", devId);

        dev->error = false;

        // Create drv str.
        char str[16] = "0:";
        str[0] = devId + '0';

        FRESULT fret = f_mount(&dev->fs, str, 0);
        if (fret != FR_OK) {
            PRINT(IOS_DevMgr, ERROR, "Failed to mount device %d: %d", devId,
                  fret);
            dev->error = true;
            dev->enabled = false;
            return;
        }

        PRINT(IOS_DevMgr, INFO, "Successfully mounted device %d", devId);

        dev->mounted = true;
        dev->error = false;

        // Open log file if it's enabled
        if (!m_logEnabled && Config::sInstance->IsFileLogEnabled() &&
            std::holds_alternative<SDCard>(dev->disk)) {
            m_logDevice = devId;
            OpenLogFile();
        }

        IPCLog::sInstance->NotifyDeviceInsertion(devId);
    }
}

bool DeviceMgr::OpenLogFile()
{
    PRINT(IOS_DevMgr, INFO, "Opening log file");

    char path[16] = "0:log.txt";
    path[0] = m_logDevice + '0';

    auto fret = f_open(&m_logFile, path, FA_CREATE_ALWAYS | FA_WRITE);
    if (fret != FR_OK) {
        PRINT(IOS_DevMgr, ERROR, "Failed to open log file: %d", fret);
        return false;
    }

    m_logEnabled = true;
    PRINT(IOS_DevMgr, INFO, "Log file opened");
    return true;
}
