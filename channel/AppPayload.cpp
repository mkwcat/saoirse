#include "AppPayload.hpp"
#include <stdio.h>
#include <stdlib.h>

#include "dvd.h"
#include "irse.h"
#include <util.h>

static s32 payloadPrintf(const char* format, ...)
{
    va_list args;
	va_start(args, format);
    irse::VLog(LogS::Payload, LogL::INFO, format, args);
    va_end(args);
    return 0;
}

AppPayload::AppPayload(const ApploaderInfo& info)
{
    readPayload(info);

    irse::Log(LogS::Loader, LogL::INFO,
        "Calling payload START %p", reinterpret_cast<void*>(init_func));
    assert(info.payload_entrypoint);
    info.payload_entrypoint(&init_func, &main_func, &final_func);

    irse::Log(LogS::Loader, LogL::INFO,
        "Calling payload INIT %p", reinterpret_cast<void*>(init_func));
    assert(init_func);
    init_func(&payloadPrintf);
}

void AppPayload::readPayload(const ApploaderInfo& info)
{
    if (info.payload_size == 0) {
        irse::Log(LogS::Loader, LogL::ERROR, "Was unable to read payload");
        abort();
    }
    
    irse::Log(LogS::Loader, LogL::INFO, "Reading apploader payload..");
        
    void* payload_addr = reinterpret_cast<void*>(0x81200000);
        
    DVD::UniqueCommand cmd;
    assert(cmd.cmd() != nullptr);
    DVDLow::EncryptedReadAsync(*cmd.cmd(), payload_addr,
        round_up(info.payload_size, 32), 0x2460 / 4);
    const auto result = cmd.cmd()->syncReply();

    if (result != DiErr::OK) {
        irse::Log(LogS::Loader, LogL::ERROR, "Failed to read info block");
        abort();
    }

    irse::Log(LogS::Loader, LogL::INFO,
        "Apploader info: %p", reinterpret_cast<const void*>(&info));
    irse::Log(LogS::Loader, LogL::INFO,
        "Entrypoint: %p", reinterpret_cast<void*>(info.payload_entrypoint));
}

std::optional<AppPayload::CopyCommand> AppPayload::popCopyCommand()
{
    CopyCommand cmd;
    irse::Log(LogS::Loader, LogL::INFO,
        "Calling main %p", reinterpret_cast<void*>(main_func));
    assert(main_func);
    assert(*(u32*)main_func);
    const bool not_exhausted = main_func(&cmd.dest, &cmd.length, &cmd.offset);

    if (!not_exhausted)
        return std::nullopt;

    return cmd;
}
