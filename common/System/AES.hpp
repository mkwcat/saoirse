#pragma once
#include <System/OS.hpp>
#include <System/Types.h>
#include <System/Util.h>

class AES
{
public:
    static AES* sInstance;

private:
    enum class AESIoctl
    {
        Encrypt = 2,
        Decrypt = 3,
    };

public:
    /*
     * AES-128 CBC encrypt a block using the AES hardware engine.
     */
    s32 Encrypt(const u8* key, u8* iv, const void* input, u32 size,
                void* output)
    {
        IOS::IOVector<2, 2> vec;
        vec.in[0].data = input;
        vec.in[0].len = size;
        vec.in[1].data = key;
        vec.in[1].len = 16;
        vec.out[0].data = output;
        vec.out[0].len = size;
        vec.out[1].data = iv;
        vec.out[1].len = 16;
        return m_rm.ioctlv(AESIoctl::Encrypt, vec);
    }

    /*
     * AES-128 CBC decrypt a block using the AES hardware engine.
     */
    s32 Decrypt(const u8* key, u8* iv, const void* input, u32 size,
                void* output)
    {
        IOS::IOVector<2, 2> vec;
        vec.in[0].data = input;
        vec.in[0].len = size;
        vec.in[1].data = key;
        vec.in[1].len = 16;
        vec.out[0].data = output;
        vec.out[0].len = size;
        vec.out[1].data = iv;
        vec.out[1].len = 16;
        return m_rm.ioctlv(AESIoctl::Decrypt, vec);
    }

private:
    IOS::ResourceCtrl<AESIoctl> m_rm{"/dev/aes"};
};