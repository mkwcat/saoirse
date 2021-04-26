#ifndef _IOS_H
#define _IOS_H

#include "ios_heap.h"
#include "ios_ipc.h"
#include "ios_msg.h"
#include "ios_thread.h"
#include "ios_haxx.h"

#define IOS_SUCCESS     0
#define IOS_EACCES      -1
#define IOS_EEXIST      -2
#define IOS_EINVAL      -4
#define IOS_EMAX        -5
#define IOS_ENOENT      -6
#define IOS_EQUEUEFULL  -8
#define IOS_EIO         -12
#define IOS_ENOMEM      -22

#define FS_EINVAL       -101
#define FS_EACCESS      -102
#define FS_ECORRUPT     -103
#define FS_EEXIST       -105
#define FS_ENOENT       -106
#define FS_ENFILE       -107
#define FS_EFBIG        -108
#define FS_EFDEXHAUSTED -109
#define FS_ENAMELEN     -110
#define FS_EFDOPEN      -111
#define FS_EIO          -114
#define FS_ENOTEMPTY    -115
#define FS_EDIRDEPTH    -116
#define FS_EBUSY        -118

#endif // _IOS_H