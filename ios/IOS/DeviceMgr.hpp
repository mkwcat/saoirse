// DeviceMgr.hpp - I/O storage device manager
//   Written by Palapeli
//
// Copyright (C) 2022 Team Saoirse
// SPDX-License-Identifier: MIT

#pragma once

#include <FAT/ff.h>
#include <System/OS.hpp>

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
    };

    bool IsInserted(DeviceKind device);
    bool IsMounted(DeviceKind device);
    static int DeviceKindToDRV(DeviceKind device);
    static DeviceKind DRVToDeviceKind(int drv);
    FATFS* GetFilesystem(DeviceKind device);

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

private:
    DeviceHandle m_devices[DEVICE_COUNT];

    Thread m_thread;
    Queue<int> m_timerQueue;
    s32 m_timer;
};