#include "usb.h"
#include <types.h>
#include <util.h>

s32 USB::ctrlMsg(s32 devId, u8 requestType, u8 request, u16 value, u16 index,
                 u16 length, void* data)
{
    if (!aligned(data, 32))
        return IOSErr::Invalid;
    if (length && !data)
        return IOSErr::Invalid;
    if (!length && data)
        return IOSErr::Invalid;

    Message msg ATTRIBUTE_ALIGN(32) = {
        .fd = devId,
        .heapBuffers = 0,
        .ctrl = {
            .requestType = requestType,
            .request = request,
            .value = value,
            .index = index,
            .length = length,
            .data = data
        },
        .vec = {
            {.data = &msg, .len = 64},
            {.data = data, .len = length}
        }
    };

    bool isInput = requestType & DirDevice2Host;
    return ven.ioctlv(USBv5Ioctl::CtrlTransfer,
        isInput ? 2 : 1, isInput ? 0 : 1, msg.vec);
}

s32 USB::intrBulkMsg(s32 devId, USBv5Ioctl ioctl, u8 endpoint, u16 length,
                     void* data)
{
    if (!aligned(data, 32))
        return IOSErr::Invalid;
    if (length && !data)
        return IOSErr::Invalid;
    if (!length && data)
        return IOSErr::Invalid;
    
    Message msg ATTRIBUTE_ALIGN(32) = {
        .fd = devId,
        .heapBuffers = 0,
        .pad = {0},
        .vec = {
            {.data = &msg, .len = 64},
            {.data = data, .len = length}
        }
    };

    if (ioctl == USBv5Ioctl::IntrTransfer) {
        msg.intr = {
            data = data,
            length = length,
            endpoint = endpoint
        };
    } else if (ioctl == USBv5Ioctl::BulkTransfer) {
        msg.intr = {
            data = data,
            length = length,
            endpoint = endpoint
        };
    } else {
        return IOSErr::Invalid;
    }

    bool isInput = endpoint & DirEndpointIn;
    return ven.ioctlv(ioctl, isInput ? 2 : 1, isInput ? 0 : 1, msg.vec);
}
