// DI.hpp - DI types and I/O
//   Written by Palapeli
//
// SPDX-License-Identifier: MIT

#pragma once

#include <System/ES.hpp>
#include <System/OS.hpp>
#include <System/Types.h>
#include <System/Util.h>

// Documentation of the drive used here is referenced from
// - https://wiibrew.org/wiki//dev/di
// - Yet Another GameCube Documentation

class DI
{
public:
    static DI* sInstance;

    enum class DIError : s32 {
        Unknown = 0x0,
        OK = 0x1,
        Drive = 0x2,
        CoverClosed = 0x4,
        Timeout = 0x10,
        Security = 0x20,
        Verify = 0x40,
        Invalid = 0x80,
    };

    static const char* PrintError(DIError error);

    enum class DIIoctl : u8 {
        Inquiry = 0x12,
        ReadDiskID = 0x70,
        Read = 0x71,
        WaitForCoverClose = 0x79,
        GetCoverRegister = 0x7A,
        NotifyReset = 0x7E,
        SetSpinupFlag = 0x7F,
        ReadDvdPhysical = 0x80,
        ReadDvdCopyright = 0x81,
        ReadDvdDiscKey = 0x82,
        GetLength = 0x83,
        GetDIMMBUF = 0x84,
        MaskCoverInterrupt = 0x85,
        ClearCoverInterrupt = 0x86,
        UnmaskStatusInterrupts = 0x87,
        GetCoverStatus = 0x88,
        UnmaskCoverInterrupt = 0x89,
        Reset = 0x8A,
        OpenPartition = 0x8B,
        ClosePartition = 0x8C,
        UnencryptedRead = 0x8D,
        EnableDvdVideo = 0x8E,
        GetNoDiscOpenPartitionParams = 0x90,
        NoDiscOpenPartition = 0x91,
        GetNoDiscBufferSizes = 0x92,
        OpenPartitionWithTmdAndTicket = 0x93,
        OpenPartitionWithTmdAndTicketView = 0x94,
        GetStatusRegister = 0x95,
        GetControlRegister = 0x96,
        ReportKey = 0xA4,
        Seek = 0xAB,
        ReadDvd = 0xD0,
        ReadDvdConfig = 0xD1,
        StopLaser = 0xD2,
        Offset = 0xD9,
        ReadDiskBca = 0xDA,
        RequestDiscStatus = 0xDB,
        RequestRetryNumber = 0xDC,
        SetMaximumRotation = 0xDD,
        SerMeasControl = 0xDF,
        RequestError = 0xE0,
        AudioStream = 0xE1,
        RequestAudioStatus = 0xE2,
        StopMotor = 0xE3,
        AudioBufferConfig = 0xE4,
    };

    struct DriveInfo {
        u16 revisionLevel;
        u16 deviceCode;
        u32 releaseDate;
        u8 version; // ?
        u8 pad[0x17];
    };

    struct DiskID {
        char gameID[4];
        u16 groupID;
        u8 discNum;
        u8 discVer;
        u8 discStreamFlag;
        u8 discStreamSize;
        u8 pad[0xE];
        u32 discMagic;
        u32 discMagicGC;
    };

    struct DICommand {
        DIIoctl cmd;
        // implicit pad
        u32 args[7];
    };

    static_assert(sizeof(DICommand) == 0x20);

    struct Partition {
        ES::Ticket ticket;
        u32 tmdByteLength;
        u32 tmdWordOffset;
        u32 certChainByteLength;
        u32 certChainWordOffset;
        u32 h3TableWordOffset;
        u32 dataWordOffset;
        u32 dataWordLength;
    };

    static_assert(sizeof(DICommand) == 0x20);

    // DVDLowInquiry; Retrieves information about the drive version.
    DIError Inquiry(DriveInfo* info);

    // DVDLowReadDiskID; Reads the current disc ID and initializes the drive.
    DIError ReadDiskID(DiskID* diskid);

    // DVDLowRead; Reads and decrypts disc data. This command can only be used
    // if hashing and encryption are enabled for the disc. DVDLowOpenPartition
    // needs to have been called before for the keys to be read.
    DIError Read(void* data, u32 lenBytes, u32 wordOffset);

    // DVDLowWaitForCoverClose; Waits for a disc to be inserted; if there is
    // already a disc inserted, it must be removed first. This command does not
    // time out; if no disc is inserted, it will wait forever.
    // Returns DIError::CoverClosed on success.
    DIError WaitForCoverClose();

    // DVDLowGetLength; Returns the length of the last transfer.
    DIError GetLength(u32* length);

    // DVDLowReset; Resets the drive. spinup(true) is used to spinup the drive
    // on start or once a disc is inserted. spinup(false) is used to turn it off
    // when launching a new title.
    DIError Reset(bool spinup);

    // DVDLowOpenPartition; Opens a partition, including verifying it through
    // ES. ReadDiskID needs to have been called beforehand.
    // PARAMETERS:
    // tmd - output (required, must be 32 byte aligned)
    // ticket - input (optional, must be 32 byte aligned)
    // certs - input (optional, must be 32 byte aligned)
    DIError OpenPartition(u32 wordOffset, ES::TMDFixed<512>* tmd,
      ES::ESError* esError = nullptr, const ES::Ticket* ticket = nullptr,
      const void* certs = nullptr, u32 certsLen = 0);

    // DVDLowClosePartition; Closes the currently-open partition, removing
    // information about its keys and such.
    DIError ClosePartition();

    // DVDLowUnencryptedRead; Reads raw data from the disc. Only usable in the
    // "System Area" of the disc.
    DIError UnencryptedRead(void* data, u32 lenBytes, u32 wordOffset);

    // DVDLowOpenPartitionWithTmdAndTicket; Opens a partition, including
    // verifying it through ES. ReadDiskID needs to have been called beforehand.
    // This function takes an already-read TMD and can take an already-read
    // ticket, which means it can be faster since the ticket does not need to be
    // read from the disc.
    // PARAMETERS:
    // tmd - input (required, must be 32 byte aligned)
    // ticket - input (optional, must be 32 byte aligned)
    // certs - input (optional, must be 32 byte aligned)
    DIError OpenPartitionWithTmdAndTicket(u32 wordOffset, ES::TMD* tmd,
      ES::ESError* esError = nullptr, const ES::Ticket* ticket = nullptr,
      const void* certs = nullptr, u32 certsLen = 0);

    // DVDLowOpenPartitionWithTmdAndTicketView; Opens a partition, including
    // verifying it through ES. ReadDiskID needs to have been called beforehand.
    // This function takes an already-read TMD and can take an already-read
    // ticket view, which means it can be faster since the ticket does not need
    // to be read from the disc.
    // PARAMETERS:
    // tmd - input (required, must be 32 byte aligned)
    // ticketView - input (optional, must be 32 byte aligned)
    // certs - input (optional, must be 32 byte aligned)
    DIError OpenPartitionWithTmdAndTicketView(u32 wordOffset, ES::TMD* tmd,
      ES::ESError* esError = nullptr,
      const ES::TicketView* ticketView = nullptr, const void* certs = nullptr,
      u32 certsLen = 0);

    // DVDLowSeek; Seeks to the sector containing a specific position on the
    // disc.
    DIError Seek(u32 wordOffset);

    // DVDLowReadDiskBca; Reads the last 64 bytes of the burst cutting area.
    DIError ReadDiskBca(u8* out);

    s32 GetFd() const
    {
        return di.fd();
    }

private:
    DIError CallIoctl(
      DICommand& block, DIIoctl cmd, void* out = nullptr, u32 outLen = 0);

    IOS::ResourceCtrl<DIIoctl> di{"/dev/di"};
};
