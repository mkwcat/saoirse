// Loader.cpp - IOS module loader
//   Written by Palapeli
//
// SPDX-License-Identifier: MIT

#include <Boot/AddressMap.hpp>
#include <Debug/Console.hpp>
#include <Debug/Debug_VI.hpp>
#include <IOS/Syscalls.h>
#include <IOS/System.hpp>
#include <System/Hollywood.hpp>
#include <System/ISFS.hpp>
#include <System/OS.hpp>
#include <System/Util.h>
#include <cstring>

// Uncomment this line to enable logging. This will break the module's IPC log.
// #define LOADER_DEBUG

#ifdef LOADER_DEBUG

#  include <Debug/Log.hpp>

#  define LOADER_PRINT(...) PRINT(IOS_Loader, __VA_ARGS__)
#  define LOADER_ASSERT(...) assert(__VA_ARGS__)

#else

// To prevent linking the enormous print functions in.
#  define LOADER_PRINT(...)
#  define LOADER_ASSERT(expr) (((expr) ? (void) 0 : LoaderAssertFail(__LINE__)))

#endif

namespace Loader
{

#define DEVICE_NAME "/dev/sao_loader"

// clang-format off
ASM_ARM_FUNCTION(static u32 GetStackPointer(),
    mov     r0, sp;
    bx      lr;
)

// clang-format on

void LoaderAssertFail(int line)
{
    Console::Print("error!\n");

    if (IOS_GetThreadId() != 0) {
        IOS_CancelThread(0, 0);
        // Shouldn't return, but spin infinitely if it somehow does.
    }

    IOS_SetThreadPriority(0, 0);
    while (true) {
    }
}

static s32 s_fileRMQueue = -1;
static bool s_isOpened = false;

// Memory file information.
static u8* s_fileAddr = nullptr;
static u32 s_fileSize = 0;
static u32 s_filePos = 0;

static s32 ReqOpen(const char* path, u32 mode)
{
    // Check the full path (/dev/sao_loader* would be caught here).
    if (strcmp(path, DEVICE_NAME) != 0) {
        return IOSError::NotFound;
    }

    // Check if the device has already been opened.
    if (s_isOpened) {
        return ISFSError::Locked;
    }

    // The only open mode we support is read-only (the specific open mode used
    // by IOS_LaunchRM).
    if (mode != IOS_OPEN_READ) {
        return ISFSError::Invalid;
    }

    s_isOpened = true;
    return 0;
}

static s32 ReqClose(s32 fd)
{
    LOADER_ASSERT(fd == 0);

    s_isOpened = false;
    return IOSError::OK;
}

static s32 ReqRead(s32 fd, void* data, u32 len)
{
    LOADER_ASSERT(fd == 0);

    // Check if the size overflows.
    if (s_filePos + len > s_fileSize) {
        LOADER_PRINT(ERROR,
          "Read off the end of the file (size: 0x%X, read: 0x%X)", s_fileSize,
          s_filePos + len);
        return ISFSError::Invalid;
    }

    // Print to know if the memcpy doesn't return or something.
    LOADER_PRINT(INFO, "Enter memcpy");

    // Read from the memory file.
    memcpy(data, s_fileAddr + s_filePos, len);
    s_filePos += len;
    LOADER_PRINT(INFO, "Exit memcpy");

    return len;
}

static s32 ReqWrite(s32 fd, const void* data, u32 len)
{
    LOADER_ASSERT(fd == 0);

    // We don't allow this!
    return ISFSError::NoAccess;
}

static s32 ReqSeek(s32 fd, s32 where, s32 whence)
{
    LOADER_ASSERT(fd == 0);

    switch (whence) {
    case NAND_SEEK_SET:
        s_filePos = where;
        break;

    case NAND_SEEK_CUR:
        s_filePos += where;
        break;

    case NAND_SEEK_END:
        s_filePos = s_fileSize + where;
        break;

    default:
        LOADER_PRINT(ERROR, "Invalid origin: %d", whence);
        return ISFSError::Invalid;
    }

    LOADER_PRINT(INFO, "Seeked to position 0x%X", s_filePos);
    return s_filePos;
}

static s32 ReqIoctl(
  s32 fd, u32 cmd, const void* in, u32 in_len, void* io, u32 io_len)
{
    LOADER_ASSERT(fd == 0);

    if (cmd != static_cast<s32>(IOS::FileIoctl::GetFileStats)) {
        // Not a file ioctl!
        LOADER_PRINT(ERROR, "Received unknown ioctl: %d", cmd);
        return ISFSError::Invalid;
    }

    // /dev/fs uses less than.
    if (io_len < sizeof(IOS::File::Stat)) {
        LOADER_PRINT(ERROR, "Output buffer is too small!");
        return ISFSError::Invalid;
    }

    // In case the output buffer is not aligned (somehow), memcpy into it.
    IOS::File::Stat stats;
    stats.size = s_fileSize;
    stats.pos = s_filePos;
    LOADER_PRINT(INFO, "ISFS_GetFileStats: size: 0x%08X, pos: 0x%08X",
      stats.size, stats.pos);
    memcpy(io, &stats, sizeof(stats));

    return IOSError::OK;
}

static s32 HandleRequest(IOSRequest* req)
{
    switch (req->cmd) {
    case IOS_OPEN:
        LOADER_PRINT(
          INFO, "IOS_Open(\"%s\", 0x%X)", req->open.path, req->open.mode);
        return ReqOpen(req->open.path, req->open.mode);

    case IOS_CLOSE:
        LOADER_PRINT(INFO, "IOS_Close(%d)", req->fd);
        return ReqClose(req->fd);

    case IOS_READ:
        LOADER_PRINT(INFO, "IOS_Read(%d, 0x%08X, 0x%X)", req->fd,
          req->read.data, req->read.len);
        return ReqRead(req->fd, req->read.data, req->read.len);

    case IOS_WRITE:
        LOADER_PRINT(INFO, "IOS_Read(%d, 0x%08X, 0x%X)", req->fd,
          req->write.data, req->write.len);
        return ReqWrite(req->fd, req->write.data, req->write.len);

    case IOS_SEEK:
        LOADER_PRINT(INFO, "IOS_Seek(%d, %d, %d)", req->fd, req->seek.where,
          req->seek.whence);
        return ReqSeek(req->fd, req->seek.where, req->seek.whence);

    case IOS_IOCTL:
        LOADER_PRINT(INFO, "IOS_Ioctl(%d, %d, 0x%08X, 0x%X, 0x%08X, 0x%X)",
          req->fd, req->ioctl.cmd, req->ioctl.in, req->ioctl.in_len,
          req->ioctl.io, req->ioctl.io_len);
        return ReqIoctl(req->fd, req->ioctl.cmd, req->ioctl.in,
          req->ioctl.in_len, req->ioctl.io, req->ioctl.io_len);

    default:
        LOADER_PRINT(ERROR, "Received unknown command: %d", req->cmd);
        return ISFSError::Invalid;
    }
}

static s32 FileRMThreadEntry([[maybe_unused]] void* arg)
{
    LOADER_PRINT(INFO, "File RM thread entry");

    s_fileAddr = (u8*) (read32(IOS_FILE_INFO_ADDRESS) & ~0xC0000000);
    s_fileSize = read32(IOS_FILE_INFO_ADDRESS + 4);

    // Check for ELF header
    LOADER_ASSERT(read32(s_fileAddr) == 0x7F454C46);

    s_fileAddr[7] = 0x61;
    s_fileAddr[8] = 1;

    while (true) {
        IOSRequest* req;
        s32 ret =
          IOS_ReceiveMessage(s_fileRMQueue, reinterpret_cast<u32*>(&req), 0);
        LOADER_ASSERT(ret == IOSError::OK);

        // Error to reply to the request with.
        s32 reply = HandleRequest(req);

        // Reply back to the user.
        ret = IOS_ResourceReply(req, reply);
        LOADER_ASSERT(ret == IOSError::OK);

        // First request should always be IOS_Open, so this will always be true
        // if the file is opened.
        if (!s_isOpened)
            break;
    }

    LOADER_PRINT(INFO, "File RM thread exit");
    return 0;
}

static s32 LoaderThreadEntry([[maybe_unused]] void* arg)
{
    LOADER_PRINT(INFO, "Loader thread entry");

    Console::Print("Launching IOS module... ");

    // Create resource manager that we will use to emulate a file.
    u32 queueData[8];
    s32 queue = IOS_CreateMessageQueue(queueData, 8);
    LOADER_ASSERT(queue >= 0);
    LOADER_PRINT(INFO, "Created message queue (%d)", queue);

    s32 ret = IOS_RegisterResourceManager(DEVICE_NAME, queue);
    LOADER_ASSERT(ret == IOSError::OK);
    LOADER_PRINT(INFO, "Registered resource manager");

    // Pass on the queue to another thread to handle the file operations.
    s_fileRMQueue = queue;

    // New stack derived from current stack pointer.
    u32 stackTop = round_down(GetStackPointer() - 0x400, 32);

    // Create the thread.
    s32 thread = IOS_CreateThread(FileRMThreadEntry, nullptr,
      reinterpret_cast<u32*>(stackTop),
      0x400, // 1 KB stack
      80, true);
    LOADER_ASSERT(thread >= 0);
    LOADER_PRINT(INFO, "Created file RM thread (%d)", thread);

    // Begin execution.
    ret = IOS_StartThread(thread);
    LOADER_ASSERT(ret == IOSError::OK);
    LOADER_PRINT(INFO, "Started file RM thread");

    // Launch the module.
    ret = IOS_LaunchRM(DEVICE_NAME);
    LOADER_ASSERT(ret == IOSError::OK);
    LOADER_PRINT(INFO, "Module launched!");

    Console::Print("done\n");

    LOADER_PRINT(INFO, "Loader thread exit");
    return 0;
}

extern "C" {
ATTRIBUTE_SECTION(.start)

void LoaderEntry()
{
    // The main IOS heap. Should be fine to use as long as we always free
    // everything we allocate.
    System::SetHeap(0);

    // Clear VISOLID.
    ACRWriteTrusted(ACRReg::VISOLID, 0);

    // Give the PPC full bus access.
    ACRMaskTrusted(ACRReg::BUSPROT, 0, 0x80000DFE);

    // Enable PPC access to SRAM.
    ACRSetFlag(ACRSRNPROTBit::AHPEN, true);

    // Give the PPC full ISFS permissions.
    IOS_SetUid(15, 0);

    // Initialize console
    Debug_VI::Init();
    Console::Init();

    write32(IOS_BOOT_MSG_ADDRESS, 1);
    IOS_FlushDCache((void*) IOS_BOOT_MSG_ADDRESS, 4);

    // Subtract 0x800 from the current stack pointer to use as the main loader
    // thread's.
    u32 stackTop = round_down(GetStackPointer() - 0x800, 32);

    // Create the main loader thread.
    s32 thread = IOS_CreateThread(LoaderThreadEntry, nullptr,
      reinterpret_cast<u32*>(stackTop),
      0x400, // 1 KB stack
      127, // Max priority
      true);
    LOADER_ASSERT(thread >= 0);

    // Begin execution on the main loader thread.
    s32 ret = IOS_StartThread(thread);
    LOADER_ASSERT(ret == IOSError::OK);
}
}

} // namespace Loader
