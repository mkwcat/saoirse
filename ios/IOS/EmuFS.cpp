// EmuFS.cpp - Emulated IOS filesystem RM
//   Written by Star
//   Written by Palapeli
//
// SPDX-License-Identifier: MIT

#include "EmuFS.hpp"
#include <Debug/Log.hpp>
#include <Disk/SDCard.hpp>
#include <FAT/ff.h>
#include <IOS/IPCLog.hpp>
#include <IOS/Syscalls.h>
#include <IOS/System.hpp>
#include <System/Config.hpp>
#include <System/ISFS.hpp>
#include <System/OS.hpp>
#include <System/Types.h>
#include <array>
#include <climits>
#include <cstdio>
#include <cstring>

namespace EmuFS
{

/*
 * ISFS (Internal Storage FileSystem) is split into two parts:
 *
 * Files; these are opened directly using for example
 * IOS_Open("/tmp/file.bin"). These can be interacted with using seek, read,
 * write, and one ioctl (GetFileStats).
 *
 * Manager (/dev/fs); this takes ioctl commands to do general tasks like create
 * file, read directory, etc.
 *
 * File descriptor: {
 * 0 .. 99: Reserved for proxy/replaced files
 * 100 .. 199: Reserved for real FS files (not used)
 * 200 .. 232: Proxy /dev/fs
 * 300 .. 399: Reserved for direct file access
 * }
 *
 * The manager is blocked from using read, write, seek automatically from the
 * IsFileDescriptorValid check.
 */

constexpr int REPLACED_HANDLE_BASE = 0;
constexpr int REPLACED_HANDLE_NUM = NAND_MAX_FILE_DESCRIPTOR_AMOUNT;

constexpr int REAL_HANDLE_BASE = 100;
constexpr int REAL_HANDLE_MAX = NAND_MAX_FILE_DESCRIPTOR_AMOUNT;

constexpr int MGR_HANDLE_BASE = 200;
// Not sure the actual limit so we'll allow up to 32 (the actual limit will be
// enforced by real FS after this check).
constexpr int MGR_HANDLE_MAX = 32;

constexpr int DIRECT_HANDLE_BASE = 300;
constexpr int DIRECT_HANDLE_MAX = NAND_MAX_FILE_DESCRIPTOR_AMOUNT;

#define EFS_DRIVE "0:"

static char efsFilepath[EFS_MAX_PATH_LEN];
static char efsFilepath2[EFS_MAX_PATH_LEN];

struct ProxyFile {
    bool ipcFile;
    bool inUse;
    bool filOpened;
    char path[64];
    u32 mode;
    // TODO: Use a std::variant for this
    bool isDir;

    union {
        FIL fil;
        DIR dir;
    };
};

static std::array<ProxyFile, NAND_MAX_FILE_DESCRIPTOR_AMOUNT> sFileArray;

struct DirectFile {
    bool inUse;
    int fd;
};

static std::array<DirectFile, DIRECT_HANDLE_MAX> sDirectFileArray;

static std::array<IOS::ResourceCtrl<ISFSIoctl>, MGR_HANDLE_MAX> realFS;

enum class DescType {
    Replaced,
    Real,
    Manager,
    Direct,
    Unknown,
};

static DescType GetDescriptorType(s32 fd)
{
    if (fd >= REPLACED_HANDLE_BASE &&
        fd < REPLACED_HANDLE_BASE + REPLACED_HANDLE_NUM)
        return DescType::Replaced;

    if (fd >= REAL_HANDLE_BASE && fd < REAL_HANDLE_BASE + REAL_HANDLE_MAX)
        return DescType::Real;

    if (fd >= MGR_HANDLE_BASE && fd < MGR_HANDLE_BASE + MGR_HANDLE_MAX)
        return DescType::Manager;

    if (fd >= DIRECT_HANDLE_BASE && fd < DIRECT_HANDLE_BASE + DIRECT_HANDLE_MAX)
        return DescType::Direct;

    return DescType::Unknown;
}

static s32 FResultToISFSError(FRESULT fret)
{
    switch (fret) {
    case FR_OK:
        return ISFSError::OK;

    case FR_INVALID_NAME:
    case FR_INVALID_DRIVE:
    case FR_INVALID_PARAMETER:
    case FR_INVALID_OBJECT:
        return ISFSError::Invalid;

    case FR_DISK_ERR:
    case FR_INT_ERR:
    case FR_NO_FILESYSTEM:
        return ISFSError::Corrupt;

    case FR_NOT_READY:
    case FR_NOT_ENABLED:
        return ISFSError::NotReady;

    case FR_NO_FILE:
    case FR_NO_PATH:
        return ISFSError::NotFound;

    case FR_DENIED:
    case FR_WRITE_PROTECTED:
        return ISFSError::NoAccess;

    case FR_EXIST:
        return ISFSError::Exists;

    case FR_LOCKED:
        return ISFSError::Locked;

    case FR_TOO_MANY_OPEN_FILES:
        return ISFSError::MaxOpen;

    case FR_MKFS_ABORTED:
    case FR_NOT_ENOUGH_CORE:
    case FR_TIMEOUT:
    default:
        return ISFSError::Unknown;
    }
}

static u32 ISFSModeToFileMode(u32 mode)
{
    u32 out = 0;

    if (mode & IOS::Mode::Read)
        out |= FA_READ;

    if (mode & IOS::Mode::Write)
        out |= FA_WRITE;

    return out;
}

static bool IsManagerHandle(s32 fd)
{
    if (fd >= MGR_HANDLE_BASE && fd < (MGR_HANDLE_BASE + MGR_HANDLE_MAX))
        return true;

    return false;
}

static IOS::ResourceCtrl<ISFSIoctl>* GetManagerResource(s32 fd)
{
    if (!IsManagerHandle(fd)) {
        return nullptr;
    }

    auto resource = &realFS[fd - MGR_HANDLE_BASE];
    assert(resource->fd() >= 0);

    return resource;
}

/**
 * Check if a file descriptor is valid.
 */
static bool IsFileDescriptorValid(int fd)
{
    if (fd < 0 || fd >= static_cast<int>(sFileArray.size()))
        return false;

    if (!sFileArray[fd].inUse)
        return false;

    if (sFileArray[fd].isDir)
        return false;

    return true;
}

/**
 * Check if a file descriptor is a valid directory.
 */
static bool IsDirDescValid(int fd)
{
    if (fd < 0 || fd >= static_cast<int>(sFileArray.size()))
        return false;

    if (!sFileArray[fd].inUse)
        return false;

    if (!sFileArray[fd].isDir)
        return false;

    return true;
}

static int RegisterFileDescriptor(const char* path, bool dir = false)
{
    int match = 0;

    for (int i = 0; i < NAND_MAX_FILE_DESCRIPTOR_AMOUNT; i++) {
        // If the file was already opened, reuse the descriptor
        if (sFileArray[i].filOpened && sFileArray[i].ipcFile &&
            !strcmp(sFileArray[i].path, path)) {

            if (sFileArray[i].inUse)
                return ISFSError::Locked;

            sFileArray[i].inUse = true;
            return i;
        }

        if (!sFileArray[i].inUse && sFileArray[match].inUse)
            match = i;

        if (!sFileArray[i].filOpened && sFileArray[match].filOpened)
            match = i;
    }

    if (sFileArray[match].inUse)
        return ISFSError::MaxOpen;

    // Close and use the file descriptor

    if (sFileArray[match].filOpened)
        f_close(&sFileArray[match].fil);

    sFileArray[match].filOpened = false;
    sFileArray[match].inUse = true;
    sFileArray[match].ipcFile = true;
    strncpy(sFileArray[match].path, path, 64);

    return match;
}

static void FreeFileDescriptor(int fd)
{
    if (!IsFileDescriptorValid(fd))
        return;

    sFileArray[fd].inUse = false;
}

static int FindOpenFileDescriptor(const char* path)
{
    for (int i = 0; i < NAND_MAX_FILE_DESCRIPTOR_AMOUNT; i++) {
        if (sFileArray[i].filOpened && !strcmp(path, sFileArray[i].path))
            return i;
    }

    return NAND_MAX_FILE_DESCRIPTOR_AMOUNT;
}

static int FindAvailableFileDescriptor()
{
    int match = 0;

    for (int i = 0; i < NAND_MAX_FILE_DESCRIPTOR_AMOUNT; i++) {
        if (!sFileArray[i].inUse && sFileArray[match].inUse)
            match = i;

        if (!sFileArray[i].filOpened && sFileArray[match].filOpened)
            match = i;
    }

    if (sFileArray[match].inUse)
        return ISFSError::MaxOpen;

    return match;
}

static s32 TryCloseFileDescriptor(int fd)
{
    if (sFileArray[fd].inUse)
        return ISFSError::Locked;

    if (!sFileArray[fd].filOpened)
        return ISFSError::OK;

    const FRESULT fret = f_close(&sFileArray[fd].fil);
    if (fret != FR_OK) {
        PRINT(IOS_EmuFS, ERROR, "Failed to close file, error: %d", fret);
        return FResultToISFSError(fret);
    }

    sFileArray[fd].filOpened = false;
    return FR_OK;
}

/*---------------------------------------------------------------------------*
 * Name        : strnlen
 * Description : Gets the number of characters in a character string, excluding
 *               the null terminator, up to maxLength characters.
 * Arguments   : string       The character string to check.
 *             : maxLength    The maximum number of characters to check.
 * Returns     : The number of characters in the character string, excluding
 *               the null terminator, up to maxLength characters.
 *---------------------------------------------------------------------------*/
static size_t strnlen(const char* string, size_t maxLength)
{
    size_t i;
    for (i = 0; i < maxLength && string[i]; i++)
        ;
    return i;
}

/*---------------------------------------------------------------------------*
 * Name        : IsFilepathValid
 * Description : Checks if a filepath valid.
 * Arguments   : filepath    The filepath to check.
 * Returns     : If the filepath is valid.
 *---------------------------------------------------------------------------*/
static bool IsFilepathValid(const char* filepath)
{
    if (!filepath)
        return false;

    if (filepath[0] != NAND_DIRECTORY_SEPARATOR_CHAR)
        return false;

    return (
      strnlen(filepath, NAND_MAX_FILEPATH_LENGTH) < NAND_MAX_FILEPATH_LENGTH);
}

/*---------------------------------------------------------------------------*
 * Name        : IsReplacedFilepath
 * Description : Checks if a filepath is allowed to be replaced.
 * Arguments   : filepath    The filepath to check.
 * Returns     : If the filepath is allowed to be replaced.
 *---------------------------------------------------------------------------*/
static bool IsReplacedFilepath(const char* filepath)
{
    if (!IsFilepathValid(filepath))
        return false;

    return Config::sInstance->IsISFSPathReplaced(filepath);
}

/*---------------------------------------------------------------------------*
 * Name        : GetReplacedFilepath
 * Description : Gets the replaced filepath of a filepath.
 * Arguments   : filepath    The filepath to get the replaced filepath of.
 *               out_buf     A pointer to a buffer to store the replaced
 *filepath in. out_len     The length of the output buffer. Returns     : A
 *pointer to the buffer containing the replaced filepath, or nullptr on error.
 *---------------------------------------------------------------------------*/
static const char* GetReplacedFilepath(
  const char* filepath, char* out_buf, size_t out_len)
{
    if (!IsFilepathValid(filepath))
        return nullptr;

    if (!out_buf)
        return nullptr;

    // Create and write the replaced filepath
    filepath = strchr(filepath, NAND_DIRECTORY_SEPARATOR_CHAR);
    if (snprintf(out_buf, out_len, EFS_DRIVE "%s", filepath + 1) <= 0) {
        PRINT(IOS_EmuFS, ERROR, "Failed to format the replaced filepath");
        return nullptr;
    }

    PRINT(IOS_EmuFS, INFO, "Replaced file path: \"%s\"", out_buf);

    return out_buf;
}

static u8 efsCopyBuffer[0x2000] ATTRIBUTE_ALIGN(32); // 8 KB

static s32 CopyFromNandToEFS(const char* nandPath, FIL& fil)
{
    // Only allow renaming files from /tmp
    if (strncmp(nandPath, "/tmp", 4) != 0) {
        PRINT(
          IOS_EmuFS, ERROR, "Attempting to rename a file from outside of /tmp");
        return ISFSError::NoAccess;
    }

    IOS::File isfsFile(nandPath, IOS::Mode::Read);

    if (isfsFile.fd() < 0) {
        PRINT(IOS_EmuFS, ERROR, "Failed to open ISFS file: %d", isfsFile.fd());
        return isfsFile.fd();
    }

    s32 size = isfsFile.size();
    PRINT(IOS_EmuFS, INFO, "File size: 0x%X", size);

    for (s32 pos = 0; pos < size; pos += sizeof(efsCopyBuffer)) {
        u32 readlen = size - pos;
        if (readlen > sizeof(efsCopyBuffer))
            readlen = sizeof(efsCopyBuffer);

        s32 ret = isfsFile.read(efsCopyBuffer, readlen);

        if ((u32) ret != readlen) {
            f_close(&fil);
            PRINT(IOS_EmuFS, ERROR, "Failed to read from ISFS file: %d != %d",
              ret, readlen);
            if (ret < 0)
                return ret;
            return ISFSError::Unknown;
        }

        UINT bw;
        auto fret = f_write(&fil, efsCopyBuffer, readlen, &bw);

        if (fret != FR_OK || (u32) bw != readlen) {
            PRINT(IOS_EmuFS, ERROR,
              "Failed to write to EFS file: %d != 0 OR %d != %d", fret, readlen,
              bw);
            if (fret != FR_OK)
                return FResultToISFSError(fret);
            return ISFSError::Unknown;
        }
    }

    return ISFSError::OK;
}

static s32 ReopenFile(s32 fd)
{
    const FRESULT fret = f_lseek(&sFileArray[fd].fil, 0);
    if (fret != FR_OK) {
        PRINT(IOS_EmuFS, ERROR,
          "Failed to seek to position 0x%08X in file descriptor %d", 0, fd);

        FreeFileDescriptor(fd);
        return FResultToISFSError(fret);
    }
    return fd;
}

/*
 * Handle open file request from the filesystem proxy.
 * Returns: File descriptor, or ISFS error code.
 */
static s32 ReqProxyOpen(const char* filepath, u32 mode)
{
    if (mode > IOS_OPEN_RW)
        return ISFSError::Invalid;

    // Get the replaced filepath
    if (!GetReplacedFilepath(filepath, efsFilepath, sizeof(efsFilepath)))
        return ISFSError::Invalid;

    int fd = RegisterFileDescriptor(filepath);
    if (fd < 0) {
        PRINT(IOS_EmuFS, ERROR, "Could not register file descriptor: %d", fd);
        return fd;
    }
    PRINT(IOS_EmuFS, INFO, "Registered file descriptor %d", fd);

    ASSERT(IsFileDescriptorValid(fd));

    sFileArray[fd].mode = mode;

    if (sFileArray[fd].filOpened) {
        PRINT(IOS_EmuFS, INFO, "File already open, reusing descriptor");
        return ReopenFile(fd);
    }

    const FRESULT fret =
      f_open(&sFileArray[fd].fil, efsFilepath, FA_READ | FA_WRITE);
    if (fret != FR_OK) {
        PRINT(IOS_EmuFS, ERROR, "Failed to open file '%s', error: %d",
          efsFilepath, fret);

        FreeFileDescriptor(fd);
        return FResultToISFSError(fret);
    }

    sFileArray[fd].filOpened = true;

    PRINT(IOS_EmuFS, INFO, "Successfully opened file '%s' (fd=%d, mode=%u)",
      efsFilepath, fd, mode);

    return fd;
}

/**
 * Handles direct open file requests.
 * @returns File descriptor, or ISFS error code.
 */
static s32 ReqDirectOpen(const char* filepath, u32 mode)
{
    int fd = FindAvailableFileDescriptor();
    if (fd < 0) {
        PRINT(IOS_EmuFS, ERROR, "Could not find an open file descriptor");
        return fd;
    }

    sFileArray[fd].inUse = false;
    sFileArray[fd].filOpened = false;
    memset(sFileArray[fd].path, 0, 64);

    const FRESULT fret =
      f_open(&sFileArray[fd].fil, filepath, ISFSModeToFileMode(mode));
    if (fret != FR_OK) {
        PRINT(IOS_EmuFS, ERROR, "Failed to open file '%s', mode: %X, error: %d",
          filepath, mode, fret);
        return FResultToISFSError(fret);
    }

    sFileArray[fd].mode = mode;
    sFileArray[fd].inUse = true;
    sFileArray[fd].isDir = false;
    sFileArray[fd].filOpened = true;

    PRINT(IOS_EmuFS, INFO, "Successfully opened file '%s' (fd=%d, mode=%u)",
      filepath, fd, mode);

    return fd;
}

/**
 * Handles direct open directory requests.
 * @returns File descriptor, or ISFS error code.
 */
static s32 ReqDirectOpenDir(const char* path)
{
    int fd = FindAvailableFileDescriptor();
    if (fd < 0) {
        PRINT(IOS_EmuFS, ERROR, "Could not find an open file descriptor");
        return fd;
    }

    sFileArray[fd].inUse = false;
    sFileArray[fd].filOpened = false;

    const FRESULT fret = f_opendir(&sFileArray[fd].dir, path);
    if (fret != FR_OK) {
        PRINT(
          IOS_EmuFS, ERROR, "Failed to open dir '%s' error: %d", path, fret);
        return FResultToISFSError(fret);
    }

    sFileArray[fd].inUse = true;
    sFileArray[fd].isDir = true;

    PRINT(
      IOS_EmuFS, INFO, "Successfully opened directory '%s' (fd=%d)", path, fd);

    return fd;
}

/**
 * Close open file descriptor.
 * @returns 0 for success, or IOS error code.
 */
static s32 ReqClose(s32 fd)
{
    if (IsManagerHandle(fd)) {
        s32 ret = GetManagerResource(fd)->close();
        assert(ret == IOSError::OK);
        return IOSError::OK;
    }

    auto type = GetDescriptorType(fd);

    if (type == DescType::Direct) {
        PRINT(IOS_EmuFS, INFO, "Closing direct handle %d", fd);
        if (!sDirectFileArray[fd - DIRECT_HANDLE_BASE].inUse)
            return IOSError::OK;

        s32 realFd = sDirectFileArray[fd - DIRECT_HANDLE_BASE].fd;
        sDirectFileArray[fd - DIRECT_HANDLE_BASE].inUse = false;
        sDirectFileArray[fd - DIRECT_HANDLE_BASE].fd = ISFSError::NotFound;

        if (sFileArray[realFd].isDir)
            return IOSError::OK;

        fd = realFd;

        if (!IsFileDescriptorValid(fd))
            return ISFSError::Invalid;

        if (f_close(&sFileArray[fd].fil) != FR_OK) {
            PRINT(IOS_EmuFS, ERROR, "Failed to close file descriptor %d", fd);
            return ISFSError::Unknown;
        }

        sFileArray[fd].filOpened = false;
        FreeFileDescriptor(fd);
    } else {
        if (!IsFileDescriptorValid(fd))
            return ISFSError::Invalid;

        if (f_sync(&sFileArray[fd].fil) != FR_OK) {
            PRINT(IOS_EmuFS, ERROR, "Failed to sync file descriptor %d", fd);
            return ISFSError::Unknown;
        }

        FreeFileDescriptor(fd);
    }

    PRINT(IOS_EmuFS, INFO, "Successfully closed file descriptor %d", fd);

    return ISFSError::OK;
}

/**
 * Read data from open file descriptor.
 * @returns Amount read, or ISFS error code.
 */
static s32 ReqRead(s32 fd, void* data, u32 len)
{
    if (!IsFileDescriptorValid(fd))
        return ISFSError::Invalid;

    if (len == 0)
        return ISFSError::OK;

    if (!(sFileArray[fd].mode & IOS::Mode::Read))
        return ISFSError::NoAccess;

    unsigned int bytesRead;
    const FRESULT fret = f_read(&sFileArray[fd].fil, data, len, &bytesRead);
    if (fret != FR_OK) {
        PRINT(IOS_EmuFS, ERROR,
          "Failed to read %u bytes from file descriptor %d, error: %d", len, fd,
          fret);
        return FResultToISFSError(fret);
    }

    PRINT(IOS_EmuFS, INFO, "Successfully read %u bytes from file descriptor %d",
      bytesRead, fd);

    return bytesRead;
}

/*
 * Write data to open file descriptor.
 * Returns: Amount wrote, or ISFS error code.
 */
static s32 ReqWrite(s32 fd, const void* data, u32 len)
{
    if (!IsFileDescriptorValid(fd))
        return ISFSError::Invalid;

    if (len == 0)
        return ISFSError::OK;

    if (!(sFileArray[fd].mode & IOS::Mode::Write))
        return ISFSError::NoAccess;

    unsigned int bytesWrote;
    const FRESULT fret = f_write(&sFileArray[fd].fil, data, len, &bytesWrote);
    if (fret != FR_OK) {
        PRINT(IOS_EmuFS, ERROR,
          "Failed to write %u bytes to file descriptor %d, error: %d", len, fd,
          fret);
        return FResultToISFSError(fret);
    }

    PRINT(IOS_EmuFS, INFO, "Successfully wrote %u bytes to file descriptor %d",
      bytesWrote, fd);

    return bytesWrote;
}

/*
 * Moves the file read/write of an open file descriptor.
 * Returns: Current offset, or an ISFS error code.
 */
static s32 ReqSeek(s32 fd, s32 where, s32 whence)
{
    if (!IsFileDescriptorValid(fd))
        return ISFSError::Invalid;

    if (whence < NAND_SEEK_SET || whence > NAND_SEEK_END)
        return ISFSError::Invalid;

    FIL* fil = &sFileArray[fd].fil;
    FSIZE_t offset = f_tell(fil);
    FSIZE_t endPosition = f_size(fil);

    switch (whence) {
    case NAND_SEEK_SET: {
        offset = 0;
        break;
    }
    case NAND_SEEK_CUR: {
        break;
    }
    case NAND_SEEK_END: {
        offset = endPosition;
        break;
    }
    }

    offset += where;
    if (offset > endPosition)
        return ISFSError::Invalid;

    if (offset == f_tell(fil)) {
        PRINT(IOS_EmuFS, INFO, "Skipping seek");
        return offset;
    }

    const FRESULT fresult = f_lseek(fil, offset);
    if (fresult != FR_OK) {
        PRINT(IOS_EmuFS, ERROR,
          "Failed to seek to position 0x%08X in file descriptor %d", offset,
          fd);
        return FResultToISFSError(fresult);
    }

    PRINT(IOS_EmuFS, INFO,
      "Successfully seeked to position 0x%08X in file descriptor %d", offset,
      fd);

    return offset;
}

/*
 * Handles filesystem ioctl commands.
 * Returns: ISFSError result.
 */
static s32 ReqIoctl(
  s32 fd, ISFSIoctl cmd, void* in, u32 in_len, void* io, u32 io_len)
{
    if (in_len == 0)
        in = nullptr;
    if (io_len == 0)
        io = nullptr;

    /* File commands */
    if (IsFileDescriptorValid(fd)) {
        if (cmd == ISFSIoctl::GetFileStats) {
            if (io_len < sizeof(IOS::File::Stat))
                return ISFSError::Invalid;
            // Real FS doesn't seem to even check alignment before writing, but
            // I'd rather not have the whole of IOS panic over an alignment
            // exception.
            if (!aligned(io, 4)) {
                PRINT(IOS_EmuFS, ERROR, "Invalid GetFileStats input alignment");
                return ISFSError::Invalid;
            }
            IOS::File::Stat* stat = reinterpret_cast<IOS::File::Stat*>(io);
            stat->size = f_size(&sFileArray[fd].fil);
            stat->pos = f_tell(&sFileArray[fd].fil);
            return ISFSError::OK;
        }

        PRINT(
          IOS_EmuFS, ERROR, "Unknown file ioctl: %u", static_cast<s32>(cmd));
        return ISFSError::Invalid;
    }

    // Manager commands!
    if (!IsManagerHandle(fd)) {
        // ...oh, nevermind :(
        return ISFSError::Invalid;
    }

    auto mgrRes = GetManagerResource(fd);

    // TODO Add ISFS_Shutdown
    switch (cmd) {
    // [ISFS_Format]
    // in: not used
    // out: not used
    case ISFSIoctl::Format:
        /* Hmm, a command to remove everything in the filesystem and brick the
         * Wii. Very good. */
        PRINT(IOS_EmuFS, ERROR, "Format: Attempt to use ISFS_Format!");
        return ISFSError::NoAccess;

    // [ISFS_CreateDir]
    // in: Accepts ISFSAttrBlock. Reads path, ownerPerm, groupPerm, otherPerm,
    // and attributes.
    // out: not used
    case ISFSIoctl::CreateDir: {
        if (!aligned(in, 4))
            return ISFSError::Invalid;

        if (in_len < sizeof(ISFSAttrBlock))
            return ISFSError::Invalid;

        ISFSAttrBlock* isfsAttrBlock = (ISFSAttrBlock*) in;

        const char* path = isfsAttrBlock->path;

        // Check if the filepath is valid
        if (!IsFilepathValid(path))
            return ISFSError::Invalid;

        // Check if the filepath should be replaced
        if (!IsReplacedFilepath(path))
            return mgrRes->ioctl(ISFSIoctl::CreateDir, in, in_len, io, io_len);

        // Get the replaced filepath
        if (!GetReplacedFilepath(path, efsFilepath, sizeof(efsFilepath)))
            return ISFSError::Invalid;

        const FRESULT fresult = f_mkdir(path);
        if (fresult != FR_OK) {
            PRINT(IOS_EmuFS, ERROR,
              "CreateDir: Failed to create directory '%s'", efsFilepath);
            return FResultToISFSError(fresult);
        }

        PRINT(IOS_EmuFS, INFO, "CreateDir: Successfully created directory '%s'",
          efsFilepath);

        return ISFSError::OK;
    }

    // [ISFS_SetAttr]
    // in: Accepts ISFSAttrBlock. All fields are read. If the caller's UID is
    // not zero, ownerID and groupID must be equal to the caller's. Otherwise,
    // throw ISFSError::NoAccess.
    // out: not used
    case ISFSIoctl::SetAttr: {
        if (!aligned(in, 4))
            return ISFSError::Invalid;

        if (in_len < sizeof(ISFSAttrBlock))
            return ISFSError::Invalid;

        ISFSAttrBlock* isfsAttrBlock = (ISFSAttrBlock*) in;

        const char* path = isfsAttrBlock->path;

        // Check if the filepath is valid
        if (!IsFilepathValid(path))
            return ISFSError::Invalid;

        // Check if the filepath should be replaced
        if (!IsReplacedFilepath(path))
            return mgrRes->ioctl(ISFSIoctl::SetAttr, in, in_len, io, io_len);

        // Get the replaced filepath
        if (!GetReplacedFilepath(path, efsFilepath, sizeof(efsFilepath)))
            return ISFSError::Invalid;

        const FRESULT fresult = f_stat(efsFilepath, nullptr);
        if (fresult != FR_OK) {
            PRINT(IOS_EmuFS, ERROR,
              "SetAttr: Failed to set attributes for file or directory '%s'",
              efsFilepath);
            return FResultToISFSError(fresult);
        }

        PRINT(IOS_EmuFS, INFO,
          "SetAttr: Successfully set attributes for file or directory '%s'",
          efsFilepath);

        return ISFSError::OK;
    }

    // [ISFS_GetAttr]
    // in: Path to a file or directory.
    // out: File/directory's attributes (ISFSAttrBlock).
    case ISFSIoctl::GetAttr: {
        const int OWNER_PERM = 3;
        const int GROUP_PERM = 3;
        const int OTHER_PERM = 1;
        const int ATTRIBUTES = 0;

        if (!aligned(in, 4) || !aligned(io, 4))
            return ISFSError::Invalid;

        if (in_len < ISFSMaxPath || io_len < sizeof(ISFSAttrBlock))
            return ISFSError::Invalid;

        const char* filepath = (const char*) in;

        // Check if the filepath is valid
        if (!IsFilepathValid(filepath))
            return ISFSError::Invalid;

        // Check if the filepath should be replaced
        if (!IsReplacedFilepath(filepath))
            return mgrRes->ioctl(ISFSIoctl::GetAttr, in, in_len, io, io_len);

        // Get the replaced filepath
        if (!GetReplacedFilepath(filepath, efsFilepath, sizeof(efsFilepath)))
            return ISFSError::Invalid;

        const FRESULT fresult = f_stat(efsFilepath, nullptr);
        if (fresult != FR_OK) {
            PRINT(IOS_EmuFS, ERROR,
              "GetAttr: Failed to get attributes for file or directory '%s'",
              efsFilepath);
            return FResultToISFSError(fresult);
        }

        ISFSAttrBlock* isfsAttrBlock = (ISFSAttrBlock*) io;
        isfsAttrBlock->ownerId = IOS_GetUid();
        isfsAttrBlock->groupId = IOS_GetGid();
        strcpy(isfsAttrBlock->path, filepath);
        isfsAttrBlock->ownerPerm = OWNER_PERM;
        isfsAttrBlock->groupPerm = GROUP_PERM;
        isfsAttrBlock->otherPerm = OTHER_PERM;
        isfsAttrBlock->attributes = ATTRIBUTES;

        PRINT(IOS_EmuFS, INFO,
          "GetAttr: Successfully got attributes for file or directory '%s'",
          efsFilepath);

        return ISFSError::OK;
    }

    // [ISFS_Delete]
    // in: Path to the file or directory to delete.
    // out: not used
    case ISFSIoctl::Delete: {
        if (!aligned(in, 4))
            return ISFSError::Invalid;

        if (in_len < ISFSMaxPath)
            return ISFSError::Invalid;

        const char* filepath = (const char*) in;

        // Check if the filepath is valid
        if (!IsFilepathValid(filepath))
            return ISFSError::Invalid;

        // Check if the filepath should be replaced
        if (!IsReplacedFilepath(filepath))
            return mgrRes->ioctl(ISFSIoctl::Delete, in, in_len, io, io_len);

        s32 ret = FindOpenFileDescriptor(filepath);
        if (ret < 0) {
            return ret;
        }

        if (ret != NAND_MAX_FILE_DESCRIPTOR_AMOUNT) {
            ret = TryCloseFileDescriptor(ret);
            if (ret != ISFSError::OK) {
                return ret;
            }
        }

        // Get the replaced filepath
        if (!GetReplacedFilepath(filepath, efsFilepath, sizeof(efsFilepath)))
            return ISFSError::Invalid;

        const FRESULT fresult = f_unlink(efsFilepath);
        if (fresult != FR_OK) {
            PRINT(IOS_EmuFS, ERROR,
              "Delete: Failed to delete file or directory '%s'", efsFilepath);
            return FResultToISFSError(fresult);
        }

        PRINT(IOS_EmuFS, INFO,
          "Delete: Successfully deleted file or directory '%s'", efsFilepath);

        return ISFSError::OK;
    }

    // [ISFS_Rename]
    // in: ISFSRenameBlock.
    // out: not used
    case ISFSIoctl::Rename: {
        if (!aligned(in, 4))
            return ISFSError::Invalid;

        if (in_len < sizeof(ISFSRenameBlock))
            return ISFSError::Invalid;

        ISFSRenameBlock* isfsRenameBlock = (ISFSRenameBlock*) in;

        const char* pathOld = isfsRenameBlock->pathOld;
        const char* pathNew = isfsRenameBlock->pathNew;
        PRINT(IOS_EmuFS, INFO, "Rename: ISFS_Rename(\"%s\", \"%s\")", pathOld,
          pathNew);

        // Check if the old and new filepaths are valid
        if (!IsFilepathValid(pathOld) || !IsFilepathValid(pathNew))
            return ISFSError::Invalid;

        const bool isOldFilepathReplaced = IsReplacedFilepath(pathOld);
        const bool isNewFilepathReplaced = IsReplacedFilepath(pathNew);

        // Neither of the filepaths are replaced
        if (!isOldFilepathReplaced && !isNewFilepathReplaced)
            return mgrRes->ioctl(ISFSIoctl::Rename, in, in_len, io, io_len);

        char* efsOldFilepath = efsFilepath;
        char* efsNewFilepath = efsFilepath2;

        // Rename from NAND to EFS file
        if (!isOldFilepathReplaced && isNewFilepathReplaced) {
            // Check if the file is already open somewhere
            int openFd = FindOpenFileDescriptor(pathNew);

            s32 ret;
            if (openFd < 0 || openFd >= static_cast<int>(sFileArray.size())) {
                // File is not open
                if (!GetReplacedFilepath(
                      pathNew, efsNewFilepath, EFS_MAX_PATH_LEN))
                    return ISFSError::Invalid;

                FIL destFil;
                auto fret =
                  f_open(&destFil, efsNewFilepath, FA_WRITE | FA_CREATE_ALWAYS);
                if (fret != FR_OK)
                    return FResultToISFSError(fret);

                ret = CopyFromNandToEFS(pathOld, destFil);
                fret = f_close(&destFil);
                assert(fret == FR_OK);
            } else {
                // File is open
                assert(sFileArray[openFd].filOpened);

                if (sFileArray[openFd].inUse)
                    return ISFSError::Locked;

                // Seek back to the beginning
                auto fret = f_lseek(&sFileArray[openFd].fil, 0);
                if (fret != FR_OK)
                    return FResultToISFSError(fret);

                // Truncate from the beginning of the file
                fret = f_truncate(&sFileArray[openFd].fil);
                if (fret != FR_OK)
                    return FResultToISFSError(fret);

                ret = CopyFromNandToEFS(pathOld, sFileArray[openFd].fil);
                fret = f_sync(&sFileArray[openFd].fil);
                assert(fret == FR_OK);
            }

            if (ret != ISFSError::OK)
                return ret;

            ret = mgrRes->ioctl(ISFSIoctl::Delete, const_cast<char*>(pathOld),
              ISFSMaxPath, nullptr, 0);
            return ret;
        }

        // Other way not supported (yet?)
        if (isOldFilepathReplaced ^ isNewFilepathReplaced) {
            return ISFSError::Invalid;
        }

        // Both of the filepaths are replaced

        // Get the replaced filepaths
        if (!GetReplacedFilepath(pathOld, efsOldFilepath, EFS_MAX_PATH_LEN) ||
            !GetReplacedFilepath(pathNew, efsNewFilepath, EFS_MAX_PATH_LEN))
            return ISFSError::Invalid;

        const FRESULT fresult = f_rename(efsOldFilepath, efsNewFilepath);
        if (fresult != FR_OK) {
            PRINT(IOS_EmuFS, ERROR,
              "Rename: Failed to rename file or directory '%s' to '%s'",
              efsOldFilepath, efsNewFilepath);
            return FResultToISFSError(fresult);
        }

        PRINT(IOS_EmuFS, INFO,
          "Rename: Successfully renamed file or directory '%s' to '%s'",
          efsOldFilepath, efsNewFilepath);

        return ISFSError::OK;
    }

    // [ISFS_CreateFile]
    // in: Accepts ISFSAttrBlock. Reads path, ownerPerm, groupPerm, otherPerm,
    // and attributes.
    // out: not used
    case ISFSIoctl::CreateFile: {
        if (!aligned(in, 4))
            return ISFSError::Invalid;

        if (in_len < sizeof(ISFSAttrBlock))
            return ISFSError::Invalid;

        ISFSAttrBlock* isfsAttrBlock = (ISFSAttrBlock*) in;

        const char* path = isfsAttrBlock->path;

        PRINT(IOS_EmuFS, INFO, "CreateFile: ISFS_CreateFile(\"%s\")", path);

        // Check if the filepath is valid
        if (!IsFilepathValid(path))
            return ISFSError::Invalid;

        // Check if the filepath should be replaced
        if (!IsReplacedFilepath(path))
            return mgrRes->ioctl(ISFSIoctl::CreateFile, in, in_len, io, io_len);

        // Get the replaced filepath
        if (!GetReplacedFilepath(path, efsFilepath, sizeof(efsFilepath)))
            return ISFSError::Invalid;

        FIL fil;
        const FRESULT fresult =
          f_open(&fil, efsFilepath, FA_CREATE_NEW | FA_READ | FA_WRITE);
        if (fresult != FR_OK) {
            PRINT(IOS_EmuFS, ERROR, "CreateFile: Failed to create file '%s'",
              efsFilepath);
            return FResultToISFSError(fresult);
        }

        f_sync(&fil);

        s32 ret = FindAvailableFileDescriptor();
        if (ret >= 0 && ret < NAND_MAX_FILE_DESCRIPTOR_AMOUNT) {
            sFileArray[ret].filOpened = true;
            sFileArray[ret].fil = fil; // Copy
        }

        PRINT(IOS_EmuFS, INFO, "CreateFile: Successfully created file '%s'",
          efsFilepath);

        return ISFSError::OK;
    }

    // [ISFS_Shutdown]
    case ISFSIoctl::Shutdown: {
        // This command is called to wait for any in-progress file operations to
        // be completed before shutting down
        PRINT(IOS_EmuFS, INFO, "Shutdown: ISFS_Shutdown()");
        return ISFSError::OK;
    }

    default:
        PRINT(IOS_EmuFS, ERROR, "Unknown manager ioctl: %u", cmd);
        return ISFSError::Invalid;
    }
}

/*
 * Handles filesystem ioctlv commands.
 * Returns: ISFSError result.
 */
static s32 ReqIoctlv(
  s32 fd, ISFSIoctl cmd, u32 in_count, u32 out_count, IOS::Vector* vec)
{
    if (in_count >= 32 || out_count >= 32)
        return ISFSError::Invalid;

    // NULL any zero length vectors to prevent any accidental writes.
    for (u32 i = 0; i < in_count + out_count; i++) {
        if (vec[i].len == 0)
            vec[i].data = nullptr;
    }

    // Open a direct file
    if (GetDescriptorType(fd) == DescType::Direct) {
        switch (cmd) {
        default:
            PRINT(IOS_EmuFS, ERROR, "Unknown direct ioctl: %u",
              static_cast<s32>(cmd));
            return ISFSError::Invalid;

        case ISFSIoctl::Direct_Open: {
            if (in_count != 2 || out_count != 0) {
                PRINT(IOS_EmuFS, ERROR, "Direct_Open: Wrong vector count!");
                return ISFSError::Invalid;
            }

            if (vec[0].len < 1 || vec[0].len > EFS_MAX_PATH_LEN) {
                PRINT(IOS_EmuFS, ERROR, "Direct_Open: Invalid path length: %d",
                  vec[0].len);
                return ISFSError::Invalid;
            }

            if (vec[1].len != sizeof(u32)) {
                PRINT(IOS_EmuFS, ERROR,
                  "Direct_Open: Invalid open mode length: %d", vec[1].len);
                return ISFSError::Invalid;
            }

            if (!aligned(vec[1].data, 4)) {
                PRINT(
                  IOS_EmuFS, ERROR, "Direct_Open: Invalid open mode alignment");
                return ISFSError::Invalid;
            }

            // Check if the supplied file path length is valid.
            if (strnlen(reinterpret_cast<const char*>(vec[0].data),
                  vec[0].len) == vec[0].len) {
                PRINT(IOS_EmuFS, ERROR, "Direct_Open: Path does not terminate");
                return ISFSError::Invalid;
            }

            // Check if the file is already open
            if (sDirectFileArray[fd - DIRECT_HANDLE_BASE].fd !=
                ISFSError::NotFound) {
                PRINT(IOS_EmuFS, ERROR, "Direct_Open: File already open");
                return ISFSError::Invalid;
            }

            s32 realFd =
              ReqDirectOpen(reinterpret_cast<const char*>(vec[0].data),
                *reinterpret_cast<u32*>(vec[1].data));
            if (realFd < 0)
                return realFd;

            sDirectFileArray[fd - DIRECT_HANDLE_BASE].inUse = true;
            sDirectFileArray[fd - DIRECT_HANDLE_BASE].fd = realFd;
            return ISFSError::OK;
        }

        case ISFSIoctl::Direct_DirOpen: {
            if (in_count != 1 || out_count != 0) {
                PRINT(IOS_EmuFS, ERROR, "Direct_DirOpen: Wrong vector count!");
                return ISFSError::Invalid;
            }

            if (vec[0].len < 1 || vec[0].len > EFS_MAX_PATH_LEN) {
                PRINT(IOS_EmuFS, ERROR,
                  "Direct_DirOpen: Invalid path length: %d", vec[0].len);
                return ISFSError::Invalid;
            }

            // Check if the supplied file path length is valid.
            if (strnlen(reinterpret_cast<const char*>(vec[0].data),
                  vec[0].len) == vec[0].len) {
                PRINT(IOS_EmuFS, ERROR, "Direct_Open: Path does not terminate");
                return ISFSError::Invalid;
            }

            // Check if the file is already open
            if (sDirectFileArray[fd - DIRECT_HANDLE_BASE].fd !=
                ISFSError::NotFound) {
                PRINT(IOS_EmuFS, ERROR, "Direct_DirOpen: File already open");
                return ISFSError::Invalid;
            }

            s32 realFd =
              ReqDirectOpenDir(reinterpret_cast<const char*>(vec[0].data));
            if (realFd < 0)
                return realFd;

            sDirectFileArray[fd - DIRECT_HANDLE_BASE].inUse = true;
            sDirectFileArray[fd - DIRECT_HANDLE_BASE].fd = realFd;
            return ISFSError::OK;
        }

        case ISFSIoctl::Direct_DirNext: {
            if (in_count != 0 || out_count != 1) {
                PRINT(IOS_EmuFS, ERROR, "Direct_DirNext: Wrong vector count!");
                return ISFSError::Invalid;
            }

            if (vec[0].len != sizeof(ISFSDirect_Stat)) {
                PRINT(IOS_EmuFS, ERROR,
                  "Direct_DirNext: Wrong ISFSDirect_Stat length: %u",
                  vec[0].len);
                return ISFSError::Invalid;
            }

            auto stat = reinterpret_cast<ISFSDirect_Stat*>(vec[0].data);
            memset(stat, 0, vec[0].len);

            if (!sDirectFileArray[fd - DIRECT_HANDLE_BASE].inUse ||
                sDirectFileArray[fd - DIRECT_HANDLE_BASE].fd ==
                  ISFSError::NotFound) {
                PRINT(IOS_EmuFS, ERROR, "Direct_DirNext: File not open!");
                return ISFSError::Invalid;
            }

            s32 realFd = sDirectFileArray[fd - DIRECT_HANDLE_BASE].fd;
            if (!sFileArray[realFd].isDir) {
                PRINT(IOS_EmuFS, ERROR,
                  "Direct_DirNext: Requested FD is not a directory!");
                return ISFSError::Invalid;
            }

            FILINFO fno = {};
            auto fret = f_readdir(&sFileArray[realFd].dir, &fno);
            if (fret != FR_OK) {
                PRINT(IOS_EmuFS, ERROR, "Direct_DirNext: f_readdir error: %d",
                  fret);
                return FResultToISFSError(fret);
            }

            if (fno.fname[0] == '\0') {
                PRINT(
                  IOS_EmuFS, INFO, "Direct_DirNext: Reached end of directory");
                // Caller should recognize a blank filename as the end of the
                // directory.
                return ISFSError::OK;
            }

            ISFSDirect_Stat tmpStat = {};

            tmpStat.dirOffset = 0; // TODO
            tmpStat.attribute = fno.fattrib;
            tmpStat.size = fno.fsize;
            strncpy(tmpStat.name, fno.fname, EFS_MAX_PATH_LEN);
            System::UnalignedMemcpy(stat, &tmpStat, sizeof(ISFSDirect_Stat));
            return ISFSError::OK;
        }
        }
    }

    if (!IsManagerHandle(fd))
        return ISFSError::Invalid;

    auto mgrRes = GetManagerResource(fd);

    switch (cmd) {
    // [ISFS_ReadDir]
    // vec[0]: path
    // todo
    case ISFSIoctl::ReadDir: {
        if (in_count != out_count || in_count < 1 || in_count > 2) {
            PRINT(IOS_EmuFS, ERROR, "ReadDir: Wrong vector count");
            return ISFSError::Invalid;
        }

        if (!aligned(vec[0].data, 4) || vec[0].len < ISFSMaxPath) {
            PRINT(IOS_EmuFS, ERROR, "ReadDir: Invalid input path vector");
            return ISFSError::Invalid;
        }

        char path[ISFSMaxPath];
        memcpy(path, vec[0].data, ISFSMaxPath);
        PRINT(IOS_EmuFS, INFO, "ReadDir: ISFS_ReadDir(\"%s\")", path);

        u32 inMaxCount = 0;
        char* outNames = nullptr;
        u32* outCountPtr = nullptr;

        if (in_count == 2) {
            if (!aligned(vec[1].data, 4) || vec[1].len < sizeof(u32)) {
                PRINT(IOS_EmuFS, ERROR,
                  "ReadDir: Invalid input max file count vector");
                return ISFSError::Invalid;
            }

            inMaxCount = *reinterpret_cast<u32*>(vec[1].data);

            if (!aligned(vec[2].data, 4) || vec[2].len < inMaxCount * 13) {
                PRINT(IOS_EmuFS, ERROR,
                  "ReadDir: Invalid output file names vector");
                return ISFSError::Invalid;
            }

            outNames = reinterpret_cast<char*>(vec[2].data);
            memset(outNames, 0, inMaxCount * 13);

            if (!aligned(vec[3].data, 4) || vec[3].len < sizeof(u32)) {
                PRINT(IOS_EmuFS, ERROR,
                  "ReadDir: Invalid output file count vector");
                return ISFSError::Invalid;
            }

            outCountPtr = reinterpret_cast<u32*>(vec[3].data);
        } else {
            if (!aligned(vec[1].data, 4) || vec[1].len < sizeof(u32)) {
                PRINT(IOS_EmuFS, ERROR,
                  "ReadDir: Invalid output file count vector");
                return ISFSError::Invalid;
            }

            outCountPtr = reinterpret_cast<u32*>(vec[1].data);
        }

        // Check if the filepath should be replaced
        if (!IsReplacedFilepath(path))
            return mgrRes->ioctlv(ISFSIoctl::ReadDir, in_count, out_count, vec);

        // Get the replaced filepath
        if (!GetReplacedFilepath(path, efsFilepath, sizeof(efsFilepath)))
            return ISFSError::Invalid;

        DIR dir;
        auto fret = f_opendir(&dir, efsFilepath);
        if (fret != FR_OK) {
            PRINT(IOS_EmuFS, ERROR,
              "ReadDir: Failed to open replaced directory: %d", fret);
            return FResultToISFSError(fret);
        }

        FILINFO info;
        u32 count = 0;
        while ((fret = f_readdir(&dir, &info)) == FR_OK) {
            const char* name = info.fname;
            auto len = strlen(name);

            if (len <= 0)
                break;

            if (len > 12) {
                if (strlen(info.altname) < 1 || !strcmp(info.altname, "?"))
                    continue;
                name = info.altname;
            }

            if (count < inMaxCount) {
                char nameData[13] = {0};
                strncpy(nameData, name, sizeof(nameData));
                System::UnalignedMemcpy(
                  outNames + count * 13, nameData, sizeof(nameData));
            }

            assert(count < INT_MAX);
            count++;
        }

        auto fret2 = f_closedir(&dir);
        if (fret2 != FR_OK) {
            PRINT(IOS_EmuFS, ERROR, "ReadDir: f_closedir error: %d", fret2);
            return ISFSError::Unknown;
        }

        if (fret != FR_OK) {
            PRINT(IOS_EmuFS, ERROR, "ReadDir: f_readdir error: %d", fret);
            return FResultToISFSError(fret);
        }

        PRINT(IOS_EmuFS, INFO, "ReadDir: count: %u", count);
        *outCountPtr = count;

        return ISFSError::OK;
    }

    // [ISFS_GetUsage]
    // documentation todo
    case ISFSIoctl::GetUsage: {
        // TODO
        return mgrRes->ioctlv(ISFSIoctl::GetUsage, in_count, out_count, vec);
    }

    default:
        PRINT(IOS_EmuFS, ERROR, "Unknown manager ioctlv: %u", cmd);
        return ISFSError::Invalid;
    }
}

static s32 ForwardRequest(IOS::Request* req)
{
    const s32 fd = req->fd - REAL_HANDLE_BASE;
    assert(req->cmd == IOS::Command::Open || (fd >= 0 && fd < REAL_HANDLE_MAX));

    switch (req->cmd) {
    case IOS::Command::Open:
        // Should never reach here.
        assert(!"Open in ForwardRequest");
        return IOSError::NotFound;

    case IOS::Command::Close:
        return IOS_Close(fd);

    case IOS::Command::Read:
        return IOS_Read(fd, req->read.data, req->read.len);

    case IOS::Command::Write:
        return IOS_Write(fd, req->write.data, req->write.len);

    case IOS::Command::Seek:
        return IOS_Seek(fd, req->seek.where, req->seek.whence);

    case IOS::Command::Ioctl:
        return IOS_Ioctl(fd, req->ioctl.cmd, req->ioctl.in, req->ioctl.in_len,
          req->ioctl.io, req->ioctl.io_len);

    case IOS::Command::Ioctlv:
        return IOS_Ioctlv(fd, req->ioctlv.cmd, req->ioctlv.in_count,
          req->ioctlv.io_count, req->ioctlv.vec);

    default:
        PRINT(
          IOS_EmuFS, ERROR, "Unknown command: %u", static_cast<u32>(req->cmd));
        return ISFSError::Invalid;
    }
}

static s32 OpenReplaced(IOS::Request* req)
{
    char path[64];
    strncpy(path, req->open.path, 64);
    path[0] = '/';

    PRINT(IOS_EmuFS, INFO, "IOS_Open(\"%s\", 0x%X)", path, req->open.mode);

    if (!strcmp(path, "/dev/fs")) {
        PRINT(IOS_EmuFS, INFO, "Open /dev/fs from PPC");

        // Find open handle.
        u32 i = 0;
        for (; i < MGR_HANDLE_MAX; i++) {
            if (realFS[i].fd() < 0)
                break;
        }

        // There should always be an open handle.
        assert(i != MGR_HANDLE_MAX);

        // Security note! Interrupts will be disabled at this point
        // (IOS_Open always does), and the IPC thread can't do anything else
        // while it's waiting for a response from us, so this should be safe
        // to do to the root process..?
        s32 pid = IOS_GetProcessId();
        assert(pid >= 0);

        PRINT(IOS_EmuFS, INFO, "Set PID %d to uid %08X gid %04X", pid,
          req->open.uid, req->open.gid);

        s32 ret2 = IOS_SetUid(pid, req->open.uid);
        assert(ret2 == IOSError::OK);
        ret2 = IOS_SetGid(pid, req->open.gid);
        assert(ret2 == IOSError::OK);

        new (&realFS[i]) IOS::ResourceCtrl<ISFSIoctl>("/dev/fs");

        ret2 = IOS_SetUid(pid, 0);
        assert(ret2 == IOSError::OK);
        ret2 = IOS_SetGid(pid, 0);
        assert(ret2 == IOSError::OK);

        if (realFS[i].fd() < 0) {
            PRINT(IOS_EmuFS, INFO, "/dev/fs open error: %d", realFS[i].fd());
            return realFS[i].fd();
        }

        PRINT(IOS_EmuFS, INFO, "/dev/fs open success");
        return MGR_HANDLE_BASE + i;
    }

    if (!strncmp(path, "/dev", 4)) {
        // Fall through to the next resource.
        return IOSError::NotFound;
    }

    if (IsReplacedFilepath(path)) {
        return ReqProxyOpen(path, req->open.mode);
    }

    PRINT(IOS_EmuFS, INFO, "Forwarding open to real FS");
    return ForwardRequest(req);
}

static s32 IPCRequest(IOS::Request* req)
{
    s32 ret = IOSError::Invalid;

    s32 fd = req->fd;
    if (req->cmd != IOS::Command::Open &&
        GetDescriptorType(fd) == DescType::Real)
        return ForwardRequest(req);

    if (req->cmd != IOS::Command::Open &&
        GetDescriptorType(fd) == DescType::Direct &&
        (req->cmd == IOS::Command::Read || req->cmd == IOS::Command::Write ||
          req->cmd == IOS::Command::Seek || req->cmd == IOS::Command::Ioctl)) {
        s32 realFd = sDirectFileArray[fd - DIRECT_HANDLE_BASE].fd;

        // Switch to replaced file fd for future commands.
        if (!sDirectFileArray[fd - DIRECT_HANDLE_BASE].inUse ||
            !IsFileDescriptorValid(realFd)) {
            PRINT(
              IOS_EmuFS, ERROR, "Attempting to use an unopened direct file");
            return ISFSError::Invalid;
        }

        fd = realFd;
    }

    switch (req->cmd) {
    case IOS::Command::Open: {
        if (req->open.path[0] == '$') {
            // Replaced ISFS path.
            ret = OpenReplaced(req);
            break;
        }

        if (strcmp(req->open.path, "/dev/saoirse/file") != 0) {
            ret = IOSError::NotFound;
            break;
        }

        // Direct file open.
        // Find available direct file index.
        int i = 0;
        for (; i < DIRECT_HANDLE_MAX; i++) {
            if (!sDirectFileArray[i].inUse)
                break;
        }

        if (i == DIRECT_HANDLE_MAX) {
            ret = ISFSError::MaxOpen;
            break;
        }

        sDirectFileArray[i].inUse = true;
        sDirectFileArray[i].fd = ISFSError::NotFound;
        ret = DIRECT_HANDLE_BASE + i;
        break;
    }

    case IOS::Command::Close:
        PRINT(IOS_EmuFS, INFO, "IOS_Close(%d)", fd);
        ret = ReqClose(fd);
        break;

    case IOS::Command::Read:
        PRINT(IOS_EmuFS, INFO, "IOS_Read(%d, 0x%08X, 0x%X)", fd, req->read.data,
          req->read.len);
        ret = ReqRead(fd, req->read.data, req->read.len);
        break;

    case IOS::Command::Write:
        PRINT(IOS_EmuFS, INFO, "IOS_Write(%d, 0x%08X, 0x%X)", fd,
          req->write.data, req->write.len);
        ret = ReqWrite(fd, req->write.data, req->write.len);
        break;

    case IOS::Command::Seek:
        PRINT(IOS_EmuFS, INFO, "IOS_Seek(%d, %d, %d)", fd, req->seek.where,
          req->seek.whence);
        ret = ReqSeek(fd, req->seek.where, req->seek.whence);
        break;

    case IOS::Command::Ioctl:
        PRINT(IOS_EmuFS, INFO, "IOS_Ioctl(%d, %d, 0x%08X, 0x%X, 0x%08X, 0x%X)",
          fd, req->ioctl.cmd, req->ioctl.in, req->ioctl.in_len, req->ioctl.io,
          req->ioctl.io_len);
        ret = ReqIoctl(fd, static_cast<ISFSIoctl>(req->ioctl.cmd),
          req->ioctl.in, req->ioctl.in_len, req->ioctl.io, req->ioctl.io_len);
        break;

    case IOS::Command::Ioctlv:
        PRINT(IOS_EmuFS, INFO, "IOS_Ioctlv(%d, %d, %d, %d, 0x%08X)", fd,
          req->ioctlv.cmd, req->ioctlv.in_count, req->ioctlv.io_count,
          req->ioctlv.vec);
        ret = ReqIoctlv(fd, static_cast<ISFSIoctl>(req->ioctlv.cmd),
          req->ioctlv.in_count, req->ioctlv.io_count, req->ioctlv.vec);
        break;

    default:
        PRINT(
          IOS_EmuFS, ERROR, "Unknown command: %u", static_cast<u32>(req->cmd));
        ret = ISFSError::Invalid;
        break;
    }

    // Can't print on IOS_Open before game launches due to locked IPC thread
    if (req->cmd != IOS::Command::Open) {
        PRINT(IOS_EmuFS, INFO, "Reply: %d", ret);
    }

    return ret;
}

s32 ThreadEntry([[maybe_unused]] void* arg)
{
    PRINT(IOS_EmuFS, INFO, "Starting FS...");
    PRINT(IOS_EmuFS, INFO, "EmuFS thread ID: %d", IOS_GetThreadId());

    // Reset files
    for (int i = 0; i < REPLACED_HANDLE_NUM; i++) {
        sFileArray[REPLACED_HANDLE_BASE + i].inUse = false;
        sFileArray[REPLACED_HANDLE_BASE + i].filOpened = false;
    }

    for (int i = 0; i < DIRECT_HANDLE_MAX; i++) {
        sDirectFileArray[DIRECT_HANDLE_BASE + i].inUse = false;
        sDirectFileArray[DIRECT_HANDLE_BASE + i].fd = ISFSError::NotFound;
    }

    Queue<IOS::Request*> queue(8);
    s32 ret = IOS_RegisterResourceManager("$", queue.id());
    if (ret != IOSError::OK) {
        PRINT(IOS_EmuFS, ERROR, "IOS_RegisterResourceManager failed: %d", ret);
        abort();
    }

    ret = IOS_RegisterResourceManager("/dev/saoirse/file", queue.id());
    if (ret != IOSError::OK) {
        PRINT(IOS_EmuFS, ERROR, "IOS_RegisterResourceManager failed: %d", ret);
        abort();
    }

    IPCLog::sInstance->Notify();
    while (true) {
        IOS::Request* req = queue.receive();
        IOS_ResourceReply(reinterpret_cast<IOSRequest*>(req), IPCRequest(req));
    }
    /* Can never reach here */
    return 0;
}

} // namespace EmuFS
