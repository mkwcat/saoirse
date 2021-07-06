#include "File.hpp"
#include "os.h"
#include <cstring>
#include <new>
#include "irse.h"

using namespace irse;

enum class StorageIOCTL
{
    Open      = 1,
    Close     = 2,
    Read      = 3,
    Write     = 4,
    LSeek     = 5,
    Truncate  = 6,
    Sync      = 7,
};

static IOS::ResourceCtrl<StorageIOCTL> storage(-1);

void file::init()
{
    new (&storage) IOS::ResourceCtrl<StorageIOCTL>("/dev/storage");
    ASSERT(storage.fd() >= 0);
}

FRESULT file::open(const TCHAR* path, u8 mode)
{
    IOS::IOVector<2, 1> vec;

    vec.in[0].data = path;
    vec.in[0].len = std::strlen(path) + 1;

    BYTE _mode = static_cast<BYTE>(mode);
    vec.in[1].data = &_mode;
    vec.in[1].len = sizeof(BYTE);

    vec.out[0].data = &this->m_f;
    vec.out[0].len = sizeof(FIL);

    s32 ret = storage.ioctlv(StorageIOCTL::Open, vec);
    this->m_result = static_cast<FRESULT>(ret);
    return this->m_result;
}

FRESULT file::close()
{
    IOS::IOVector<1, 1> vec;

    vec.in[0].data = &this->m_f;
    vec.in[0].len = sizeof(FIL);
    vec.out[0].data = &this->m_f;
    vec.out[0].len = sizeof(FIL);

    s32 ret = storage.ioctlv(StorageIOCTL::Close, vec);
    this->m_result = static_cast<FRESULT>(ret);
    return this->m_result;
}

FRESULT file::read(void* data, u32 len, u32& read)
{
    IOS::IOVector<1, 3> vec;

    vec.in[0].data = &this->m_f;
    vec.in[0].len = sizeof(FIL);
    vec.out[0].data = &this->m_f;
    vec.out[0].len = sizeof(FIL);

    vec.out[1].data = data;
    vec.out[1].len = len;

    UINT _read;
    vec.out[2].data = &_read;
    vec.out[2].len = sizeof(UINT);

    s32 ret = storage.ioctlv(StorageIOCTL::Read, vec);
    read = _read;
    this->m_result = static_cast<FRESULT>(ret);
    return this->m_result;
}

FRESULT file::write(const void* data, u32 len, u32& wrote)
{
    IOS::IOVector<2, 2> vec;

    vec.in[0].data = &this->m_f;
    vec.in[0].len = sizeof(FIL);
    vec.out[0].data = &this->m_f;
    vec.out[0].len = sizeof(FIL);

    vec.in[1].data = data;
    vec.in[1].len = len;

    UINT _wrote;
    vec.out[1].data = &_wrote;
    vec.out[1].len = sizeof(UINT);

    s32 ret = storage.ioctlv(StorageIOCTL::Write, vec);
    wrote = _wrote;
    this->m_result = static_cast<FRESULT>(ret);
    return this->m_result;
}

FRESULT file::lseek(u32 offset)
{
    IOS::IOVector<2, 1> vec;

    vec.in[0].data = &this->m_f;
    vec.in[0].len = sizeof(FIL);
    vec.out[0].data = &this->m_f;
    vec.out[0].len = sizeof(FIL);

    FSIZE_t _offset = offset;
    vec.in[1].data = &_offset;
    vec.in[1].len = sizeof(FSIZE_t);

    s32 ret = storage.ioctlv(StorageIOCTL::LSeek, vec);
    this->m_result = static_cast<FRESULT>(ret);
    return this->m_result;
}

FRESULT file::truncate()
{
    IOS::IOVector<1, 1> vec;

    vec.in[0].data = &this->m_f;
    vec.in[0].len = sizeof(FIL);
    vec.out[0].data = &this->m_f;
    vec.out[0].len = sizeof(FIL);

    s32 ret = storage.ioctlv(StorageIOCTL::Truncate, vec);
    this->m_result = static_cast<FRESULT>(ret);
    return this->m_result;
}

FRESULT file::sync()
{
    IOS::IOVector<1, 1> vec;

    vec.in[0].data = &this->m_f;
    vec.in[0].len = sizeof(FIL);
    vec.out[0].data = &this->m_f;
    vec.out[0].len = sizeof(FIL);

    s32 ret = storage.ioctlv(StorageIOCTL::Sync, vec);
    this->m_result = static_cast<FRESULT>(ret);
    return this->m_result;
}