#pragma once

#include <System/Types.h>

#include <cstring>

namespace IOS
{

void Init();
void SafeFlush(const void* start, size_t size);
bool BootstrapEntry();
void Shutdown();

enum class Mode : u32 {
    None = 0,
    Read = 1 << 0,
    Write = 1 << 1,
    Both = Write | Read,
};

class Resource
{
public:
    struct IoctlvPair {
        void* data;
        u32 size;
    };

    Resource(const Resource&) = delete;
    Resource& operator=(const Resource&) = delete;
    Resource(s32 fd);
    Resource(const char* path, Mode mode);
    ~Resource();
    s32 ioctl(u32 ioctl, const void* input, u32 inputSize, void* output,
      u32 outputSize);
    s32 ioctlv(u32 ioctlv, u32 inputCount, u32 outputCount, IoctlvPair* pairs);
    bool ioctlvReboot(u32 ioctlv, u32 inputCount, IoctlvPair* pairs);
    bool ok() const;

private:
    s32 open(const char* path, Mode mode);
    s32 close();

protected:
    s32 m_fd = -1;
};

class File : protected Resource
{
public:
    struct Ioctl {
        enum {
            CreateDir = 0x3,
            Delete = 0x7,
            Rename = 0x8,
            CreateFile = 0x9,
            GetFileStats = 0xb,
        };
    };

    struct Stats {
        u32 size;
        u32 offset;
    };

    File(s32 fd);
    File(const char* path, Mode mode);
    ~File();
    s32 read(void* output, u32 outputSize);
    s32 write(const void* input, u32 inputSize);
    s32 seek(u32 offset, u32 origin);
    s32 getStats(Stats* stats);
    using Resource::ok;
};

} // namespace IOS
