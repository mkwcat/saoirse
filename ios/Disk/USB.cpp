// USB.cpp - USB2 Device I/O
//   Written by Palapeli
//
// SPDX-License-Identifier: MIT

#include "USB.hpp"
#include <Debug/Log.hpp>
#include <Disk/DeviceMgr.hpp>
#include <IOS/Syscalls.h>
#include <System/Types.h>
#include <System/Util.h>

USB* USB::sInstance = nullptr;

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
    u32* verBuffer = (u32*) IOS::Alloc(32);
    s32 ret = ven.ioctl(USBv5Ioctl::GetVersion, nullptr, 0, verBuffer, 32);
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

    return true;
}

/*
 * Aynchronous call to get the next device change. Sends 'req' to 'queue'
 * when GetDeviceChange responds.
 * devices - Output device entries, must have USB::MaxDevices entries,
 * 32-bit aligned, MEM2 virtual = physical address.
 */
bool USB::EnqueueDeviceChange(
  DeviceEntry* devices, Queue<IOS::Request*>* queue, IOS::Request* req)
{
    if (m_reqSent) {
        s32 ret = ven.ioctl(USBv5Ioctl::AttachFinish, nullptr, 0, nullptr, 0);
        if (ret != IOSError::OK) {
            PRINT(IOS_USB, ERROR, "AttachFinish error: %d", ret);
            return false;
        }
    }

    m_reqSent = false;
    s32 ret = ven.ioctlAsync(USBv5Ioctl::GetDeviceChange, nullptr, 0, devices,
      sizeof(DeviceEntry) * MaxDevices, queue, req);
    if (ret != IOSError::OK) {
        PRINT(IOS_USB, ERROR, "GetDeviceChange async error: %d", ret);
        return false;
    }

    m_reqSent = true;
    return true;
}

/*
 * Get USB descriptors for a device.
 */
USB::USBError USB::GetDeviceInfo(u32 devId, DeviceInfo* outInfo, u8 alt)
{
    u8* input = (u8*) IOS::Alloc(32);
    write32(input, devId);
    write8(input + 0x8, alt);

    void* tempInfo = IOS::Alloc(sizeof(DeviceInfo));

    s32 ret = ven.ioctl(
      USBv5Ioctl::GetDeviceInfo, input, 32, tempInfo, sizeof(DeviceInfo));
    memcpy(outInfo, tempInfo, sizeof(DeviceInfo));

    IOS::Free(input);
    IOS::Free(tempInfo);
    return static_cast<USBError>(ret);
}

/*
 * Attaches the provided device to the current handle.
 */
USB::USBError USB::Attach(u32 devId)
{
    u8* input = (u8*) IOS::Alloc(32);
    write32(input, devId);

    s32 ret = ven.ioctl(USBv5Ioctl::Attach, input, 32, nullptr, 0);

    IOS::Free(input);
    return static_cast<USBError>(ret);
}

/*
 * Suspend or resume a device. Returns Invalid if the new state is the same
 * as the current one.
 */
USB::USBError USB::SuspendResume(u32 devId, State state)
{
    u8* input = (u8*) IOS::Alloc(32);
    write32(input, devId);
    write8(input + 0xB, state == State::Resume ? 1 : 0);

    s32 ret = ven.ioctl(USBv5Ioctl::SuspendResume, input, 32, nullptr, 0);

    IOS::Free(input);
    return static_cast<USBError>(ret);
}

/*
 * Cancel ongoing transfer on an endpoint.
 */
USB::USBError USB::CancelEndpoint(u32 devId, u8 endpoint)
{
    u8* input = (u8*) IOS::Alloc(32);

    // Cancel all control messages
    write32(input, devId);
    write8(input + 0x8, endpoint);
    s32 ret = ven.ioctl(USBv5Ioctl::CancelEndpoint, input, 32, nullptr, 0);

    IOS::Free(input);
    return static_cast<USBError>(ret);
}

USB::USBError USB::CtrlMsg(u32 devId, u8 requestType, u8 request, u16 value,
  u16 index, u16 length, void* data)
{
    // Must be in a physical = virtual region.
    assert((u32) data >= 0x10000000 && (u32) data < 0x14000000);

    if (!aligned(data, 32))
        return USBError::Invalid;
    if (length && !data)
        return USBError::Invalid;
    if (!length && data)
        return USBError::Invalid;

    Input* msg = (Input*) IOS::Alloc(sizeof(Input));
    msg->fd = devId;
    msg->ctrl = {
      .requestType = requestType,
      .request = request,
      .value = value,
      .index = index,
      .length = length,
      .data = data,
    };

    s32 ret;
    if ((requestType & CtrlType::Dir_Mask) == CtrlType::Dir_Host2Device) {
        IOS::IVector<2> vec;
        IOS_FlushDCache(data, length);
        vec.in[0].data = msg;
        vec.in[0].len = sizeof(Input);
        vec.in[1].data = data;
        vec.in[1].len = length;
        ret = ven.ioctlv(USBv5Ioctl::CtrlTransfer, vec);
    } else {
        IOS::IOVector<1, 1> vec;
        IOS_FlushDCache(data, length);
        vec.in[0].data = msg;
        vec.in[0].len = sizeof(Input);
        vec.out[0].data = data;
        vec.out[0].len = length;
        ret = ven.ioctlv(USBv5Ioctl::CtrlTransfer, vec);
    }

    IOS::Free(msg);
    if ((ret - 8) == length)
        return USBError::OK;

    if (ret >= 0)
        return USBError::ShortTransfer;

    return static_cast<USBError>(ret);
}

USB::USBError USB::IntrBulkMsg(
  u32 devId, USBv5Ioctl ioctl, u8 endpoint, u16 length, void* data)
{
    // Must be in a physical = virtual region.
    assert((u32) data >= 0x10000000 && (u32) data < 0x14000000);

    if (!aligned(data, 32))
        return USBError::Invalid;
    if (length && !data)
        return USBError::Invalid;
    if (!length && data)
        return USBError::Invalid;

    Input* msg = (Input*) IOS::Alloc(sizeof(Input));
    msg->fd = devId;

    if (ioctl == USBv5Ioctl::IntrTransfer) {
        msg->intr = {
          .data = data,
          .length = length,
          .endpoint = endpoint,
        };
    } else if (ioctl == USBv5Ioctl::BulkTransfer) {
        msg->bulk = {
          .data = data,
          .length = length,
          .pad = {0},
          .endpoint = endpoint,
        };
    } else {
        IOS::Free(msg);
        return USBError::Invalid;
    }

    s32 ret;
    if (endpoint & DirEndpointIn) {
        IOS::IVector<2> vec;
        IOS_FlushDCache(data, length);
        vec.in[0].data = msg;
        vec.in[0].len = sizeof(Input);
        vec.in[1].data = data;
        vec.in[1].len = length;
        ret = ven.ioctlv(ioctl, vec);
    } else {
        IOS::IOVector<1, 1> vec;
        IOS_FlushDCache(data, length);
        vec.in[0].data = msg;
        vec.in[0].len = sizeof(Input);
        vec.out[0].data = data;
        vec.out[0].len = length;
        ret = ven.ioctlv(ioctl, vec);
    }

    IOS::Free(msg);
    if (ret == length)
        return USBError::OK;

    if (ret >= 0)
        return USBError::ShortTransfer;

    return static_cast<USBError>(ret);
}
