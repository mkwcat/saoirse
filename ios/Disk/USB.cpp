// USB.cpp - USB2 Device I/O
//   Written by Palapeli
//
// Copyright (C) 2022 Team Saoirse
// SPDX-License-Identifier: MIT

#if 0

#include "USB.hpp"
#include <Debug/Log.hpp>
#include <IOS/Syscalls.h>
#include <System/Types.h>
#include <System/Util.h>

USB::USB(s32 id)
{
    if (id >= 0)
        new (&ven) IOS::ResourceCtrl<USBv5Ioctl>("/dev/usb/ven", id);
}

bool USB::Init()
{
    if (ven.fd() < 0) {
        PRINT(IOS_USB, ERROR, "Failed to open /dev/usb/ven: %d", ven.fd());
        return false;
    }

    // Check USB RM version.
    u32* verBuffer = (u32*)IOS::Alloc(32);
    s32 ret = ven.ioctl(USBv5Ioctl::GetVersion, nullptr, 0,
                        reinterpret_cast<void*>(verBuffer), 32);
    u32 ver = verBuffer[0];
    IOS::Free(verBuffer);

    if (ret != IOSError::OK) {
        PRINT(IOS_USB, ERROR, "GetVersion error: %d", ret);
        return false;
    }

    if (ver != 0x00050001) {
        PRINT(IOS_USB, ERROR, "Unrecognized USB RM version: 0x%X", ver);
        return false;
    }

    // Create USB device thread.
    m_thread.create(ThreadEntry, reinterpret_cast<void*>(this), nullptr, 0x800,
                    40);
    return true;
}

s32 USB::ThreadEntry(void* arg)
{
    USB* that = reinterpret_cast<USB*>(arg);
    that->Run();

    return 0;
}

void USB::Run()
{
    DeviceEntry* devices =
        (DeviceEntry*)IOS::Alloc(sizeof(DeviceEntry) * MaxDevices);

    while (true) {
        // big TODO
        ven.ioctl(USBv5Ioctl::GetDeviceChange, nullptr, 0,
                  reinterpret_cast<void*>(devices),
                  sizeof(DeviceEntry) * MaxDevices);
    }
}

s32 USB::CtrlMsg(s32 devId, u8 requestType, u8 request, u16 value, u16 index,
                 u16 length, void* data)
{
    // Must be in a physical = virtual region.
    ASSERT((u32)data >= 0x10000000 && (u32)data < 0x14000000);

    if (!aligned(data, 32))
        return IOSError::Invalid;
    if (length && !data)
        return IOSError::Invalid;
    if (!length && data)
        return IOSError::Invalid;

    Input msg ATTRIBUTE_ALIGN(32) = {
        .fd = devId,
        .heapBuffers = 0,
        .ctrl =
            {
                .requestType = requestType,
                .request = request,
                .value = value,
                .index = index,
                .length = length,
                .data = data,
            },
    };

    if (requestType & CtrlType::Dir_Device2Host) {
        IOS::IVector<2> vec;
        vec.in[0].data = &msg;
        vec.in[0].len = sizeof(Input);
        vec.in[1].data = data;
        vec.in[1].len = length;
        return ven.ioctlv(USBv5Ioctl::CtrlTransfer, vec);
    } else {
        IOS::IOVector<1, 1> vec;
        vec.in[0].data = &msg;
        vec.in[0].len = sizeof(Input);
        vec.out[0].data = data;
        vec.out[0].len = length;
        return ven.ioctlv(USBv5Ioctl::CtrlTransfer, vec);
    }
}

s32 USB::IntrBulkMsg(s32 devId, USBv5Ioctl ioctl, u8 endpoint, u16 length,
                     void* data)
{
    // Must be in a physical = virtual region.
    ASSERT((u32)data >= 0x10000000 && (u32)data < 0x14000000);

    if (!aligned(data, 32))
        return IOSError::Invalid;
    if (length && !data)
        return IOSError::Invalid;
    if (!length && data)
        return IOSError::Invalid;

    Input msg ATTRIBUTE_ALIGN(32) = {
        .fd = devId,
        .heapBuffers = 0,
        .args = {0},
    };

    if (ioctl == USBv5Ioctl::IntrTransfer) {
        msg.intr = {
            .data = data,
            .length = length,
            .endpoint = endpoint,
        };
    } else if (ioctl == USBv5Ioctl::BulkTransfer) {
        msg.intr = {
            .data = data,
            .length = length,
            .endpoint = endpoint,
        };
    } else {
        return IOSError::Invalid;
    }

    if (endpoint & DirEndpointIn) {
        IOS::IVector<2> vec;
        vec.in[0].data = &msg;
        vec.in[0].len = sizeof(Input);
        vec.in[1].data = data;
        vec.in[1].len = length;
        return ven.ioctlv(ioctl, vec);
    } else {
        IOS::IOVector<1, 1> vec;
        vec.in[0].data = &msg;
        vec.in[0].len = sizeof(Input);
        vec.out[0].data = data;
        vec.out[0].len = length;
        return ven.ioctlv(ioctl, vec);
    }
}

s32 USB::ClearHalt(s32 devId, u8 endpoint)
{
    s32 msg[8] ATTRIBUTE_ALIGN(32) = {
        devId, 0, endpoint, 0, 0, 0, 0, 0,
    };
    return ven.ioctl(USBv5Ioctl::CancelEndpoint, reinterpret_cast<void*>(msg),
                     sizeof(msg), nullptr, 0);
}

#endif