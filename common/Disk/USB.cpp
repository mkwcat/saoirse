// USB.cpp - USB2 Device I/O
//   Written by Palapeli
//
// Copyright (C) 2022 Team Saoirse
// SPDX-License-Identifier: MIT

#include "USB.hpp"
#include <System/Types.h>
#include <System/Util.h>

USB::USB(s32 id)
{
    if (id >= 0)
        new (&ven) IOS::ResourceCtrl<USBv5Ioctl>("/dev/usb/ven", id);
}

s32 USB::ctrlMsg(s32 devId, u8 requestType, u8 request, u16 value, u16 index,
                 u16 length, void* data)
{
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

s32 USB::intrBulkMsg(s32 devId, USBv5Ioctl ioctl, u8 endpoint, u16 length,
                     void* data)
{
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
        msg.intr = {data = data, length = length, endpoint = endpoint};
    } else if (ioctl == USBv5Ioctl::BulkTransfer) {
        msg.intr = {data = data, length = length, endpoint = endpoint};
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

s32 USB::clearHalt(s32 devId, u8 endpoint)
{
    s32 msg[8] ATTRIBUTE_ALIGN(32) = {devId, 0, endpoint, 0, 0, 0, 0, 0};
    return ven.ioctl(USBv5Ioctl::CancelEndpoint, reinterpret_cast<void*>(msg),
                     sizeof(msg), nullptr, 0);
}