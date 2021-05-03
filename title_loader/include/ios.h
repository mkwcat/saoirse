#ifndef _IOS_H
#define _IOS_H

#include <ios/cache.h>
#include <ios/heap.h>
#include <ios/ipc.h>
#include <ios/msg.h>
#include <ios/thread.h>
#include <ios/haxx.h>

#define IOS_SUCCESS       0
#define IOS_EACCES        -1
#define IOS_EEXIST        -2
#define IOS_EINVAL        -4
#define IOS_EMAX          -5
#define IOS_ENOENT        -6
#define IOS_EQUEUEFULL    -8
#define IOS_EIO           -12
#define IOS_ENOMEM        -22

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

#endif // _IOS_H