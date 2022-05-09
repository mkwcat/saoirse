// ISO.cpp - ISO virtual disc
//   Written by Palapeli
//
// Copyright (C) 2022 Team Saoirse
// SPDX-License-Identifier: MIT

#include "ISO.hpp"
#include <Debug/Log.hpp>
#include <IOS/EmuES.hpp>
#include <IOS/System.hpp>
#include <System/AES.hpp>
#include <algorithm>
#include <cstring>

ISO::ISO(const char* path, const char* path2)
{
    m_numParts = 0;
    m_partSize = 0;
    m_lastPartSize = 0;

    assert(path != nullptr);
    auto fret = f_open(&m_isoFile, path, FA_READ);
    assert(fret == FR_OK);

    m_numParts = 1;
    m_partSize = f_size(&m_isoFile);
    m_lastPartSize = m_partSize;

    if (path2 != nullptr) {
        fret = f_open(&m_isoFile2, path2, FA_READ);
        assert(fret == FR_OK);

        m_numParts = 2;
        m_lastPartSize = f_size(&m_isoFile2);
    }

    // Use FatFS fast seek function to speed up long backwards seeks
    // Distribute cluster map equally across the two parts
    const u32 clmtSize = sizeof(m_isoClmt) / sizeof(DWORD);

    if (m_numParts == 1) {
        m_isoFile.cltbl = m_isoClmt;
        m_isoClmt[0] = clmtSize;

        fret = f_lseek(&m_isoFile, CREATE_LINKMAP);
        assert(fret == FR_OK);
    } else {
        u32 dist = ((u64)m_partSize + (u64)m_lastPartSize) / clmtSize;
        u32 clmt1stSize = m_partSize / dist;

        m_isoFile.cltbl = m_isoClmt;
        m_isoClmt[0] = clmt1stSize;

        m_isoFile2.cltbl = &m_isoClmt[clmt1stSize];
        m_isoClmt[clmt1stSize] = clmtSize - clmt1stSize;

        fret = f_lseek(&m_isoFile, CREATE_LINKMAP);
        assert(fret == FR_OK);

        fret = f_lseek(&m_isoFile2, CREATE_LINKMAP);
        assert(fret == FR_OK);
    }

    PRINT(IOS_EmuDI, INFO, "Successfully opened ISO file");
    PRINT(IOS_EmuDI, INFO, "Part size: %08X", m_partSize);
    PRINT(IOS_EmuDI, INFO, "Num parts: %08X", m_numParts);
    PRINT(IOS_EmuDI, INFO, "Last part size: %08X", m_lastPartSize);
}

ISO::~ISO()
{
}

bool ISO::IsInserted()
{
    return DeviceMgr::sInstance->IsInserted(m_devId);
}

bool ISO::ReadRaw(void* buffer, u32 wordOffset, u32 byteLen)
{
    const u32 lastPart = m_numParts - 1;

    if (byteLen == 0) {
        PRINT(IOS_EmuDI, WARN, "Zero length read");
        return false;
    }

    u64 realOffset = (u64)wordOffset * 4;

    u32 partNum = realOffset / m_partSize;
    u32 partNumEnd = (realOffset + byteLen - 1) / m_partSize;
    u64 partOffset = realOffset % m_partSize;

    if (partNumEnd >= m_numParts) {
        PRINT(IOS_EmuDI, ERROR, "Read off the end of the ISO parts");
        return false;
    }

    if (partNumEnd == lastPart && (partOffset + byteLen) >= m_lastPartSize) {
        PRINT(IOS_EmuDI, ERROR,
              "Read off the end of the last part (%llX >= %llx)", partOffset,
              m_lastPartSize);
        return false;
    }

    while (byteLen > 0) {
        // TODO: Add support for larger discs that require more than two parts.
        FIL* fp = partNum == 0   ? &m_isoFile
                  : partNum == 1 ? &m_isoFile2
                                 : nullptr;

        u32 lengthToRead =
            partNum == partNumEnd ? byteLen : m_partSize - partOffset;

        if (fp == nullptr) {
            PRINT(IOS_EmuDI, ERROR,
                  "Part counts greater than 1 currently aren't supported");
            return false;
        }

        auto fret = f_lseek(fp, partOffset);
        if (fret != FR_OK)
            return false;
        UINT br;
        fret = f_read(fp, buffer, lengthToRead, &br);
        if (fret != FR_OK)
            return false;

        partOffset = 0;
        byteLen -= lengthToRead;
        partNum++;
    }

    return true;
}

bool ISO::UnencryptedRead(void* out, u32 wordOffset, u32 byteLen)
{
    return ReadRaw(out, wordOffset, byteLen);
}

bool ISO::ReadAndDecryptBlock(u32 wordOffset)
{
    if (m_lastReadBlock == wordOffset)
        return true;

    m_lastReadBlock = wordOffset;

    if (!ReadRaw(m_dataBlock, wordOffset, BlockSize)) {
        PRINT(IOS_EmuDI, ERROR, "Failed to read block from disc image");
        return false;
    }

    // Decrypt the block using the unique title key
    s32 ret = AES::sInstance->Decrypt(m_titleKey, &m_dataBlock[0x3D0],
                                      &m_dataBlock[BlockHeaderSize],
                                      BlockDataSize, m_dataBlockDecrypted);
    assert(ret == IOSError::OK);
    return true;
}

bool ISO::ReadFromPartition(void* out, u32 wordOffset, u32 byteLen)
{
    if (!m_partitionOpened) {
        PRINT(IOS_EmuDI, ERROR, "Attempt read with no open partition");
        return false;
    }

    if (!aligned(byteLen, 32)) {
        PRINT(IOS_EmuDI, ERROR, "Read length not 32-byte aligned");
        return false;
    }

    if (byteLen == 0)
        return true;

    u32 dataStart = m_partitionOffset + m_partition.dataWordOffset;
    // TODO use this to check for out of bounds reads
    [[maybe_unused]] u32 dataEnd = dataStart + m_partition.dataWordLength;

    // Find offset of the encrypted block
    u32 blockWordOffset =
        dataStart + wordOffset / (BlockDataSize >> 2) * (BlockSize >> 2);
    u8* writeBuffer = reinterpret_cast<u8*>(out);

    // Decrypt first block separately if it's not aligned to a block boundary
    if (wordOffset % (BlockDataSize >> 2)) {
        if (!ReadAndDecryptBlock(blockWordOffset))
            return false;

        u32 copyOffset = wordOffset % (BlockDataSize >> 2);
        u32 copyLen = std::min(byteLen, BlockDataSize - (copyOffset << 2));
        memcpy(writeBuffer, &m_dataBlockDecrypted[copyOffset << 2], copyLen);

        writeBuffer += copyLen;
        byteLen -= copyLen;
        blockWordOffset += (BlockSize >> 2);
    }

    // Read the next full blocks
    while (byteLen >= BlockDataSize) {
        if (!ReadAndDecryptBlock(blockWordOffset))
            return false;

        memcpy(writeBuffer, m_dataBlockDecrypted, BlockDataSize);

        writeBuffer += BlockDataSize;
        byteLen -= BlockDataSize;
        blockWordOffset += (BlockSize >> 2);
    }

    // Read the last short block
    if (byteLen > 0) {
        if (!ReadAndDecryptBlock(blockWordOffset))
            return false;

        memcpy(writeBuffer, m_dataBlockDecrypted, byteLen);
    }

    return true;
}

bool ISO::ReadDiskID(DI::DiskID* out)
{
    if (!ReadRawStruct(&m_diskID, DiskID_OFFSET))
        return false;

    *out = m_diskID;
    m_readDiskIDCalled = true;
    return true;
}

DI::DIError ISO::ReadTMD(ES::TMDFixed<512>* out)
{
    if (m_partition.tmdByteLength > sizeof(ES::TMDFixed<512>) ||
        m_partition.tmdByteLength < sizeof(ES::TMDFixed<1>)) {
        PRINT(IOS_EmuDI, ERROR, "TMD size is invalid");
        return DI::DIError::Security;
    }

    if (m_partition.tmdWordOffset == 0) {
        PRINT(IOS_EmuDI, ERROR, "TMD offset is invalid");
        return DI::DIError::Security;
    }

    if (!ReadRaw(reinterpret_cast<void*>(out),
                 m_partitionOffset + m_partition.tmdWordOffset,
                 m_partition.tmdByteLength)) {
        PRINT(IOS_EmuDI, ERROR, "Failed to read TMD from disc image");
        return DI::DIError::Drive;
    }

    return DI::DIError::OK;
}

DI::DIError ISO::OpenPartition(u32 wordOffset, ES::TMDFixed<512>* tmdOut)
{
    if (m_partitionOpened) {
        PRINT(IOS_EmuDI, ERROR,
              "Attempt to open a partition when one is already open");
        return DI::DIError::Invalid;
    }

    if (!m_readDiskIDCalled) {
        PRINT(
            IOS_EmuDI, ERROR,
            "ReadDiskID must be called before attempting to open a partition");
        return DI::DIError::Invalid;
    }

    m_partitionOffset = wordOffset;

    if (!ReadRawStruct(&m_partition, wordOffset)) {
        PRINT(IOS_EmuDI, ERROR, "Failed to read partition at offset 0x%08X",
              wordOffset);
        return DI::DIError::Drive;
    }

    // Read TMD from disc image
    auto ret = ReadTMD(tmdOut);
    if (ret != DI::DIError::OK) {
        return ret;
    }

    auto esRet =
        EmuES::DIVerify(m_partition.ticket.info.titleID, &m_partition.ticket);
    if (esRet != ES::ESError::OK) {
        PRINT(IOS_EmuDI, ERROR, "DIVerify failed: %d", esRet);
        return DI::DIError::Verify;
    }

    u8 titleKeyBuffer[32] ATTRIBUTE_ALIGN(32);
    memcpy(titleKeyBuffer, m_partition.ticket.titleKey, 16);

    // TODO: Get the keys and decrypt a more 'normal' way
    u8 key[16] ATTRIBUTE_ALIGN(32) = {
        0xeb, 0xe4, 0x2a, 0x22, 0x5e, 0x85, 0x93, 0xe4,
        0x48, 0xd9, 0xc5, 0x45, 0x73, 0x81, 0xaa, 0xf7,
    };

    u8 iv[16] = {0};
    memcpy(iv, &m_partition.ticket.info.titleID, 8);

    auto ret2 = AES::sInstance->Decrypt(key, iv, titleKeyBuffer,
                                        sizeof(titleKeyBuffer), titleKeyBuffer);
    assert(ret2 == IOSError::OK);
    memcpy(m_titleKey, titleKeyBuffer, 16);

    m_partitionOpened = true;
    return DI::DIError::OK;
}