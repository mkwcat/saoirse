#pragma once
#include "AppInfo.hpp"
#include <assert.h>
#include <optional>

struct PayloadFunctions {
    ApploaderInfo::InitFunction init_func = nullptr;
    ApploaderInfo::MainFunction main_func = nullptr;
    ApploaderInfo::FinalFunction final_func = nullptr;

    void setLogCallback(ApploaderInfo::LogFunction callback);
    bool takeCopyCommand(void*& dest, int& size, int& offset);
    EntryPoint queryEntrypoint() const;
};

// The partition must be open
//
// Aborts on failure.
PayloadFunctions ReadApploaderPayload(const ApploaderInfo& info);

class AppPayload
{
public:
    // The partition must be open
    AppPayload(const ApploaderInfo& info);

private:
    PayloadFunctions mFunctions;

public:
    struct CopyCommand {
        void* dest;
        int length;
        int offset;
    };

    std::optional<CopyCommand> popCopyCommand();

    auto get_entrypoint() const
    {
        return mFunctions.queryEntrypoint();
    }
};