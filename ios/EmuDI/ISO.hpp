// ISO.hpp - ISO virtual disc
//   Written by Palapeli
//
// SPDX-License-Identifier: MIT

#pragma once

#include "VirtualDisc.hpp"
#include <Disk/DeviceMgr.hpp>
#include <FAT/ff.h>
#include <System/Types.h>

class ISO : public VirtualDisc
{
public:
    ISO(const char* path, const char* path2);
    virtual ~ISO();
    virtual bool IsInserted() override;

protected:
    static constexpr u32 DiskID_OFFSET = 0;

    static constexpr u32 BlockSize = 0x8000;
    static constexpr u32 BlockHeaderSize = 0x400;
    static constexpr u32 BlockDataSize = 0x7C00;

    virtual bool ReadRaw(void* buffer, u32 wordOffset, u32 byteLen);

    template <class T>
    bool ReadRawStruct(T* data, u32 wordOffset)
    {
        return ReadRaw(reinterpret_cast<void*>(data), wordOffset, sizeof(T));
    }

    bool ReadAndDecryptBlock(u32 wordOffset);

private:
    FIL m_isoFile;

    // If ISO is split into multiple parts.
    u32 m_numParts;
    u64 m_partSize;
    u64 m_lastPartSize;

    FIL m_isoFile2;

    // FatFS fast seek feature
    DWORD m_isoClmt[0x1000] = {0};

protected:
    u32 m_devId = 0;

    DI::DiskID m_diskID;
    bool m_readDiskIDCalled = false;
    DI::Partition m_partition;
    u32 m_partitionOffset;
    bool m_partitionOpened = false;

    bool m_isEncrypted = true;
    u8 m_titleKey[16] ATTRIBUTE_ALIGN(4);
    u8 m_dataBlock[BlockSize] ATTRIBUTE_ALIGN(32);
    u8 m_dataBlockDecrypted[BlockDataSize] ATTRIBUTE_ALIGN(32);
    // Offset of the block in m_dataBlockDecrypted. 1 is an invalid block offset
    // so this won't be initially used.
    u32 m_lastReadBlock = 1;

public:
    bool UnencryptedRead(void* out, u32 wordOffset, u32 byteLen) override;
    bool ReadFromPartition(void* out, u32 wordOffset, u32 byteLen) override;
    bool ReadDiskID(DI::DiskID* out) override;
    DI::DIError ReadTMD(ES::TMDFixed<512>* out) override;
    DI::DIError OpenPartition(
      u32 wordOffset, ES::TMDFixed<512>* tmdOut) override;
};
