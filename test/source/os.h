#pragma once

#include <gccore.h>
#include <ogc/mutex.h>
#include <bit>
#include <cassert>

#define DASSERT assert
#define ASSERT assert

#ifdef T_IOS
#define MEM1_BASE ((void*) 0x00000000)
#else
#define MEM1_BASE ((void*) 0x80000000)
#endif

namespace IOSErr
{

constexpr s32 OK = 0;
constexpr s32 NoAccess = -1;
constexpr s32 Invalid = -4;
constexpr s32 NotFound = -6;

}

#ifdef T_IOS
/* IOS implementation */
template<typename T>
class Queue
{
    static_assert(sizeof(T) == 4, "T must be equal to 4 bytes");
public:
    Queue(const Queue& from) = delete;
    explicit Queue(u32 count = 8) {
        this->m_base = new u32[count];
        const s32 ret = IOS_CreateMessageQueue(this->m_base, count);
        ASSERT(ret >= 0);
    }

    ~Queue() {
        const s32 ret = IOS_DestroyMessageQueue(this->m_queue);
        ASSERT(ret == IOSErr::OK);
        delete[] this->m_base;
    }

    void send(T msg, u32 flags = 0) {
        const s32 ret = IOS_SendMessage(this->m_queue, reinterpret_cast<u32>(msg), flags);
        ASSERT(ret == IOSErr::OK);
    }

    T receive(u32 flags = 0) {
        T msg;
        const s32 ret = IOS_ReceiveMessage(this->m_queue, reinterpret_cast<u32*>(&msg), flags);
        ASSERT(ret == IOSErr::OK);
        return reinterpret_cast<T>(msg);
    }

    s32 id() const {
        return this->m_queue;
    }

private:
    u32* m_base;
    s32 m_queue;
};

#else
/* libogc implementation */
template<typename T>
class Queue
{
    static_assert(sizeof(T) == 4, "T must be equal to 4 bytes");
public:
    
    Queue(const Queue& from) = delete;
    explicit Queue(u32 count = 8) {
        if (count != 0) {
            const s32 ret = MQ_Init(&this->m_queue, count);
            ASSERT(ret == 0);
        }
    }

    ~Queue() {
        MQ_Close(m_queue);
    }

    void send(T msg, u32 flags = 0) {
        const BOOL ret = MQ_Send(this->m_queue, reinterpret_cast<mqmsg_t>(msg), flags);
        ASSERT(ret == TRUE);
    }

    T receive(u32 flags = 0) {
        T msg;
        const BOOL ret = MQ_Receive(this->m_queue, reinterpret_cast<mqmsg_t*>(&msg), flags);
        ASSERT(ret == TRUE);
        return msg;
    }

    mqbox_t id() const {
        return this->m_queue;
    }

private:
    mqbox_t m_queue;
};

#endif

#ifdef T_IOS
typedef s32 mutexid;
#else
typedef mutex_t mutexid;
#endif

class Mutex
{
#ifdef T_IOS
public:
    static_assert(0, "Not implemented!");
#else
public:
    Mutex(const Mutex& from) = delete;
    Mutex() {
        const s32 ret = LWP_MutexInit(&this->m_mutex, 0);
        ASSERT(ret == 0);
    }
    explicit Mutex(mutexid id) : m_mutex(id) { };

    void lock() {
        const s32 ret = LWP_MutexLock(this->m_mutex);
        ASSERT(ret == 0);
    }

    void unlock() {
        const s32 ret = LWP_MutexUnlock(this->m_mutex);
        ASSERT(ret == 0);
    }

    mutexid id() const {
        return m_mutex;
    }

protected:
    mutexid m_mutex;
#endif
};


namespace IOS
{

enum class Command
{
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
}

struct Vector
{
	void *data;
	u32 len;
};

struct Request
{
    Command cmd;
    s32 result;
    union {
        s32 fd;
        u32 req_cmd;
    };
    union
    {
        struct {
            char* path;
            u32 mode;
            s32 fd;
            //! check these next two
            u32 uid;
            u16 gid;
        } open;

        struct {
            void* data;
            u32 len;
        } read, write;

        struct {
            s32 where;
            s32 whence;
        } seek;

        struct {
            u32 cmd;
            void* in;
            u32 in_len;
            void* io;
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
};



#ifdef T_IOS
//! TODO
#define IPC_QUEUE_CALLBACK(callback, req)
#else
#define IPC_QUEUE_CALLBACK(callback, req) callback, reinterpret_cast<void*>(req)
#endif

class Resource
{
public:
    Resource(s32 fd) : m_fd(fd) { }
    explicit Resource(const char* path, u32 mode = 0)
        : m_fd(IOS_Open(path, mode)) { }

    Resource(const Resource& from) = delete;

    ~Resource() {
        if (this->m_fd >= 0)
            IOS_Close(this->m_fd);
    }

    s32 read(void* data, u32 length) {
        return IOS_Read(this->m_fd, data, length);
    }
    s32 write(const void* data, u32 length) {
        return IOS_Write(this->m_fd, data, length);
    }
    s32 seek(s32 where, s32 whence) {
        return IOS_Seek(this->m_fd, where, whence);
    }
    s32 readAsync(void* data, u32 length, ipccallback callback, void* usrdata) {
        return IOS_ReadAsync(this->m_fd, data, length,
            IPC_QUEUE_CALLBACK(callback, usrdata));
    }
    s32 writeAsync(const void* data, u32 length,
                   ipccallback callback, void* usrdata) {
        return IOS_WriteAsync(this->m_fd, data, length,
            IPC_QUEUE_CALLBACK(callback, usrdata));
    }
    s32 seekAsync(s32 where, s32 whence,
                  ipccallback callback, void* usrdata) {
        return IOS_SeekAsync(this->m_fd, where, whence,
            IPC_QUEUE_CALLBACK(callback, usrdata));
    }

protected:
    const s32 m_fd;
};

template<typename Ioctl>
class ResourceCtrl : public Resource
{
public:
    using Resource::Resource;

    s32 ioctl(Ioctl cmd, void* input, u32 inputLen,
              void* output, u32 outputLen) {
        return IOS_Ioctl(
            this->m_fd, static_cast<u32>(cmd),
            input, inputLen, output, outputLen);
    }
    s32 ioctlv(Ioctl cmd, u32 inputCnt, u32 outputCnt, Vector* vec) {
        return IOS_Ioctlv(
            this->m_fd, static_cast<u32>(cmd),
            inputCnt, outputCnt, reinterpret_cast<struct _ioctlv*>(vec));
    }
    
    s32 ioctlAsync(Ioctl cmd, void* input, u32 inputLen,
                   void* output, u32 outputLen,
                   ipccallback callback, void* usrdata) {
        return IOS_IoctlAsync(
            this->m_fd, static_cast<u32>(cmd),
            input, inputLen, output, outputLen,
            IPC_QUEUE_CALLBACK(callback, usrdata));
    }
    s32 ioctlvAsync(Ioctl cmd, u32 inputCnt, u32 outputCnt,
                    Vector* vec,
                    ipccallback callback, void* usrdata) {
        return IOS_IoctlvAsync(
            this->m_fd, static_cast<u32>(cmd),
            inputCnt, outputCnt, reinterpret_cast<struct _ioctlv*>(vec),
            IPC_QUEUE_CALLBACK(callback, usrdata));
    }

    s32 fd() const {
        return this->m_fd;
    }
};


/* Only one IOCTL for specific files */
enum class FileIoctl
{
    GetFileStats = 11
};

class File : public ResourceCtrl<FileIoctl>
{
public:
    struct Stat
    {
        u32 size;
        u32 pos;
    };

    using ResourceCtrl::ResourceCtrl;

    u32 tell() {
        Stat stat;
        const s32 ret = this->stats(&stat);
        ASSERT(ret == IOSErr::OK);
        return stat.pos;
    }

    u32 size() {
        Stat stat;
        const s32 ret = this->stats(&stat);
        ASSERT(ret == IOSErr::OK);
        return stat.size;
    }

    s32 stats(Stat* stat) {
        return this->ioctl(FileIoctl::GetFileStats, nullptr, 0,
            reinterpret_cast<void*>(stat), sizeof(Stat));
    }

    s32 statsAsync(Stat* stat, ipccallback callback, void* usrdata) {
        return this->ioctlAsync(FileIoctl::GetFileStats, nullptr, 0,
            reinterpret_cast<void*>(stat), sizeof(Stat),
            IPC_QUEUE_CALLBACK(callback, usrdata));
    }
};

} // namespace IOS