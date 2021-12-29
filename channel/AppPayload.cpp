#include "AppPayload.hpp"
#include <stdio.h>
#include <stdlib.h>

#include "dvd.h"
#include "irse.h"
#include <util.h>

void PayloadFunctions::setLogCallback(ApploaderInfo::LogFunction callback)
{
    irse::Log(LogS::Loader, LogL::INFO, "Calling payload INIT %p",
              reinterpret_cast<void*>(init_func));
    assert(init_func);
    assert(*(u32*)init_func);
    init_func(callback);
}

bool PayloadFunctions::takeCopyCommand(void*& dest, int& size, int& offset)
{
    irse::Log(LogS::Loader, LogL::INFO, "Calling payload MAIN %p",
              reinterpret_cast<void*>(main_func));
    assert(main_func);
    assert(*(u32*)main_func);
    return main_func(&dest, &size, &offset);
}

EntryPoint PayloadFunctions::queryEntrypoint() const
{
    irse::Log(LogS::Loader, LogL::INFO, "Calling payload FINAL %p",
              reinterpret_cast<void*>(final_func));
    assert(final_func);
    assert(*(u32*)final_func);
    return final_func();
}

// The partition must be open
static ApploaderInfo::EntryFunction
ReadApploaderFromDisc(const ApploaderInfo& info)
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

    irse::Log(LogS::Loader, LogL::INFO, "Apploader info: %p",
              reinterpret_cast<const void*>(&info));
    irse::Log(LogS::Loader, LogL::INFO, "Entrypoint: %p",
              reinterpret_cast<void*>(info.payload_entrypoint));

    return info.payload_entrypoint;
}

static PayloadFunctions
QueryPayloadFunctions(const ApploaderInfo::EntryFunction payload_entrypoint)
{
    irse::Log(LogS::Loader, LogL::INFO, "Calling payload START %p",
              reinterpret_cast<void*>(payload_entrypoint));
    assert(payload_entrypoint);

    PayloadFunctions functions;
    payload_entrypoint(&functions.init_func, &functions.main_func,
                       &functions.final_func);

    return functions;
}

PayloadFunctions ReadApploaderPayload(const ApploaderInfo& info) {
    auto* main_func = ReadApploaderFromDisc(info);
    assert(main_func != nullptr);

    // Query the functions from the apploader
    return QueryPayloadFunctions(main_func);
}

static s32 AppPayload_Printf(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    irse::VLog(LogS::Payload, LogL::INFO, format, args);
    va_end(args);
    return 0;
}

AppPayload::AppPayload(const ApploaderInfo& info)
{
    mFunctions = ReadApploaderPayload(info);

    // Intercept payload logging
    mFunctions.setLogCallback(&AppPayload_Printf);
}

std::optional<AppPayload::CopyCommand> AppPayload::popCopyCommand()
{
    CopyCommand cmd;
    bool not_exhausted =
        mFunctions.takeCopyCommand(cmd.dest, cmd.length, cmd.offset);

    if (!not_exhausted)
        return std::nullopt;

    return cmd;
}
