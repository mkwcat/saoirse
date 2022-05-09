// USB.hpp - USB2 Device I/O
//   Written by Palapeli
//
// Copyright (C) 2022 Team Saoirse
// SPDX-License-Identifier: MIT

#pragma once

#include <System/OS.hpp>
#include <System/Types.h>

class USB
{
public:
    static USB* sInstance;

    enum class USBv5Ioctl
    {
        GetVersion = 0,
        GetDeviceChange = 1,
        Shutdown = 2,
        GetDeviceInfo = 3,
        Attach = 4,
        Release = 5,
        AttachFinish = 6,
        SetAlternateSetting = 7,

        SuspendResume = 16,
        CancelEndpoint = 17,
        CtrlTransfer = 18,
        IntrTransfer = 19,
        IsoTransfer = 20,
        BulkTransfer = 21,
    };

    enum class USBError
    {
        /* Results from IOS or the resource manager */
        OK = IOSError::OK,
        NoAccess = IOSError::NoAccess,
        Invalid = IOSError::Invalid,

        /* Results from this interface */
        ShortTransfer = -2000,

        /* Results from the host controller */
        Halted = -7102,
    };

    enum class ClassCode : u8
    {
        HID = 0x03,
        MassStorage = 0x8,
    };

    enum class SubClass : u8
    {
        MassStorage_SCSI = 0x06,
    };

    enum class Protocol : u8
    {
        MassStorage_BulkOnly = 0x50,
    };

    /* Common bitmasks */
    struct CtrlType {
        static constexpr u32 Dir_Mask = (1 << 7);
        static constexpr u32 Dir_Host2Device = (0 << 7);
        static constexpr u32 Dir_Device2Host = (1 << 7);

        static constexpr u32 TransferType_Mask = 3;
        static constexpr u32 TransferType_Control = 0;
        static constexpr u32 TransferType_Isochronous = 1;
        static constexpr u32 TransferType_Bulk = 2;
        static constexpr u32 TransferType_Interrupt = 3;

        static constexpr u32 Rec_Mask = 31;
        static constexpr u32 Rec_Device = 0;
        static constexpr u32 Rec_Interface = 1;
        static constexpr u32 Rec_Endpoint = 2;
        static constexpr u32 Rec_Other = 3;

        static constexpr u32 ReqType_Mask = (3 << 5);
        static constexpr u32 ReqType_Standard = (0 << 5);
        static constexpr u32 ReqType_Class = (1 << 5);
        static constexpr u32 ReqType_Vendor = (2 << 5);
        static constexpr u32 ReqType_Reserved = (3 << 5);
        CtrlType() = delete;
    };

    static constexpr u32 DirEndpointIn = 0x80;
    static constexpr u32 DirEndpointOut = 0x00;

    static constexpr u32 MaxDevices = 32;

    struct DeviceEntry {
        u32 devId;
        u16 vid;
        u16 pid;
        u16 devNum2;
        u8 ifNum;
        u8 altSetCount;
    };
    static_assert(sizeof(DeviceEntry) == 0xC);

    struct Input {
        u32 fd;
        u32 heapBuffers;
        union {
            struct {
                u8 requestType;
                u8 request;
                u16 value;
                u16 index;
                u16 length;
                void* data;
            } ctrl;

            struct {
                void* data;
                u16 length;
                u8 pad[4];
                u8 endpoint;
            } bulk;

            struct {
                void* data;
                u16 length;
                u8 endpoint;
            } intr;

            struct {
                void* data;
                void* packetSizes;
                u8 packets;
                u8 endpoint;
            } iso;

            struct {
                u16 pid;
                u16 vid;
            } notify;

            u32 args[14];
        };
    };

    struct DeviceDescriptor {
        u8 length;
        u8 descType;
        u16 usbVer;
        u8 devClass;
        u8 devSubClass;
        u8 devProtocol;
        u8 maxPacketSize0;
        u16 vid;
        u16 pid;
        u16 devVer;
        u8 manufacturer;
        u8 product;
        u8 serialNum;
        u8 numConfigs;
        u8 _12[0x14 - 0x12];
    };
    static_assert(sizeof(DeviceDescriptor) == 0x14);

    struct ConfigDescriptor {
        u8 length;
        u8 descType;
        u16 totalLength;
        u8 numInterfaces;
        u8 configValue;
        u8 config;
        u8 attributes;
        u8 maxPower;
        u8 _9[0xC - 0x9];
    };
    static_assert(sizeof(ConfigDescriptor) == 0xC);

    struct InterfaceDescriptor {
        u8 length;
        u8 descType;
        u8 ifNum;
        u8 altSetting;
        u8 numEndpoints;
        ClassCode ifClass;
        SubClass ifSubClass;
        Protocol ifProtocol;
        u8 interface;
        u8 _9[0xC - 0x9];
    };
    static_assert(sizeof(InterfaceDescriptor) == 0xC);

    struct EndpointDescriptor {
        u8 length;
        u8 descType;
        u8 endpointAddr;
        u8 attributes;
        u16 maxPacketSize;
        u8 interval;
        u8 _7[0x8 - 0x7];
    };
    static_assert(sizeof(EndpointDescriptor) == 0x8);

    struct DeviceInfo {
        u32 devId;
        u8 _4[0x14 - 0x04];
        DeviceDescriptor device;
        ConfigDescriptor config;
        InterfaceDescriptor interface;
        EndpointDescriptor endpoint[16];
    };
    static_assert(sizeof(DeviceInfo) == 0xC0);

    USB(s32 id);

    /*
     * Initialize the interface.
     */
    bool Init();

    bool IsOpen() const
    {
        return ven.fd() >= 0;
    }

    /*
     * Aynchronous call to get the next device change. Sends 'req' to 'queue'
     * when GetDeviceChange responds.
     * devices - Output device entries, must have USB::MaxDevices entries,
     * 32-bit aligned, MEM2 virtual = physical address.
     */
    bool EnqueueDeviceChange(DeviceEntry* devices, Queue<IOS::Request*>* queue,
                             IOS::Request* req);

    /*
     * Get USB descriptors for a device.
     */
    USBError GetDeviceInfo(u32 devId, DeviceInfo* outInfo, u8 alt = 0);

    /*
     * Attaches the provided device to the current handle.
     */
    USBError Attach(u32 devId);

    enum class State
    {
        Suspend = 0,
        Resume = 1,
    };

    /*
     * Suspend or resume a device. Returns Invalid if the new state is the same
     * as the current one.
     */
    USBError SuspendResume(u32 devId, State state);

    /*
     * Cancel ongoing transfer on an endpoint.
     */
    USBError CancelEndpoint(u32 devId, u8 endpoint);

    /*
     * Read interrupt transfer on the device.
     */
    USBError ReadIntrMsg(u32 devId, u8 endpoint, u16 length, void* data)
    {
        return IntrBulkMsg(devId, USBv5Ioctl::IntrTransfer, endpoint, length,
                           data);
    }

    /*
     * Read bulk transfer on the device.
     */
    USBError ReadBulkMsg(u32 devId, u8 endpoint, u16 length, void* data)
    {
        return IntrBulkMsg(devId, USBv5Ioctl::BulkTransfer, endpoint, length,
                           data);
    }

    /*
     * Read control message on the device.
     */
    USBError ReadCtrlMsg(u32 devId, u8 requestType, u8 request, u16 value,
                         u16 index, u16 length, void* data)
    {
        return CtrlMsg(devId, requestType, request, value, index, length, data);
    }

    /*
     * Write interrupt transfer on the device.
     */
    USBError WriteIntrMsg(u32 devId, u8 endpoint, u16 length, void* data)
    {
        return IntrBulkMsg(devId, USBv5Ioctl::IntrTransfer, endpoint, length,
                           data);
    }

    /*
     * Write bulk transfer on the device.
     */
    USBError WriteBulkMsg(u32 devId, u8 endpoint, u16 length, void* data)
    {
        return IntrBulkMsg(devId, USBv5Ioctl::BulkTransfer, endpoint, length,
                           data);
    }

    /*
     * Read control message on the device.
     */
    USBError WriteCtrlMsg(u32 devId, u8 requestType, u8 request, u16 value,
                          u16 index, u16 length, void* data)
    {
        return CtrlMsg(devId, requestType, request, value, index, length, data);
    }

private:
    USBError CtrlMsg(u32 devId, u8 requestType, u8 request, u16 value,
                     u16 index, u16 length, void* data);

    USBError IntrBulkMsg(u32 devId, USBv5Ioctl ioctl, u8 endpoint, u16 length,
                         void* data);

    IOS::ResourceCtrl<USBv5Ioctl> ven{-1};
    Thread m_thread;
    bool m_reqSent = false;
};