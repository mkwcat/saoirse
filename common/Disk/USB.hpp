#pragma once
#include <System/OS.hpp>
#include <System/Types.h>
#include <new>

class USB
{
public:
    static USB* sInstance;

    static constexpr s32 USB_OK = 0;
    static constexpr s32 USB_FAILED = 1;

    /* Common bitmasks */
    struct CtrlType {
        static constexpr u32 Dir_Host2Device = (0 << 7);
        static constexpr u32 Dir_Device2Host = (1 << 7);
        static constexpr u32 Type_Standard = (0 << 5);
        static constexpr u32 Type_Class = (1 << 5);
        static constexpr u32 Type_Vendor = (2 << 5);
        static constexpr u32 Type_Reserved = (3 << 5);
        static constexpr u32 Rec_Device = 0;
        static constexpr u32 Rec_Interface = 1;
        static constexpr u32 Rec_Endpoint = 2;
        static constexpr u32 Rec_Other = 3;
        CtrlType() = delete;
    };

    static constexpr u32 DirEndpointIn = 0x80;
    static constexpr u32 DirEndpointOut = 0x00;

    static constexpr u32 MaxDevices = 32;

    struct DeviceEntry {
        s32 devId;
        u16 vid;
        u16 pid;
        u32 token;
    };

    struct Input {
        s32 fd;
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

    enum class USBv5Ioctl
    {
        GetVersion = 0,
        GetDeviceChange = 1,
        Shutdown = 2,
        GetDevParams = 3,
        Attach = 4,
        Release = 5,
        AttachFinish = 6,
        SetAlternateSetting = 7,

        SuspendResume = 16,
        CancelEndpoint = 17,
        CtrlTransfer = 18,
        IntrTransfer = 19,
        IsoTransfer = 20,
        BulkTransfer = 21
    };

    USB(s32 id);

    bool isOpen() const
    {
        return ven.fd() >= 0;
    }

    s32 readIntrMsg(s32 devId, u8 endpoint, u16 length, void* data)
    {
        return intrBulkMsg(devId, USBv5Ioctl::IntrTransfer, endpoint, length,
                           data);
    }
    s32 readBlkMsg(s32 devId, u8 endpoint, u16 length, void* data)
    {
        return intrBulkMsg(devId, USBv5Ioctl::BulkTransfer, endpoint, length,
                           data);
    }
    s32 readCtrlMsg(s32 devId, u8 requestType, u8 request, u16 value, u16 index,
                    u16 length, void* data)
    {
        return ctrlMsg(devId, requestType, request, value, index, length, data);
    }

    s32 writeIntrMsg(s32 devId, u8 endpoint, u16 length, void* data)
    {
        return intrBulkMsg(devId, USBv5Ioctl::IntrTransfer, endpoint, length,
                           data);
    }
    s32 writeBlkMsg(s32 devId, u8 endpoint, u16 length, void* data)
    {
        return intrBulkMsg(devId, USBv5Ioctl::BulkTransfer, endpoint, length,
                           data);
    }
    s32 writeCtrlMsg(s32 devId, u8 requestType, u8 request, u16 value,
                     u16 index, u16 length, void* data)
    {
        return ctrlMsg(devId, requestType, request, value, index, length, data);
    }

    s32 clearHalt(s32 devId, u8 endpoint);

private:
    s32 ctrlMsg(s32 devId, u8 requestType, u8 request, u16 value, u16 index,
                u16 length, void* data);
    s32 intrBulkMsg(s32 devId, USBv5Ioctl ioctl, u8 endpoint, u16 length,
                    void* data);

    IOS::ResourceCtrl<USBv5Ioctl> ven{-1};
};