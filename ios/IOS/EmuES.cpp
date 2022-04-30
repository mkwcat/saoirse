#include "EmuES.hpp"
#include <Debug/Log.hpp>
#include <IOS/IPCLog.hpp>
#include <System/ES.hpp>
#include <System/OS.hpp>
#include <System/Util.h>
#include <cstdio>
#include <cstring>

namespace EmuES
{

bool s_useTitleCtx = false;
static u64 s_titleID;
static ES::TicketView s_ticketView;

ES::ESError DIVerify(u64 titleID, const ES::Ticket* ticket)
{
    s_titleID = titleID;
    if (ticket->info.titleID != titleID)
        return ES::ESError::InvalidTicket;

    s_ticketView.view = 0;
    s_ticketView.info = ticket->info;

    s_useTitleCtx = true;
    return ES::ESError::OK;
}

/*
 * Handles ES ioctlv commands.
 */
static ES::ESError ReqIoctlv(ES::ESIoctl cmd, u32 inCount, u32 outCount,
                             IOS::Vector* vec)
{
    if (inCount >= 32 || outCount >= 32)
        return ES::ESError::Invalid;

    // NULL any zero length vectors to prevent any accidental writes.
    for (u32 i = 0; i < inCount + outCount; i++) {
        if (vec[i].len == 0)
            vec[i].data = nullptr;
    }

    switch (cmd) {
    case ES::ESIoctl::GetDeviceID: {
        if (inCount != 0 || outCount != 1) {
            PRINT(IOS_EmuES, ERROR, "GetDeviceID: Wrong vector count");
            return ES::ESError::Invalid;
        }

        if (vec[0].len != sizeof(u32) || !aligned(vec[0].data, 4)) {
            PRINT(IOS_EmuES, ERROR,
                  "GetDeviceID: Wrong device ID size or alignment");
            return ES::ESError::Invalid;
        }

        return ES::sInstance->GetDeviceID(reinterpret_cast<u32*>(vec[0].data));
    }

    case ES::ESIoctl::LaunchTitle: {
        if (inCount != 2 || outCount != 0) {
            PRINT(IOS_EmuES, ERROR, "LaunchTitle: Wrong vector count");
            return ES::ESError::Invalid;
        }

        if (vec[0].len != sizeof(u64) || !aligned(vec[0].data, 4)) {
            PRINT(IOS_EmuES, ERROR,
                  "LaunchTitle: Wrong title ID size or alignment");
            return ES::ESError::Invalid;
        }

        if (vec[1].len != sizeof(ES::TicketView) || !aligned(vec[1].data, 4)) {
            PRINT(IOS_EmuES, ERROR,
                  "LaunchTitle: Wrong ticket view size or alignment");
            return ES::ESError::Invalid;
        }

        u64 titleID = *reinterpret_cast<u64*>(vec[0].data);
        ES::TicketView view = *reinterpret_cast<ES::TicketView*>(vec[1].data);

        // Redirect to system menu on attempted IOS reload
        if (u64Hi(titleID) == 1 && u64Lo(titleID) != 2) {
            PRINT(IOS_EmuES, WARN,
                  "LaunchTitle: Attempt to launch IOS title %016llX", titleID);
            u64 titleID = 0x0000000100000002;
            auto ret = ES::sInstance->GetTicketViews(titleID, 1, &view);
            assert(ret == ES::ESError::OK);
        }

        PRINT(IOS_EmuES, INFO, "LaunchTitle: Launching %016llX...", titleID);
        return ES::sInstance->LaunchTitle(titleID, &view);
    }

    case ES::ESIoctl::GetOwnedTitlesCount: {
        if (inCount != 0 || outCount != 1) {
            PRINT(IOS_EmuES, ERROR, "GetOwnedTitlesCount: Wrong vector count");
            return ES::ESError::Invalid;
        }

        if (vec[0].len != sizeof(u32) || !aligned(vec[0].data, 4)) {
            PRINT(IOS_EmuES, ERROR,
                  "GetOwnedTitlesCount: Wrong count vector size or alignment");
            return ES::ESError::Invalid;
        }

        return ES::sInstance->GetOwnedTitlesCount(
            reinterpret_cast<u32*>(vec[0].data));
    }

    case ES::ESIoctl::GetTitlesCount: {
        if (inCount != 0 || outCount != 1) {
            PRINT(IOS_EmuES, ERROR, "GetTitlesCount: Wrong vector count");
            return ES::ESError::Invalid;
        }

        if (vec[0].len != sizeof(u32) || !aligned(vec[0].data, 4)) {
            PRINT(IOS_EmuES, ERROR,
                  "GetTitlesCount: Wrong count vector size or alignment");
            return ES::ESError::Invalid;
        }

        return ES::sInstance->GetTitlesCount(
            reinterpret_cast<u32*>(vec[0].data));
    }

    case ES::ESIoctl::GetTitles: {
        if (inCount != 1 || outCount != 1) {
            PRINT(IOS_EmuES, ERROR, "GetTitles: Wrong vector count");
            return ES::ESError::Invalid;
        }

        if (vec[0].len != sizeof(u32) || !aligned(vec[0].data, 4)) {
            PRINT(IOS_EmuES, ERROR,
                  "GetTitles: Wrong count vector size or alignment");
            return ES::ESError::Invalid;
        }

        // u64 to prevent multiply overflow
        u64 count = *reinterpret_cast<u32*>(vec[0].data);

        if (vec[1].len != count * sizeof(u64) || !aligned(vec[1].data, 4)) {
            PRINT(IOS_EmuES, ERROR,
                  "GetTitles: Wrong title ID vector size or alignment");
            return ES::ESError::Invalid;
        }

        return ES::sInstance->GetTitles((u32)count,
                                        reinterpret_cast<u64*>(vec[1].data));
    }

    case ES::ESIoctl::GetTitleContentsCount: {
        if (inCount != 1 || outCount != 1) {
            PRINT(IOS_EmuES, ERROR,
                  "GetTitleContentsCount: Wrong vector count");
            return ES::ESError::Invalid;
        }

        if (vec[0].len != sizeof(u64) || !aligned(vec[0].data, 4)) {
            PRINT(IOS_EmuES, ERROR,
                  "GetTitleContentsCount: Wrong title ID size or alignment");
            return ES::ESError::Invalid;
        }

        u64 titleID = *reinterpret_cast<u64*>(vec[0].data);

        if (vec[1].len != sizeof(u32) || !aligned(vec[1].data, 4)) {
            PRINT(
                IOS_EmuES, ERROR,
                "GetTitleContentsCount: Wrong count vector size or alignment");
            return ES::ESError::Invalid;
        }

        return ES::sInstance->GetTitleContentsCount(
            titleID, reinterpret_cast<u32*>(vec[1].data));
    }

    case ES::ESIoctl::GetTitleContents: {
        if (inCount != 2 || outCount != 1) {
            PRINT(IOS_EmuES, ERROR, "GetTitleContents: Wrong vector count");
            return ES::ESError::Invalid;
        }

        if (vec[0].len != sizeof(u64) || !aligned(vec[0].data, 4)) {
            PRINT(IOS_EmuES, ERROR,
                  "GetTitleContents: Wrong title ID size or alignment");
            return ES::ESError::Invalid;
        }

        u64 titleID = *reinterpret_cast<u64*>(vec[0].data);

        if (vec[1].len != sizeof(u32) || !aligned(vec[1].data, 4)) {
            PRINT(IOS_EmuES, ERROR,
                  "GetTitleContents: Wrong count vector size or alignment");
            return ES::ESError::Invalid;
        }

        // u64 to prevent multiply overflow
        u64 count = *reinterpret_cast<u32*>(vec[1].data);

        if (vec[2].len != count * sizeof(u32) || !aligned(vec[2].data, 4)) {
            PRINT(IOS_EmuES, ERROR,
                  "GetTitleContents: Wrong content vector size or alignment");
            return ES::ESError::Invalid;
        }

        return ES::sInstance->GetTitleContents(
            titleID, (u32)count, reinterpret_cast<u32*>(vec[1].data));
    }

    case ES::ESIoctl::GetNumTicketViews: {
        if (inCount != 1 || outCount != 1) {
            PRINT(IOS_EmuES, ERROR, "GetNumTicketViews: Wrong vector count");
            return ES::ESError::Invalid;
        }

        if (vec[0].len != sizeof(u64) || !aligned(vec[0].data, 4)) {
            PRINT(IOS_EmuES, ERROR,
                  "GetNumTicketViews: Wrong title ID size or alignment");
            return ES::ESError::Invalid;
        }

        u64 titleID = *reinterpret_cast<u64*>(vec[0].data);

        if (vec[1].len != sizeof(u32) || !aligned(vec[1].data, 4)) {
            PRINT(IOS_EmuES, ERROR,
                  "GetNumTicketViews: Wrong count vector size or alignment");
            return ES::ESError::Invalid;
        }

        return ES::sInstance->GetNumTicketViews(
            titleID, reinterpret_cast<u32*>(vec[1].data));
    }

    case ES::ESIoctl::GetTicketViews: {
        if (inCount != 2 || outCount != 1) {
            PRINT(IOS_EmuES, ERROR, "GetTicketViews: Wrong vector count");
            return ES::ESError::Invalid;
        }

        if (vec[0].len != sizeof(u64) || !aligned(vec[0].data, 4)) {
            PRINT(IOS_EmuES, ERROR,
                  "GetTicketViews: Wrong title ID size or alignment");
            return ES::ESError::Invalid;
        }

        u64 titleID = *reinterpret_cast<u64*>(vec[0].data);

        if (vec[1].len != sizeof(u32) || !aligned(vec[1].data, 4)) {
            PRINT(IOS_EmuES, ERROR,
                  "GetTicketViews: Wrong count vector size or alignment");
            return ES::ESError::Invalid;
        }

        // u64 to prevent multiply overflow
        u64 count = *reinterpret_cast<u32*>(vec[1].data);

        if (vec[2].len != count * sizeof(ES::TicketView) ||
            !aligned(vec[2].data, 4)) {
            PRINT(IOS_EmuES, ERROR,
                  "GetTicketViews: Wrong ticket view vector size or alignment");
            return ES::ESError::Invalid;
        }

        return ES::sInstance->GetTicketViews(
            titleID, count, reinterpret_cast<ES::TicketView*>(vec[2].data));
    }

    case ES::ESIoctl::GetTMDViewSize: {
        if (inCount != 1 || outCount != 1) {
            PRINT(IOS_EmuES, ERROR, "GetTMDViewSize: Wrong vector count");
            return ES::ESError::Invalid;
        }

        if (vec[0].len != sizeof(u64) || !aligned(vec[0].data, 4)) {
            PRINT(IOS_EmuES, ERROR,
                  "GetTMDViewSize: Wrong title ID size or alignment");
            return ES::ESError::Invalid;
        }

        u64 titleID = *reinterpret_cast<u64*>(vec[0].data);

        if (vec[1].len != sizeof(u32) || !aligned(vec[1].data, 4)) {
            PRINT(IOS_EmuES, ERROR,
                  "GetTMDViewSize: Wrong size vector size or alignment");
            return ES::ESError::Invalid;
        }

        return ES::sInstance->GetTMDViewSize(
            titleID, reinterpret_cast<u32*>(vec[1].data));
    }

    case ES::ESIoctl::GetTMDView: {
        if (inCount != 1 || outCount != 1) {
            PRINT(IOS_EmuES, ERROR, "GetTMDView: Wrong vector count");
            return ES::ESError::Invalid;
        }

        if (vec[0].len != sizeof(u64) || !aligned(vec[0].data, 4)) {
            PRINT(IOS_EmuES, ERROR,
                  "GetTMDView: Wrong title ID size or alignment");
            return ES::ESError::Invalid;
        }

        u64 titleID = *reinterpret_cast<u64*>(vec[0].data);

        if (!aligned(vec[1].data, 4)) {
            PRINT(IOS_EmuES, ERROR, "GetTMDView: Wrong tmd view alignment");
            return ES::ESError::Invalid;
        }

        return ES::sInstance->GetTMDView(
            titleID, reinterpret_cast<u32*>(vec[1].data), vec[1].len);
    }

    case ES::ESIoctl::GetDataDir: {
        if (inCount != 1 || outCount != 1) {
            PRINT(IOS_EmuES, ERROR, "GetDataDir: Wrong vector count");
            return ES::ESError::Invalid;
        }

        if (vec[0].len != sizeof(u64) || !aligned(vec[0].data, 4)) {
            PRINT(IOS_EmuES, ERROR,
                  "GetDataDir: Wrong title ID size or alignment");
            return ES::ESError::Invalid;
        }

        u64 titleID = *reinterpret_cast<u64*>(vec[0].data);

        if (vec[1].len != 30) {
            PRINT(IOS_EmuES, ERROR, "GetDataDir: Wrong path length");
            return ES::ESError::Invalid;
        }

        return ES::sInstance->GetDataDir(titleID,
                                         reinterpret_cast<char*>(vec[1].data));
    }

    case ES::ESIoctl::GetDeviceCert: {
        if (inCount != 0 || outCount != 1) {
            PRINT(IOS_EmuES, ERROR, "GetDeviceCert: Wrong vector count");
            return ES::ESError::Invalid;
        }

        if (vec[0].len != 0x180 || !aligned(vec[0].data, 4)) {
            PRINT(IOS_EmuES, ERROR,
                  "GetDeviceCert: Wrong device cert size or alignment");
            return ES::ESError::Invalid;
        }

        return ES::sInstance->GetDeviceCert(vec[0].data);
    }

    case ES::ESIoctl::GetTitleID: {
        if (inCount != 0 || outCount != 1) {
            PRINT(IOS_EmuES, ERROR, "GetTitleID: Wrong vector count");
            return ES::ESError::Invalid;
        }

        if (vec[0].len != sizeof(u64) || !aligned(vec[0].data, 4)) {
            PRINT(IOS_EmuES, ERROR,
                  "GetTitleID: Wrong title ID size or alignment");
            return ES::ESError::Invalid;
        }

        if (s_useTitleCtx) {
            *reinterpret_cast<u64*>(vec[0].data) = s_titleID;
            return ES::ESError::OK;
        }

        return ES::sInstance->GetTitleID(reinterpret_cast<u64*>(vec[0].data));
    }

    default:
        PRINT(IOS_EmuES, ERROR, "Invalid ioctlv cmd: %d",
              static_cast<s32>(cmd));
        return ES::ESError::Invalid;
    }
}

static s32 IPCRequest(IOS::Request* req)
{
    switch (req->cmd) {
    case IOS::Command::Open:
        if (strcmp(req->open.path, "~dev/es") != 0)
            return IOSError::NotFound;

        return IOSError::OK;

    case IOS::Command::Close:
        return IOSError::OK;

    case IOS::Command::Ioctlv:
        return static_cast<s32>(ReqIoctlv(
            static_cast<ES::ESIoctl>(req->ioctlv.cmd), req->ioctlv.in_count,
            req->ioctlv.io_count, req->ioctlv.vec));

    default:
        PRINT(IOS_EmuES, ERROR, "Invalid cmd: %d", static_cast<s32>(req->cmd));
        return static_cast<s32>(ES::ESError::Invalid);
    }
}

s32 ThreadEntry(void* arg)
{
    PRINT(IOS_EmuES, INFO, "Starting ES...");
    PRINT(IOS_EmuES, INFO, "EmuES thread ID: %d", IOS_GetThreadId());

    Queue<IOS::Request*> queue(8);
    s32 ret = IOS_RegisterResourceManager("~dev/es", queue.id());
    assert(ret == IOSError::OK);

    IPCLog::sInstance->Notify();
    while (true) {
        IOS::Request* req = queue.receive();
        req->reply(IPCRequest(req));
    }
}

} // namespace EmuES