// DeviceMgr.cpp - I/O storage device manager
//   Written by Palapeli
//
// Copyright (C) 2022 Team Saoirse
// SPDX-License-Identifier: MIT

#include "DeviceMgr.hpp"
#include <Debug/Log.hpp>
#include <Disk/SDCard.hpp>
#include <System/Config.hpp>
#include <System/Types.h>

DeviceMgr* DeviceMgr::sInstance;

DeviceMgr::DeviceMgr()
{
    // 64 ms repeating timer
    m_timer = IOS_CreateTimer(0, 64000, m_timerQueue.id(), 0);
    ASSERT(m_timer >= 0);

    bool ret = SDCard::Open();
    ASSERT(ret);

    // Reset everything to default.
    for (int i = 0; i < DEVICE_COUNT; i++) {
        InitHandle((DeviceKind)i);
    }

    m_thread.create(ThreadEntry, reinterpret_cast<void*>(this), nullptr, 0x800,
                    40);
}

bool DeviceMgr::IsInserted(DeviceKind device)
{
    ASSERT((u32)device < DEVICE_COUNT);

    return m_devices[device].inserted & !m_devices[device].error;
}

bool DeviceMgr::IsMounted(DeviceKind device)
{
    ASSERT((u32)device < DEVICE_COUNT);

    return IsInserted(device) && m_devices[device].mounted;
}

void DeviceMgr::SetError(DeviceKind device)
{
    ASSERT((u32)device < DEVICE_COUNT);

    m_devices[device].error = true;
}

int DeviceMgr::DeviceKindToDRV(DeviceKind device)
{
    ASSERT((u32)device < DEVICE_COUNT);

    return static_cast<s32>(device);
}

DeviceMgr::DeviceKind DeviceMgr::DRVToDeviceKind(int drv)
{
    ASSERT((u32)drv < DEVICE_COUNT);

    return static_cast<DeviceKind>(drv);
}

FATFS* DeviceMgr::GetFilesystem(DeviceKind device)
{
    ASSERT((u32)device < DEVICE_COUNT);

    return &m_devices[device].fs;
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

bool DeviceMgr::DeviceInit(DeviceKind device)
{
    ASSERT((u32)device < DEVICE_COUNT);

    if (device == Dev_SDCard) {
        if (SDCard::Startup())
            return true;

        SetError(device);
        ForceUpdate();
        PRINT(IOS_DevMgr, ERROR, "SDCard::Startup failed");
        return false;
    }

    PRINT(IOS_DevMgr, ERROR, "Device not recognized: %d", device);
    return false;
}

bool DeviceMgr::DeviceRead(DeviceKind device, void* data, u32 sector, u32 count)
{
    ASSERT((u32)device < DEVICE_COUNT);

    if (device == Dev_SDCard) {
        auto ret = SDCard::ReadSectors(sector, count, data);
        if (ret == IOSError::OK)
            return true;

        SetError(device);
        ForceUpdate();
        PRINT(IOS_DevMgr, ERROR, "SDCard::ReadSectors failed: %08X", ret);
        return false;
    }

    PRINT(IOS_DevMgr, ERROR, "Device not recognized: %d", device);
    return false;
}

bool DeviceMgr::DeviceWrite(DeviceKind device, const void* data, u32 sector,
                            u32 count)
{
    ASSERT((u32)device < DEVICE_COUNT);

    if (device == Dev_SDCard) {
        auto ret = SDCard::WriteSectors(sector, count, data);
        if (ret == 0)
            return true;

        SetError(device);
        ForceUpdate();
        PRINT(IOS_DevMgr, ERROR, "SDCard::WriteSectors failed: %08X", ret);
        return false;
    }

    PRINT(IOS_DevMgr, ERROR, "Device not recognized: %d", device);
    return false;
}

bool DeviceMgr::DeviceSync(DeviceKind device)
{
    ASSERT((u32)device < DEVICE_COUNT);

    if (device == Dev_SDCard) {
        return true;
    }

    PRINT(IOS_DevMgr, ERROR, "Device not recognized: %d", device);
    return false;
}

void DeviceMgr::Run()
{
    PRINT(IOS_DevMgr, INFO, "Entering DeviceMgr...");
    PRINT(IOS_DevMgr, INFO, "DevMgr thread ID: %d", IOS_GetThreadId());

    while (true) {
        // Wait for 64 ms.
        m_timerQueue.receive(0);

        m_devices[Dev_SDCard].inserted = SDCard::IsInserted();

        // TODO: USB

        for (int i = 0; i < DEVICE_COUNT; i++) {
            UpdateHandle((DeviceKind)i);
        }
    }
}

s32 DeviceMgr::ThreadEntry(void* arg)
{
    DeviceMgr* that = reinterpret_cast<DeviceMgr*>(arg);
    that->Run();

    return 0;
}

void DeviceMgr::InitHandle(DeviceKind id)
{
    ASSERT((u32)id < DEVICE_COUNT);

    m_devices[id].inserted = false;
    m_devices[id].error = false;
    m_devices[id].mounted = false;
}

void DeviceMgr::UpdateHandle(DeviceKind id)
{
    ASSERT((u32)id < DEVICE_COUNT);
    DeviceHandle* dev = &m_devices[id];

    // Clear error if the device has been ejected, so we can try again if it's
    // reinserted.
    if (!dev->inserted)
        dev->error = false;

    if (!dev->inserted && dev->mounted) {
        // Disable file log if it was writing to this device
        if (m_logEnabled && id == m_logDevice) {
            m_logEnabled = false;
        }

        PRINT(IOS_DevMgr, INFO, "Unmount device %d", id);

        // If we don't finish then it's an error.
        dev->error = true;
        dev->mounted = false;

        FRESULT fret = f_unmount("0:");
        if (fret != FR_OK) {
            PRINT(IOS_DevMgr, ERROR, "Failed to unmount device %d: %d", id,
                  fret);
            return;
        }

        PRINT(IOS_DevMgr, INFO, "Successfully unmounted device %d", id);

        dev->error = false;
    }

    if (dev->inserted && !dev->mounted && !dev->error) {
        // Mount the device.
        PRINT(IOS_DevMgr, INFO, "Mount device %d", id);

        // If we don't finish then it's an error.
        dev->error = true;

        // Create drv str.
        char str[16] = "0:";
        str[0] = DeviceKindToDRV(id) + '\0';

        FRESULT fret = f_mount(&dev->fs, str, 0);
        if (fret != FR_OK) {
            PRINT(IOS_DevMgr, ERROR, "Failed to mount device %d: %d", id, fret);
            return;
        }

        // Create default path str.
        char str2[16] = "0:/saoirse";
        str2[0] = DeviceKindToDRV(id) + '0';

        fret = f_chdir(str2);
        if (fret != FR_OK) {
            PRINT(IOS_DevMgr, ERROR, "Failed to change directory to %s: %d",
                  str2, fret);
            return;
        }

        PRINT(IOS_DevMgr, INFO, "Successfully mounted device %d", id);

        dev->mounted = true;
        dev->error = false;

        // Open log file if it's enabled
        if (!m_logEnabled && Config::sInstance->IsFileLogEnabled() &&
            id == m_logDevice) {
            OpenLogFile();
        }
    }
}

bool DeviceMgr::OpenLogFile()
{
    PRINT(IOS_DevMgr, INFO, "Opening log file");

    char path[16] = "0:log.txt";
    path[0] = DeviceKindToDRV(m_logDevice) + '0';

    auto fret = f_open(&m_logFile, path, FA_CREATE_ALWAYS | FA_WRITE);
    if (fret != FR_OK) {
        PRINT(IOS_DevMgr, ERROR, "Failed to open log file: %d", fret);
        return false;
    }

    m_logEnabled = true;
    PRINT(IOS_DevMgr, INFO, "Log file opened");
    return true;
}