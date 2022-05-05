// DeviceMgr.hpp - I/O storage device manager
//   Written by Palapeli
//
// Copyright (C) 2022 Team Saoirse
// SPDX-License-Identifier: MIT

#pragma once

#include <FAT/ff.h>
#include <System/OS.hpp>
#include <cstring>

class DeviceMgr
{
public:
    static DeviceMgr* sInstance;

    DeviceMgr();
    ~DeviceMgr();

    enum DeviceKind
    {
        Dev_SDCard,
        Dev_USB0,
        Dev_USB1,
        Dev_USB2,
        Dev_USB3,
        Dev_USB4,
        Dev_USB5,
        Dev_USB6,
        Dev_USB7,
        DEVICE_COUNT,
        Dev_None = -1,
    };

    bool IsInserted(DeviceKind device);
    bool IsMounted(DeviceKind device);
    void SetError(DeviceKind device);
    static int DeviceKindToDRV(DeviceKind device);
    static DeviceKind DRVToDeviceKind(int drv);
    FATFS* GetFilesystem(DeviceKind device);
    void ForceUpdate();
    bool IsLogEnabled();
    void WriteToLog(const char* str, u32 len);

    bool DeviceInit(DeviceKind device);
    bool DeviceRead(DeviceKind device, void* data, u32 sector, u32 count);
    bool DeviceWrite(DeviceKind device, const void* data, u32 sector,
                     u32 count);
    bool DeviceSync(DeviceKind device);

private:
    void Run();
    static s32 ThreadEntry(void* arg);

    struct DeviceHandle {
        FATFS fs;
        bool inserted;
        bool error;
        bool mounted;
    };

    void InitHandle(DeviceKind id);
    void UpdateHandle(DeviceKind id);
    bool OpenLogFile();

private:
    DeviceHandle m_devices[DEVICE_COUNT];

    Thread m_thread;
    Queue<int> m_timerQueue;
    s32 m_timer;

    bool m_logEnabled;
    DeviceKind m_logDevice = Dev_SDCard;
    FIL m_logFile;
};