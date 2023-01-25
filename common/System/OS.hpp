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
LIBOGC_SUCKS_BEGIN
#  include <ogc/ipc.h>
#  include <ogc/lwp.h>
#  include <ogc/message.h>
#  include <ogc/mutex.h>
LIBOGC_SUCKS_END
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
template <typename T>
class LibOGC_Queue
{
    static_assert(sizeof(T) == 4, "T must be equal to 4 bytes");

public:
    LibOGC_Queue(const LibOGC_Queue& from) = delete;

    explicit LibOGC_Queue(u32 count = 8)
    {
        if (count != 0) {
            const s32 ret = MQ_Init(&this->m_queue, count);
            ASSERT(ret == 0);
        }
    }

    ~LibOGC_Queue()
    {
        // This should definitely be readded at some point, it's just a hack
        // because this is causing problems
        // MQ_Close(m_queue);
    }

    void send(T msg, u32 flags = 0)
    {
        const BOOL ret = MQ_Send(this->m_queue, (mqmsg_t) (msg), flags);
        ASSERT(ret == TRUE);
    }

    T receive()
    {
        T msg;
        const BOOL ret = MQ_Receive(this->m_queue, (mqmsg_t*) (&msg), 0);
        ASSERT(ret == TRUE);
        return msg;
    }

    bool tryreceive(T& msg)
    {
        const BOOL ret = MQ_Receive(this->m_queue, (mqmsg_t*) (&msg), 1);
        return ret;
    }

    mqbox_t id() const
    {
        return this->m_queue;
    }

private:
    mqbox_t m_queue;
};

template <typename T>
using Queue = LibOGC_Queue<T>;

#endif

class Mutex
{
public:
    Mutex(const Mutex& from) = delete;

    Mutex()
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

protected:
    Queue<u32> m_queue;
};

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

class LibOGC_Thread
{
public:
    typedef s32 (*Proc)(void* arg);

    LibOGC_Thread(const LibOGC_Thread& rhs) = delete;

    LibOGC_Thread()
    {
        m_arg = nullptr;
        f_proc = nullptr;
        m_valid = false;
        m_tid = 0;
    }

    LibOGC_Thread(lwp_t thread)
    {
        m_arg = nullptr;
        f_proc = nullptr;
        m_valid = true;
        m_tid = thread;
    }

    LibOGC_Thread(Proc proc, void* arg, u8* stack, u32 stackSize, s32 prio)
    {
        create(proc, arg, stack, stackSize, prio);
    }

    void create(Proc proc, void* arg, u8* stack, u32 stackSize, s32 prio)
    {
        f_proc = proc;
        m_arg = arg;
        const s32 ret =
          LWP_CreateThread(&m_tid, &__threadProc, reinterpret_cast<void*>(this),
            reinterpret_cast<void*>(stack), stackSize, prio);
        ASSERT(ret == 0);
        m_valid = true;
    }

    static void* __threadProc(void* arg)
    {
        LibOGC_Thread* thr = reinterpret_cast<LibOGC_Thread*>(arg);
        if (thr->f_proc != nullptr)
            thr->f_proc(thr->m_arg);
        return NULL;
    }

protected:
    void* m_arg;
    Proc f_proc;
    bool m_valid;
    lwp_t m_tid;
};

using Thread = LibOGC_Thread;

#endif

namespace IOS
{

typedef s32 (*IPCCallback)(s32 result, void* userdata);

// Allocate memory for IPC. Always 32-bit aligned.
static inline void* Alloc(u32 size);

// Free memory allocated using IOS::Alloc.
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

#else

// Created in OS.cpp
extern s32 ipcHeap;

static inline void* Alloc(u32 size)
{
    void* ptr = iosAlloc(ipcHeap, round_up(size, 32));
    ASSERT(ptr);
    return ptr;
}

static inline void Free(void* ptr)
{
    iosFree(ipcHeap, ptr);
}

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
    union {
        Command cmd;
        Queue<IOS::Request*>* cbQueue;
    };

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
    // PPC Callback to Queue

    static s32 ipcToQueueCb(s32 result, void* userdata)
    {
        if (userdata == nullptr)
            return 0;

        Request* req = reinterpret_cast<Request*>(userdata);
        Queue<Request*>* queue = req->cbQueue;

        req->cmd = Command::Reply;
        req->result = result;
        queue->send(req);
        return 0;
    }

    static void* makeToQueueUserdata(Request* req, Queue<Request*>* queue)
    {
        req->cbQueue = queue;
        return reinterpret_cast<void*>(req);
    }

#  define IPC_TO_QUEUE(queue, req)                                             \
    &IOS::Resource::ipcToQueueCb, makeToQueueUserdata((req), (queue))

#  define IPC_TO_CALLBACK_INIT()

#  define IPC_TO_CALLBACK(cb, userdata) (cb), (userdata)

#  define IPC_TO_CB_CHECK_DELETE(ret)

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

    s32 readAsync(void* data, u32 length, IPCCallback cb, void* userdata)
    {
        IPC_TO_CALLBACK_INIT();
        s32 ret = IOS_ReadAsync(
          this->m_fd, data, length, IPC_TO_CALLBACK(cb, userdata));
        IPC_TO_CB_CHECK_DELETE(ret);
        return ret;
    }

    s32 writeAsync(const void* data, u32 length, IPCCallback cb, void* userdata)
    {
        IPC_TO_CALLBACK_INIT();
        s32 ret = IOS_WriteAsync(
          this->m_fd, data, length, IPC_TO_CALLBACK(cb, userdata));
        IPC_TO_CB_CHECK_DELETE(ret);
        return ret;
    }

    s32 seekAsync(s32 where, s32 whence, IPCCallback cb, void* userdata)
    {
        IPC_TO_CALLBACK_INIT();
        s32 ret = IOS_SeekAsync(
          this->m_fd, where, whence, IPC_TO_CALLBACK(cb, userdata));
        IPC_TO_CB_CHECK_DELETE(ret);
        return ret;
    }

    s32 readAsync(
      void* data, u32 length, Queue<IOS::Request*>* queue, IOS::Request* req)
    {
        return IOS_ReadAsync(
          this->m_fd, data, length, IPC_TO_QUEUE(queue, req));
    }

    s32 writeAsync(const void* data, u32 length, Queue<IOS::Request*>* queue,
      IOS::Request* req)
    {
        return IOS_WriteAsync(
          this->m_fd, data, length, IPC_TO_QUEUE(queue, req));
    }

    s32 seekAsync(
      s32 where, s32 whence, Queue<IOS::Request*>* queue, IOS::Request* req)
    {
        return IOS_SeekAsync(
          this->m_fd, where, whence, IPC_TO_QUEUE(queue, req));
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

    s32 fd() const
    {
        return this->m_fd;
    }
};

/* Only one IOCTL for specific files */
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

    s32 statsAsync(Stat* stat, IPCCallback cb, void* userdata)
    {
        return this->ioctlAsync(FileIoctl::GetFileStats, nullptr, 0,
          reinterpret_cast<void*>(stat), sizeof(Stat), cb, userdata);
    }

    s32 statsAsync(Stat* stat, Queue<IOS::Request*>* queue, IOS::Request* req)
    {
        return this->ioctlAsync(FileIoctl::GetFileStats, nullptr, 0,
          reinterpret_cast<void*>(stat), sizeof(Stat), queue, req);
    }
};

} // namespace IOS
