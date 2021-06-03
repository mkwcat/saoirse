#ifndef _ISFS_H
#define _ISFS_H

#include <types.h>
#include <ios.h>

#ifdef __cplusplus
    extern "C" {
#endif

#define ISFS_PATH                           "/dev/fs"

#define ISFS_IOCTL_GETFILESTATS             11

#define ISFS_EINVAL       -101
#define ISFS_EACCESS      -102
#define ISFS_ECORRUPT     -103
#define ISFS_EEXIST       -105
#define ISFS_ENOENT       -106
#define ISFS_ENFILE       -107
#define ISFS_EFBIG        -108
#define ISFS_EFDEXHAUSTED -109
#define ISFS_ENAMELEN     -110
#define ISFS_EFDOPEN      -111
#define ISFS_EIO          -114
#define ISFS_ENOTEMPTY    -115
#define ISFS_EDIRDEPTH    -116
#define ISFS_EBUSY        -118

typedef struct
{
    u32 length;
    u32 pos;
} Fstats;

#ifdef IOS
#define ISFS_Open IOS_Open
#define ISFS_Close IOS_Close
#define ISFS_Read IOS_Read
#define ISFS_ReadAsync IOS_ReadAsync
#define ISFS_Write IOS_Write
#define ISFS_WriteAsync IOS_WriteAsync
#define ISFS_Seek IOS_Seek
#define ISFS_SeekAsync IOS_SeekAsync
#define ISFS_GetFileStats(fd, stats) IOS_Ioctl( \
    fd, ISFS_IOCTL_GETFILESTATS, NULL, 0, (void*) stats, sizeof(Fstats))
#define ISFS_GetFileStatsAsync(fd, stats, queue_id, usrdata) IOS_IoctlAsync( \
    fd, ISFS_IOCTL_GETFILESTATS, NULL, 0, (void*) stats, sizeof(Fstats), \
    queue_id, usrdata)
#elif defined(CHANNEL)
/* TODO: System Menu ISFS */
#endif

#ifdef __cplusplus
    }
#endif

#endif // _ISFS_H