#ifndef _ISFS_H
#define _ISFS_H

#include <gctypes.h>
#include <ios.h>

#define ISFS_PATH                           "/dev/fs"

#define ISFS_IOCTL_GETFILESTATS             11

typedef struct
{
    u32 length;
    u32 pos;
} Fstats;

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

#endif // _ISFS_H