#include "AppPayload.hpp"
#include <stdio.h>
#include <stdlib.h>

#include "dvd.h"

template <typename T> static inline T round_up(T num, unsigned int align) {
    return (num + align - 1) & -align;
}

template <typename T> static inline T round_down(T num, unsigned int align) {
    return num & -align;
}

AppPayload::AppPayload(const ApploaderInfo& info) {
    readPayload(info);
    printf("Calling payload START %p\n", reinterpret_cast<void*>(init_func));
    assert(info.payload_entrypoint);
    info.payload_entrypoint(&init_func, &main_func, &final_func);
    printf("Calling payload INIT %p\n", reinterpret_cast<void*>(init_func));
    assert(init_func);
    init_func(&printf);
}

void AppPayload::readPayload(const ApploaderInfo& info) {
    if (info.payload_size == 0) {
        printf("Was unable to read payload.\n");
        abort();
    }
    
    printf("Reading apploader payload..\n");
        
    void* payload_addr = reinterpret_cast<void*>(0x81200000);
        
    DVD::UniqueCommand cmd;
    assert(cmd.cmd() != nullptr);
    DVDLow::EncryptedReadAsync(*cmd.cmd(), payload_addr, round_up(info.payload_size, 32), 0x2460 / 4);
    const auto result = DVDLow::SyncReply(cmd.cmd());

    if (result != DiErr::OK) {
        printf("Failed to read info block\n");
        abort();
    }

    printf("Apploader info: %p\n", reinterpret_cast<const void*>(&info));
    printf("Entrypoint: %p\n", reinterpret_cast<void*>(info.payload_entrypoint));
}

std::optional<AppPayload::CopyCommand> AppPayload::popCopyCommand() {
    CopyCommand cmd;
    printf("Calling main %p\n", reinterpret_cast<void*>(main_func));
    assert(main_func);
    assert(*(u32*)main_func);
    const bool not_exhausted = main_func(&cmd.dest, &cmd.length, &cmd.offset);

    if (!not_exhausted)
        return std::nullopt;

    return cmd;
}
