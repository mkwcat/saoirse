#include <types.h>
#include <main.h>
#include <ios.h>
#include <os.h>
#include <ff.h>
#include <disk.h>
#include <sdcard.h>
#include <cstring>

namespace EFS
{

static s32 ForwardRequest([[maybe_unused]] IOS::Request* req)
{
    /* TODO */
    return IOSErr::NotFound;
}

/* 
 * Handle open request from the filesystem proxy.
 * Returns: File descriptor, or ISFS error code.
 */
static s32 ReqOpen([[maybe_unused]] const char* path, [[maybe_unused]] u32 mode)
{
    /* TODO */
    return IOSErr::NotFound;
}

/*
 * Close open file descriptor.
 * Returns: 0 for success, or IOS error code.
 */
static s32 ReqClose([[maybe_unused]] s32 fd)
{
    /* TODO */
    return IOSErr::Invalid;
}

/*
 * Read data from open file descriptor.
 * Returns: Amount read, or ISFS error code.
 */
static s32 ReqRead([[maybe_unused]] s32 fd, [[maybe_unused]] void* data,
                   [[maybe_unused]] u32 len)
{
    /* TODO */
    return IOSErr::Invalid;
}

/*
 * Write data to open file descriptor.
 * Returns: Amount wrote, or ISFS error code.
 */
static s32 ReqWrite([[maybe_unused]] s32 fd, [[maybe_unused]] const void* data,
                   [[maybe_unused]] u32 len)
{
    /* TODO */
    return IOSErr::Invalid;
}

static s32 IPCRequest(IOS::Request* req)
{
    s32 ret = IOSErr::Invalid;

    switch (req->cmd)
    {
    case IOS::Command::Open:
        if (req->open.path[0] == '?')
            ret = ReqOpen(req->open.path, req->open.mode);
        else
            ret = IOSErr::NotFound;
        break;
    
    case IOS::Command::Close:
        ret = ReqClose(req->fd);
        break;
    
    case IOS::Command::Read:
        ret = ReqRead(req->fd, req->read.data, req->read.len);
        break;
    
    case IOS::Command::Write:
        ret = ReqWrite(req->fd, req->write.data, req->write.len);
        break;
    
    default:
        peli::Log(LogL::ERROR,
            "EFS: Unknown command: %u", static_cast<u32>(req->cmd));
        break;
    }

    /* Not Found (-6) forwards the message to real FS */
    if (ret == IOSErr::NotFound)
        return ForwardRequest(req);
    return ret;
}

static void OpenTestFile()
{
    /* We must attempt to open a file first for FatFS to function properly */
    FIL testFile;
    FRESULT fret = f_open(&testFile, "0:/", FA_READ);
    peli::Log(LogL::INFO, "Test open result: %d", fret);
}

extern "C" s32 FS_StartRM([[maybe_unused]] void* arg)
{
    usleep(10000);

    peli::Log(LogL::INFO, "Starting FS...");

    if (!SDCard::Open()) {
        peli::Log(LogL::ERROR, "FS_StartRM: SDCard::Open returned false");
        abort();
    }
    FSServ::MountSDCard();
    peli::Log(LogL::INFO, "SD card mounted");

    OpenTestFile();

    Queue<IOS::Request*> queue(8);

    /* [TODO] ? is temporary until we can actually mount over / */
    const s32 ret = IOS_RegisterResourceManager("?", queue.id());
    if (ret != IOSErr::OK) {
        peli::Log(LogL::ERROR,
            "FS_StartRM: IOS_RegisterResourceManager failed: %d", ret);
        abort();
    }

    while (true) {
        IOS::Request* req = queue.receive();
        IOS_ResourceReply(reinterpret_cast<IOSRequest*>(req), IPCRequest(req));
    }
    /* Can never reach here */
    return 0;
}

}