#pragma once
#include <System/OS.hpp>
#include <System/Types.h>
#include <System/Util.h>

class SHA
{
public:
    static SHA* sInstance;

    struct Context {
        u32 state[5];
        u32 count[2];
    };

private:
    enum class SHAIoctl
    {
        Init = 0,
        Update = 1,
        Final = 2,
    };

    s32 Command(SHAIoctl cmd, Context* ctx, const void* data, u32 len,
                u8* hashOut)
    {
        IOS::IOVector<1, 2> vec;
        vec.in[0].data = data;
        vec.in[0].len = len;
        vec.out[0].data = reinterpret_cast<void*>(ctx);
        vec.out[0].len = sizeof(Context);
        vec.out[1].data = hashOut;
        vec.out[1].len = hashOut ? 0x14 : 0;

        return m_rm.ioctlv(cmd, vec);
    }

public:
    s32 Init(Context* ctx)
    {
        return Command(SHAIoctl::Init, ctx, nullptr, 0, nullptr);
    }

    /*
     * Update hash in the SHA-1 context.
     */
    s32 Update(Context* ctx, const void* data, u32 len)
    {
        return Command(SHAIoctl::Update, ctx, data, len, nullptr);
    }

    /*
     * Finalize the SHA-1 context and get the result hash.
     */
    s32 Final(Context* ctx, u8* hashOut)
    {
        return Command(SHAIoctl::Final, ctx, nullptr, 0, hashOut);
    }

    /*
     * Finalize the SHA-1 context and get the result hash.
     */
    s32 Final(Context* ctx, const void* data, u32 len, u8* hashOut)
    {
        if (!len)
            return Final(ctx, hashOut);

        return Command(SHAIoctl::Final, ctx, data, len, hashOut);
    }

    /*
     * Quick full hash calculate.
     */
    static s32 Calculate(const void* data, u32 len, u8* hashOut)
    {
        ASSERT(sInstance != nullptr);

        Context ctx PPC_ALIGN;

        s32 ret = sInstance->Init(&ctx);
        if (ret != IOSError::OK)
            return ret;

        return sInstance->Final(&ctx, data, len, hashOut);
    }

private:
    IOS::ResourceCtrl<SHAIoctl> m_rm{"/dev/sha"};
};