#include "IODeviceManager.hpp"
#include "irse.h"
#include <unistd.h>

IODeviceManager* IODeviceManager::sInstance;

s32 IODeviceManager::threadEntry([[maybe_unused]] void* arg)
{
    IODeviceManager::sInstance = new IODeviceManager;
    IODeviceManager::sInstance->init();

    while (1) {
        usleep(100000);
        IODeviceManager::sInstance->eventLoop();
    }

    return 0;
}

constexpr int DEVLIST_MAXSIZE = 8;
constexpr int USB_CLASS_MASS_STORAGE = 0x08;

void IODeviceManager::init()
{
    USB_Initialize();
    USBStorage_Initialize();
}

int IODeviceManager::getMSCCount() const
{
    int count = 0;
    for (int i = 0; i < maxMSCCount; i++) {
        if (m_mscHandle[i].m_inserted)
            count++;
    }
    return count;
}

int IODeviceManager::getFreeMSCHandle() const
{
    for (int i = 0; i < maxMSCCount; i++) {
        if (!m_mscHandle[i].m_inserted)
            return i;
    }
    // This theoretically should never happen, even if there were more than
    // 8 USB ports. The driver just doesn't allow it.
    irse::Log(LogS::IOMgr, LogL::ERROR,
              "No free MSC handle available. This should not happen!");
    abort();
}

void IODeviceManager::insertMSCDevice(s32 deviceId, u16 vid, u16 pid)
{
    irse::Log(LogS::IOMgr, LogL::INFO,
              "Insert USB Mass Storage Device:\n"
              "deviceId: 0x%08X\n"
              "vid: 0x%04X\n"
              "pid: 0x%04X",
              deviceId, vid, pid);

    int entry = getFreeMSCHandle();
    m_mscHandle[entry] = {
        .m_inserted = true,
        .m_valid = false,
        .m_deviceId = deviceId,
        .m_vid = vid,
        .m_pid = pid,
        .m_dev = m_mscHandle[entry].m_dev,
    };
}

void IODeviceManager::initMSC()
{
    usb_device_entry devList[DEVLIST_MAXSIZE];
    memset(devList, 0, sizeof(devList));

    u8 deviceCount;
    s32 ret = USB_GetDeviceList(devList, DEVLIST_MAXSIZE,
                                USB_CLASS_MASS_STORAGE, &deviceCount);
    if (ret < 0) {
        irse::Log(LogS::IOMgr, LogL::ERROR, "USB_GetDeviceList failed! (%d)",
                  ret);
        abort();
    }

    if (deviceCount > 0) {
        irse::Log(LogS::IOMgr, LogL::WARN,
                  "Initializing %d USB storage device(s)", deviceCount);

        for (int i = 0; i < deviceCount; i++) {
            insertMSCDevice(devList[i].device_id, devList[i].vid,
                            devList[i].pid);
        }
    }
}

void IODeviceManager::eventLoop()
{
}