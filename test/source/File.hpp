#pragma once

#include <EfsFile.h>
#include <gctypes.h>

/* 
 * The struct must be 4 byte aligned due to a hardware bug and a memcpy
 * implementation that doesn't work around it
 */
static_assert((sizeof(EfsFile) & 3) == 0,
    "sizeof(EfsFile) must be 4 byte aligned");

namespace irse
{

enum class FResult
{
    FR_OK = 0,				/* (0) Succeeded */
	FR_DISK_ERR,			/* (1) A hard error occurred in the low level disk I/O layer */
	FR_INT_ERR,				/* (2) Assertion failed */
	FR_NOT_READY,			/* (3) The physical drive cannot work */
	FR_NO_FILE,				/* (4) Could not find the file */
	FR_NO_PATH,				/* (5) Could not find the path */
	FR_INVALID_NAME,		/* (6) The path name format is invalid */
	FR_DENIED,				/* (7) Access denied due to prohibited access or directory full */
	FR_EXIST,				/* (8) Access denied due to prohibited access */
	FR_INVALID_OBJECT,		/* (9) The file/directory object is invalid */
	FR_WRITE_PROTECTED,		/* (10) The physical drive is write protected */
	FR_INVALID_DRIVE,		/* (11) The logical drive number is invalid */
	FR_NOT_ENABLED,			/* (12) The volume has no work area */
	FR_NO_FILESYSTEM,		/* (13) There is no valid FAT volume */
	FR_MKFS_ABORTED,		/* (14) The f_mkfs() aborted due to any problem */
	FR_TIMEOUT,				/* (15) Could not get a grant to access the volume within defined period */
	FR_LOCKED,				/* (16) The operation is rejected according to the file sharing policy */
	FR_NOT_ENOUGH_CORE,		/* (17) LFN working buffer could not be allocated */
	FR_TOO_MANY_OPEN_FILES,	/* (18) Number of open files > FF_FS_LOCK */
	FR_INVALID_PARAMETER	/* (19) Given parameter is invalid */
};

/* File access mode and open method flags (3rd argument of f_open) */
#define	FA_READ				0x01
#define	FA_WRITE			0x02
#define	FA_OPEN_EXISTING	0x00
#define	FA_CREATE_NEW		0x04
#define	FA_CREATE_ALWAYS	0x08
#define	FA_OPEN_ALWAYS		0x10
#define	FA_OPEN_APPEND		0x30

class file
{
public:
    static void init();

    file(EfsFile& f) {
        /* [FIXME] Maybe the old file should be invalidated? Unsure, it should
         * be fine with multiple instances */
        this->m_f = f;
    }
    file(const char* path, u8 mode) {
        open(path, mode);
    }

    ~file() {
        close();
    }
    
    FResult read(void* data, u32 len, u32& read);
    FResult write(const void* data, u32 len, u32& wrote);
    FResult lseek(u32 offset);
    FResult truncate();
    FResult sync();

    u32 tell() {
        return static_cast<u32>(this->m_f.fat.fptr);
    }
    /* Returns true if the end of the file has been reached */
    bool eof() {
        return this->m_f.fat.fptr == this->m_f.fat.obj_size;
    }
    u32 size() {
        return static_cast<u32>(this->m_f.fat.obj_size);
    }
    /* Returns true if an error has occurred */
    bool error() {
        return static_cast<bool>(this->m_f.fat.err);
    }
    FResult result() {
        return this->m_result;
    }

    EfsFile& fil() { return m_f; }

protected:
    FResult open(const char* path, u8 mode);
    FResult close();
    EfsFile m_f;
    FResult m_result;
};

} // namespace irse