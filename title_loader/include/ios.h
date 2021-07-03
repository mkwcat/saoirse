#ifndef _IOS_H
#define _IOS_H

#include <ios/cache.h>
#include <ios/heap.h>
#include <ios/ipc.h>
#include <ios/msg.h>
#include <ios/thread.h>
#include <ios/time.h>

#define IOS_SUCCESS       0
#define IOS_EACCES        -1
#define IOS_EEXIST        -2
#define IOS_EINVAL        -4
#define IOS_EMAX          -5
#define IOS_ENOENT        -6
#define IOS_EQUEUEFULL    -8
#define IOS_EIO           -12
#define IOS_ENOMEM        -22

#endif // _IOS_H