// OS.hpp - libogc-IOS compatible types and functions
//   Written by Palapeli
//
// SPDX-License-Identifier: MIT

#pragma once
#include <System/Types.h>
#include <System/Util.h>
#ifdef TARGET_IOS
#  include <IOS/Syscalls.h>
#  include <IOS/System.hpp>
#else
extern "C" {
s32 IOS_Open(const char* path, u32 mode);
s32 IOS_Close(s32 fd);
s32 IOS_Read(s32 fd, void* data, u32 len);
s32 IOS_Write(s32 fd, const void* data, u32 len);
s32 IOS_Ioctl(s32 fd, u32 ioctl, void* in, u32 inLen, void* out, u32 outLen);
s32 IOS_Ioctlv(s32 fd, u32 ioctl, u32 inCount, u32 outCount, void* vec);
s32 IOS_Seek(s32 fd, u32 where, u32 whence);
}
#  include <Import/RVL_OS.h>
#  include <cassert>
#endif
#include <new>

#define DASSERT assert
#define ASSERT assert

#ifdef TARGET_IOS
#  define MEM1_BASE ((void*) 0x00000000)
#else
#  define MEM1_BASE ((void*) 0x80000000)
#endif

namespace IOSError
{
enum {
    OK = 0,
    NoAccess = -1,
    Invalid = -4,
    NotFound = -6
};
} // namespace IOSError

namespace ISFSError
{
enum {
    OK = 0,
    Invalid = -101,
    NoAccess = -102,
    Corrupt = -103,
    NotReady = -104,
    Exists = -105,
    NotFound = -106,
    MaxOpen = -109,
    MaxDepth = -110,
    Locked = -111,
    Unknown = -117
};
} // namespace ISFSError

#ifdef TARGET_IOS
/* IOS implementation */
template <typename T>
class IOS_Queue
{
    static_assert(sizeof(T) == 4, "T must be equal to 4 bytes");

public:
    IOS_Queue(const IOS_Queue& from) = delete;

    explicit IOS_Queue(u32 count = 8)
    {
        this->m_base = new u32[count];
        const s32 ret = IOS_CreateMessageQueue(this->m_base, count);
        this->m_queue = ret;
        ASSERT(ret >= 0);
    }

    ~IOS_Queue()
    {
        const s32 ret = IOS_DestroyMessageQueue(this->m_queue);
        ASSERT(ret == IOSError::OK);
        delete[] this->m_base;
    }

    void send(T msg, u32 flags = 0)
    {
        const s32 ret = IOS_SendMessage(this->m_queue, (u32) (msg), flags);
        ASSERT(ret == IOSError::OK);
    }

    T receive(u32 flags = 0)
    {
        T msg;
        const s32 ret = IOS_ReceiveMessage(this->m_queue, (u32*) (&msg), flags);
        ASSERT(ret == IOSError::OK);
        return reinterpret_cast<T>(msg);
    }

    s32 id() const
    {
        return this->m_queue;
    }

private:
    u32* m_base;
    s32 m_queue;
};

template <typename T>
using Queue = IOS_Queue<T>;

#else

/* libogc implementation */

#endif

#ifdef TARGET_IOS

// TODO: Create recursive IOS mutex
class IOS_Mutex
{
public:
    IOS_Mutex(const IOS_Mutex& from) = delete;

    IOS_Mutex()
      : m_queue(1)
    {
        m_queue.send(0);
    }

    void lock()
    {
        m_queue.receive();
    }

    void unlock()
    {
        m_queue.send(0);
    }

private:
    Queue<u32> m_queue;
};

using Mutex = IOS_Mutex;

#else

class RVL_Mutex
{
public:
    RVL_Mutex(const RVL_Mutex& from) = delete;

    RVL_Mutex()
    {
        OSInitMutex(&m_mutex);
    }

    void lock()
    {
        OSLockMutex(&m_mutex);
    }

    void unlock()
    {
        OSUnlockMutex(&m_mutex);
    }

private:
    OSMutex m_mutex;
};

using Mutex = RVL_Mutex;

#endif

#ifdef TARGET_IOS

class IOS_Thread
{
public:
    typedef s32 (*Proc)(void* arg);

    IOS_Thread(const IOS_Thread& rhs) = delete;

    IOS_Thread()
    {
        m_arg = nullptr;
        f_proc = nullptr;
        m_valid = false;
        m_tid = -1;
        m_ownedStack = nullptr;
    }

    IOS_Thread(s32 thread)
    {
        m_arg = nullptr;
        f_proc = nullptr;
        m_tid = thread;
        if (m_tid >= 0)
            m_valid = true;
        m_ownedStack = nullptr;
    }

    IOS_Thread(Proc proc, void* arg, u8* stack, u32 stackSize, s32 prio)
    {
        m_arg = nullptr;
        f_proc = nullptr;
        m_valid = false;
        m_tid = -1;
        m_ownedStack = nullptr;
        create(proc, arg, stack, stackSize, prio);
    }

    ~IOS_Thread()
    {
        if (m_ownedStack != nullptr)
            delete m_ownedStack;
    }

    void create(Proc proc, void* arg, u8* stack, u32 stackSize, s32 prio)
    {
        f_proc = proc;
        m_arg = arg;

        if (stack == nullptr) {
            stack = new ((std::align_val_t) 32) u8[stackSize];
            m_ownedStack = stack;
        }
        u32* stackTop = reinterpret_cast<u32*>(stack + stackSize);

        m_ret = IOS_CreateThread(__threadProc, reinterpret_cast<void*>(this),
          stackTop, stackSize, prio, true);
        if (m_ret < 0)
            return;

        m_tid = m_ret;
        m_ret = IOS_StartThread(m_tid);
        if (m_ret < 0)
            return;

        m_valid = true;
    }

    static s32 __threadProc(void* arg)
    {
        IOS_Thread* thr = reinterpret_cast<IOS_Thread*>(arg);
        if (thr->f_proc != nullptr)
            thr->f_proc(thr->m_arg);
        return 0;
    }

    s32 id() const
    {
        return this->m_tid;
    }

    s32 getError() const
    {
        return this->m_ret;
    }

protected:
    void* m_arg;
    Proc f_proc;
    bool m_valid;
    s32 m_tid;
    s32 m_ret;
    u8* m_ownedStack;
};

using Thread = IOS_Thread;

#else

#endif

namespace IOS
{

typedef s32 (*IPCCallback)(s32 result, void* userdata);

/**
 * Allocate memory for IPC. Always 32-bit aligned.
 */
static inline void* Alloc(u32 size);

/**
 * Free memory allocated using IOS::Alloc.
 */
static inline void Free(void* ptr);

#ifdef TARGET_IOS

constexpr s32 ipcHeap = 0;

static inline void* Alloc(u32 size)
{
    void* ptr = IOS_AllocAligned(ipcHeap, round_up(size, 32), 32);
    ASSERT(ptr);
    return ptr;
}

static inline void Free(void* ptr)
{
    s32 ret = IOS_Free(ipcHeap, ptr);
    ASSERT(ret == IOSError::OK);
}

#  define IPC_TO_QUEUE(queue, req)                                             \
    (queue)->id(), reinterpret_cast<IOSRequest*>((req))

#else

#endif

enum class Command : u32 {
    Open = 1,
    Close = 2,
    Read = 3,
    Write = 4,
    Seek = 5,
    Ioctl = 6,
    Ioctlv = 7,
    Reply = 8
};

namespace Mode
{
constexpr s32 None = 0;
constexpr s32 Read = 1;
constexpr s32 Write = 2;
constexpr s32 RW = Read | Write;
} // namespace Mode

#ifdef TARGET_IOS
typedef IOVector Vector;
#else
typedef struct _ioctlv Vector;
#endif

template <u32 in_count, u32 out_count>
struct IOVector {
    struct {
        const void* data;
        u32 len;
    } in[in_count];

    struct {
        void* data;
        u32 len;
    } out[out_count];
};

template <u32 in_count>
struct IVector {
    struct {
        const void* data;
        u32 len;
    } in[in_count];
};

template <u32 out_count>
struct OVector {
    struct {
        void* data;
        u32 len;
    } out[out_count];
};

struct Request {
    Command cmd;
    s32 result;

    union {
        s32 fd;
        u32 req_cmd;
    };

    union {
        struct {
            char* path;
            u32 mode;
            u32 uid;
            u16 gid;
        } open;

        struct {
            u8* data;
            u32 len;
        } read, write;

        struct {
            s32 where;
            s32 whence;
        } seek;

        struct {
            u32 cmd;
            u8* in;
            u32 in_len;
            u8* io;
            u32 io_len;
        } ioctl;

        struct {
            u32 cmd;
            u32 in_count;
            u32 io_count;
            Vector* vec;
        } ioctlv;

        u32 args[5];
    };

#ifdef TARGET_IOS
    s32 reply(s32 ret)
    {
        return IOS_ResourceReply(reinterpret_cast<IOSRequest*>(this), ret);
    }

    IPCCallback cb;
    void* userdata;
#endif
};

class Resource
{
public:
#ifndef TARGET_IOS
#else
    // IOS Queue to Callback

    static IOSRequest* makeToCallbackReq(
      Request* req, IPCCallback cb, void* userdata)
    {
        req->cb = cb;
        req->userdata = userdata;
        return reinterpret_cast<IOSRequest*>(req);
    }

    static s32 ipcToCallbackThread(void* arg);
    static void MakeIPCToCallbackThread();

    static s32 s_toCbQueue;

#  define IPC_TO_QUEUE(queue, req)                                             \
    (queue)->id(), reinterpret_cast<IOSRequest*>((req))

#  define IPC_TO_CALLBACK_INIT() IOS::Request* req = new IOS::Request

#  define IPC_TO_CALLBACK(cb, userdata)                                        \
    IOS::Resource::s_toCbQueue, makeToCallbackReq(req, cb, userdata)

#  define IPC_TO_CB_CHECK_DELETE(ret)                                          \
    if ((ret) < 0)                                                             \
    delete req

#endif

    Resource()
    {
        this->m_fd = -1;
    }

    Resource(s32 fd)
    {
        this->m_fd = fd;
    }

    explicit Resource(const char* path, u32 mode = 0)
    {
        this->m_fd = IOS_Open(path, mode);
    }

    Resource(const Resource& from) = delete;

    Resource(Resource&& from)
    {
        this->m_fd = from.m_fd;
        from.m_fd = -1;
    }

    ~Resource()
    {
        if (this->m_fd >= 0)
            close();
    }

    s32 close()
    {
        const s32 ret = IOS_Close(this->m_fd);
        if (ret >= 0)
            this->m_fd = -1;
        return ret;
    }

    s32 read(void* data, u32 length)
    {
        return IOS_Read(this->m_fd, data, length);
    }

    s32 write(const void* data, u32 length)
    {
        return IOS_Write(this->m_fd, data, length);
    }

    s32 seek(s32 where, s32 whence)
    {
        return IOS_Seek(this->m_fd, where, whence);
    }

protected:
    s32 m_fd;
};

template <typename Ioctl>
class ResourceCtrl : public Resource
{
public:
    using Resource::Resource;

    s32 ioctl(Ioctl cmd, void* input, u32 inputLen, void* output, u32 outputLen)
    {
        return IOS_Ioctl(this->m_fd, static_cast<u32>(cmd), input, inputLen,
          output, outputLen);
    }

    s32 ioctlv(Ioctl cmd, u32 inputCnt, u32 outputCnt, Vector* vec)
    {
        return IOS_Ioctlv(
          this->m_fd, static_cast<u32>(cmd), inputCnt, outputCnt, vec);
    }

    template <u32 in_count, u32 out_count>
    s32 ioctlv(Ioctl cmd, IOVector<in_count, out_count>& vec)
    {
        return IOS_Ioctlv(this->m_fd, static_cast<u32>(cmd), in_count,
          out_count, reinterpret_cast<Vector*>(&vec));
    }

    template <u32 in_count>
    s32 ioctlv(Ioctl cmd, IVector<in_count>& vec)
    {
        return IOS_Ioctlv(this->m_fd, static_cast<u32>(cmd), in_count, 0,
          reinterpret_cast<Vector*>(&vec));
    }

    template <u32 out_count>
    s32 ioctlv(Ioctl cmd, OVector<out_count>& vec)
    {
        return IOS_Ioctlv(this->m_fd, static_cast<u32>(cmd), 0, out_count,
          reinterpret_cast<Vector*>(&vec));
    }

#ifdef TARGET_IOS
    s32 ioctlAsync(Ioctl cmd, void* input, u32 inputLen, void* output,
      u32 outputLen, IPCCallback cb, void* userdata)
    {
        IPC_TO_CALLBACK_INIT();
        s32 ret = IOS_IoctlAsync(this->m_fd, static_cast<u32>(cmd), input,
          inputLen, output, outputLen, IPC_TO_CALLBACK(cb, userdata));
        IPC_TO_CB_CHECK_DELETE(ret);
        return ret;
    }

    s32 ioctlvAsync(Ioctl cmd, u32 inputCnt, u32 outputCnt, Vector* vec,
      IPCCallback cb, void* userdata)
    {
        IPC_TO_CALLBACK_INIT();
        s32 ret = IOS_IoctlvAsync(this->m_fd, static_cast<u32>(cmd), inputCnt,
          outputCnt, vec, IPC_TO_CALLBACK(cb, userdata));
        IPC_TO_CB_CHECK_DELETE(ret);
        return ret;
    }

    template <u32 in_count, u32 out_count>
    s32 ioctlvAsync(Ioctl cmd, IOVector<in_count, out_count>& vec,
      IPCCallback cb, void* userdata)
    {
        IPC_TO_CALLBACK_INIT();
        s32 ret = IOS_IoctlvAsync(this->m_fd, static_cast<u32>(cmd), in_count,
          out_count, reinterpret_cast<Vector*>(&vec),
          IPC_TO_CALLBACK(cb, userdata));
        IPC_TO_CB_CHECK_DELETE(ret);
        return ret;
    }

    template <u32 in_count>
    s32 ioctlvAsync(
      Ioctl cmd, IVector<in_count>& vec, IPCCallback cb, void* userdata)
    {
        IPC_TO_CALLBACK_INIT();
        s32 ret = IOS_IoctlvAsync(this->m_fd, static_cast<u32>(cmd), in_count,
          0, reinterpret_cast<Vector*>(&vec), IPC_TO_CALLBACK(cb, userdata));
        IPC_TO_CB_CHECK_DELETE(ret);
        return ret;
    }

    template <u32 out_count>
    s32 ioctlvAsync(
      Ioctl cmd, OVector<out_count>& vec, IPCCallback cb, void* userdata)
    {
        IPC_TO_CALLBACK_INIT();
        s32 ret =
          IOS_IoctlvAsync(this->m_fd, static_cast<u32>(cmd), 0, out_count,
            reinterpret_cast<Vector*>(&vec), IPC_TO_CALLBACK(cb, userdata));
        IPC_TO_CB_CHECK_DELETE(ret);
        return ret;
    }

    s32 ioctlAsync(Ioctl cmd, void* input, u32 inputLen, void* output,
      u32 outputLen, Queue<IOS::Request*>* queue, IOS::Request* req)
    {
        return IOS_IoctlAsync(this->m_fd, static_cast<u32>(cmd), input,
          inputLen, output, outputLen, IPC_TO_QUEUE(queue, req));
    }

    s32 ioctlvAsync(Ioctl cmd, u32 inputCnt, u32 outputCnt, Vector* vec,
      Queue<IOS::Request*>* queue, IOS::Request* req)
    {
        return IOS_IoctlvAsync(this->m_fd, static_cast<u32>(cmd), inputCnt,
          outputCnt, vec, IPC_TO_QUEUE(queue, req));
    }

    template <u32 in_count, u32 out_count>
    s32 ioctlvAsync(Ioctl cmd, IOVector<in_count, out_count>& vec,
      Queue<IOS::Request*>* queue, IOS::Request* req)
    {
        return IOS_IoctlvAsync(this->m_fd, static_cast<u32>(cmd), in_count,
          out_count, reinterpret_cast<Vector*>(&vec), IPC_TO_QUEUE(queue, req));
    }

    template <u32 in_count>
    s32 ioctlvAsync(Ioctl cmd, IVector<in_count>& vec,
      Queue<IOS::Request*>* queue, IOS::Request* req)
    {
        return IOS_IoctlvAsync(this->m_fd, static_cast<u32>(cmd), in_count, 0,
          reinterpret_cast<Vector*>(&vec), IPC_TO_QUEUE(queue, req));
    }

    template <u32 out_count>
    s32 ioctlvAsync(Ioctl cmd, OVector<out_count>& vec,
      Queue<IOS::Request*>* queue, IOS::Request* req)
    {
        return IOS_IoctlvAsync(this->m_fd, static_cast<u32>(cmd), 0, out_count,
          reinterpret_cast<Vector*>(&vec), IPC_TO_QUEUE(queue, req));
    }
#endif

    s32 fd() const
    {
        return this->m_fd;
    }
};

enum class FileIoctl {
    GetFileStats = 11
};

class File : public ResourceCtrl<FileIoctl>
{
public:
    struct Stat {
        u32 size;
        u32 pos;
    };

    using ResourceCtrl::ResourceCtrl;

    u32 tell()
    {
        Stat stat;
        const s32 ret = this->stats(&stat);
        ASSERT(ret == IOSError::OK);
        return stat.pos;
    }

    u32 size()
    {
        Stat stat;
        const s32 ret = this->stats(&stat);
        ASSERT(ret == IOSError::OK);
        return stat.size;
    }

    s32 stats(Stat* stat)
    {
        return this->ioctl(FileIoctl::GetFileStats, nullptr, 0,
          reinterpret_cast<void*>(stat), sizeof(Stat));
    }
};

} // namespace IOS
