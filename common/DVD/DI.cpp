#include "DI.hpp"

DI* DI::sInstance;

const char* DI::PrintError(DIError error)
{
#define DI_ERROR_STR(val)                                                      \
    case DIError::val:                                                         \
        return #val
    switch (error) {
        DI_ERROR_STR(Unknown);
        DI_ERROR_STR(OK);
        DI_ERROR_STR(Drive);
        DI_ERROR_STR(CoverClosed);
        DI_ERROR_STR(Timeout);
        DI_ERROR_STR(Security);
        DI_ERROR_STR(Verify);
        DI_ERROR_STR(Invalid);
    }
#undef DI_ERR_STR
    return "Unknown";
}

// DVDLowInquiry; Retrieves information about the drive version.
DI::DIError DI::Inquiry(DriveInfo* info)
{
    DICommand block = {
        .cmd = DIIoctl::Inquiry,
        .args = {0},
    };
    DriveInfo drvInfo ATTRIBUTE_ALIGN(32);

    DIError res =
        CallIoctl(block, DIIoctl::Inquiry, reinterpret_cast<void*>(&drvInfo),
                  sizeof(DriveInfo));
    if (res == DIError::OK)
        *info = drvInfo;
    return res;
}

// DVDLowReadDiskID; Reads the current disc ID and initializes the drive.
DI::DIError DI::ReadDiskID(DiskID* diskid)
{
    DICommand block = {
        .cmd = DIIoctl::ReadDiskID,
        .args = {0},
    };
    DiskID drvDiskid ATTRIBUTE_ALIGN(32);

    DIError res =
        CallIoctl(block, DIIoctl::ReadDiskID,
                  reinterpret_cast<void*>(&drvDiskid), sizeof(DiskID));
    if (res == DIError::OK)
        *diskid = drvDiskid;
    return res;
}

// DVDLowRead; Reads and decrypts disc data. This command can only be used
// if hashing and encryption are enabled for the disc. DVDLowOpenPartition
// needs to have been called before for the keys to be read.
DI::DIError DI::Read(void* data, u32 lenBytes, u32 wordOffset)
{
    DICommand block = {
        .cmd = DIIoctl::Read,
        .args = {lenBytes, wordOffset},
    };
    return CallIoctl(block, DIIoctl::Read, data, lenBytes);
}

// DVDLowWaitForCoverClose; Waits for a disc to be inserted; if there is
// already a disc inserted, it must be removed first. This command does not
// time out; if no disc is inserted, it will wait forever.
// Returns DIError::CoverClosed on success.
DI::DIError DI::WaitForCoverClose()
{
    DICommand block = {
        .cmd = DIIoctl::WaitForCoverClose,
        .args = {0},
    };
    return CallIoctl(block, DIIoctl::WaitForCoverClose);
}

// DVDLowGetLength; Returns the length of the last transfer.
DI::DIError DI::GetLength(u32* length)
{
    DICommand block = {
        .cmd = DIIoctl::GetLength,
        .args = {0},
    };
    u32 drvLength;
    DIError res = CallIoctl(block, DIIoctl::GetLength,
                            reinterpret_cast<void*>(&drvLength), sizeof(u32));
    *length = drvLength;
    return res;
}

// DVDLowReset; Resets the drive. spinup(true) is used to spinup the drive
// on start or once a disc is inserted. spinup(false) is used to turn it off
// when launching a new title.
DI::DIError DI::Reset(bool spinup)
{
    DICommand block = {
        .cmd = DIIoctl::Reset,
        .args = {static_cast<u32>(spinup)},
    };
    return CallIoctl(block, DIIoctl::Reset);
}

// DVDLowOpenPartition; Opens a partition, including verifying it through
// ES. ReadDiskID needs to have been called beforehand.
// PARAMETERS:
// tmd - output (required, must be 32 byte aligned)
// ticket - input (optional, must be 32 byte aligned)
// certs - input (optional, must be 32 byte aligned)
DI::DIError DI::OpenPartition(u32 wordOffset, ES::TMDFixed<512>* tmd,
                              ES::ESError* esError, const ES::Ticket* ticket,
                              const void* certs, u32 certsLen)
{
    if (!aligned(tmd, 32) || !aligned(ticket, 32) || !aligned(certs, 32))
        return DIError::Invalid;
    DICommand block = {
        .cmd = DIIoctl::OpenPartition,
        .args = {wordOffset},
    };
    u32 output[8] ATTRIBUTE_ALIGN(32);

    IOS::IOVector<3, 2> vec;
    // input - Command block
    vec.in[0].data = &block;
    vec.in[0].len = sizeof(DICommand);
    // input - Ticket (optional)
    vec.in[1].data = ticket;
    vec.in[1].len = ticket ? sizeof(ES::Ticket) : 0;
    // input - Shared certs (optional)
    vec.in[2].data = certs;
    vec.in[2].len = certs ? certsLen : 0;
    // output - TMD
    vec.out[0].data = tmd;
    vec.out[0].len = sizeof(ES::TMDFixed<512>);
    // output - ES Error
    vec.out[1].data = output;
    vec.out[1].len = sizeof(output);

    DIError res = static_cast<DIError>(di.ioctlv(DIIoctl::OpenPartition, vec));
    if (esError != nullptr)
        *esError = static_cast<ES::ESError>(output[0]);

    return res;
}

// DVDLowClosePartition; Closes the currently-open partition, removing
// information about its keys and such.
DI::DIError DI::ClosePartition()
{
    DICommand block = {
        .cmd = DIIoctl::ClosePartition,
        .args = {0},
    };
    return CallIoctl(block, DIIoctl::ClosePartition);
}

// DVDLowUnencryptedRead; Reads raw data from the disc. Only usable in the
// "System Area" of the disc.
DI::DIError DI::UnencryptedRead(void* data, u32 lenBytes, u32 wordOffset)
{
    DICommand block = {.cmd = DIIoctl::UnencryptedRead,
                       .args = {lenBytes, wordOffset}};
    return CallIoctl(block, DIIoctl::UnencryptedRead, data, lenBytes);
}

// DVDLowOpenPartitionWithTmdAndTicket; Opens a partition, including
// verifying it through ES. ReadDiskID needs to have been called beforehand.
// This function takes an already-read TMD and can take an already-read
// ticket, which means it can be faster since the ticket does not need to be
// read from the disc.
// PARAMETERS:
// tmd - input (required, must be 32 byte aligned)
// ticket - input (optional, must be 32 byte aligned)
// certs - input (optional, must be 32 byte aligned)
DI::DIError DI::OpenPartitionWithTmdAndTicket(u32 wordOffset, ES::TMD* tmd,
                                              ES::ESError* esError,
                                              const ES::Ticket* ticket,
                                              const void* certs, u32 certsLen)
{
    if (!aligned(tmd, 32) || !aligned(ticket, 32) || !aligned(certs, 32))
        return DIError::Invalid;
    DICommand block = {
        .cmd = DIIoctl::OpenPartitionWithTmdAndTicket,
        .args = {wordOffset},
    };
    u32 output[8] ATTRIBUTE_ALIGN(32);

    IOS::IOVector<4, 1> vec;
    // input - Command block
    vec.in[0].data = &block;
    vec.in[0].len = sizeof(DICommand);
    // input - Ticket (optional)
    vec.in[1].data = ticket;
    vec.in[1].len = ticket ? sizeof(ES::Ticket) : 0;
    // input - TMD
    vec.in[2].data = tmd;
    vec.in[2].len = tmd->size();
    // input - Shared certs (optional)
    vec.in[3].data = certs;
    vec.in[3].len = certs ? certsLen : 0;
    // output - ES Error
    vec.out[0].data = output;
    vec.out[0].len = sizeof(output);

    DIError res = static_cast<DIError>(
        di.ioctlv(DIIoctl::OpenPartitionWithTmdAndTicket, vec));
    if (esError != nullptr)
        *esError = static_cast<ES::ESError>(output[0]);

    return res;
}

// DVDLowOpenPartitionWithTmdAndTicketView; Opens a partition, including
// verifying it through ES. ReadDiskID needs to have been called beforehand.
// This function takes an already-read TMD and can take an already-read
// ticket view, which means it can be faster since the ticket does not need
// to be read from the disc.
// PARAMETERS:
// tmd - input (required, must be 32 byte aligned)
// ticketView - input (optional, must be 32 byte aligned)
// certs - input (optional, must be 32 byte aligned)
DI::DIError DI::OpenPartitionWithTmdAndTicketView(
    u32 wordOffset, ES::TMD* tmd, ES::ESError* esError,
    const ES::TicketView* ticketView, const void* certs, u32 certsLen)
{
    if (!aligned(tmd, 32) || !aligned(ticketView, 32) || !aligned(certs, 32))
        return DIError::Invalid;
    DICommand block = {
        .cmd = DIIoctl::OpenPartitionWithTmdAndTicketView,
        .args = {wordOffset},
    };
    u32 output[8] ATTRIBUTE_ALIGN(32);

    IOS::IOVector<4, 1> vec;
    // input - Command block
    vec.in[0].data = &block;
    vec.in[0].len = sizeof(DICommand);
    // input - Ticket View (optional)
    vec.in[1].data = ticketView;
    vec.in[1].len = ticketView ? sizeof(ES::TicketView) : 0;
    // input - TMD
    vec.in[2].data = tmd;
    vec.in[2].len = tmd->size();
    // input - Shared certs (optional)
    vec.in[3].data = certs;
    vec.in[3].len = certs ? certsLen : 0;
    // output - ES Error
    vec.out[0].data = output;
    vec.out[0].len = sizeof(output);

    DIError res = static_cast<DIError>(
        di.ioctlv(DIIoctl::OpenPartitionWithTmdAndTicketView, vec));
    if (esError != nullptr)
        *esError = static_cast<ES::ESError>(output[0]);

    return res;
}

// DVDLowSeek; Seeks to the sector containing a specific position on the
// disc.
DI::DIError DI::Seek(u32 wordOffset)
{
    DICommand block = {
        .cmd = DIIoctl::Seek,
        .args = {wordOffset},
    };
    return CallIoctl(block, DIIoctl::Seek);
}

// DVDLowReadDiskBca; Reads the last 64 bytes of the burst cutting area.
DI::DIError DI::ReadDiskBca(u8* out)
{
    if (!aligned(out, 32))
        return DIError::Invalid;
    DICommand block = {
        .cmd = DIIoctl::ReadDiskBca,
        .args = {0},
    };
    return CallIoctl(block, DIIoctl::ReadDiskBca, out, 64);
}

DI::DIError DI::CallIoctl(DICommand& block, DIIoctl cmd, void* out, u32 outLen)
{
    return static_cast<DIError>(di.ioctl(cmd, reinterpret_cast<void*>(&block),
                                         sizeof(DICommand), out, outLen));
}