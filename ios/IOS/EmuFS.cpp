// EmuFS.cpp - Emulated IOS filesystem RM
//   Written by Star
//   Written by Palapeli
//
// Copyright (C) 2022 Team Saoirse
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
#define EFS_MAX_PATH_LEN 2048

static char efsFilepath[EFS_MAX_PATH_LEN];
static char efsFilepath2[EFS_MAX_PATH_LEN];

struct ProxyFile {
    bool ipcFile;
    bool inUse;
    bool filOpened;
    char path[64];
    u32 mode;
    FIL fil;
};

static std::array<ProxyFile, NAND_MAX_FILE_DESCRIPTOR_AMOUNT> sFileArray;

struct DirectFile {
    bool inUse;
    int fd;
};

static std::array<DirectFile, DIRECT_HANDLE_MAX> sDirectFileArray;

static std::array<IOS::ResourceCtrl<ISFSIoctl>, MGR_HANDLE_MAX> realFS;

enum class DescType
{
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

/*---------------------------------------------------------------------------*
 * Name        : IsFileDescriptorValid
 * Description : Checks if a file descriptor is valid.
 * Arguments   : fd    The file descriptor to check.
 * Returns     : If the file descriptor is valid.
 *---------------------------------------------------------------------------*/
static bool IsFileDescriptorValid(int fd)
{
    if (fd < 0 || fd >= static_cast<int>(sFileArray.size()))
        return false;

    if (!sFileArray[fd].inUse)
        return false;

    return true;
}

static int RegisterFileDescriptor(const char* path)
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
        PRINT(IOS_EmuFS, ERROR,
              "[EmuFS::TryCloseFileDescriptor] Failed to close file, error: %d",
              fret);
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

    return (strnlen(filepath, NAND_MAX_FILEPATH_LENGTH) <
            NAND_MAX_FILEPATH_LENGTH);
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
static const char* GetReplacedFilepath(const char* filepath, char* out_buf,
                                       size_t out_len)
{
    if (!IsFilepathValid(filepath))
        return nullptr;

    if (!out_buf)
        return nullptr;

    // Create and write the replaced filepath
    filepath = strchr(filepath, NAND_DIRECTORY_SEPARATOR_CHAR);
    if (snprintf(out_buf, out_len, EFS_DRIVE "%s", filepath + 1) <= 0) {
        PRINT(IOS_EmuFS, ERROR,
              "[EmuFS::GetReplacedFilepath] Failed to format "
              "the replaced filepath !");
        return nullptr;
    }

    PRINT(IOS_EmuFS, INFO,
          "[EmuFS::GetReplacedFilepath] Replaced file path: \"%s\"", out_buf);

    return out_buf;
}

static u8 efsCopyBuffer[0x2000] ATTRIBUTE_ALIGN(32); // 8 KB

static s32 CopyFromNandToEFS(const char* nandPath, const char* efsPath)
{
    IOS::File isfsFile(nandPath, IOS::Mode::Read);

    if (isfsFile.fd() < 0) {
        PRINT(IOS_EmuFS, ERROR,
              "[EmuFS::CopyFromNandToEFS] Failed to open ISFS file: %d",
              isfsFile.fd());
        return isfsFile.fd();
    }

    s32 size = isfsFile.size();
    PRINT(IOS_EmuFS, INFO, "[EmuFS::CopyFromNandToEFS] File size: 0x%X", size);

    FIL fil;
    FRESULT fret = f_open(&fil, efsPath, FA_WRITE | FA_CREATE_NEW);

    if (fret != FR_OK) {
        PRINT(IOS_EmuFS, ERROR,
              "[EmuFS::CopyFromNandToEFS] Failed to open EFS file: %d", fret);
        return FResultToISFSError(fret);
    }

    for (s32 pos = 0; pos < size; pos += sizeof(efsCopyBuffer)) {
        u32 readlen = size - pos;
        if (readlen > sizeof(efsCopyBuffer))
            readlen = sizeof(efsCopyBuffer);

        s32 ret = isfsFile.read(efsCopyBuffer, readlen);

        if ((u32)ret != readlen) {
            f_close(&fil);
            PRINT(IOS_EmuFS, ERROR,
                  "[EmuFS::CopyFromNandToEFS] Failed to read from ISFS file: "
                  "%d != %d",
                  ret, readlen);
            if (ret < 0)
                return ret;
            return ISFSError::Unknown;
        }

        UINT bw;
        fret = f_write(&fil, efsCopyBuffer, readlen, &bw);

        if (fret != FR_OK || (u32)bw != readlen) {
            f_close(&fil);
            PRINT(IOS_EmuFS, ERROR,
                  "[EmuFS::CopyFromNandToEFS] Failed to write to EFS file: "
                  "%d != 0 OR %d != %d",
                  fret, readlen, bw);
            if (fret != FR_OK)
                return FResultToISFSError(fret);
            return ISFSError::Unknown;
        }
    }

    f_close(&fil);
    return ISFSError::OK;
}

static s32 ReopenFile(s32 fd)
{
    const FRESULT fret = f_lseek(&sFileArray[fd].fil, 0);
    if (fret != FR_OK) {
        PRINT(IOS_EmuFS, ERROR,
              "[EmuFS::ReqProxyOpen] Failed to seek to position 0x%08X "
              "in file descriptor %d !",
              0, fd);

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
        PRINT(IOS_EmuFS, ERROR,
              "[EmuFS::ReqProxyOpen] Could not register file descriptor: %d",
              fd);
        return fd;
    }
    PRINT(IOS_EmuFS, INFO,
          "[EmuFS::ReqProxyOpen] Registered file descriptor %d", fd);

    ASSERT(IsFileDescriptorValid(fd));

    sFileArray[fd].mode = mode;

    if (sFileArray[fd].filOpened) {
        PRINT(IOS_EmuFS, INFO,
              "[EmuFS::ReqProxyOpen] File already open, reusing descriptor");
        return ReopenFile(fd);
    }

    const FRESULT fret =
        f_open(&sFileArray[fd].fil, efsFilepath, FA_READ | FA_WRITE);
    if (fret != FR_OK) {
        PRINT(IOS_EmuFS, ERROR,
              "[EmuFS::ReqProxyOpen] Failed to open file '%s', error: %d",
              efsFilepath, fret);

        FreeFileDescriptor(fd);
        return FResultToISFSError(fret);
    }

    sFileArray[fd].filOpened = true;

    PRINT(IOS_EmuFS, INFO,
          "[EmuFS::ReqProxyOpen] Successfully opened file '%s' (fd=%d, "
          "mode=%u) !",
          efsFilepath, fd, mode);

    return fd;
}

/*
 * Handles direct open file requests.
 * Returns: File descriptor, or ISFS error code.
 */
static s32 ReqDirectOpen(const char* filepath, u32 mode)
{
    int fd = FindAvailableFileDescriptor();
    if (fd < 0) {
        PRINT(IOS_EmuFS, ERROR,
              "[EmuFS::ReqDirectOpen] Could not find an open file descriptor!");
        return fd;
    }

    sFileArray[fd].mode = mode;
    sFileArray[fd].inUse = false;
    sFileArray[fd].filOpened = false;

    const FRESULT fret = f_open(&sFileArray[fd].fil, filepath, mode);
    if (fret != FR_OK) {
        PRINT(IOS_EmuFS, ERROR,
              "[EmuFS::ReqDirectOpen] Failed to open file '%s', mode: %X, "
              "error: %d",
              filepath, mode, fret);
        return FResultToISFSError(fret);
    }

    sFileArray[fd].inUse = true;
    sFileArray[fd].filOpened = true;

    PRINT(IOS_EmuFS, INFO,
          "[EmuFS::ReqDirectOpen] Successfully opened file '%s' (fd=%d, "
          "mode=%u) !",
          filepath, fd, mode);

    return fd;
}

/*
 * Close open file descriptor.
 * Returns: 0 for success, or IOS error code.
 */
static s32 ReqClose(s32 fd)
{
    if (IsManagerHandle(fd)) {
        s32 ret = GetManagerResource(fd)->close();
        assert(ret == IOSError::OK);
        return IOSError::OK;
    }

    if (!IsFileDescriptorValid(fd))
        return ISFSError::Invalid;

    if (f_sync(&sFileArray[fd].fil) != FR_OK) {
        PRINT(IOS_EmuFS, ERROR,
              "[EmuFS::ReqClose] Failed to sync file descriptor %d !", fd);
        return ISFSError::Unknown;
    }

    FreeFileDescriptor(fd);

    PRINT(IOS_EmuFS, INFO,
          "[EmuFS::ReqClose] Successfully closed file descriptor %d !", fd);

    return ISFSError::OK;
}

/*
 * Read data from open file descriptor.
 * Returns: Amount read, or ISFS error code.
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
              "[EmuFS::ReqRead] Failed to read %u bytes from file descriptor "
              "%d, error: %d",
              len, fd, fret);
        return FResultToISFSError(fret);
    }

    PRINT(
        IOS_EmuFS, INFO,
        "[EmuFS::ReqRead] Successfully read %u bytes from file descriptor %d !",
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
              "[EmuFS::ReqWrite] Failed to write %u bytes to file descriptor "
              "%d, error: %d",
              len, fd, fret);
        return FResultToISFSError(fret);
    }

    PRINT(
        IOS_EmuFS, INFO,
        "[EmuFS::ReqWrite] Successfully wrote %u bytes to file descriptor %d !",
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
              "[EmuFS::ReqSeek] Failed to seek to position 0x%08X in file "
              "descriptor %d !",
              offset, fd);
        return FResultToISFSError(fresult);
    }

    PRINT(IOS_EmuFS, INFO,
          "[EmuFS::ReqSeek] Successfully seeked to position 0x%08X in file "
          "descriptor %d !",
          offset, fd);

    return offset;
}

/*
 * Handles filesystem ioctl commands.
 * Returns: ISFSError result.
 */
static s32 ReqIoctl(s32 fd, ISFSIoctl cmd, void* in, u32 in_len, void* io,
                    u32 io_len)
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
                PRINT(IOS_EmuFS, ERROR,
                      "[EmuFS::ReqIoctl] Invalid GetFileStats input alignment");
                return ISFSError::Invalid;
            }
            IOS::File::Stat* stat = reinterpret_cast<IOS::File::Stat*>(io);
            stat->size = f_size(&sFileArray[fd].fil);
            stat->pos = f_tell(&sFileArray[fd].fil);
            return ISFSError::OK;
        }

        PRINT(IOS_EmuFS, ERROR, "[EmuFS::ReqIoctl] Unknown file ioctl: %u",
              static_cast<s32>(cmd));
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
        PRINT(IOS_EmuFS, ERROR,
              "[EmuFS::ReqIoctl] Attempt to use ISFS_Format!");
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

        ISFSAttrBlock* isfsAttrBlock = (ISFSAttrBlock*)in;

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
                  "[EmuFS::ReqIoctl] Failed to create directory '%s' !",
                  efsFilepath);
            return FResultToISFSError(fresult);
        }

        PRINT(IOS_EmuFS, INFO,
              "[EmuFS::ReqIoctl] Successfully created directory '%s' !",
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

        ISFSAttrBlock* isfsAttrBlock = (ISFSAttrBlock*)in;

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
                  "[EmuFS::ReqIoctl] Failed to set attributes for file or "
                  "directory '%s' !",
                  efsFilepath);
            return FResultToISFSError(fresult);
        }

        PRINT(IOS_EmuFS, INFO,
              "[EmuFS::ReqIoctl] Successfully set attributes for file or "
              "directory '%s' !",
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

        const char* filepath = (const char*)in;

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
                  "[EmuFS::ReqIoctl] Failed to get attributes for file or "
                  "directory '%s' !",
                  efsFilepath);
            return FResultToISFSError(fresult);
        }

        ISFSAttrBlock* isfsAttrBlock = (ISFSAttrBlock*)io;
        isfsAttrBlock->ownerId = IOS_GetUid();
        isfsAttrBlock->groupId = IOS_GetGid();
        strcpy(isfsAttrBlock->path, filepath);
        isfsAttrBlock->ownerPerm = OWNER_PERM;
        isfsAttrBlock->groupPerm = GROUP_PERM;
        isfsAttrBlock->otherPerm = OTHER_PERM;
        isfsAttrBlock->attributes = ATTRIBUTES;

        PRINT(IOS_EmuFS, INFO,
              "[EmuFS::ReqIoctl] Successfully got attributes for file or "
              "directory '%s' !",
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

        const char* filepath = (const char*)in;

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
                  "[EmuFS::ReqIoctl] Failed to delete file or directory '%s' !",
                  efsFilepath);
            return FResultToISFSError(fresult);
        }

        PRINT(IOS_EmuFS, INFO,
              "[EmuFS::ReqIoctl] Successfully deleted file or directory '%s' !",
              efsFilepath);

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

        ISFSRenameBlock* isfsRenameBlock = (ISFSRenameBlock*)in;

        const char* pathOld = isfsRenameBlock->pathOld;
        const char* pathNew = isfsRenameBlock->pathNew;
        PRINT(IOS_EmuFS, INFO, "[EmuFS::ReqIoctl] Rename(%s, %s)", pathOld,
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
            if (!GetReplacedFilepath(pathNew, efsNewFilepath, EFS_MAX_PATH_LEN))
                return ISFSError::Invalid;
            s32 ret = CopyFromNandToEFS(pathOld, efsNewFilepath);
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
                  "[EmuFS::ReqIoctl] Failed to rename file or directory '%s' "
                  "to '%s' !",
                  efsOldFilepath, efsNewFilepath);
            return FResultToISFSError(fresult);
        }

        PRINT(IOS_EmuFS, INFO,
              "[EmuFS::ReqIoctl] Successfully renamed file or directory '%s' "
              "to '%s' !",
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

        ISFSAttrBlock* isfsAttrBlock = (ISFSAttrBlock*)in;

        const char* path = isfsAttrBlock->path;

        PRINT(IOS_EmuFS, INFO, "[EmuFS::ReqIoctl] ISFS_CreateFile(\"%s\")",
              path);

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
            PRINT(IOS_EmuFS, ERROR,
                  "[EmuFS::ReqIoctl] Failed to create file '%s' !",
                  efsFilepath);
            return FResultToISFSError(fresult);
        }

        f_sync(&fil);

        s32 ret = FindAvailableFileDescriptor();
        if (ret >= 0 && ret < NAND_MAX_FILE_DESCRIPTOR_AMOUNT) {
            sFileArray[ret].filOpened = true;
            sFileArray[ret].fil = fil; // Copy
        }

        PRINT(IOS_EmuFS, INFO,
              "[EmuFS::ReqIoctl] Successfully created file '%s' !",
              efsFilepath);

        return ISFSError::OK;
    }

    default:
        PRINT(IOS_EmuFS, ERROR, "[EmuFS::ReqIoctl] Unknown manager ioctl: %u",
              cmd);
        return ISFSError::Invalid;
    }
}

/*
 * Handles filesystem ioctlv commands.
 * Returns: ISFSError result.
 */
static s32 ReqIoctlv(s32 fd, ISFSIoctl cmd, u32 in_count, u32 out_count,
                     IOS::Vector* vec)
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
        if (cmd != ISFSIoctl::OpenDirect) {
            PRINT(IOS_EmuFS, ERROR,
                  "[EmuFS::ReqIoctlv] Unknown direct ioctl: %u",
                  static_cast<s32>(cmd));
            return ISFSError::Invalid;
        }

        if (in_count != 2 || out_count != 0) {
            PRINT(IOS_EmuFS, ERROR,
                  "[EmuFS::ReqIoctlv] Direct ioctl: wrong vector count!");
            return ISFSError::Invalid;
        }

        if (vec[0].len < 1 || vec[0].len > EFS_MAX_PATH_LEN) {
            PRINT(IOS_EmuFS, ERROR,
                  "[EmuFS::ReqIoctlv] Direct ioctl: invalid path length: %d",
                  vec[0].len);
            return ISFSError::Invalid;
        }

        if (vec[1].len != sizeof(u32)) {
            PRINT(
                IOS_EmuFS, ERROR,
                "[EmuFS::ReqIoctlv] Direct ioctl: invalid open mode length: %d",
                vec[1].len);
            return ISFSError::Invalid;
        }

        if (!aligned(vec[1].data, 4)) {
            PRINT(IOS_EmuFS, ERROR,
                  "[EmuFS::ReqIoctlv] Direct ioctl: invalid open mode "
                  "alignment!");
            return ISFSError::Invalid;
        }

        // Check if the supplied file path length is valid.
        if (strnlen(reinterpret_cast<const char*>(vec[0].data), vec[0].len) ==
            vec[0].len) {
            PRINT(IOS_EmuFS, ERROR,
                  "[EmuFS::ReqIoctlv] Direct ioctl: path does not terminate!");
            return ISFSError::Invalid;
        }

        s32 realFd = ReqDirectOpen(reinterpret_cast<const char*>(vec[0].data),
                                   *reinterpret_cast<u32*>(vec[1].data));
        if (realFd < 0)
            return realFd;

        sDirectFileArray[fd - DIRECT_HANDLE_BASE].inUse = true;
        sDirectFileArray[fd - DIRECT_HANDLE_BASE].fd = realFd;
        return ISFSError::OK;
    }

    if (!IsManagerHandle(fd))
        return ISFSError::Invalid;

    auto mgrRes = GetManagerResource(fd);

    switch (cmd) {
    // [ISFS_ReadDir]
    // vec[0]: path
    // todo
    case ISFSIoctl::ReadDir: {
        const char* path = (const char*)vec[0].data;

        PRINT(IOS_EmuFS, INFO, "[EmuFS::ReqIoctlv] ISFS_ReadDir(\"%s\")", path);

        // XXX actually implement this properly

        // Check if the filepath should be replaced
        if (!IsReplacedFilepath(path))
            return mgrRes->ioctlv(ISFSIoctl::ReadDir, in_count, out_count, vec);

        // Get the replaced filepath
        if (!GetReplacedFilepath(path, efsFilepath, sizeof(efsFilepath)))
            return ISFSError::Invalid;

        FIL fil;
        if (f_open(&fil, efsFilepath, FA_READ | FA_OPEN_EXISTING) !=
            FR_NO_FILE) {
            f_close(&fil);
            return ISFSError::Invalid;
        }

        return ISFSError::NotFound;
    }

    // [ISFS_GetUsage]
    // documentation todo
    case ISFSIoctl::GetUsage: {
        // TODO
        return mgrRes->ioctlv(ISFSIoctl::GetUsage, in_count, out_count, vec);
    }

    default:
        PRINT(IOS_EmuFS, ERROR, "[EmuFS::ReqIoctlv] Unknown manager ioctlv: %u",
              cmd);
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
        assert(0);
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
        PRINT(IOS_EmuFS, ERROR, "EFS: Unknown command: %u",
              static_cast<u32>(req->cmd));
        return ISFSError::Invalid;
    }
}

static s32 OpenReplaced(IOS::Request* req)
{
    char path[64];
    strncpy(path, req->open.path, 64);
    path[0] = '/';

    PRINT(IOS_EmuFS, INFO, "[EmuFS::IPCRequest] IOS_Open(%s, 0x%X)", path,
          req->open.mode);

    if (!strcmp(path, "/dev/fs")) {
        PRINT(IOS_EmuFS, INFO, "[EmuFS::IPCRequest] Open /dev/fs from PPC");

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

        PRINT(IOS_EmuFS, INFO,
              "[EmuFS::IPCRequest] Set PID %d to uid %08X gid %04X", pid,
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
            PRINT(IOS_EmuFS, INFO, "[EmuFS::IPCRequest] /dev/fs open error: %d",
                  realFS[i].fd());
            return realFS[i].fd();
        }

        PRINT(IOS_EmuFS, INFO, "[EmuFS::IPCRequest] /dev/fs open success");
        return MGR_HANDLE_BASE + i;
    }

    if (!strncmp(path, "/dev", 4)) {
        // Fall through to the next resource.
        return IOSError::NotFound;
    }

    if (IsReplacedFilepath(path)) {
        return ReqProxyOpen(path, req->open.mode);
    }

    PRINT(IOS_EmuFS, INFO, "[EmuFS::IPCRequest] Forwarding open to real FS");
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
        GetDescriptorType(fd) == DescType::Direct) {
        ASSERT(sDirectFileArray[fd - DIRECT_HANDLE_BASE].inUse);
        s32 realFd = sDirectFileArray[fd - DIRECT_HANDLE_BASE].fd;

        // Switch to replaced file fd for future commands.
        if (IsFileDescriptorValid(realFd))
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
            PRINT(IOS_EmuFS, ERROR,
                  "[EmuFS::IPCRequest] No direct file handles available!");
            ret = ISFSError::MaxOpen;
            break;
        }

        sDirectFileArray[i].inUse = true;
        sDirectFileArray[i].fd = ISFSError::NotFound;
        ret = DIRECT_HANDLE_BASE + i;
        break;
    }

    case IOS::Command::Close:
        PRINT(IOS_EmuFS, INFO, "[EmuFS::IPCRequest] IOS_Close(%d)", fd);
        ret = ISFSError::OK;

        // If the fd is Direct, this means the file hasn't been opened and this
        // will error. We probably need a much more clear "has this been opened"
        // flag.
        if (GetDescriptorType(fd) != DescType::Direct)
            ret = ReqClose(fd);

        if (ret >= 0 && GetDescriptorType(req->fd) == DescType::Direct) {
            sDirectFileArray[req->fd - DIRECT_HANDLE_BASE].inUse = false;
            sDirectFileArray[req->fd - DIRECT_HANDLE_BASE].fd =
                ISFSError::NotFound;
        }
        break;

    case IOS::Command::Read:
        PRINT(IOS_EmuFS, INFO, "[EmuFS::IPCRequest] IOS_Read(%d, 0x%08X, 0x%X)",
              fd, req->read.data, req->read.len);
        ret = ReqRead(fd, req->read.data, req->read.len);
        break;

    case IOS::Command::Write:
        PRINT(IOS_EmuFS, INFO,
              "[EmuFS::IPCRequest] IOS_Write(%d, 0x%08X, 0x%X)", fd,
              req->write.data, req->write.len);
        ret = ReqWrite(fd, req->write.data, req->write.len);
        break;

    case IOS::Command::Seek:
        PRINT(IOS_EmuFS, INFO, "[EmuFS::IPCRequest] IOS_Seek(%d, %d, %d)", fd,
              req->seek.where, req->seek.whence);
        ret = ReqSeek(fd, req->seek.where, req->seek.whence);
        break;

    case IOS::Command::Ioctl:
        PRINT(IOS_EmuFS, INFO,
              "[EmuFS::IPCRequest] IOS_Ioctl(%d, %d, 0x%08X, 0x%X, 0x%08X, "
              "0x%X)",
              fd, req->ioctl.cmd, req->ioctl.in, req->ioctl.in_len,
              req->ioctl.io, req->ioctl.io_len);
        ret =
            ReqIoctl(fd, static_cast<ISFSIoctl>(req->ioctl.cmd), req->ioctl.in,
                     req->ioctl.in_len, req->ioctl.io, req->ioctl.io_len);
        break;

    case IOS::Command::Ioctlv:
        PRINT(IOS_EmuFS, INFO,
              "[EmuFS::IPCRequest] IOS_Ioctlv(%d, %d, %d, %d, 0x%08X)", fd,
              req->ioctlv.cmd, req->ioctlv.in_count, req->ioctlv.io_count,
              req->ioctlv.vec);
        ret = ReqIoctlv(fd, static_cast<ISFSIoctl>(req->ioctlv.cmd),
                        req->ioctlv.in_count, req->ioctlv.io_count,
                        req->ioctlv.vec);
        break;

    default:
        PRINT(IOS_EmuFS, ERROR, "[EmuFS::IPCRequest] Unknown command: %u",
              static_cast<u32>(req->cmd));
        ret = ISFSError::Invalid;
        break;
    }

    PRINT(IOS_EmuFS, INFO, "[EmuFS::IPCRequest] Reply: %d", ret);
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
        PRINT(IOS_EmuFS, ERROR,
              "[EmuFS::ThreadEntry] IOS_RegisterResourceManager failed: %d",
              ret);
        abort();
    }

    ret = IOS_RegisterResourceManager("/dev/saoirse/file", queue.id());
    if (ret != IOSError::OK) {
        PRINT(IOS_EmuFS, ERROR,
              "[EmuFS::ThreadEntry] IOS_RegisterResourceManager failed: %d",
              ret);
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