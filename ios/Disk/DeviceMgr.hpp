// DeviceMgr.hpp - I/O storage device manager
//   Written by Palapeli
//
// SPDX-License-Identifier: MIT

#pragma once

#include <Disk/SDCard.hpp>
#include <Disk/USB.hpp>
#include <Disk/USBStorage.hpp>
#include <FAT/ff.h>
#include <System/OS.hpp>
#include <cstring>
#include <variant>

class DeviceMgr
{
public:
    static DeviceMgr* sInstance;

    DeviceMgr();
    ~DeviceMgr();

    static constexpr u32 DeviceCount = 9;

    bool IsInserted(u32 devId);
    bool IsMounted(u32 devId);
    void SetError(u32 devId);
    FATFS* GetFilesystem(u32 devId);
    void ForceUpdate();

    u32 DRVToDevID(u32 drv) const
    {
        return drv;
    }

private:
    void USBFatal();
    void USBChange(USB::DeviceEntry* devices, u32 count);

public:
    bool IsLogEnabled();
    void WriteToLog(const char* str, u32 len);

    bool DeviceInit(u32 devId);
    bool DeviceRead(u32 devId, void* data, u32 sector, u32 count);
    bool DeviceWrite(u32 devId, const void* data, u32 sector, u32 count);
    bool DeviceSync(u32 devId);

private:
    void Run();
    static s32 ThreadEntry(void* arg);

    struct DeviceHandle {
        FATFS fs;
        std::variant<SDCard, USBStorage> disk;
        bool enabled;
        bool inserted;
        bool error;
        bool mounted;
    };

    void InitHandle(u32 devId);
    void UpdateHandle(u32 devId);
    bool OpenLogFile();

private:
    DeviceHandle m_devices[DeviceCount];

    struct USBDeviceHandle {
        bool inUse;
        u32 usbId;
        u32 intId;
    };

    USBDeviceHandle m_usbDevices[USB::MaxDevices];

    u32 m_usbDeviceCount = 0;
    bool m_usbError = false;

    Thread m_thread;
    Queue<IOS::Request*> m_timerQueue;
    s32 m_timer;

    bool m_logEnabled;
    u32 m_logDevice;
    FIL m_logFile;
};
