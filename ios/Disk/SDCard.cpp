// Resources:
// - https://wiibrew.org/wiki//dev/sdio
// -
// https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/Core/IOS/SDIO/SDIOSlot0.cpp
// -
// https://www.sdcard.org/cms/wp-content/themes/sdcard-org/dl.php?f=Part1_Physical_Layer_Simplified_Specification_Ver8.00.pdf
// - https://github.com/devkitPro/libogc/blob/master/libogc/wiisd.c

#include "SDCard.hpp"

#include <Debug/Log.hpp>
#include <IOS/Syscalls.h>
#include <System/OS.hpp>

#include <algorithm>
#include <stdalign.h>
#include <string.h>

enum {
    IOCTL_WRITE_HCR = 0x1,
    IOCTL_READ_HCR = 0x2,
    IOCTL_RESET_CARD = 0x4,
    IOCTL_SET_CLOCK = 0x6,
    IOCTL_SEND_COMMAND = 0x7,
    IOCTL_GET_STATUS = 0xb,
};

enum {
    IOCTLV_SEND_COMMAND = 0x7,
};

enum {
    STATUS_CARD_INSERTED = 1 << 0,
    STATUS_TYPE_MEMORY = 1 << 16,
    STATUS_TYPE_SDHC = 1 << 20,
};

enum {
    HCR_HOST_CONTROL_1 = 0x28,
};

enum {
    HCR_HOST_CONTROL_1_4_BIT = 1 << 1,
};

typedef struct {
    u32 reg;
    u32 _04;
    u32 _08;
    u32 size;
    u32 val;
    u32 _14;
} RegOp;

enum {
    CMD_SELECT = 7,
    CMD_SET_BLOCKLEN = 16,
    CMD_READ_MULTIPLE_BLOCK = 18,
    CMD_WRITE_MULTIPLE_BLOCK = 25,
    CMD_APP_CMD = 55,
};

enum {
    ACMD_SET_BUS_WIDTH = 6,
};

enum {
    RESPONSE_TYPE_R1 = 0x1,
    RESPONSE_TYPE_R1B = 0x2,
};

typedef struct {
    u32 command;
    u32 commandType;
    u32 responseType;
    u32 arg;
    u32 blockCount;
    u32 blockSize;
    void* buffer;
    u32 isDma;
    u32 _20;
} Request;

static_assert(sizeof(Request) == 0x24);

SDCard::SDCard()
{
    m_ok = false;

    if (m_fd < 0) {
        m_fd = IOS_Open("/dev/sdio/slot0", 0);
    }

    if (m_fd < 0) {
        PRINT(IOS_SDCard, ERROR,
          "Failed to open /dev/sdio/slot0: Returned error %i", m_fd);
        return;
    } else {
        PRINT(IOS_SDCard, INFO, "Successfully opened interface: ID: %i", m_fd);
    }

    m_tmpBuffer = IOS::Alloc(TMP_BUFFER_SIZE);
    assert(m_tmpBuffer != nullptr);

    m_ok = true;
    return;
}

SDCard::~SDCard()
{
    if (m_fd > 0) {
        IOS_Close(m_fd);
        m_fd = -1;
    }

    if (m_tmpBuffer != nullptr) {
        IOS::Free(m_tmpBuffer);
        m_tmpBuffer = nullptr;
    }
}

bool SDCard::ResetCard()
{
    alignas(0x20) u32 out;

    if (IOS_Ioctl(m_fd, IOCTL_RESET_CARD, nullptr, 0, &out, sizeof(out)) < 0) {
        PRINT(IOS_SDCard, INFO, "Failed to reset interface");
        return false;
    }

    PRINT(IOS_SDCard, INFO, "Successfully reset interface");
    m_rca = out >> 16;
    return true;
}

bool SDCard::GetStatus(u32* status)
{
    alignas(0x20) u32 out;

    if (IOS_Ioctl(m_fd, IOCTL_GET_STATUS, nullptr, 0, &out, sizeof(out)) < 0) {
        PRINT(IOS_SDCard, INFO, "Failed to get status");
        return false;
    }

    *status = out;
    return true;
}

bool SDCard::ReadHCR(u8 reg, u8 size, u32* val)
{
    alignas(0x20) RegOp regOp = {
      .reg = reg,
      ._04 = 0,
      ._08 = 0,
      .size = size,
      .val = 0,
      ._14 = 0,
    };
    alignas(0x20) u32 out;

    if (IOS_Ioctl(
          m_fd, IOCTL_READ_HCR, &regOp, sizeof(regOp), &out, sizeof(out)) < 0) {
        PRINT(IOS_SDCard, INFO, "Failed to read host controller register 0x%x",
          reg);
        return false;
    }

    *val = out;
    return true;
}

bool SDCard::WriteHCR(u8 reg, u8 size, u32 val)
{
    alignas(0x20) RegOp regOp = {
      .reg = reg,
      ._04 = 0,
      ._08 = 0,
      .size = size,
      .val = val,
      ._14 = 0,
    };

    if (IOS_Ioctl(m_fd, IOCTL_WRITE_HCR, &regOp, sizeof(regOp), nullptr, 0) <
        0) {
        PRINT(IOS_SDCard, INFO,
          "Failed to write to host controller register 0x%x", reg);
        return false;
    }

    return true;
}

bool SDCard::SetClock(u32 clock)
{
    alignas(0x20) u32 in = clock;

    if (IOS_Ioctl(m_fd, IOCTL_SET_CLOCK, &in, sizeof(in), nullptr, 0) < 0) {
        PRINT(IOS_SDCard, INFO, "Failed to set clock");
        return false;
    }

    return true;
}

bool SDCard::SendCommand(u32 command, u32 commandType, u32 responseType,
  u32 arg, u32 blockCount, u32 blockSize, void* buffer, u32* response)
{
    alignas(0x20) Request request = {
      .command = command,
      .commandType = commandType,
      .responseType = responseType,
      .arg = arg,
      .blockCount = blockCount,
      .blockSize = blockSize,
      .buffer = buffer,
      .isDma = !!buffer,
      ._20 = 0,
    };
    alignas(0x20) u32 out[4];

    if (buffer || m_isSdhc) {
        alignas(0x20) IOVector vec[] = {
          {
            .data = &request,
            .len = sizeof(request),
          },
          {
            .data = buffer,
            .len = blockCount * blockSize,
          },
          {
            .data = out,
            .len = sizeof(out),
          },
        };
        if (IOS_Ioctlv(m_fd, IOCTLV_SEND_COMMAND, 2, 1, vec) < 0) {
            if (command != CMD_SELECT) {
                PRINT(IOS_SDCard, INFO, "Failed to send command 0x%x", command);
            }
            return false;
        }
    } else {
        if (IOS_Ioctl(m_fd, IOCTL_SEND_COMMAND, &request, sizeof(request), &out,
              sizeof(out)) < 0) {
            if (command != CMD_SELECT) {
                PRINT(IOS_SDCard, INFO, "Failed to send command 0x%x", command);
            }
            return false;
        }
    }

    if (response) {
        *response = out[0];
    }
    return true;
}

bool SDCard::Enable4BitBus()
{
    u32 val;
    if (!ReadHCR(HCR_HOST_CONTROL_1, sizeof(u8), &val)) {
        return false;
    }

    val |= HCR_HOST_CONTROL_1_4_BIT;

    return WriteHCR(HCR_HOST_CONTROL_1, sizeof(u8), val);
}

bool SDCard::Select()
{
    return SendCommand(
      CMD_SELECT, 3, RESPONSE_TYPE_R1B, m_rca << 16, 0, 0, nullptr, nullptr);
}

bool SDCard::Deselect()
{
    return SendCommand(
      CMD_SELECT, 3, RESPONSE_TYPE_R1B, 0, 0, 0, nullptr, nullptr);
}

bool SDCard::SetCardBlockLength(u32 blockLength)
{
    return SendCommand(CMD_SET_BLOCKLEN, 3, RESPONSE_TYPE_R1, blockLength, 0, 0,
      nullptr, nullptr);
}

bool SDCard::EnableCard4BitBus()
{
    if (!SendCommand(CMD_APP_CMD, 3, RESPONSE_TYPE_R1, m_rca << 16, 0, 0,
          nullptr, nullptr)) {
        return false;
    }

    return SendCommand(
      ACMD_SET_BUS_WIDTH, 3, RESPONSE_TYPE_R1, 0x2, 0, 0, nullptr, nullptr);
}

bool SDCard::TransferAligned(
  bool isWrite, u32 firstSector, u32 sectorCount, void* buffer)
{
    u32 command = isWrite ? CMD_WRITE_MULTIPLE_BLOCK : CMD_READ_MULTIPLE_BLOCK;
    u32 firstBlock = m_isSdhc ? firstSector : firstSector * SECTOR_SIZE;

    if (isWrite) {
        IOS_FlushDCache(buffer, sectorCount * SECTOR_SIZE);
    } else {
        IOS_InvalidateDCache(buffer, sectorCount * SECTOR_SIZE);
    }

    return SendCommand(command, 3, RESPONSE_TYPE_R1, firstBlock, sectorCount,
      SECTOR_SIZE, buffer, nullptr);
}

bool SDCard::Transfer(
  bool isWrite, u32 firstSector, u32 sectorCount, void* buffer)
{
    assert(buffer);

    if (!Select()) {
        return false;
    }

    while (sectorCount > 0) {
        u32 chunkSectorCount = std::min<u32>(sectorCount, TMP_SECTOR_COUNT);
        if (isWrite) {
            memcpy(m_tmpBuffer, buffer, chunkSectorCount * SECTOR_SIZE);
        }
        if (!TransferAligned(
              isWrite, firstSector, chunkSectorCount, m_tmpBuffer)) {
            Deselect();
            return false;
        }
        if (!isWrite) {
            memcpy(buffer, m_tmpBuffer, chunkSectorCount * SECTOR_SIZE);
        }
        firstSector += chunkSectorCount;
        sectorCount -= chunkSectorCount;
        buffer += chunkSectorCount * SECTOR_SIZE;
    }

    Deselect();

    return true;
}

bool SDCard::IsInserted()
{
    u32 status;
    if (!GetStatus(&status)) {
        return false;
    }

    if (!(status & STATUS_CARD_INSERTED)) {
        return false;
    }

    return true;
}

bool SDCard::Init()
{
    if (!ResetCard()) {
        return false;
    }

    u32 status;
    if (!GetStatus(&status)) {
        return false;
    }

    if (!(status & STATUS_CARD_INSERTED)) {
        PRINT(IOS_SDCard, INFO, "No card inserted");
        return false;
    }

    if (!(status & STATUS_TYPE_MEMORY)) {
        PRINT(IOS_SDCard, INFO, "Not a memory card");
        return false;
    }

    m_isSdhc = !!(status & STATUS_TYPE_SDHC);

    if (!Enable4BitBus()) {
        PRINT(IOS_SDCard, INFO, "Failed to enable 4-bit bus");
        return false;
    }

    if (!SetClock(1)) {
        return false;
    }

    if (!Select()) {
        return false;
    }

    if (!SetCardBlockLength(SECTOR_SIZE)) {
        Deselect();
        return false;
    }

    if (!EnableCard4BitBus()) {
        Deselect();
        return false;
    }

    Deselect();
    return true;
}

u32 SDCard::GetSectorSize()
{
    return SECTOR_SIZE;
}

bool SDCard::ReadSectors(u32 firstSector, u32 sectorCount, void* buffer)
{
    return Transfer(false, firstSector, sectorCount, buffer);
}

bool SDCard::WriteSectors(u32 firstSector, u32 sectorCount, const void* buffer)
{
    return Transfer(true, firstSector, sectorCount, (void*) buffer);
}
