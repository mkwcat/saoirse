#pragma once
#include "util.h"
#include <string.h>
#include <types.h>
// saoirse implementation
#include <usbstorage.h>
LIBOGC_SUCKS_BEGIN
#include <ogc/usb.h>
// libogc implementation
#include <ogc/ipc.h>
#include <ogc/usbstorage.h>
LIBOGC_SUCKS_END

class IODeviceManager
{
public:
    static IODeviceManager* sInstance;
    static s32 threadEntry(void* arg);

    void eventLoop();
    int getMSCCount() const;
    int getFreeMSCHandle() const;
    void insertMSCDevice(s32 deviceId, u16 vid, u16 pid);
    void initMSC();
    void init();

    static constexpr int maxMSCCount = 8;

    struct MSCHandle
    {
        bool m_inserted;
        bool m_valid;
        s32 m_deviceId;
        u16 m_vid;
        u16 m_pid;
        usbstorage_handle m_dev;
    };

    MSCHandle m_mscHandle[maxMSCCount];
};