#pragma once

#include "AppInfo.hpp"
#include <optional>
#include <assert.h>

class AppPayload {
public:
    // The partition must be open
    void init(const ApploaderInfo& info);

private:
    ApploaderInfo::InitFunction init_func;
    ApploaderInfo::MainFunction main_func;
    ApploaderInfo::FinalFunction final_func;

    // The partition must be open
    void readPayload(const ApploaderInfo& info);

public:
    struct CopyCommand {
        void* dest;
        int length;
        int offset;
    };

    std::optional<CopyCommand> popCopyCommand();

    auto get_entrypoint() const {
        assert(final_func);
        return final_func();
    }
};