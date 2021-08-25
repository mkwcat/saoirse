#include <types.h>
#include <main.h>
#include <ios.h>
#include <nand.h>
#include <os.h>
#include <ff.h>
#include <disk.h>
#include <sdcard.h>

#include <cstring>
#include <cstdio>

namespace EFS
{

#define EFS_DRIVE "0:/"

static FIL* spFileDescriptorArray[NAND_MAX_FILE_DESCRIPTOR_AMOUNT];

/*---------------------------------------------------------------------------*
 * Name        : IsFileDescriptorValid
 * Description : Checks if a file descriptor is valid.
 * Arguments   : fd    The file descriptor to check.
 * Returns     : 0     The file descriptor is invalid.
 *               1     The file descriptor is valid.
 *---------------------------------------------------------------------------*/
static bool IsFileDescriptorValid(int fd)
{
    if (fd < 0 || fd > NAND_MAX_FILE_DESCRIPTOR_AMOUNT)
        return false;

    return (spFileDescriptorArray[fd] != NULL);
}

/*---------------------------------------------------------------------------*
 * Name        : GetAvailableFileDescriptor
 * Description : Gets an available index in the file descriptor array.
 * Returns     : An available index in the file descriptor array, or -1 if
 *               there is no index available in the file descriptor array.
 *---------------------------------------------------------------------------*/
static int GetAvailableFileDescriptor()
{
    for (int i = 0; i < NAND_MAX_FILE_DESCRIPTOR_AMOUNT; i++)
        if (spFileDescriptorArray[i] == NULL)
            return i;
    return -1;
}

/*---------------------------------------------------------------------------*
 * Name        : IsFilenameValid
 * Description : Checks if a filename is a valid 8.3 filename.
 * Arguments   : filename    The filename to check.
 * Returns     : 0           The filename is not a valid 8.3 filename.
 *               1           The filename is a valid 8.3 filename.
 *---------------------------------------------------------------------------*/
static bool IsFilenameValid(const char* filename)
{
    const int MIN_83_FILENAME_LENGTH           = 1;
    const int MAX_83_FILENAME_LENGTH           = 8;
    const int MAX_83_FILENAME_EXTENSION_LENGTH = 3;

    enum
    {
        CHARACTER_SPACE  = 0x20,
        CHARACTER_PERIOD = 0x2E,
        CHARACTER_DELETE = 0x7F
    };

    int filenameLength;
    const char* filenameExtension = NULL;
    for (filenameLength = 0; filename[filenameLength]; filenameLength++)
    {
        char c = filename[filenameLength];

        // A 8.3 filename must only contain characters that can be represented in ASCII, in the range below 0x80
        if ((unsigned char)c > CHARACTER_DELETE)
            return false;

        // A 8.3 filename must not contain the space character
        if (c == CHARACTER_SPACE)
            return false;

        // A 8.3 filename must not contain more than one period character
        if (c == CHARACTER_PERIOD)
        {
            if (filenameExtension)
                return false;

            filenameExtension = &filename[filenameLength + 1];
        }
    }
    
    size_t filenameExtensionLength;
    if (filenameExtension && (filenameExtensionLength = strlen(filenameExtension)))
    {
        // Don't include the length of the extension in the filename length
        filenameLength = ((filenameExtension - 1) - filename);

        // The filename extension, if present, must be 1-3 characters in length
        if (filenameExtensionLength > MAX_83_FILENAME_EXTENSION_LENGTH)
            return false;
    }
    
    // The base filename must be 1-8 characters in length
    return (filenameLength >= MIN_83_FILENAME_LENGTH && filenameLength <= MAX_83_FILENAME_LENGTH);
}

static s32 ForwardRequest([[maybe_unused]] IOS::Request* req)
{
    /* TODO */
    return IOSErr::NotFound;
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

/* 
 * Handle open request from the filesystem proxy.
 * Returns: File descriptor, or ISFS error code.
 */
static s32 ReqOpen(const char* path, u32 mode)
{
    int fd = GetAvailableFileDescriptor();
    if (fd < 0)
        return ISFSError::MaxOpen;

    if (!path)
        return ISFSError::Invalid;

    if (path[0] != NAND_DIRECTORY_SEPARATOR_CHAR)
        return ISFSError::Invalid;
    
    if (strlen(path) >= NAND_MAX_FILEPATH_LENGTH)
        return ISFSError::Invalid;
    
    if (mode > IOS_OPEN_RW)
        return ISFSError::Invalid;

    // The second check guarantees that there will be at least one instance of the "NAND_DIRECTORY_SEPARATOR_CHAR" character in the character string
    const char* filename = strrchr(path, NAND_DIRECTORY_SEPARATOR_CHAR) + 1;
    if (!IsFilenameValid(filename))
    {
        peli::Log(LogL::ERROR, "[EFS::ReqOpen] Filename '%s' is invalid !", filename);
        return ISFSError::Invalid;
    }

    // Create the filepath to open
    char efsFilepath[NAND_MAX_FILENAME_LENGTH + sizeof(EFS_DRIVE)];
    snprintf(efsFilepath, NAND_MAX_FILENAME_LENGTH + sizeof(EFS_DRIVE), EFS_DRIVE "%s", filename);

    spFileDescriptorArray[fd] = new FIL;
    ASSERT(IsFileDescriptorValid(fd));
    const FRESULT fret = f_open(spFileDescriptorArray[fd], efsFilepath, mode);
    if (fret != FR_OK)
    {
        peli::Log(LogL::ERROR,
            "[EFS::ReqOpen] Failed to open file '%s', error: %d",
            efsFilepath, fret);
        
        delete spFileDescriptorArray[fd];
        return FResultToISFSError(fret);
    }
    
    peli::Log(LogL::INFO, "[EFS::ReqOpen] Successfully opened file '%s' (fd=%d, mode=%d) !", efsFilepath, fd, mode);

    return fd;
}

/*
 * Close open file descriptor.
 * Returns: 0 for success, or IOS error code.
 */
static s32 ReqClose(s32 fd)
{
    if (!IsFileDescriptorValid(fd))
        return ISFSError::Invalid;

    if (f_close(spFileDescriptorArray[fd]) != FR_OK)
    {
        peli::Log(LogL::ERROR, "[EFS::ReqClose] Failed to close file descriptor %d !", fd);
        return ISFSError::Unknown;
    }

    delete spFileDescriptorArray[fd];
    spFileDescriptorArray[fd] = nullptr;

    peli::Log(LogL::INFO, "[EFS::ReqClose] Successfully closed file descriptor %d !", fd);

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

    unsigned int bytesRead;
    const FRESULT fret
        = f_read(spFileDescriptorArray[fd], data, len, &bytesRead);
    if (fret != FR_OK)
    {
        peli::Log(LogL::ERROR,
                  "[EFS::ReqRead] Failed to read %d bytes from file descriptor "
                  "%d, error: %d",
                  bytesRead, fd, fret);
        return FResultToISFSError(fret);
    }

    peli::Log(LogL::INFO, "[EFS::ReqRead] Successfully read %d bytes from file descriptor %d !", bytesRead, fd);

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

    unsigned int bytesWrote;
    const FRESULT fret
        = f_write(spFileDescriptorArray[fd], data, len, &bytesWrote);
    if (fret != FR_OK)
    {
        peli::Log(LogL::ERROR,
                  "[EFS::ReqWrite] Failed to write %d bytes to file descriptor "
                  "%d, error: %d",
                  bytesWrote, fd, fret);
        return FResultToISFSError(fret);
    }

    peli::Log(LogL::INFO, "[EFS::ReqWrite] Successfully wrote %d bytes to file descriptor %d !", bytesWrote, fd);

    return bytesWrote;
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